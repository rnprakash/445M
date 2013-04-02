#include "eFile.h"
#include "eDisk.h"
#include "retarget.h"
#include "hw_types.h"
#include "lm3s8962.h"
#include <string.h>

static unsigned char _blockBuff[BLOCK_SIZE];
static unsigned int num_files = 0;
static eFile_File _eFile_List[MAX_FILES + 1];
eFile_File* _freeList = &_eFile_List[0];
static eFile_File* _wFile;
static char _wOpen, _rOpen, _sysInit = 0;
static unsigned short _wIndex, _rIndex, _rSize, _wSector, _rSector;
static unsigned char _writeBuff[BLOCK_SIZE], _readBuff[BLOCK_SIZE];

//---------- eFile_Init-----------------
// Activate the file system, without formating
// disk periodic task needs to be running for this to work
// Input: none
// Output: 0 if successful and 1 on failure (already initialized)
// since this program initializes the disk, it must run with 
//    the disk periodic task operating
int eFile_Init(void) {
  // initialize file system
	int i;
  volatile unsigned long delay;
	if(_sysInit)
		return 1;
  eDisk_Init(DRIVE);
  eDisk_ReadBlock(_blockBuff, DIRECTORY);
	memcpy(_eFile_List, _blockBuff, sizeof(eFile_File) * MAX_FILES);
	//count files
	num_files = 0;
	for(i = 1; i < MAX_FILES; i++)
		if(_eFile_List[i].name[0])
			num_files++;
	_wOpen = _rOpen = 0;
	_sysInit = 1;
  // profiling code : set PB0 low when reading, high otherwise
  SYSCTL_RCGC2_R |= SYSCTL_RCGC2_GPIOB;
  delay = SYSCTL_RCGC2_R;
  GPIO_PORTB_DIR_R |= 0x01; // output
  GPIO_PORTB_DEN_R |= 0x01;  // digital mode
  GPIO_PORTB_DATA_R |= 0x01;  // initialize high
	return 0;
}

void _eFile_MakeFreeList(void)
{
	// initialize file system
  int i;
  unsigned short pt;
  
  // make the dll of free blocks
  for(i = 1; i < BLOCKS; i++) {
		_eFile_ClearBlockBuff();
//    eDisk_ReadBlock(_blockBuff, i);
    // nextpt
    if(i != BLOCKS - 1) {
      // assuming little endian
      pt = i + 1;
      _blockBuff[0] = pt & 0xFF;
      _blockBuff[1] = (pt >> 8) & 0xFF;
    }
    else {
      _blockBuff[0] = _blockBuff[1] = 0;  // end of list
    }
    eDisk_WriteBlock(_blockBuff, i);
  }
}

//---------- eFile_Format-----------------
// Erase all files, create blank directory, initialize free space manager
// Input: none
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
int eFile_Format(void)
{
	eFile_WClose();
	eFile_RClose();
  // erase disk, add format
	memset(_eFile_List, 0, sizeof(eFile_File) * MAX_FILES);
  // create free space manager entry in the directory
  strcpy(_freeList->name, "spc_mgr");
  _freeList->firstBlock = 1;
  _freeList->lastBlock = BLOCKS - 1;
  _freeList->size = 0;
  // write the space manager to the first entry in the directory
	_eFile_ClearBlockBuff();
  memcpy(_blockBuff, _eFile_List, sizeof(eFile_File) * MAX_FILES);
  eDisk_WriteBlock(_blockBuff, DIRECTORY);
  _eFile_MakeFreeList();
	num_files = 0;
  return 0;
}

//---------- eFile_Create-----------------
// Create a new, empty file with one allocated block
// Input: file name is an ASCII string up to seven characters 
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
int eFile_Create(const char name[FILE_NAME_SIZE]) {
  // create new file, make it empty 
	int i;
	if(_freeList->firstBlock == 0 || _eFile_Find(name))
		return 1;
	
	for(i = 1; i < MAX_FILES; i++)
	{
		if(_eFile_List[i].firstBlock == 0) //free space in directory
		{
			eFile_File* file = &_eFile_List[i];
			char new_name[FILE_NAME_SIZE];
			memcpy(new_name, name, FILE_NAME_SIZE);
			new_name[FILE_NAME_SIZE - 1] = 0;
			strcpy(file->name, new_name);	// write file name
			file->firstBlock = file->lastBlock = _freeList->firstBlock; // first block is first available block
			_eFile_ClearBlockBuff();
			eDisk_ReadBlock(_blockBuff, _freeList->firstBlock);
			_freeList->firstBlock = (_blockBuff[0] | (_blockBuff[1] << 8)); //update freeList
			_eFile_ClearBlockBuff();
			memset(_blockBuff, 0, sizeof(_blockBuff[0]) * 2); // first 2 bytes are a null ptr
			_blockBuff[WRITE_INDEX] = WRITE_INDEX + 2;	// start writing to 5th byte
			_blockBuff[WRITE_INDEX + 1] = 0;
			eDisk_WriteBlock(_blockBuff, file->firstBlock); // set next to zero
			memcpy(_blockBuff, _eFile_List, sizeof(_blockBuff)); // copy dir
			eDisk_WriteBlock(_blockBuff, DIRECTORY); // write dir
			num_files++;
			return 0;
		}
	}
	return 1; // no free space
}

