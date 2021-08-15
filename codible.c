/*** includes ***/

#include <ctype.h> // iscntrl() resides in it
#include <stdio.h> // printf(), perror() reside in it
#include <stdlib.h> // atexit(), exit() reside in it 
#include <termios.h> // struct termios, tcgetattr(), tcsetattr(), ECHO, TCSAFLUSH, ICANON, ISIG, IXON. IEXTEN, ICRNL, OPOST, BRKINT, INPCK, ISTRIP, CS8, VMIN, VTIME reside in it
#include <unistd.h> // read(), STDIN_FILENO reside in it
#include <errno.h> // errno, EAGAIN reside in it

/*** defines ***/

#define CTRL_KEY(k) ((k) & 0x1f)

/*** data ***/

struct termios original;

/*** terminal ***/

void die(const char *s) {
  perror(s); // returns error messages from errno global variable
  exit(1);
}

void disableRawMode() {
  // error checking for setting up
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &original) == -1) {
    die("tcsetattr");
  }
}

void enableRawMode() {
  // error checking for loading up
  if (tcgetattr(STDIN_FILENO, &original) == -1) {
    die("tcgetattr");
  }
  atexit(disableRawMode); 

  struct termios raw = original;
  raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON); // IXON used to ignore XOFF and XON & ICRNL used to handle Carriage Return (CR) and New Line (NL), BRKINT used to handle SIGINT, INPCK used to handle Parity Check, ISTRIP used to handle 8-bit stripping, CS8 used to handle character size (CS) to 8 bits per byte
  raw.c_oflag &= ~(OPOST); // OPOST used to handle post-processing output
  raw.c_cflag |= ~(CS8);
  raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG); // ICANON used for reading input byte by byte & ISIG used to ignore SIGINT and SIGTSTP, IEXTEN used to disable Ctrl-o and Ctrl-V
  raw.c_cc[VMIN] = 0; // control characters for terminal settings
  raw.c_cc[VTIME] = 1; // control characters for terminal settings
  // error checking for setting up raw mode
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) {
    die("tcsetattr");
  }
}

/*** initialization ***/

int main()
{
  enableRawMode();
  char c;
  while (1) {
    char c = '\0';
    // marking EAGAIN as safe (for Cygwin)
    if (read(STDIN_FILENO, &c, 1) == -1 && errno != EAGAIN) {
      die("read");
    }
    if (iscntrl(c)) // check it's printable or not
    {
      printf("%d\r\n", c); // Just the ASCII value
    }
    else {
      printf("%d ('%c')\r\n", c, c); // ASCII value & the corresponding character
    }
    if (c == CTRL_KEY('q')) // checking 'q' for quit
    {
      break;
    }
  }
  return 0;
}


