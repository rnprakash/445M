// fileName ************** eFile.c *****************************
// Middle-level routines to implement a solid-state disk 
// Jonathan W. Valvano 3/16/11
#include <string.h>
#include <stdio.h>
#include "OS.h"
#include "eFile.h"
#include "edisk.h"
#include "UART2.h"


char State = 5;
char Path[15][15] = {/* Init  Format  Create  WOpen  Write  Close  WClose  ROpen ReadNext  RClose  Directory  Delete  RedirectToFile  endRedirectToFile*/
	/*column               0      1       2       3     4       5      6       7     8         9        10        11         12               13*/
	/*Init      row 0*/{   0,     1,      1,      1,    0,      1,     0,      1,    0,        0 ,       1,        1,         1,               0},
	/*Format    row 1*/{   0,     1,      1,      1,    0,      1,     0,      0,    0,        0,        1,        1,         1,               0},
	/*Create    row 2*/{   0,     1,      1,      1,    0,      1,     0,      0,    0,        0,        1,        1,         1,               0},
  /*WOpen     row 3*/{   0,     0,      0,      0,    1,      0,     1,      0,    0,        0,        0,        0,         0,               0},
  /*Write     row 4*/{   0,     0,      0,      0,    1,      0,     1,      0,    0,        0,        0,        0,         0,               1},
  /*Close     row 5*/{   1,     0,      0,      0,    0,      0,     0,      0,    0,        0,        0,        0,         0,               0},
  /*WClose    row 6*/{   0,     1,      1,      1,    0,      1,     0,      1,    0,        0 ,       1,        1,         1,               0},
  /*ROpen     row 7*/{   0,     0,      0,      0,    0,      0,     0,      0,    1,        1 ,       0,        0,         0,               0},
  /*ReadNext  row 8*/{   0,     0,      0,      0,    0,      0,     0,      0,    1,        1 ,       0,        0,         0,               0},
  /*RClose    row 9*/{   0,     1,      1,      1,    0,      1,     0,      1,    0,        0 ,       1,        1,         1,               0},
  /*Directory row10*/{   0,     1,      1,      1,    0,      1,     0,      1,    0,        0 ,       1,        1,         1,               0},
  /*Delete    row11*/{   0,     1,      1,      1,    0,      1,     0,      1,    0,        0 ,       1,        1,         1,               0},
  /*Redirect  row12*/{   0,     0,      1,      1,    0,      0,     0,      0,    0,        0 ,       0,        0,         0,               1},
  /*endRedir  row13*/{   0,     1,      1,      1,    1,      1,     1,      1,    0,        0 ,       1,        1,         1,               1},

};


int StreamToFile = 0;

unsigned char dataBuffer[512] = {0,}; //holds a block 
unsigned long dataBufferBlock;
unsigned long dataBufferIndex;
unsigned char *readPt;
dir RAMdirectory[32]; //ram directory (512 bytes/16 bytes)



