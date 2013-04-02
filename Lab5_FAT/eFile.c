#include "eFile.h"
#include "eDisk.h"
#include "retarget.h"
#include "ctype.h"
#include <stdio.h>
#include <string.h>

#define memcpy memmove

static unsigned char _blockBuff[BLOCK_SIZE];
unsigned int num_files = 0;
eFile_File _wFile, _rFile, _file;
char _wOpen, _rOpen, _sysInit = 0;
unsigned int _wIndex, _wCluster, _wSector, _wSize, _dirIndex, _wDirIndex;
unsigned int _rIndex, _rCluster, _rSector, _rSize;
unsigned char _writeBuff[BLOCK_SIZE], _readBuff[BLOCK_SIZE];

static unsigned short FAT_OFFSET = 0;
static unsigned char	SECT_PER_CLUSTER = 0;
static unsigned char	NUM_FATS = 0;
static unsigned int		FAT_SIZE = 0;
static unsigned int		ROOT_OFFSET = 0;
static unsigned int		_dir = 0;
static unsigned int		_cluster = 0;

//---------- eFile_Init-----------------
// Activate the file system, without formating
// disk periodic task needs to be running for this to work
// Input: none
// Output: 0 if successful and 1 on failure (already initialized)
// since this program initializes the disk, it must run with 
//    the disk periodic task operating
int eFile_Init(void) {
  // initialize file system
	if(_sysInit)
		return 1;
  if(eDisk_Init(DRIVE))
	{
		//fprintf(stderr, "Error reading disk\n");
		return 1;
	}
  eDisk_ReadBlock(_blockBuff, BOOT_SECTOR);
	
	SECT_PER_CLUSTER = _blockBuff[SECT_PER_CLUST_INDEX];
	FAT_OFFSET = (_blockBuff[RES_SECT_INDEX + 1] << 8) | _blockBuff[RES_SECT_INDEX];
	NUM_FATS = _blockBuff[NUM_FATS_INDEX];
	FAT_SIZE = (_blockBuff[FAT_SIZE_INDEX + 3] << 24)
					 | (_blockBuff[FAT_SIZE_INDEX + 2] << 16)
					 | (_blockBuff[FAT_SIZE_INDEX + 1] << 8)
					 | _blockBuff[FAT_SIZE_INDEX]; // fat size in sectors
	_dir = ROOT_OFFSET = FAT_OFFSET + (NUM_FATS * FAT_SIZE);
	_cluster = 2;
	_sysInit = 1;
	return 0;
}

int eFile_Info(void)
{
	CHECK_DISK // check disk state
	
	printf("Sectors per cluster: %d\n", SECT_PER_CLUSTER);
	printf("FAT start index: %d\n", FAT_OFFSET);
	printf("Number of FATs: %d\n", NUM_FATS);
	printf("Sectors per FAT: %d\n", FAT_SIZE);
	printf("Root start index: %d\n", ROOT_OFFSET);
	return 0;
}

//---------- eFile_Format-----------------
// Erase all files, create blank directory, initialize free space manager
// Input: none
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
int eFile_Format(void)
{
	CHECK_DISK // check disk state
	
	eFile_WClose();
	eFile_RClose();
	
	_eFile_ClearBlockBuff();
	_blockBuff[0] = _blockBuff[32] = 0xE5;
	eDisk_WriteBlock(_blockBuff, _dir);
	_eFile_ClearBlockBuff();
	memset(_blockBuff, 0xFF, 16);
	_blockBuff[0] = 0xF8;
	_blockBuff[3] = _blockBuff[7] = 
		_blockBuff[11] = _blockBuff[15] = 0x0F;
	eDisk_WriteBlock(_blockBuff, FAT_OFFSET);
	
  return 0;
}

