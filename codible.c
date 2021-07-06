#include <ctype.h> // iscntrl() resides in it
#include <stdio.h> // printf() resides in it
#include <stdlib.h> // atexit() resides in it 
#include <termios.h> // struct termios, tcgetattr(), tcsetattr(), ECHO, TCSAFLUSH, ICANON, ISIG, IXON reside in it
#include <unistd.h> // read(), STDIN_FILENO reside in it

struct termios original;

void disableRawMode() {
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &original); // setting up original terminal attributes
}

void enableRawMode() {
  tcgetattr(STDIN_FILENO, &original); // collecting terminal attributes
  atexit(disableRawMode); 

  struct termios raw = original;
  raw.c_iflag &= ~(IXON); // IXON used to ignore XOFF and XON
  raw.c_lflag &= ~(ECHO | ICANON | ISIG); // ICANON used for reading input byte by byte & ISIG used to ignore SIGINT and SIGTSTP
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw); // Setting up modified terminal attributes
}

int main()
{
  enableRawMode();
  char c;
  while (read(STDIN_FILENO, &c, 1) == 1 && c != 'q') // checking 'q' to quit
  {
    if (iscntrl(c)) // check it's printable or not
    {
      printf("%d\n", c); // Just the ASCII value
    }
    else {
      printf("%d ('%c')\n", c, c); // ASCII value & the corresponding character
    }
  }
  return 0;
}