//---------- eFile_Init-----------------
// Activate the file system, without formating
// Input: none
// Output: 0 if successful and 1 on failure (already initialized)
// since this program initializes the disk, it must run with 
//    the disk periodic task operating
int eFile_Init(void){ // initialize file system
  unsigned char status;
	if(Path[State][0] == 0) OS_Kill();
	State = 0;
  
	status = eDisk_Init(0);
	if(status != 0) return status;

	status = eDisk_ReadBlock((unsigned char*)RAMdirectory, 0);
	if(status != 0) return status;

	return 0; //initializtion success
}
//---------- eFile_Format-----------------
// Erase all files, create blank directory, initialize free space manager
// Input: none
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
int eFile_Format(void){int i; int j; // erase disk, add format
    unsigned char status;
	  
	if(Path[State][1] == 0) OS_Kill();
	State = 1;


	//**********initializing the file system, particulary the first block(the directory)
  RAMdirectory[0].fileName[0] = 'e'; //first entry in the directory indicates the free block
	RAMdirectory[0].fileName[1] = 'm';
	RAMdirectory[0].fileName[2] = 'p';
	RAMdirectory[0].fileName[3] = 't';
	RAMdirectory[0].fileName[4] = 'y';
	RAMdirectory[0].fileName[5] = 0;
	
	RAMdirectory[0].startBlock = 1; //free block is 1
	RAMdirectory[0].size = 0;

	//initializing all the directory fileNames to 0(empty)
  for(j = 1; j < 32; j++)
	{
     RAMdirectory[j].fileName[0] = 0;
  }		
	status = eDisk_WriteBlock((unsigned char*)RAMdirectory, 0); //writing to the first block in the disk (Directory)
	if(status != 0){return status;}
 
 //*************initializing the rest of the blocks
   ((unsigned long*)dataBuffer)[0] = 2; //next block
   ((unsigned long*)dataBuffer)[1] = 2047; //prev block
   ((unsigned long*)dataBuffer)[2] = 0;  //byte count
 	
 status = eDisk_WriteBlock(dataBuffer, 1); //writing to a block... goes through all 2047 blocks
 if(status != 0){return status;}
 
 for (i = 2; i < 2047; i++){
    
   ((unsigned long*)dataBuffer)[0] = i+1; //next block
	 ((unsigned long*)dataBuffer)[1] = i-1; //previous block
	 ((unsigned long*)dataBuffer)[2] = 0;  //byte count
	 
	status = eDisk_WriteBlock(dataBuffer, i); //writing to a block... goes through all 2047 blocks
  if(status != 0){return status;}
 }
	
	return 0;
}
//---------- eFile_Create-----------------
// Create a new, empty file with one allocated block
// Input: file name is an ASCII string up to seven characters 
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
int eFile_Create( char name[]){int i; int j;  // create new file, make it empty 
		long oldFree;
	long newFree;
	unsigned char status;
	if(Path[State][2] == 0) OS_Kill();
	State = 2;
	
	


	//reading the block that is free to find the next free space
	oldFree = RAMdirectory[0].startBlock; //oldFree will have the current free block
  
	//reading the next free space
  status = eDisk_ReadBlock(dataBuffer,oldFree);	
	if(status != 0) {return status;}
	newFree = ((unsigned long*)dataBuffer)[0]; //this gives the new free space
	
	//set the new free space in the directory
	RAMdirectory[0].startBlock = newFree; //setting the free space to the new free space
	
	//find an empty space in the directory to add the file name
  i = 1;	
  while((RAMdirectory[i].fileName[0] != 0)&&(i<32)){
       i++;
  }
	if(i >= 32){ //full directory, return error
		return 1;
  }
	
	j = 0;
	while(name[j] != 0){
	RAMdirectory[i].fileName[j] = name[j]; //1)adding the file name
		j++;
	}
	RAMdirectory[i].fileName[j] = 0; //null character
	
	//2)adding the next block 
	RAMdirectory[i].startBlock = oldFree;
	
	//3)adding the size
	RAMdirectory[i].size = 1;
	
	//writing the directory in to the disk.
	status = eDisk_WriteBlock((unsigned char*)RAMdirectory, 0);
	if(status != 0){return status;}
	
	//modifying the new block
	((unsigned long*)dataBuffer)[0] = 0; //this is last block
	((unsigned long*)dataBuffer)[1] = RAMdirectory[i].startBlock; //comes in handy when you are adding nodes to end of list
	((unsigned long*)dataBuffer)[2] = 0; //size is 0 since it is an empty file
	
	//writing the block in to the disk.
	status = eDisk_WriteBlock(dataBuffer, RAMdirectory[i].startBlock);
  	if(status != 0){return status;}

	return 0;
}
//---------- eFile_WOpen-----------------
// Open the file, read into RAM last block
// Input: file name is a single ASCII letter
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
int eFile_WOpen(char* name){      // open a file for writing 
 int index; long blockPointer; long fileSize; unsigned long prevBlock;
	unsigned char status;
	if(Path[State][3] == 0) OS_Kill();
	State = 3;

	index = 1;
	
	while((strncmp(RAMdirectory[index].fileName, name, 7) != 0) && index < 32){ index++;} //loop through directory to find matching file name
  if(index == 32) return 1; //if no matching file was found return with an error
  blockPointer = RAMdirectory[index].startBlock;
  fileSize = RAMdirectory[index].size;
  dataBufferIndex = index;
  //read the previous pointer to get the end of the file
		status = eDisk_ReadBlock(dataBuffer, blockPointer);
		if(status != 0) return status;
	
    prevBlock = ((unsigned long*)dataBuffer)[1];
	
	//data is now the end of the file
	dataBufferBlock = prevBlock;
  
	return 0;
	
}
//---------- eFile_Write-----------------
// save at end of the open file
// Input: data to be saved
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
int eFile_Write( char data){ unsigned long size, newBlock, emptyTail; DSTATUS status;
  if(Path[State][4] == 0) OS_Kill();
	State = 4;
	
	//find out where the end of the file is
	size = ((unsigned long *)dataBuffer)[2];
	
	if(size >= 500){// if the current block is full, start a new block.
	  newBlock = RAMdirectory[0].startBlock; //get first empty block
    ((unsigned long*)dataBuffer)[0] = newBlock; //set the full block's next pointer to the new empty block
		
	  status = eDisk_WriteBlock(dataBuffer, dataBufferBlock); //write the full block to the disk
	  if(status != 0) return status;
		
		//set the head.prev to the new last block
		status = eDisk_ReadBlock(dataBuffer, RAMdirectory[dataBufferIndex].startBlock);
		if(status != 0) return status;
		((unsigned long*)dataBuffer)[1] = newBlock;
		status = eDisk_ReadBlock(dataBuffer, RAMdirectory[dataBufferIndex].startBlock);
		if(status != 0) return status;
		
		//get the old empty tail
		status = eDisk_ReadBlock(dataBuffer, RAMdirectory[0].startBlock);
		if(status != 0) return status;
		
		emptyTail = ((unsigned long*)dataBuffer)[1];
		
		
		
		//update empty block linked list
		RAMdirectory[0].startBlock = ((unsigned long*)dataBuffer)[0]; //set empty.head to empty.head.next
		RAMdirectory[0].size--;
		RAMdirectory[dataBufferIndex].size++;
		
		//set the new emtpy.head.tail
		status = eDisk_ReadBlock(dataBuffer, RAMdirectory[0].startBlock);
		if(status != 0) return status;
		((unsigned long*)dataBuffer)[1] = emptyTail;
		status = eDisk_WriteBlock(dataBuffer, RAMdirectory[0].startBlock);
		if(status != 0) return status;
		
		//read the new block into RAM
		status = eDisk_ReadBlock(dataBuffer, newBlock);
		if(status != 0) return status;
		

		status = eDisk_WriteBlock((unsigned char*)RAMdirectory, 0); //save new directory
		if(status != 0) return status;
		
	  //initialize the new block
		((unsigned long*)dataBuffer)[0] = 0; //update links in the newly allocated block 
		((unsigned long*)dataBuffer)[1] = dataBufferBlock;
		((unsigned long*)dataBuffer)[2] = 0;
		size = 0;
		dataBufferBlock = newBlock;
		

	}
	
	//write the character
	dataBuffer[size + 12] = data; //the first 12 bytes (indeces 0 to 11) are the header, so the index of the character to write is the size + 12 (if size = 0, the index will be 12)
	
	//update block header
	((unsigned long *) dataBuffer)[2] = size + 1;
	
	return 0;
} 