//---------- eFile_Create-----------------
// Create a new, empty file with one allocated block
// Input: file name is an ASCII string up to seven characters 
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
int eFile_Create(const char name[FILE_NAME_SIZE], char attr)
{
	int cluster, i;
	
	CHECK_DISK // check disk state
	
	memset(&_file, 0, sizeof(eFile_File));
	_file = _eFile_Find(name, 0);
	if(_file.name[0]) // exists
		return 1;
	
	_eFile_FATName(name, _file.name, &toupper, 0); // somewhat unsafe; overflows to ext
	_file.attr = attr; // archive is 0x20, dir is 0x10
	cluster = _eFile_FreeCluster(); // also marks as in-use
	if(attr == 0x10)
			cluster += 2;
	_file.cluster[0] = cluster & 0xFF;
	_file.cluster[1] = (cluster >> 8) & 0xFF;
	_file.hiCluster[0] = (cluster >> 16) & 0xFF;
	_file.hiCluster[1] = (cluster >> 24) & 0xFF;
	memset(_file.size, 0, 4);
	
	if(cluster == -1)
		return 1; // no free space
	
	eDisk_ReadBlock(_blockBuff, _dir);
	// write two empty files
	if(!_blockBuff[0])
		_blockBuff[0] = 0xe5;
	if(!_blockBuff[32])
		_blockBuff[32] = 0xe5;
	
	for(i = 64; i < 512; i += 32) // doesn't support large dirs
	{
		if(_blockBuff[i] != 0xe5 && _blockBuff[i] != 0x00)
			continue;
		memcpy(&_blockBuff[i], &_file, sizeof(eFile_File));
		eDisk_WriteBlock(_blockBuff, _dir);
		if(attr == 0x10) // make directory
		{
			// directory "."
			memset(&_file, 0, 32);
			memset(&_file.name[1], 0x20, 10);
			_file.name[0] = '.';
			_file.attr = 0x10;
			memset(&_file.size[0], 0, 4);
			_file.cluster[0] = cluster & 0xFF;
			_file.cluster[1] = (cluster >> 8) & 0xFF;
			_file.hiCluster[0] = (cluster >> 16) & 0xFF;
			_file.hiCluster[1] = (cluster >> 24) & 0xFF;
			memset(_blockBuff, 0, 512);
			memcpy(_blockBuff, &_file, 32);
			// directory ".."
			memset(&_file, 0, 32);
			memset(&_file.name[2], 0x20, 9);
			_file.name[0] = _file.name[1] = '.';
			_file.attr = 0x10;
			memset(&_file.size[0], 0, 4);
			_file.cluster[0] = _cluster & 0xFF;
			_file.cluster[1] = (_cluster >> 8) & 0xFF;
			_file.hiCluster[0] = (_cluster >> 16) & 0xFF;
			_file.hiCluster[1] = (_cluster >> 24) & 0xFF;
			memcpy(&_blockBuff[32], &_file, 32);
			eDisk_WriteBlock(_blockBuff, (ROOT_OFFSET + SECT_PER_CLUSTER*(cluster-2)));
		}
		return 0;
	}
	
	return 1; // no space in dir
}

//---------- eFile_WOpen-----------------
// Open the file, read into RAM last block
// Input: file name is a single ASCII letter
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
int eFile_WOpen(const char name[FILE_NAME_SIZE])
{
	CHECK_DISK // check disk state
	
	memset(&_file, 0, sizeof(eFile_File));
	if(_wOpen)
		return 1;
	
	_file = _eFile_Find(name, 0);
	if(!_file.name[0])
		eFile_Create(name, 0x20);
	
	_wFile = _file;
	_wDirIndex = _dirIndex;
	_wCluster = (_file.cluster[0] | (_file.cluster[1] << 8));
	_wSize = _file.size[0] | (_file.size[1] << 8) |
					(_file.size[1] << 16) | (_file.size[3] << 24);
	_wIndex = _wSize % 512;
	// seek to end of file
	eDisk_ReadBlock(_writeBuff, FAT_OFFSET + _wCluster / 128);
	while(1)
	{
		int next, // next cluster
				index;
		index = (_wCluster % 128) * 4;
		next = _writeBuff[index] | (_writeBuff[index + 1] << 8)
					 | (_writeBuff[index + 2] << 16) | (_writeBuff[index + 3] << 24);
		if((next & 0x0FFFFFF8) == 0x0FFFFFF8)
			break; // last block
		if((FAT_OFFSET + _wCluster / 128) != (FAT_OFFSET + next / 128))
			eDisk_ReadBlock(_writeBuff, FAT_OFFSET + next / 128);
		_wCluster = next;
	}
	_wSector = ROOT_OFFSET + SECT_PER_CLUSTER*(_wCluster - 2);
	eDisk_ReadBlock(_writeBuff, _wSector);
	_wOpen = 1;
  return 0;
}

