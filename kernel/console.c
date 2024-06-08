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
#define LEFT_KEY 0xff51

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
char charsToBeMoved[INPUT_BUF]; //temporary storage for input.buf in a certain context


//Store input.buf into charsToBeMoved (to use later)
//called when a new key is pressed and the cursor is not at EOL
void copyCharsToBeMoved() {
    for (uint i = 0; i < (uint)(cons.rightmost - cons.r); i++) {
        charsToBeMoved[i] = cons.buf[(cons.e + i) % INPUT_BUF];
    }
}

//Shift input.buf to the right by one unit and print the same on-screen
//called when a new key is pressed and the cursor is not at EOL
void rightShiftBuffer() {
    uint n = cons.rightmost - cons.e;
    for (uint i = 0; i < n; i++) {
        cons.buf[(cons.e + i) % INPUT_BUF] = charsToBeMoved[i];
        consputc(charsToBeMoved[i]);
    }

    //reset charsToBeMoved for further use
    for(uint i = 0; i < INPUT_BUF; i++) {
        charsToBeMoved[i] = '\0';
    }
    //return the caret to its correct position
    int i = n;
    while (i--) {
        consputc(LEFT_KEY);
    }
}


//Shift input.buf to the left by one unit and print the same on-screen
//called when a BACKSPACE is pressed and the cursor is not at EOL
void leftShiftBuffer() {
    uint n = cons.rightmost - cons.e;
    consputc(LEFT_KEY); // cursor (display) is b/w b and c
    cons.e--; // set the backend part of cursor to the final correct position

    //abcdef -> abdeff
    for (uint i = 0; i < n; i++) {
        cons.buf[(cons.e + i) % INPUT_BUF] = cons.buf[(cons.e + i + 1) % INPUT_BUF];
        consputc(cons.buf[(cons.e + i + 1) % INPUT_BUF]);
    }
    //cursor (display) is b/w f and f

    cons.rightmost--; // set input.rightmost to the final correct position
    consputc(' '); // delete the last char in line and advance cursor (display) by 1

    // set the cursor (display) to the correct position
    int i = n + 1;
    while (i--){
        consputc(LEFT_KEY); // shift the caret back to the left
    }

    // at this point, the cursor (display) i.e. pos is in sync with input.e
}
char bufff[INPUT_BUF];
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
void consoleintr(int c) {
    acquire(&cons.lock);
    uint index;

    static int escape_seq_state = 0;
    int ctrlC_flag = 0;

    switch (escape_seq_state) {
        case 0: //initial state
            if (c == 27) { //check for the escape character
                escape_seq_state = 1;
            } else {
                //process regular characters
                switch (c) {
                    case C('C'):  //ctrl+c
                        ctrlC_flag=1;
                        break;
                    case C('P'):  // Print process list.
                        procdump();
                        break;
                    case C('U'):  // Kill line.
                        if (cons.rightmost > cons.e) { //caret isn't at the end of the line
                            uint numtoshift = cons.rightmost - cons.e;
                            uint placestoshift = cons.e - cons.w;
                            uint i;
                            for (i = 0; i < placestoshift; i++) {
                                consputc(LEFT_KEY);
                            }
                            memset(bufff, 0, INPUT_BUF);
                            for (i = 0; i < numtoshift; i++) {
                                bufff[i] = cons.buf[(cons.w + i + placestoshift) % INPUT_BUF];
                            }
                            for (i = 0; i < numtoshift; i++) {
                                cons.buf[(cons.w + i) % INPUT_BUF] = bufff[i];
                            }
                            cons.e -= placestoshift;
                            cons.rightmost -= placestoshift;
                            for (i = 0; i < numtoshift; i++) { //repaint the chars
                                consputc(cons.buf[(cons.e + i) % INPUT_BUF]);
                            }
                            for (i = 0; i < placestoshift; i++) { //erase the leftover chars
                                consputc(' ');
                            }
                            for (i = 0; i < placestoshift + numtoshift; i++) { //move the caret back to the left
                                consputc(LEFT_KEY);
                            }
                        }
                        else { //caret is at the end of the line( deleting everything from both screen and inputbuf)
                            while(cons.e != cons.w &&
                                  cons.buf[(cons.e - 1) % INPUT_BUF] != '\n'){
                                cons.e--;
                                cons.rightmost--;
                                consputc(BACKSPACE);
                            }
                        }
                        break;
                    case C('H'): // Backspace
                    case '\x7f': // Delete key
                        if (cons.rightmost != cons.e && cons.e != cons.w) { //caret isn't at the end of the line
                            leftShiftBuffer();
                            break;
                        }
                        if(cons.e != cons.w){ //caret is at the end of the line - deleting last char
                            cons.e--;
                            cons.rightmost--;
                            consputc(BACKSPACE);
                        }
                        break;
                    default:
                        if(c != 0 && cons.e-cons.r < INPUT_BUF_SIZE){
                            c = (c == '\r') ? '\n' : c;
                            if (cons.rightmost > cons.e) { //caret isn't at the end of the line
                                copyCharsToBeMoved();
                                cons.buf[cons.e % INPUT_BUF] = c;
                                cons.e++;
                                cons.rightmost++;
                                consputc(c);
                                rightShiftBuffer();
                            }
                            else {
                                cons.buf[cons.e % INPUT_BUF] = c;
                                cons.e++;
                                cons.rightmost = cons.e - cons.rightmost == 1 ? cons.e : cons.rightmost;
                                consputc(c);
                            }
                            if(c == '\n' || c == C('D') || cons.rightmost == cons.r + INPUT_BUF){
                                // wake up consoleread() if a whole line (or end-of-file) has arrived.
                                //checking if the entered command is not "history" before adding to history
                                uint len = cons.rightmost - cons.r - 1; //length of command to be saved (-1 is for removing the last '\n')
                                if (len > 0) {
                                    char *command = &cons.buf[cons.r % INPUT_BUF];
                                    if ((strncmp("history", command , 7) != 0)) {
                                        addToHistory();
                                    }
                                }
                                cons.w = cons.rightmost;
                                wakeup(&cons.r);
                            }
                        }
                        break;
                }
            }
            break;

        case 1: //expecting second character of escape sequence
            if (c == 91) { // Check for '[' character
                escape_seq_state = 2;
            } else {
                //tot a valid escape sequence, reset state
                escape_seq_state = 0;
            }
            break;

        case 2: //expecting third character of escape sequence
            switch (c) {
                case 65: // Up arrow key
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
                                            historyBufferArray.lengthsArr[index]); //updating cons
                    }
                    break;
                case 66: // Down arrow key
                    switch (historyBufferArray.currentHistory)
                    {
                        case -1:
                            //do nothing
                            break;

                        case 0: //get string from oldBuf
                            deleteLineScreen();
                            copyBuffToInputBuff(oldBuf, oldBuffLen);//updating cons
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
                                                historyBufferArray.lengthsArr[index]); //updating cons
                            break;
                    }
                    break;
            }
            //resetting state after handling escape sequence
            escape_seq_state = 0;
            break;

    }

    release(&cons.lock);
    if(ctrlC_flag)
    {
        fgproc();
    }
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
    if (historyId < 0 || historyId > MAX_HISTORY - 1){
        return 2;//illegal history id
    }

    if (historyId >= historyBufferArray.numOfCommmandsInMem){
        return 1;//no history in the historyId given
    }

    //historyId != index of command in historyBufferArray.bufferArr
    int tempIndex = (historyBufferArray.lastCommandIndex + historyId) % MAX_HISTORY;
    char *src = historyBufferArray.bufferArr[tempIndex];

    printf(" %d:  %s\n" ,historyId, src);

    int length = historyBufferArray.lengthsArr[tempIndex];
    //copy at most INPUT_BUF - 1 characters to leave room for null termination
    int i;
    for (i = 0; i <= length && i < INPUT_BUF - 1; i++) {
        buffer[i] = src[i];
    }
    //null terminate the buffer
    buffer[i] = '\0';
    return 0;
}

//for copying the current command in input.buf to historyBufferArray
void
addToHistory(){
    uint len = cons.rightmost - cons.r - 1; //length of command to be saved (-1 is for removing the last '\n')
    if (len == 0) return; //not storing empty commands in history

    historyBufferArray.currentHistory = -1; //resetting user's current viewed history

    if (historyBufferArray.numOfCommmandsInMem < MAX_HISTORY) {
        historyBufferArray.numOfCommmandsInMem++;
    }
    //inserting to the array in a circular manner
    historyBufferArray.lastCommandIndex = (historyBufferArray.lastCommandIndex - 1) % MAX_HISTORY;
    historyBufferArray.lengthsArr[historyBufferArray.lastCommandIndex] = len;

    for (uint i = 0; i < len; i++) {
        historyBufferArray.bufferArr[historyBufferArray.lastCommandIndex][i] =  cons.buf[(cons.r + i) % INPUT_BUF];
        //not saving the last '/n' in memory
    }
}

