#include <stdlib.h> // atexit() resides in it 
#include <termios.h> // struct termios, tcgetattr(), tcsetattr(), ECHO, TCSAFLUSH reside in it
#include <unistd.h> // read(), STDIN_FILENO reside in it

struct termios original;

void disableRawMode() {
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &original); // setting up original terminal attributes
}

void enableRawMode() {
  tcgetattr(STDIN_FILENO, &original); // collecting terminal attributes
  atexit(disableRawMode); 

  struct termios raw = original;
  raw.c_lflag &= ~(ECHO);
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw); // Setting up modified terminal attributes
}

int main()
{
  enableRawMode();
  char c;
  while (read(STDIN_FILENO, &c, 1) == 1 && c != 'q'); // checking 'q' to quit
  return 0;
}