//---------- eFile_Write-----------------
// save at end of the open file
// Input: data to be saved
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
int eFile_Write(const char data)
{
	CHECK_DISK // check disk state
	
	if(!_wOpen)
		return 1;
	
	if(_wIndex == 512) // need to allocate more data
	{
		_wSector++;
		if(_wSector % SECT_PER_CLUSTER == 0)
		{
			// allocate new cluster from FAT
			int next = _eFile_FreeCluster();
			if(next == -1)
				return 1;
			eDisk_WriteBlock(_writeBuff, _wSector);
			eDisk_ReadBlock(_blockBuff, FAT_OFFSET + _wCluster / 128);
			_blockBuff[(_wCluster % 128) * 4] = next & 0xFF;
			_blockBuff[(_wCluster % 128) * 4 + 1] = (next >> 8) & 0xFF;
			_blockBuff[(_wCluster % 128) * 4 + 2] = (next >> 16) & 0xFF;
			_blockBuff[(_wCluster % 128) * 4 + 3] = (next >> 24) & 0xFF;
			eDisk_WriteBlock(_blockBuff, FAT_OFFSET + _wCluster / 128);
			_wCluster = next;
			_wSector = ROOT_OFFSET + SECT_PER_CLUSTER*(_wCluster - 2);
			eDisk_ReadBlock(_writeBuff, _wSector);
			_wIndex = 0;
		}
		else
		{
			eDisk_WriteBlock(_writeBuff, _wSector - 1);
			eDisk_ReadBlock(_writeBuff, _wSector);
			_wIndex = 0;
		}
	}
	_writeBuff[_wIndex++] = data;
	_wSize++;
  return 0;
}

//---------- eFile_Close-----------------
// Deactivate the file system
// Input: none
// Output: 0 if successful and 1 on failure (not currently open)
int eFile_Close(void)
{
	CHECK_DISK // check disk state

	if(_wOpen || _rOpen)
	{
		eFile_WClose();
		eFile_RClose();
	}	
  return (_sysInit = 0);
}

//---------- eFile_WClose-----------------
// close the file, left disk in a state power can be removed
// Input: none
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
int eFile_WClose(void)
{
	CHECK_DISK // check disk state
	
  // close the file for writing
  if(!_wOpen)
		return 1;
	eDisk_WriteBlock(_writeBuff, _wSector);
	_wFile.size[0] = _wSize & 0xFF;
	_wFile.size[1] = (_wSize >> 8) & 0xFF;
	_wFile.size[2] = (_wSize >> 16) & 0xFF;
	_wFile.size[3] = (_wSize >> 24) & 0xFF;
	eDisk_ReadBlock(_blockBuff, _dir);
	memcpy(&_blockBuff[_wDirIndex], &_wFile, 32 /* bytes */);
	eDisk_WriteBlock(_blockBuff, _dir);
	return (_wOpen = 0);
}

//---------- eFile_ROpen-----------------
// Open the file, read first block into RAM 
// Input: file name is a single ASCII letter
// Output: 0 if successful and 1 on failure (e.g., trouble read to flash)
int eFile_ROpen(const char name[FILE_NAME_SIZE])
{
	CHECK_DISK // check disk state
	
  // open a file for reading 
	memset(&_file, 0, sizeof(eFile_File));
	if(_rOpen)
		return 1;
	
	_file = _eFile_Find(name, 0);
	if(!_file.name[0])
		return 1;
	
	_rFile = _file;
	_rCluster = (_file.cluster[0] | (_file.cluster[1] << 8) |
							(_file.hiCluster[0] << 16) | (_file.hiCluster[1] << 24));
	_rSector = ROOT_OFFSET + SECT_PER_CLUSTER*(_rCluster - 2);
	_rIndex = 0;
	eDisk_ReadBlock(_readBuff, _rSector);
	_rOpen = 1;
	_rSize = 0;
  return 0;
}

//---------- eFile_ReadNext-----------------
// retreive data from open file
// Input: none
// Output: return by reference data
//         0 if successful and 1 on failure (e.g., end of file)
int eFile_ReadNext(char *pt)
{
	int size = (_rFile.size[3] << 24) | (_rFile.size[2] << 16)
						 | (_rFile.size[1] << 8) | _rFile.size[0];
	
	CHECK_DISK // check disk state
  if(_rSize >= size)
		return 1;
	
	if(_rIndex == 512)
	{
		// get next sector
		_rSector++;
		if(_rSector % SECT_PER_CLUSTER == 0)
		{
			// fetch next cluster from FAT
			int next, c;
			eDisk_ReadBlock(_blockBuff, FAT_OFFSET + _rCluster / 128);
			c = (_rCluster % 128) * 4;
			next = (_blockBuff[c + 3] << 24) | (_blockBuff[c + 2] << 16)
					 | (_blockBuff[c + 1] << 8)  | _blockBuff[c];
			if((next & 0x0FFFFFF8) == 0x0FFFFFF8) // no more clusters
				return 1;
			_rCluster = next;
			_rSector = ROOT_OFFSET + SECT_PER_CLUSTER*(_rCluster - 2);
			eDisk_ReadBlock(_readBuff, _rSector);
			_rIndex = 0;
		}
		else
		{
			eDisk_ReadBlock(_readBuff, _rSector);
			_rIndex = 0;
		}
	}
	
	*pt = _readBuff[_rIndex++];
	_rSize++;
  return 0;
}

