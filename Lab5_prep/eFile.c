#include "eFile.h"
#include "eDisk.h"

#include <string.h>

unsigned char _blockBuff[BLOCK_SIZE];
unsigned char _llPts[PT_SIZE * 2];  // next and prev pointers for blocks
eFile_File myFile;  // scratch file struct
unsigned int numFiles = 0;

//---------- eFile_Init-----------------
// Activate the file system, without formating
// Input: none
// Output: 0 if successful and 1 on failure (already initialized)
// since this program initializes the disk, it must run with 
//    the disk periodic task operating
int eFile_Init(void) {
  // initialize file system
  int i;
  unsigned short pt;
  eDisk_Init(DRIVE);
  eDisk_ReadBlock(_blockBuff, DIRECTORY);
  // create free space manager entry in the directory
  strcpy(myFile.name, "spc_mgr");
  myFile.firstBlock = 1;
  myFile.lastBlock = BLOCKS - 1;
  myFile.size = 0;
  // write the space manager to the first entry in the directory
  memcpy(_blockBuff, &myFile, sizeof(myFile));
  eDisk_WriteBlock(_blockBuff, DIRECTORY);
  // make the dll of free blocks
  for(i = 1; i < BLOCKS; i++) {
    eDisk_Read(DRIVE, _llPts, i, PT_SIZE * 2);
    // nextpt
    if(i != BLOCKS - 1) {
      // assuming little endian
      pt = i + 1;
      _llPts[0] = pt & 0xFF;
      _llPts[1] = (pt >> 8) & 0xFF;
    }
    else {
      _llPts[0] = _llPts[1] = 0;  // end of list
    }
    // prevpt
    if(i != 1) {
      pt = i - 1;
      _llPts[2] = pt & 0xFF;
      _llPts[3] = (pt >> 8) & 0xFF;
    }
    else {
      _llPts[2] = _llPts[3] = 0;  // beginning of list
    }
    eDisk_Write(DRIVE, _llPts, i, PT_SIZE * 2);
    
  }
  return 0;
}

//---------- eFile_Format-----------------
// Erase all files, create blank directory, initialize free space manager
// Input: none
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
int eFile_Format(void) {
  // erase disk, add format
  int i;
  _eFile_ClearBlockBuff();
  for(i = 0; i < BLOCKS; i++) {
    eDisk_WriteBlock(_blockBuff, i);
  }
  eFile_Init();
  return 0;
}

//---------- eFile_Create-----------------
// Create a new, empty file with one allocated block
// Input: file name is an ASCII string up to seven characters 
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
int eFile_Create(char name[FILE_NAME_SIZE]) {
  // create new file, make it empty 
//   return eDisk_Write(DRIVE, name, DIRECTORY, FILE_NAME_SIZE);
//   BYTE drv,         // Physical drive number (0)
//   const BYTE *buff, // Pointer to the data to be written
//   DWORD sector,     // Start sector number (LBA)
//   BYTE count);      // Sector count (1..255)
}


//---------- eFile_WOpen-----------------
// Open the file, read into RAM last block
// Input: file name is a single ASCII letter
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
int eFile_WOpen(char name[FILE_NAME_SIZE]) {
  // open a file for writing
  
  return 0;
}

//---------- eFile_Write-----------------
// save at end of the open file
// Input: data to be saved
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
int eFile_Write(char data) {

  return 0;
}  

//---------- eFile_Close-----------------
// Deactivate the file system
// Input: none
// Output: 0 if successful and 1 on failure (not currently open)
int eFile_Close(void) {

  return 0;  
}


//---------- eFile_WClose-----------------
// close the file, left disk in a state power can be removed
// Input: none
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
int eFile_WClose(void) {
  // close the file for writing
  
  return 0;
}

//---------- eFile_ROpen-----------------
// Open the file, read first block into RAM 
// Input: file name is a single ASCII letter
// Output: 0 if successful and 1 on failure (e.g., trouble read to flash)
int eFile_ROpen(char name[FILE_NAME_SIZE]) {
  // open a file for reading 
  
  return 0;
}
   
//---------- eFile_ReadNext-----------------
// retreive data from open file
// Input: none
// Output: return by reference data
//         0 if successful and 1 on failure (e.g., end of file)
int eFile_ReadNext(char *pt) {
  // get next byte
  
  return 0;
}
                              
//---------- eFile_RClose-----------------
// close the reading file
// Input: none
// Output: 0 if successful and 1 on failure (e.g., wasn't open)
int eFile_RClose(void) {
  // close the file for writing
  
  return 0;
}

//---------- eFile_Directory-----------------
// Display the directory with filenames and sizes
// Input: pointer to a function that outputs ASCII characters to display
// Output: characters returned by reference
//         0 if successful and 1 on failure (e.g., trouble reading from flash)
int eFile_Directory(void(*fp)(unsigned char)) {

  return 0;
}  

//---------- eFile_Delete-----------------
// delete this file
// Input: file name is a single ASCII letter
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
int eFile_Delete(char name[FILE_NAME_SIZE]) {
  // remove this file 
  
  return 0;
}

//---------- eFile_RedirectToFile-----------------
// open a file for writing 
// Input: file name is a single ASCII letter
// stream printf data into file
// Output: 0 if successful and 1 on failure (e.g., trouble read/write to flash)
int eFile_RedirectToFile(char *name) {
  
  return 0;
}

//---------- eFile_EndRedirectToFile-----------------
// close the previously open file
// redirect printf data back to UART
// Output: 0 if successful and 1 on failure (e.g., wasn't open)
int eFile_EndRedirectToFile(void) {
  
  return 0;
}


void _eFile_ClearBlockBuff(void) {
  memset(_blockBuff, 0, sizeof(unsigned char) * BLOCK_SIZE);
}
