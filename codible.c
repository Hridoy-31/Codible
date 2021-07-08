#include <ctype.h> // iscntrl() resides in it
#include <stdio.h> // printf() resides in it
#include <stdlib.h> // atexit() resides in it 
#include <termios.h> // struct termios, tcgetattr(), tcsetattr(), ECHO, TCSAFLUSH, ICANON, ISIG, IXON. IEXTEN, ICRNL, OPOST, BRKINT, INPCK, ISTRIP, CS8, VMIN, VTIME reside in it
#include <unistd.h> // read(), STDIN_FILENO reside in it

struct termios original;

void disableRawMode() {
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &original); // setting up original terminal attributes
}

void enableRawMode() {
  tcgetattr(STDIN_FILENO, &original); // collecting terminal attributes
  atexit(disableRawMode); 

  struct termios raw = original;
  raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON); // IXON used to ignore XOFF and XON & ICRNL used to handle Carriage Return (CR) and New Line (NL), BRKINT used to handle SIGINT, INPCK used to handle Parity Check, ISTRIP used to handle 8-bit stripping, CS8 used to handle character size (CS) to 8 bits per byte
  raw.c_oflag &= ~(OPOST); // OPOST used to handle post-processing output
  raw.c_cflag |= ~(CS8);
  raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG); // ICANON used for reading input byte by byte & ISIG used to ignore SIGINT and SIGTSTP, IEXTEN used to disable Ctrl-o and Ctrl-V
  raw.c_cc[VMIN] = 0; // control characters for terminal settings
  raw.c_cc[VTIME] = 1; // control characters for terminal settings
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw); // Setting up modified terminal attributes
}

int main()
{
  enableRawMode();
  char c;
  while (1) {
    char c = '\0';
    read(STDIN_FILENO, &c, 1);
    if (iscntrl(c)) // check it's printable or not
    {
      printf("%d\r\n", c); // Just the ASCII value
    }
    else {
      printf("%d ('%c')\r\n", c, c); // ASCII value & the corresponding character
    }
    if (c == 'q') // checking 'q' for quit
    {
      break;
    }
  }
  return 0;
}


