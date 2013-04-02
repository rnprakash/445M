#include "OS.h"

extern _OS_FifoType _OS_Fifo;
extern _OS_MailboxType _OS_Mailbox;
extern _OS_Event _eventLog[_OS_MAX_EVENTS]; // log timestamp data for events
extern int _eventIndex;  // index for event log

unsigned long putCount = 0;
unsigned long getCount = 0;

// ******** OS_Fifo_Init ************
// Initialize the Fifo to be empty
// Inputs: size
// Outputs: none 
// In Lab 2, you can ignore the size field
// In Lab 3, you should implement the user-defined fifo size
// In Lab 3, you can put whatever restrictions you want on size
//    e.g., 4 to 64 elements
//    e.g., must be a power of 2,4,8,16,32,64,128
void OS_Fifo_Init(unsigned long size) {
//  memset(_OS_Fifo.Fifo, 0, sizeof(_OS_Fifo.Fifo));  // initialize all values to 0
  _OS_Fifo.PutIndex = _OS_Fifo.GetIndex = 0;
  _OS_Fifo.size = size;
  OS_InitSemaphore(&_OS_Fifo.notEmpty, 0);
  OS_InitSemaphore(&_OS_Fifo.mutex, 1);
}

// ******** OS_Fifo_Put ************
// Enter one data sample into the Fifo
// Called from the background, so no waiting 
// Inputs:  data
// Outputs: true if data is properly saved,
//          false if data not saved, because it was full
// Since this is called by interrupt handlers 
//  this function can not disable or enable interrupts
int OS_Fifo_Put(unsigned long data) {
  // NOT THREAD SAFE!!
  // doesn't this actually test if there's only 1 spot left?
  putCount++;
//   OS_LogEvent(EVENT_FIFO_PUT);
  if(((_OS_Fifo.PutIndex + 1) % (_OS_Fifo.size)) == _OS_Fifo.GetIndex) {
    return 0;
  }
  _OS_Fifo.Fifo[_OS_Fifo.PutIndex] = data;
  _OS_Fifo.PutIndex = (_OS_Fifo.PutIndex + 1) % _OS_Fifo.size;
  OS_Signal(&_OS_Fifo.notEmpty);
  return 1;
}  

// ******** OS_Fifo_Get ************
// Remove one data sample from the Fifo
// Called in foreground, will spin/block if empty
// Inputs:  none
// Outputs: data 
unsigned long OS_Fifo_Get(void) {
  unsigned long data;
  getCount++;
//   OS_LogEvent(EVENT_FIFO_WAIT);
  OS_Wait(&_OS_Fifo.notEmpty);
//   OS_LogEvent(EVENT_FIFO_WAKE);
//   OS_bWait(&_OS_Fifo.mutex);
  data = _OS_Fifo.Fifo[_OS_Fifo.GetIndex];
  _OS_Fifo.Fifo[_OS_Fifo.GetIndex] = 0; // clear for debugging purposes
  _OS_Fifo.GetIndex = (_OS_Fifo.GetIndex + 1) % _OS_Fifo.size;
//   OS_bSignal(&_OS_Fifo.mutex);
  return data;
}

// ******** OS_Fifo_Size ************
// Check the status of the Fifo
// Inputs: none
// Outputs: returns the number of elements in the Fifo
//          greater than zero if a call to OS_Fifo_Get will return right away
//          zero or less than zero if the Fifo is empty 
//          zero or less than zero  if a call to OS_Fifo_Get will spin or block
long OS_Fifo_Size(void) {
  return (_OS_Fifo.PutIndex - _OS_Fifo.GetIndex) & (_OS_Fifo.size - 1);
}

// ******** OS_MailBox_Init ************
// Initialize communication channel
// Inputs:  none
// Outputs: none
void OS_MailBox_Init(void) {
  _OS_Mailbox.data = 0;
   OS_InitSemaphore(&_OS_Mailbox.hasData, 0);
   OS_InitSemaphore(&_OS_Mailbox.gotData, 1);
}

// ******** OS_MailBox_Send ************
// enter mail into the MailBox
// Inputs:  data to be sent
// Outputs: none
// This function will be called from a foreground thread
// It will spin/block if the MailBox contains data not yet received 
void OS_MailBox_Send(unsigned long data) {
  OS_bWait(&_OS_Mailbox.gotData);
  _OS_Mailbox.data = data;
  OS_bSignal(&_OS_Mailbox.hasData);  
}

// ******** OS_MailBox_Recv ************
// remove mail from the MailBox
// Inputs:  none
// Outputs: data received
// This function will be called from a foreground thread
// It will spin/block if the MailBox is empty 
unsigned long OS_MailBox_Recv(void) {
  unsigned long data;
  OS_bWait(&_OS_Mailbox.hasData);
  data = _OS_Mailbox.data;
  OS_bSignal(&_OS_Mailbox.gotData);
  return data;
}

// log an event
// returns 1 if successful, 0 otherwise
int OS_LogEvent(char type) {
  if(_eventIndex >= _OS_MAX_EVENTS) {
     return 0;
  }
  else {
    _eventLog[_eventIndex].timestamp = OS_Time();
    _eventLog[_eventIndex].type = type;
    _eventIndex++;
    return 1;
  }
}