//---------- eFile_Close-----------------
// Deactivate the file system
// Input: none
// Output: 0 if successful and 1 on failure (not currently open)
int eFile_Close(void){DSTATUS status;
  if(Path[State][5] == 0) OS_Kill();
	State = 5;
	
	status = eDisk_WriteBlock((unsigned char*)RAMdirectory, 0);
	if(status != 0) return status;
	
	status = eDisk_WriteBlock((unsigned char*)dataBuffer, dataBufferBlock);
	if(status!= 0) return status;
	
	
	return 0;
}

//---------- eFile_WClose-----------------
// close the file, left disk in a state power can be removed
// Input: none
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
int eFile_WClose(void){ // close the file for writing
unsigned char status;
  if(Path[State][6] == 0) OS_Kill();
	State = 6;
	
	
	//write the buffer and the directory back to the disk
	status = eDisk_WriteBlock((unsigned char*)RAMdirectory, 0);
	if(status != 0) return status;
	
	status = eDisk_WriteBlock((unsigned char*)dataBuffer, dataBufferBlock);
	if(status != 0) return status;
	
	return 0;
}
//---------- eFile_ROpen-----------------
// Open the file, read first block into RAM 
// Input: file name is a single ASCII letter
// Output: 0 if successful and 1 on failure (e.g., trouble read to flash)
int eFile_ROpen( char* name){     // open a file for reading 
  int i; long blockPointer; long fileSize;
unsigned char status;
	i = 1;
	
	
	if(Path[State][7] == 0) OS_Kill();
	State = 7;
	
	
	while((strncmp(RAMdirectory[i].fileName, name, 7) != 0) && i < 32){ i++;} //loop through directory to find matching file name
  if(i == 32) return 1; //if no matching file was found return with an error
  blockPointer = RAMdirectory[i].startBlock;
  fileSize = RAMdirectory[i].size;
  dataBufferIndex = i;
  //read the first block into RAM
		status = eDisk_ReadBlock(dataBuffer, blockPointer);
		if(status != 0) return status;
	
	  readPt = &dataBuffer[12];
 
	return 0;
}	
//---------- eFile_ReadNext-----------------
// retreive data from open file
// Input: none
// Output: return by reference data
//         0 if successful and 1 on failure (e.g., end of file)
int eFile_ReadNext(unsigned char *pt){int index;     // get next byte 
	unsigned char status;
	long nextBlock = ((unsigned long*)dataBuffer)[0];
	long size = ((unsigned long*)dataBuffer)[2];
	
	if(Path[State][8] == 0) OS_Kill();
	State = 8;
	
	
	*pt = *readPt;
	index = 1;
	
	//if it gets to the end of the block
	if(readPt == &dataBuffer[511] || readPt == &dataBuffer[size+12]){
     //read the next block into RAM
		
		if(nextBlock == 0) return 1; //return a 1 if this character was the last
		
		status = eDisk_ReadBlock(dataBuffer, nextBlock);
		if(status != 0) return status;
		
		readPt = &dataBuffer[12];
  }else	readPt++;
	
	
	

	
	return 0;
}
//---------- eFile_RClose-----------------
// close the reading file
// Input: none
// Output: 0 if successful and 1 on failure (e.g., wasn't open)
int eFile_RClose(void){ // close the file for writing
  if(Path[State][9] == 0) OS_Kill();
	State = 9;
	
	return 0;
}
//---------- eFile_Directory-----------------
// Display the directory with fileNames and sizes
// Input: pointer to a function that outputs ASCII characters to display
// Output: characters returned by reference
//         0 if successful and 1 on failure (e.g., trouble reading from flash)
int eFile_Directory(void(*fp)(unsigned char)){int i = 0; int j = 0; char ASCIISize[10];
	
		if(Path[State][10] == 0) OS_Kill();
	State = 10;
	
	while(RAMdirectory[i].fileName[0] != 0){ //while the directory entry is not empty
		  j=0;
		while(RAMdirectory[i].fileName[j] != 0){ //output file name
			fp(RAMdirectory[i].fileName[j]);
			j++;
		}
		
		fp(',');
		fp(' ');
		
		
		sprintf(ASCIISize, "%d", RAMdirectory[i].size);
		j = 0;
		while(ASCIISize[j] != 0){ //print size
			fp(ASCIISize[j]);
			j++;
		}
		
		fp('\n'); //go to next line
		fp('\r');
		i++;
	}

  
	return 0;
}   

