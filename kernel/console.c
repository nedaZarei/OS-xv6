//
// Console input and output, to the uart.
// Reads are line at a time.
// Implements special input characters:
//   newline -- end of line
//   control-h -- backspace
//   control-u -- kill line
//   control-d -- end of file
//   control-p -- print process list
//

#include <stdarg.h>

#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "file.h"
#include "memlayout.h"
#include "riscv.h"
#include "defs.h"
#include "proc.h"
#define BACKSPACE 0x100

#define C(x)  ((x)-'@')  // Control-x

//
// send one character to the uart.
// called by printf(), and to echo input characters,
// but not from write().
//
void
consputc(int c)
{
  if(c == BACKSPACE){
    // if the user typed backspace, overwrite with a space.
    uartputc_sync('\b'); uartputc_sync(' '); uartputc_sync('\b');
  } else {
    uartputc_sync(c);
  }
}

struct {
  struct spinlock lock;
  
  // input
#define INPUT_BUF_SIZE 128
  char buf[INPUT_BUF_SIZE];
  uint r;  // Read index
  uint w;  // Write index
  uint e;  // Edit index
  uint rightmost; // added: the first empty char in the line
} cons;
#define INPUT_BUF 128
#define MAX_HISTORY 16
//for holding history buffer array
//init(currentHistory) = -1
struct {
    char bufferArr[MAX_HISTORY][INPUT_BUF]; //holds the actual command strings
    uint lengthsArr[MAX_HISTORY]; //holds the length of each command string
    uint lastCommandIndex;  //index of the last command entered to history
    int numOfCommmandsInMem; //number of history commands in memory
    int currentHistory; //holds the current history view -> displacement from the last command index
} historyBufferArray;
///////////////////////////////////////////////////////////////////////////////functions used for up and down buttons
//for holding the details of the command that we were typing before accessing the history
char oldBuf[INPUT_BUF];
uint oldBuffLen;
#define UP 226
#define DOWN 227

void
deleteLineScreen(void) { //delete curr line form screen
    int length = cons.rightmost - cons.r;
    while (length--) {
        consputc(BACKSPACE);
    }
}
void
eraseInputBuff(void){//clear input buffer
    cons.rightmost = cons.r;
    cons.e = cons.r;
}
void
copyCharsIntoOldBuff(void) {//copy input.buf into oldBuf
    oldBuffLen = cons.rightmost - cons.r;
    for (uint i = 0; i < oldBuffLen; i++) {
        oldBuf[i] = cons.buf[(cons.r + i) % INPUT_BUF];
    }
}
void
copyBuffToInputBuff(char * bufToSaveInInput, uint length){
    for (uint i = 0; i < length; i++) {
        //copy bufToSaveInInput to input.buf
        cons.buf[(cons.r + i) % INPUT_BUF] = bufToSaveInInput[i];
    }
    //assumes input.r == input.w == input.rightmost == input.e
    cons.e = cons.r + length;
    cons.rightmost = cons.e;
}
void
printBuffScreen(char* bufToPrintOnScreen, uint length) {
    uint i = 0;
    while (length--) {
        consputc(bufToPrintOnScreen[i]); //print bufToPrintOnScreen on-screen
        i++;
    }
}
/////////////////////////////////////////////////////////////////////////////////////////////////
//
// user write()s to the console go here.
//
int
consolewrite(int user_src, uint64 src, int n)
{
  int i;

  for(i = 0; i < n; i++){
    char c;
    if(either_copyin(&c, user_src, src+i, 1) == -1)
      break;
    uartputc(c);
  }

  return i;
}

//
// user read()s from the console go here.
// copy (up to) a whole input line to dst.
// user_dist indicates whether dst is a user
// or kernel address.
//
int
consoleread(int user_dst, uint64 dst, int n)
{
  uint target;
  int c;
  char cbuf;

  target = n;
  acquire(&cons.lock);
  while(n > 0){
    // wait until interrupt handler has put some
    // input into cons.buffer.
    while(cons.r == cons.w){
      if(killed(myproc())){
        release(&cons.lock);
        return -1;
      }
      sleep(&cons.r, &cons.lock);
    }

    c = cons.buf[cons.r++ % INPUT_BUF_SIZE];

    if(c == C('D')){  // end-of-file
      if(n < target){
        // Save ^D for next time, to make sure
        // caller gets a 0-byte result.
        cons.r--;
      }
      break;
    }

    // copy the input byte to the user-space buffer.
    cbuf = c;
    if(either_copyout(user_dst, dst, &cbuf, 1) == -1)
      break;

    dst++;
    --n;

    if(c == '\n'){
      // a whole line has arrived, return to
      // the user-level read().
      break;
    }
  }
  release(&cons.lock);

  return target - n;
}