//---------- eFile_WOpen-----------------
// Open the file, read into RAM last block
// Input: file name is a single ASCII letter
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
int eFile_WOpen(const char name[FILE_NAME_SIZE]) {
  // open a file for writing
	eFile_File* file = _eFile_Find(name);
	if(_wOpen || (file == NULL && eFile_Create(name)))
		return 1;
	file = _eFile_Find(name);
	_wOpen = 1;
	eDisk_ReadBlock(_writeBuff, file->lastBlock);
	_wIndex = (_writeBuff[WRITE_INDEX] | (_writeBuff[WRITE_INDEX + 1] << 8));
	_wSector = file->lastBlock;
	_wFile = file;
  return 0;
}

//---------- eFile_Write-----------------
// save at end of the open file
// Input: data to be saved
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
int eFile_Write(const char data) {
	if(!_wOpen)
		return 1;
	if(_wIndex >= BLOCK_SIZE) // allocate a new block
	{
		if(_freeList->firstBlock == 0) // no free space
				return 1;
		_writeBuff[0] = (_freeList->firstBlock & 0xFF);
		_writeBuff[1] = (_freeList->firstBlock >> 8); // update next block
		_writeBuff[WRITE_INDEX] = ((BLOCK_SIZE + 1) & 0xFF);
		_writeBuff[WRITE_INDEX + 1] = ((BLOCK_SIZE + 1) >> 8);
		_wFile->lastBlock = _freeList->firstBlock;
		eDisk_WriteBlock(_writeBuff, _wSector); // commit finished block
		_wSector = _freeList->firstBlock; // update sector to write to
		eDisk_ReadBlock(_writeBuff, _wSector); // get new block
		_writeBuff[WRITE_INDEX] = WRITE_INDEX + 2;	// start writing to 5th byte
		_writeBuff[WRITE_INDEX + 1] = 0;
		_freeList->firstBlock = (_writeBuff[0] | (_writeBuff[1] << 8)); //update freeList
		_wIndex = WRITE_INDEX + 2; // start writing to 5th byte
	}
	_writeBuff[_wIndex++] = data;
	_wFile->size++;
  return 0;
}

//---------- eFile_Close-----------------
// Deactivate the file system
// Input: none
// Output: 0 if successful and 1 on failure (not currently open)
int eFile_Close(void) {
	if(!_sysInit)
		return 1;
	if(_wOpen || _rOpen)
	{
		eFile_WClose();
		eFile_RClose();
	}
	// TODO: write directory
	_eFile_ClearBlockBuff();
	memcpy(_blockBuff, _eFile_List, sizeof(_blockBuff));
	eDisk_WriteBlock(_blockBuff, DIRECTORY);
  return (_sysInit = 0);
}

//---------- eFile_WClose-----------------
// close the file, left disk in a state power can be removed
// Input: none
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
int eFile_WClose(void) {
  // close the file for writing
  if(!_wOpen)
		return 1;
	_writeBuff[WRITE_INDEX] = _wIndex & 0xFF;
	_writeBuff[WRITE_INDEX + 1] = (_wIndex >> 8);
	eDisk_WriteBlock(_writeBuff, _wSector);
	_eFile_ClearBlockBuff();
	memcpy(_blockBuff, _eFile_List, sizeof(_blockBuff)); // copy dir
	eDisk_WriteBlock(_blockBuff, DIRECTORY);
	_wFile = NULL;
	return (_wOpen = 0);
}

//---------- eFile_ROpen-----------------
// Open the file, read first block into RAM 
// Input: file name is a single ASCII letter
// Output: 0 if successful and 1 on failure (e.g., trouble read to flash)
int eFile_ROpen(const char name[FILE_NAME_SIZE]) {
  // open a file for reading 
  eFile_File* file = _eFile_Find(name);
	if(_rOpen || file == NULL)
		return 1;
	_rOpen = 1;
	eDisk_ReadBlock(_readBuff, file->firstBlock);
	_rSize = (_readBuff[WRITE_INDEX] | (_readBuff[WRITE_INDEX + 1] << 8));
	_rSector = file->firstBlock;
	_rIndex = WRITE_INDEX + 2; // start reading from 5th byte
  return 0;
}

