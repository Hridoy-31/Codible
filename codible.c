/*** includes ***/

#include <ctype.h> // iscntrl() resides in it
#include <stdio.h> // printf(), perror(), sscanf(), snprintf() reside in it
#include <stdlib.h> // atexit(), exit(), realloc(), free() reside in it 
#include <termios.h> // struct termios, tcgetattr(), tcsetattr(), ECHO, TCSAFLUSH, ICANON, ISIG, IXON. IEXTEN, ICRNL, OPOST, BRKINT, INPCK, ISTRIP, CS8, VMIN, VTIME reside in it
#include <unistd.h> // read(), STDIN_FILENO, write(), STDOUT_FILENO reside in it
#include <errno.h> // errno, EAGAIN reside in it
#include <sys/ioctl.h> // ioctl(), TIOCGWINSZ, struct winsize reside in it.
#include <string.h> // memcpy(), strlen() resides in it

/*** defines ***/

#define CODIBLE_VERSION "0.0.1"
#define CTRL_KEY(k) ((k) & 0x1f)

/*** data ***/

struct editorConfig {
  int cx, cy;
  int screenrows;
  int screencolumns;
  struct termios original;
};

struct editorConfig E;

/*** terminal ***/

void die(const char *s) {
  write(STDOUT_FILENO, "\x1b[2J", 4);
  write(STDOUT_FILENO, "\x1b[H", 3);
  perror(s); // returns error messages from errno global variable
  exit(1);
}

void disableRawMode() {
  // error checking for setting up
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.original) == -1) {
    die("tcsetattr");
  }
}

void enableRawMode() {
  // error checking for loading up
  if (tcgetattr(STDIN_FILENO, &E.original) == -1) {
    die("tcgetattr");
  }
  atexit(disableRawMode); 

  struct termios raw = E.original;
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

char editorReadKey() {
  // read the keypress
  int nread;
  char c;
  while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
    if (nread == -1 && errno != EAGAIN) {
      die("read");
    }
    return c;
  }
}

int getCursorPosition(int *rows, int *columns) {
  char buffer[32];
  unsigned int i = 0;
  if (write(STDOUT_FILENO, "\x1b[6n", 4)!=4) {
    return -1;
  }
  while (i < sizeof(buffer)-1) {
    if (read(STDIN_FILENO, &buffer[i], 1)!=1) {
      break;
    }
    if (buffer[i]=='R') {
      break;
    }
    i++;
  }
  buffer[i] = '\0';
  if (buffer[0] != '\x1b' || buffer[1] != '[') {
    // making sure it responded with escape sequence.
    return -1;
  }
  if (sscanf(&buffer[2], "%d;%d", rows, columns) != 2) {
    // passing the third character of buffer because of skipping
    // '\x1b' & '[' characters.
    // sscanf() will parse the integers separated by the semicolon ;
    return -1;
  }
  return 0;
}

int getWindowSize(int *rows, int *columns) {
  struct winsize ws; // the terminal size will initially stored here.
  if(ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws)==-1 || ws.ws_col==0) {
    if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12)!=12) {
      // 999C command used for cursor to go forward (right)
      // 999B command used for cursor to go downward (down);
      // these two commands are used for finding the bottom-right
      // position of the cursor.
      return -1;
    }
    return getCursorPosition(rows, columns);
  }
  else {
    *columns = ws.ws_col;
    *rows = ws.ws_row;
    return 0;
  }
}

/*** append buffer ***/

struct abuf {
  char *b;
  int len;
};

#define ABUF_INIT {NULL, 0} // initially pointing to the empty buffer
// worked as a constructor

void abAppend(struct abuf *ab, const char *s, int len) {
  char *new = realloc(ab->b, ab->len + len);
  if (new == NULL) {
    return;
  }
  memcpy(&new[ab->len],s,len); // copy the string s at the end of the buffer
  // updating pointer and length
  ab->b = new;
  ab->len += len;
}

void abFree(struct abuf *ab) {
  free(ab->b);
}

/*** output ***/

void editorDrawRows(struct abuf *ab) {
  // putting '~' in front of each row which is not part of the
  // text being edited
  int y;
  for (y=0; y<E.screenrows; y++) {
    if (y==E.screenrows/3) {
      char welcome[80];
      int welcomelen = snprintf(welcome,sizeof(welcome),
				"Codible -- version %s", CODIBLE_VERSION);
      // this will show the welcome message at 1/3 of the screen,
      if (welcomelen > E.screencolumns) {
	welcomelen = E.screencolumns;
      }
      int padding = (E.screencolumns - welcomelen)/2;
      // centering the welcome message
      if (padding != 0) {
	abAppend(ab, "~", 1);
	// first character is the ~
	padding--;
      }
      while (padding--) {
	// next spaces are filled with " " (spaces) until the message
	// character starts
	abAppend(ab, " ", 1);
      }
      // changing length according to the terminal size
      abAppend(ab, welcome, welcomelen);
    }
    else {
      abAppend(ab, "~", 1);
    }
    abAppend(ab, "\x1b[K", 3);
    // [K escape sequence will clear each line as we redraw them
    if (y < E.screenrows-1) {
      abAppend(ab, "\r\n", 2);
    }
  }
}

void editorRefreshScreen() {
  struct abuf ab = ABUF_INIT;
  abAppend(&ab, "\x1b[?25l", 6);
  // [?25l escape sequence used for hiding the cursor
  // VT100 escape sequences will be followed
  abAppend(&ab, "\x1b[H", 3);
  // [H escape sequence for cursor positioning. By default,
  // at the top left of the editor
  editorDrawRows(&ab);
  char buf[32];
  snprintf(buf, sizeof(buf), "\x1b[%d;%dH", E.cy+1, E.cx+1);
  // add cursor to the exact position
  // E.cy+1 & E.cx+1 used to make the 0-based index to
  // 1-based index.
  // The terminal uses 1-based index but C uses 0-based index
  abAppend(&ab, buf, strlen(buf));
  abAppend(&ab, "\x1b[?25h", 6);
  // [?25h escape sequence used for showing the cursor 
  write(STDOUT_FILENO, ab.b, ab.len); // writing buffer contents to
  // standard output
  abFree(&ab); // freeing the memory used by abuf
}

/*** input ***/

void editorMoveCursor (char key) {
  switch (key) {
  case 'a':
    // moving the cursor left
    E.cx--;
    break;
  case 'd':
    // moving the cursor right
    E.cx++;
    break;
  case 'w':
    // moving the cursor up
    E.cy--;
    break;
  case 's':
    // moving the cursor down
    E.cy++;
    break;
  }
}

void editorProcessKeypress() {
  // process the keypress
  char c = editorReadKey();
  switch (c) {
  case CTRL_KEY('q'):
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);
    exit(0);
    break;

  case 'w':
  case 's':
  case 'a':
  case 'd':
    editorMoveCursor(c);
    break;
  }
}

/*** initialization ***/

void initialEditor() {
  E.cx = 0; // horizontal coordinate of the cursor that denotes the column
  E.cy = 0; // vertical coordinate of the cursor that denotes the row
  if (getWindowSize(&E.screenrows, &E.screencolumns)==-1) {
    // exception handling
    die("getWindowSize");
  }
}

int main()
{
  enableRawMode();
  initialEditor(); // Initialize all fields of editorConfig
  char c;
  while (1) {
    editorRefreshScreen();
    editorProcessKeypress();
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