//---------- eFile_RClose-----------------
// close the reading file
// Input: none
// Output: 0 if successful and 1 on failure (e.g., wasn't open)
int eFile_RClose(void)
{
	CHECK_DISK // check disk state
	
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
int eFile_Directory(int(*fp)(const char *format, ...))
{	
	int offset = 0;
	
	CHECK_DISK // check disk state
	
	while(1)
	{
		int index;
		eDisk_ReadBlock(_blockBuff, _dir + offset);
		for(index = 0; index < 512; index += 32)
		{
			int i;
			unsigned char* pt = &_blockBuff[index];

			if(_blockBuff[index] == 0xE5)
				continue;
			else if(_blockBuff[index] == 0x00)
				return 0;
			for(i = 0; *pt != 0x20 && i < 8; i++, pt++)
				fp("%c", tolower(*pt));
			pt = &_blockBuff[index + 8];
			if(*pt != 0x20 && *pt != 0x00)
			{
				fp(".");
				for(i = 0; *pt != 0x20 && i < 4; i++, pt++)
					fp("%c", tolower(*pt));
			}
			
			if(_blockBuff[index + 11] & 0x10) // directory
			{
				fp("/\n");
				continue;
			}
			
			i = _blockBuff[index + 28] | (_blockBuff[index + 29] << 8) |
					(_blockBuff[index + 30] << 16) | (_blockBuff[index + 31] << 24);
			fp(": %d Bytes\n", i);
		}
		offset++;
	}
}

int eFile_List(char list[MAX_FILES][13])
{	
	int i, index;
	
	CHECK_DISK // check disk state
	
	eDisk_ReadBlock(_blockBuff, _dir);
	for(i = 0, index = 0; i < 512; i += 32)
	{
		int j, k;
		if(_blockBuff[i] == 0x00 || _blockBuff[i] == 0xe5)
			continue;
		for(j = 0; j < 8 && _blockBuff[i+j] != 0x20; j++)
		{
			list[index][j] = tolower(_blockBuff[i + j]);
			//printf("%c", list[index][j]);
		}
		list[index][j++] = '.';
		for(k = 8; k < 11 && _blockBuff[i+k] != 0x20; j++, k++)
			list[index][j] = tolower(_blockBuff[i + k]);
		index++;
	}
	return 0;
}

//---------- eFile_Delete-----------------
// delete this file
// Input: file name is a single ASCII letter
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
int eFile_Delete(const char name[FILE_NAME_SIZE])
{
	int cluster;
	
	CHECK_DISK // check disk state
	
  _file = _eFile_Find(name, 0);
	if(!_file.name[0])
		return 1; // does not exist
	
	_file.name[0] = 0xe5; // mark as deleted
	eDisk_ReadBlock(_blockBuff, _dir);
	memcpy(&_blockBuff[_dirIndex], &_file, 32 /* bytes */);
	eDisk_WriteBlock(_blockBuff, _dir);
	// reclaim FAT entries
	cluster = (_file.cluster[0] | (_file.cluster[1] << 8) |
							(_file.hiCluster[0] << 16) | (_file.hiCluster[1] << 24));
	eDisk_ReadBlock(_blockBuff, FAT_OFFSET + cluster / 128);
	while(1)
	{
		int next, // next cluster
				index;
		index = (cluster % 128) * 4;
		next = _blockBuff[index] | (_blockBuff[index + 1] << 8)
					 | (_blockBuff[index + 2] << 16) | (_blockBuff[index + 3] << 24);
		_blockBuff[index] = _blockBuff[index + 1] = 
			_blockBuff[index + 2] = _blockBuff[index + 3] = 0;
		if((next & 0x0FFFFFF8) == 0x0FFFFFF8)
			break; // last block
		if((FAT_OFFSET + cluster / 128) != (FAT_OFFSET + next / 128))
		{
			eDisk_WriteBlock(_blockBuff, FAT_OFFSET + cluster / 128);
			eDisk_ReadBlock(_blockBuff, FAT_OFFSET + next / 128);
		}
		cluster = next;
	}
	eDisk_WriteBlock(_blockBuff, FAT_OFFSET + cluster / 128);
	return 0;
}

//---------- eFile_RedirectToFile-----------------
// open a file for writing 
// Input: file name is a single ASCII letter
// stream printf data into file
// Output: 0 if successful and 1 on failure (e.g., trouble read/write to flash)
int eFile_RedirectToFile(const char *name)
{
	CHECK_DISK // check disk state
	
  if(eFile_WOpen(name)) // creates file if doesn't exist
		return 1;  // cannot open file
	RT_StreamToFile(1);
  return 0;
}

//---------- eFile_EndRedirectToFile-----------------
// close the previously open file
// redirect printf data back to UART
// Output: 0 if successful and 1 on failure (e.g., wasn't open)
int eFile_EndRedirectToFile(void)
{
	CHECK_DISK // check disk state
	
  RT_StreamToFile(0);
	if(eFile_WClose())
		return 1;
  return 0;
}


int eFile_ChangeDirectory(char newdir[13])
{
	int cluster, sector;
	
	CHECK_DISK // check disk state
	
	_file = _eFile_Find(newdir, 1);
	if(!_file.name[0] || _file.attr != 0x10)
		return 1;
	cluster = (_file.cluster[0] | (_file.cluster[1] << 8) |
							(_file.hiCluster[0] << 16) | (_file.hiCluster[1] << 24));
	_cluster = cluster;
	if(cluster == 0)
		cluster += 2;
	sector = ROOT_OFFSET + SECT_PER_CLUSTER*(cluster-2);
	_dir = sector;
	return 0;
}

static void _eFile_ClearBlockBuff(void) {
  memset(_blockBuff, 0, sizeof(unsigned char) * BLOCK_SIZE);
}

static eFile_File _eFile_Find(const char name[FILE_NAME_SIZE], int directory)
{
	eFile_File file;
	int index, done = 0;
	char n[FILE_NAME_SIZE];
	
	eDisk_ReadBlock(_blockBuff, _dir);
	_eFile_FATName(name, n, &tolower, directory); // convert name
	
	// search through files
	for(index = 64; index < 512; index += 32)
	{
		int i, found = 1;
		
		if(_blockBuff[index] == 0xE5)
			continue;
		else if(_blockBuff[index] == 0x00)
			break;		
		// compare name
		for(i = 0; i < 11; i++)
		{
			if(tolower(_blockBuff[index+i]) != n[i])
			{
				found = 0;
				break;
			}
		}
		if(!found)
			continue;
		// found it - populate struct
		memcpy(&file, &_blockBuff[index], 32 /* bytes */);
		done = 1;
		_dirIndex = index;
		break;
	}
	if(!done)
		file.name[0] = 0;
	return file;
}

static void _eFile_FATName(const char name[FILE_NAME_SIZE], char n[FILE_NAME_SIZE], int (*f)(int), int dir)
{
	int i, j;
	for(i = j = 0; i < 8; i++)
	{
		if((name[j] == '.' && !dir) || name[j] == 0x00)
			n[i] = ' ';
		else
			n[i] = f(name[j++]);
	}
	if((name[j] == '.' && !dir))
		j++;
	for(i = 8; i < 11; i++)
	{
		if((name[j] == '.' && !dir) || name[j] == 0x00)
			n[i] = ' ';
		else
			n[i] = f(name[j++]);
	}
}

static int _eFile_FreeCluster(void)
{
	// find free cluster in FAT and mark as allocated
	int i;
	for(i = 0; i < FAT_SIZE; i++)
	{
		int index;
		eDisk_ReadBlock(_blockBuff, i + FAT_OFFSET);
		for(index = 0; index < 512; index += 4)
		{
			if(_blockBuff[index] || _blockBuff[index + 1]
					|| _blockBuff[index + 2] || _blockBuff[index + 3])
				continue;
			_blockBuff[index] = _blockBuff[index + 1] = _blockBuff[index + 2] = 0xFF;
			_blockBuff[index + 3] = 0x0F;
			eDisk_WriteBlock(_blockBuff, i + FAT_OFFSET);
			return (i * 128 + index) / 4;
		}
	}
	return -1;
}