//---------- eFile_Delete-----------------
// delete this file
// Input: file name is a single ASCII letter
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
int eFile_Delete( char* name){ unsigned char status; // remove this file 
  int i; unsigned long RemovedHead, RemovedTail, EmptyHead, EmptyTail;
		if(Path[State][11] == 0) OS_Kill();
	State = 11;
	
	//find the file in the directory
	while((strncmp(RAMdirectory[i].fileName, name, 7) != 0) && (i < 32)){i++;}
  if(i >= 32) return 1; //if the file name wasn't there
  
  RAMdirectory[i].fileName[0] = 0;
  //get all the block pointers for the links that need to change
	RemovedHead = RAMdirectory[i].startBlock;

  status = eDisk_ReadBlock(dataBuffer, RAMdirectory[i].startBlock); //read first empty block to get the end of the empty linked list
  if(status != 0) return status;

  RemovedTail = ((unsigned long*)dataBuffer)[1]; //end of file is pointed to by head.prev

  EmptyHead = RAMdirectory[0].startBlock;
  status = eDisk_ReadBlock(dataBuffer, RAMdirectory[0].startBlock); //read first empty block to get the end of the empty linked list
  if(status != 0) return status;

  //at this point emptyHead is in the DataBuffer

  EmptyTail = ((unsigned long*)dataBuffer)[1];
  
  ((unsigned long*)dataBuffer)[1] = RemovedTail;
	
	status = eDisk_WriteBlock(dataBuffer, EmptyHead);
	if(status != 0) return status;

  status = eDisk_ReadBlock(dataBuffer, EmptyTail);
  if(status != 0) return status;

  ((unsigned long*)dataBuffer)[0] = RemovedHead;

  status = eDisk_WriteBlock(dataBuffer, EmptyTail);
  if(status != 0) return status;
	
	status = eDisk_ReadBlock(dataBuffer, RemovedHead);
  if(status != 0) return status;
	
	((unsigned long*)dataBuffer)[1] = EmptyTail;
	
	status = eDisk_WriteBlock(dataBuffer, RemovedHead);
  if(status != 0) return status;	
	//remove the file entry from the directory
	
	status = eDisk_WriteBlock((unsigned char*)RAMdirectory, 0);
	if(status!= 0) return status;
	
	return 0;
}
//---------- eFile_RedirectToFile-----------------
// open a file for writing 
// Input: file name is a single ASCII letter
// stream printf data into file
// Output: 0 if successful and 1 on failure (e.g., trouble read/write to flash)
int eFile_RedirectToFile(char *name){
if(Path[State][12] == 0) OS_Kill();
	State = 12;

	eFile_Create(name);
	if(eFile_WOpen(name)) return 1;
	StreamToFile = 1;
	return 0;

}

//---------- eFile_EndRedirectToFile-----------------
// close the previously open file
// redirect printf data back to UART
// Output: 0 if successful and 1 on failure (e.g., wasn't open)
int eFile_EndRedirectToFile(void){
if(Path[State][13] == 0) OS_Kill(); UART_OutUDec(State); UART_OutChar(' '); UART_OutUDec(13);
	State = 13;
	
	StreamToFile = 0;
	if(eFile_WClose()) return 1;
	return 0;

}

int fputc (int ch, FILE *f) {
if(StreamToFile){
if(eFile_Write(ch)){ // close file on error
eFile_EndRedirectToFile(); // cannot write to file
return 1; // failure
}
return 0; // success writing
}
// regular UART output
UART_OutChar(ch);
return 0;
}
int fgetc (FILE *f){
return (UART_InChar());
} 