//
// the console input interrupt handler.
// uartintr() calls this for input character.
// do erase/kill processing, append to cons.buf,
// wake up consoleread() if a whole line has arrived.
//
void
consoleintr(int c)
{
  acquire(&cons.lock);
  uint index;

    switch(c){
  case C('P'):  // Print process list.
    procdump();
    break;
  case C('U'):  // Kill line.
    while(cons.e != cons.w &&
          cons.buf[(cons.e-1) % INPUT_BUF_SIZE] != '\n'){
      cons.e--;
      consputc(BACKSPACE);
    }
    break;
  case C('H'): // Backspace
  case '\x7f': // Delete key
    if(cons.e != cons.w){
      cons.e--;
      consputc(BACKSPACE);
    }
    break;
    case UP:
          if (historyBufferArray.currentHistory < historyBufferArray.numOfCommmandsInMem - 1) {
              deleteLineScreen();

              //storing the currently entered command (in the terminal) to the oldBuf
              if (historyBufferArray.currentHistory == -1)
                  copyCharsIntoOldBuff();

              eraseInputBuff();
              historyBufferArray.currentHistory++;
              index = (historyBufferArray.lastCommandIndex + historyBufferArray.currentHistory) % MAX_HISTORY;
              printBuffScreen(historyBufferArray.bufferArr[index],
                              historyBufferArray.lengthsArr[index]);
              copyBuffToInputBuff(historyBufferArray.bufferArr[index],
                                  historyBufferArray.lengthsArr[index]);
          }
          break;
      case DOWN:
          switch (historyBufferArray.currentHistory) {
              case -1:
                  //do nothing
                  break;

              case 0: //get string from oldBuf
                  deleteLineScreen();
                  copyBuffToInputBuff(oldBuf, oldBuffLen);
                  printBuffScreen(oldBuf, oldBuffLen);
                  historyBufferArray.currentHistory--;
                  break;

              default:
                  deleteLineScreen();
                  historyBufferArray.currentHistory--;
                  index = (historyBufferArray.lastCommandIndex + historyBufferArray.currentHistory) %
                          MAX_HISTORY;
                  printBuffScreen(historyBufferArray.bufferArr[index],
                                  historyBufferArray.lengthsArr[index]);
                  copyBuffToInputBuff(historyBufferArray.bufferArr[index],
                                      historyBufferArray.lengthsArr[index]);
                  break;
          }
          break;
  default:
    if(c != 0 && cons.e-cons.r < INPUT_BUF_SIZE){
      c = (c == '\r') ? '\n' : c;

      // echo back to the user.
      consputc(c);

      // store for consumption by consoleread().
      cons.buf[cons.e++ % INPUT_BUF_SIZE] = c;

      if(c == '\n' || c == C('D') || cons.e-cons.r == INPUT_BUF_SIZE){
        // wake up consoleread() if a whole line (or end-of-file)
        // has arrived.
        cons.w = cons.e;
        wakeup(&cons.r);
      }
    }
    break;
  }
  
  release(&cons.lock);
}

void
consoleinit(void)
{
  initlock(&cons.lock, "cons");

  uartinit();

  // connect read and write system calls
  // to consoleread and consolewrite.
  devsw[CONSOLE].read = consoleread;
  devsw[CONSOLE].write = consolewrite;
}

//gets called by the sys_history and writes the requested command history in the buffer
int history(char *buffer, int historyId) {
    if (historyId < 0 || historyId > MAX_HISTORY - 1)
        return 2;//illegal history id
    if (historyId >= historyBufferArray.numOfCommmandsInMem)
        return 1;//no history in the historyId given
    //historyId != index of command in historyBufferArray.bufferArr
    memset(buffer, '\0', INPUT_BUF);

    int tempIndex = (historyBufferArray.lastCommandIndex + historyId) % MAX_HISTORY;

    memmove(buffer, historyBufferArray.bufferArr[tempIndex], historyBufferArray.lengthsArr[tempIndex]);
    //instead of memmove :add null terminator to the end of the string
    //buffer[historyBufferArray.lengthsArr[tempIndex]] = '\0';
    return 0;
}

//for copying the current command in input.buf to historyBufferArray
void
addToHistory(){
    uint len = cons.rightmost - cons.r - 1; //length of command to be saved (-1 is for removing the last '\n')
    if (len == 0) return; //not storing empty commands in history

    historyBufferArray.currentHistory = -1; //resetting user's current viewed history

    if (historyBufferArray.numOfCommmandsInMem < MAX_HISTORY) {
        historyBufferArray.numOfCommmandsInMem++; //inserting to the array in a circular manner
    }
    historyBufferArray.lastCommandIndex = (historyBufferArray.lastCommandIndex - 1) % MAX_HISTORY;
    historyBufferArray.lengthsArr[historyBufferArray.lastCommandIndex] = len;

    for (uint i = 0; i < len; i++) {
        historyBufferArray.bufferArr[historyBufferArray.lastCommandIndex][i] =  cons.buf[(cons.r + i) % INPUT_BUF];
        //not saving the last '/n' in memory
    }
}