/*** includes ***/

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>


/*** data ***/

struct termios orig_termios;

/*** terminal ***/

void die(const char *s){
   perror(s);
   exit(1);
   /*Most C library functions that fail will set the global errno variable to indicate what the error was. 
   perror() looks at the global errno variable and prints a descriptive error message for it. It also prints the string 
   given to it before it prints the error message, which is meant to provide context about what part of your code caused the error.
   After printing out the error message, we exit the program with an exit status of 1, which indicates failure (as would any non-zero value).*/
}

/* Canonical mode- In this mode, keyboard input is only sent to your program when the user presses Enter. 
   This is useful for many programs: it lets the user type in a line of text, use Backspace to fix errors until they get their 
   input exactly the way they want it, and finally press Enter to send it to the program.*/

void disableRawMode(){
   if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios) == -1)
      die("tcsetattr") ;   
   /*1. tcsetattr() to write the new terminal attributes back out
     2. TCSAFLUSH argument specifies when to apply the change: in this case, it waits for 
        all pending output to be written to the terminal, and also discards any input that hasn’t been read.*/
}

void enableRawMode(){
   if (tcgetattr(STDIN_FILENO, &orig_termios) == -1)  die("tcsgetattr");     
   //using tcgetattr() to read the current attributes into a struct
   atexit(disableRawMode); 
   //atexit() comes from <stdlib.h>. We use it to register our disableRawMode() function to be called automatically when the program exits
                        
   struct termios raw = orig_termios;
   /*We store the original terminal attributes in a global variable, orig_termios. We assign the orig_termios struct to the raw struct 
   in order to make a copy of it before we start making our changes.*/
   
   raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
   /*1. IXON comes from <termios.h>. The I stands for “input flag” (which it is, unlike the other I flags we’ve seen so far) and XON comes 
         from the names of the two control characters that Ctrl-S and Ctrl-Q produce: XOFF to pause transmission and XON to resume transmission.
      2. while holding down Ctrl, you should see that we have every letter except M. Ctrl-M is weird: it’s being read as 10, when we expect 
         it to be read as 13, since it is the 13th letter of the alphabet, and Ctrl-J already produces a 10. What else produces 10? 
         The Enter key does.
      3. ICRNL comes from <termios.h>. The I stands for “input flag”, CR stands for “carriage return”, and NL stands for “new line”.
      4. When BRKINT is turned on, a break condition will cause a SIGINT signal to be sent to the program, like pressing Ctrl-C.
      5. INPCK enables parity checking, which doesn’t seem to apply to modern terminal emulators.
      6. ISTRIP causes the 8th bit of each input byte to be stripped, meaning it will set it to 0. This is probably already turned off.*/

   raw.c_oflag &= ~(OPOST);
   /*OPOST comes from <termios.h>. O means it’s an output flag, and assume POST stands for “post-processing of output”.
      We will turn off all output processing features by turning off the OPOST flag. In practice, the "\n" to "\r\n" 
      translation is likely the only output processing feature turned on by default. \r added in printf() to print in left side*/ 

   raw.c_cflag |= (CS8);
   /*CS8 is not a flag, it is a bit mask with multiple bits, which we set using the bitwise-OR (|) operator unlike all the flags we
      are turning off. It sets the character size (CS) to 8 bits per byte.*/

   raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
   /*1. c_lflag field is for “local flags”. A comment in macOS’s <termios.h> describes it as a “dumping ground for other state”  
         tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
      2. The ECHO feature causes each key you type to be printed to the terminal, so you can see what you’re typing. 
         This is useful in canonical mode. ECHO is a bitflag, defined as 00000000000000000000000000001000 in binary. 
         We use the bitwise-NOT operator (~) on this value to get 11111111111111111111111111110111
      3. ICANON comes from <termios.h>. ICANON local flag allows us to turn off canonical mode.
         Now, reading input byte-by-byte, instead of line-by-line.
      4. Ctrl-C sends a SIGINT signal to the current process which causes it to termina te, and Ctrl-Z sends a SIGTSTP signal 
         to the current process which causes it to suspend 
         ISIG comes from <termios.h>. Like ICANON, it starts with I but isn’t an input flag.
      5. when you type Ctrl-V, the terminal waits for you to type another character and then sends that character literally. 
         For example, before we disabled Ctrl-C, you might’ve been able to type Ctrl-V and then Ctrl-C to input a 3 byte. 
         We can turn off this feature using the IEXTEN flag.
      6. IEXTEN comes from <termios.h>. It is another flag that starts with I but belongs in the c_lflag field.*/   
   raw.c_cc[VMIN] = 0;
   raw.c_cc[VTIME] = 1;
   /*VMIN and VTIME come from <termios.h>. They are indexes into the c_cc field, which stands for “control characters”, 
   an array of bytes that control various terminal settings.The VMIN value sets the minimum number of bytes of input needed before 
   read() can return. We set it to 0 so that read() returns as soon as there is any input to be read. The VTIME value sets the maximum
   amount of time to wait before read() returns. It is in tenths of a second, so we set it to 1/10 of a second, or 100 milliseconds. 
   If read() times out, it will return 0, which makes sense because its usual return value is the number of bytes read.*/
   tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

/*** init ***/

int main() {
   enableRawMode();
   char c;
   while (1) {
      char c = '\0';
      if (read(STDIN_FILENO, &c, 1) == -1 && errno != EAGAIN)  die("read");
      /*1. read() and STDIN_FILENO come from <unistd.h>
        2. errno and EAGAIN come from <errno.h>.
           tcsetattr(), tcgetattr(), and read() all return -1 on failure, and set the errno value to indicate the error*/
      if (iscntrl(c)) {
         /*iscntrl() comes from <ctype.h>.iscntrl() tests whether a character is a control character. 
         Control characters are nonprintable characters that we don’t want to print to the screen.*/
         printf("%d\r\n", c);
      } else {
         printf("%d ('%c')\r\n", c, c);
      }
      if (c == 'q') break;
   }
   return 0;  
}