//---------- eFile_ReadNext-----------------
// retreive data from open file
// Input: none
// Output: return by reference data
//         0 if successful and 1 on failure (e.g., end of file)
int eFile_ReadNext(char *pt) {
  // get next byte
  if(!_rOpen || _rIndex >= _rSize)
		return 1;
	if(_rIndex >= BLOCK_SIZE)
	{
		_rSector = (_readBuff[0] | (_readBuff[1] << 8)); // get next sector
		if(_rSector == 0) // sanity check
			return 1;
		eDisk_ReadBlock(_readBuff, _rSector); // load next block into ram
		_rIndex = WRITE_INDEX + 2; // start from 5th byte
		_rSize = (_readBuff[WRITE_INDEX] | (_readBuff[WRITE_INDEX + 1] << 8)); // load this block's size
	}
	*pt = _readBuff[_rIndex++];
  return 0;
}

//---------- eFile_RClose-----------------
// close the reading file
// Input: none
// Output: 0 if successful and 1 on failure (e.g., wasn't open)
int eFile_RClose(void) {
  // close the file for writing
  if(!_rOpen)
		return 1;
  return (_rOpen = 0);
}

//---------- eFile_Directory-----------------
// Display the directory with filenames and sizes
// Input: pointer to a function that outputs ASCII characters to display
// Output: characters returned by reference
//         0 if successful and 1 on failure (e.g., trouble reading from flash)
int eFile_Directory(int(*fp)(const char *format, ...)) {
	int i;
	fp("%d B\t.\n", sizeof(eFile_File) * num_files);
	for(i = 1; i < MAX_FILES; i++)
	{
		eFile_File* file = &_eFile_List[i];
		if(file->firstBlock)
			fp("%d B\t%s\n", file->size, file->name);
	}
  return 0;
}

int eFile_List(char list[MAX_FILES][8])
{
	int i, num = 0;
	for(i = 1; i < MAX_FILES; i++)
	{
		eFile_File* file = &_eFile_List[i];
		if(file->firstBlock)
			strcpy(list[num++], file->name);
	}
	list[num][0] = 0;
	return 0;
}

//---------- eFile_Delete-----------------
// delete this file
// Input: file name is a single ASCII letter
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
int eFile_Delete(const char name[FILE_NAME_SIZE]) {
  // remove this file 
  eFile_File* file = _eFile_Find(name);
	if(file == NULL || (_wFile != NULL && strcmp(name, _wFile->name) == 0))
		return 1;
	eDisk_ReadBlock(_blockBuff, _freeList->lastBlock);
	_blockBuff[0] = (file->firstBlock & 0xFF);
	_blockBuff[1] = (file->firstBlock >> 8);
	eDisk_WriteBlock(_blockBuff, _freeList->lastBlock);
	file->firstBlock = 0;
	file->size = 0;
	strcpy(file->name, "");
	_eFile_ClearBlockBuff();
	memcpy(_blockBuff, _eFile_List, sizeof(eFile_File) * MAX_FILES);
	eDisk_WriteBlock(_blockBuff, DIRECTORY);
	num_files--;
  return 0;
}

//---------- eFile_RedirectToFile-----------------
// open a file for writing 
// Input: file name is a single ASCII letter
// stream printf data into file
// Output: 0 if successful and 1 on failure (e.g., trouble read/write to flash)
int eFile_RedirectToFile(const char *name) {
  if(eFile_WOpen(name)) // creates file if doesn't exist
		return 1;  // cannot open file
	RT_StreamToFile(1);
  return 0;
}

//---------- eFile_EndRedirectToFile-----------------
// close the previously open file
// redirect printf data back to UART
// Output: 0 if successful and 1 on failure (e.g., wasn't open)
int eFile_EndRedirectToFile(void) {
  RT_StreamToFile(0);
	if(eFile_WClose())
		return 1;
  return 0;
}


static void _eFile_ClearBlockBuff(void) {
  memset(_blockBuff, 0, sizeof(unsigned char) * BLOCK_SIZE);
}

static eFile_File* _eFile_Find(const char name[FILE_NAME_SIZE])
{
	int i;
	for(i = 1; i < MAX_FILES; i++)
		if(strcmp(_eFile_List[i].name, name) == 0)
			return &_eFile_List[i];
	return NULL;
}

// test the linked list of free blocks after a call to format
int _eFile_TestFormat(void) {
  int i;
  for(i = 1; i < BLOCKS; i++) {
    int nextBlock = (i < BLOCKS - 1) ? i + 1 : 0;
    // read the i'th block
    _eFile_ClearBlockBuff();
    eDisk_ReadBlock(_blockBuff, i);
    // test that it points to the (i + 1)'th block, or if it is the last block points to 0
    if(_blockBuff[0] != (nextBlock & 0xFF)) {
      return 1;
    }
    if(_blockBuff[1] != (nextBlock >> 8) & 0xFF) {
      return 1;
    }
  }
  return 0;
}
