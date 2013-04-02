// filename ************** eFile.h *****************************
// Middle-level routines to implement a solid-state disk 
// Jonathan W. Valvano 3/16/11

#define FILE_NAME_SIZE 12  // max file name size, 8 for name + 3 for extension + \0
#define DRIVE 0
#define BLOCK_SIZE 512  // 512 bytes/block
#define BLOCKS 2047 // 2^11 blocks minus one for the directory, gives 1 MB storage
#define BOOT_SECTOR 0
#define SECT_PER_CLUST_INDEX 0x0D
#define RES_SECT_INDEX 0x0E
#define NUM_FATS_INDEX 0x10
#define FAT_SIZE_INDEX 0x24
#define PT_SIZE 2 // linked list pointers are shorts
#define WRITE_INDEX 2

typedef struct eFile_File {
  char name[8];
	char ext[3];
	char attr;
	char resv[8]; // reserved/don't care bytes
	char hiCluster[2];
	char modTime[2];
	char modDate[2];
	char cluster[2];
	char size[4]; // in bytes
} eFile_File;

#define MAX_FILES 16 //(512/sizeof(eFile_File))

#define CHECK_DISK if(!_sysInit) { fprintf(stderr, "Error reading disk!\n"); return 1; }


int eFile_Info(void);

//---------- eFile_Init-----------------
// Activate the file system, without formating
// Input: none
// Output: 0 if successful and 1 on failure (already initialized)
// since this program initializes the disk, it must run with 
//    the disk periodic task operating
int eFile_Init(void); // initialize file system

//---------- eFile_Format-----------------
// Erase all files, create blank directory, initialize free space manager
// Input: none
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
int eFile_Format(void); // erase disk, add format

//---------- eFile_Create-----------------
// Create a new, empty file with one allocated block
// Input: file name is an ASCII string up to seven characters 
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
int eFile_Create(const char name[FILE_NAME_SIZE], char attr);  // create new file, make it empty 


//---------- eFile_WOpen-----------------
// Open the file, read into RAM last block
// Input: file name is a single ASCII letter
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
int eFile_WOpen(const char name[FILE_NAME_SIZE]);      // open a file for writing 

//---------- eFile_Write-----------------
// save at end of the open file
// Input: data to be saved
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
int eFile_Write(const char data);  

//---------- eFile_Close-----------------
// Deactivate the file system
// Input: none
// Output: 0 if successful and 1 on failure (not currently open)
int eFile_Close(void); 


//---------- eFile_WClose-----------------
// close the file, left disk in a state power can be removed
// Input: none
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
int eFile_WClose(void); // close the file for writing

//---------- eFile_ROpen-----------------
// Open the file, read first block into RAM 
// Input: file name is a single ASCII letter
// Output: 0 if successful and 1 on failure (e.g., trouble read to flash)
int eFile_ROpen(const char name[FILE_NAME_SIZE]);      // open a file for reading 
   
//---------- eFile_ReadNext-----------------
// retreive data from open file
// Input: none
// Output: return by reference data
//         0 if successful and 1 on failure (e.g., end of file)
int eFile_ReadNext(char *pt);       // get next byte 
                              
//---------- eFile_RClose-----------------
// close the reading file
// Input: none
// Output: 0 if successful and 1 on failure (e.g., wasn't open)
int eFile_RClose(void); // close the file for writing

//---------- eFile_Directory-----------------
// Display the directory with filenames and sizes
// Input: pointer to a function that outputs ASCII characters to display
// Output: characters returned by reference
//         0 if successful and 1 on failure (e.g., trouble reading from flash)
int eFile_Directory(int(*fp)(const char *format, ...));   

//---------- eFile_Delete-----------------
// delete this file
// Input: file name is a single ASCII letter
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
int eFile_Delete(const char name[FILE_NAME_SIZE]);  // remove this file 

//---------- eFile_RedirectToFile-----------------
// open a file for writing 
// Input: file name is a single ASCII letter
// stream printf data into file
// Output: 0 if successful and 1 on failure (e.g., trouble read/write to flash)
int eFile_RedirectToFile(const char *name);

//---------- eFile_EndRedirectToFile-----------------
// close the previously open file
// redirect printf data back to UART
// Output: 0 if successful and 1 on failure (e.g., wasn't open)
int eFile_EndRedirectToFile(void);

int eFile_PrintFile(void(*fp)(unsigned char), char name[FILE_NAME_SIZE]);
int eFile_List(char list[MAX_FILES][13]);
int eFile_ChangeDirectory(char dir[13]);

static void _eFile_ClearBlockBuff(void);
static eFile_File _eFile_Find(const char name[FILE_NAME_SIZE], int dir);
static void _eFile_FATName(const char name[FILE_NAME_SIZE], char n[FILE_NAME_SIZE], int (*)(int), int dir);
static int _eFile_FreeCluster(void);
