/*** includes ***/

#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE

#include <ctype.h> // iscntrl() resides in it
#include <stdio.h> 
// printf(), perror(), sscanf(), snprintf(), FILE,
// fopen(), getline() reside in it
#include <stdlib.h> 
// atexit(), exit(), realloc(), free(), malloc() reside in it 
#include <termios.h> 
// struct termios, tcgetattr(), tcsetattr(), ECHO, TCSAFLUSH, 
// ICANON, ISIG, IXON. IEXTEN, ICRNL, OPOST, BRKINT, INPCK, 
// ISTRIP, CS8, VMIN, VTIME reside in it
#include <unistd.h> 
// read(), STDIN_FILENO, write(), STDOUT_FILENO reside in it
#include <errno.h> // errno, EAGAIN reside in it
#include <sys/ioctl.h> 
// ioctl(), TIOCGWINSZ, struct winsize reside in it.
#include <string.h> // memcpy(), strlen() resides in it
#include <sys/types.h> // ssize_t resides in it

/*** defines ***/

#define CODIBLE_VERSION "0.0.1"
#define CTRL_KEY(k) ((k) & 0x1f)

// mapping WASD keys with the arrow constants
enum editorKey {
  ARROW_LEFT = 1524, 
  // arbitrary number that is out of range of char
  ARROW_RIGHT, // 1525
  ARROW_UP, // 1526
  ARROW_DOWN, // 1527
  DEL_KEY, // 1528
  HOME_KEY, // 1529
  END_KEY, // 1530
  PAGE_UP, // 1531
  PAGE_DOWN // 1532
};

/*** data ***/

typedef struct erow {
  // editor row
  int size;
  char *chars;
  // store a line of text as a pointer
} erow;

struct editorConfig {
  int cx, cy;
  int rowoff;
  int screenrows;
  int screencolumns;
  int numrows;
  // number of rows to be displayed
  // making erow arrays for taking multiple lines
  erow *row;
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
  raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON); 
  // IXON used to ignore XOFF and XON & ICRNL used to handle 
  // Carriage Return (CR) and New Line (NL), 
  // BRKINT used to handle SIGINT, 
  // INPCK used to handle Parity Check, 
  // ISTRIP used to handle 8-bit stripping, 
  // CS8 used to handle character size (CS) to 8 bits per byte
  raw.c_oflag &= ~(OPOST); 
  // OPOST used to handle post-processing output
  raw.c_cflag |= ~(CS8);
  raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG); 
  // ICANON used for reading input byte by byte & 
  // ISIG used to ignore SIGINT and SIGTSTP, 
  // IEXTEN used to disable Ctrl-o and Ctrl-V
  raw.c_cc[VMIN] = 0; // control characters for terminal settings
  raw.c_cc[VTIME] = 1; // control characters for terminal settings
  // error checking for setting up raw mode
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) {
    die("tcsetattr");
  }
}

int editorReadKey() {
  // read the keypress
  int nread;
  char c;
  while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
    if (nread == -1 && errno != EAGAIN) {
      die("read");
    }
  }
  if (c=='\x1b') {
    char seq[3];
    // checking the escape sequence for determining "Escape" or
    // "Arrow" keys.
    if (read(STDIN_FILENO, &seq[0], 1) != 1) {
      return '\x1b';
    }
    if (read(STDIN_FILENO, &seq[1], 1) != 1) {
      return '\x1b';
    }
    if (seq[0] == '[') {
      if (seq[1]>='0' && seq[1]<='9') {
	// checking that after '[', is it a digit or not
	if (read(STDIN_FILENO, &seq[2], 1) != 1) {
	  return '\x1b';
	}
	if (seq[2] == '~') {
	  // digit 5 for page up, 6 for page down
	  // digit 1 or 7 for Home, 4 or 8 for End
	  // digit 3 for Delete
	  switch (seq[1]) {
	  case '1' : return HOME_KEY;
	  case '3' : return DEL_KEY;
	  case '4' : return END_KEY;
	  case '5' : return PAGE_UP;
	  case '6' : return PAGE_DOWN;
	  case '7' : return HOME_KEY;
	  case '8' : return END_KEY;
	  }
	}
      }
      else {
	switch (seq[1]) {
	// mapping arrow keys to the Arrow Constants.
	case 'A' : return ARROW_UP;
	case 'B' : return ARROW_DOWN;
	case 'C' : return ARROW_RIGHT;
	case 'D' : return ARROW_LEFT;
	case 'H' : return HOME_KEY; // handling the Home keys with 'H'
	case 'F' : return END_KEY; // handling the end keys with 'F'
	}
      }
    }
    else if (seq[0]=='O') {
      // handling Home & End keys escape sequence starting with O 
      // (ooo, not zero)
      switch (seq[1]) {
      case 'H' : return HOME_KEY;
      case 'F' : return END_KEY;
      }
    }
    return '\x1b';
  }
  else {
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
  struct winsize ws; 
  // the terminal size will initially stored here.
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

/*** row operations ***/

void editorAppendRow (char *s, size_t len) {
  E.row = realloc(E.row, sizeof(erow)*(E.numrows + 1));
  int at = E.numrows;
  E.row[at].size = len;
  E.row[at].chars = malloc(len + 1);
  memcpy(E.row[at].chars, s, len);
  E.row[at].chars[len] = '\0';
  E.numrows++;
}

/*** file i/o ***/

void editorOpen(char *filename) {
  // taking a filename & opens it for reading by fopen()
  FILE *fp = fopen(filename, "r");
  if (!fp) {
    die("fopen");
  }
  char *line = NULL;
  size_t linecap = 0;
  ssize_t len;
  // getting the line & len from getline() instead of hardcoded
  // getline returns the length of the line it reads
  // or -1 when it is the end of the file i.e. no more lines
  // to read
  while ((len = getline(&line, &linecap, fp)) != -1) {
    while (len>0 && (line[len-1]=='\n' || line[len-1]=='\r')) {
      // stripping the newline '\n' & carriage return '\r'
      // from the line we consider. It's a one liner. so it will
      // be redundant to include newline or carriage return
      len--;
    }
    editorAppendRow(line, len);
  }
  free(line);
  fclose(fp);
}

/*** append buffer ***/

struct abuf {
  char *b;
  int len;
};

#define ABUF_INIT {NULL, 0} 
// initially pointing to the empty buffer
// worked as a constructor

void abAppend(struct abuf *ab, const char *s, int len) {
  char *new = realloc(ab->b, ab->len + len);
  if (new == NULL) {
    return;
  }
  memcpy(&new[ab->len],s,len); 
  // copy the string s at the end of the buffer
  // updating pointer and length
  ab->b = new;
  ab->len += len;
}

void abFree(struct abuf *ab) {
  free(ab->b);
}

/*** output ***/

void editorScroll() {
  if (E.cy < E.rowoff) {
    // checking if the cursor is within the visible window
    E.rowoff = E.cy;
  }
  if (E.cy >= E.rowoff + E.screenrows) {
    // if the cursor is out of visible window, then
    // scroll & show the remaining part within the
    // visible window
    E.rowoff = E.cy - E.screenrows + 1;
  }
}

void editorDrawRows(struct abuf *ab) {
  // putting '~' in front of each row which is not part of the
  // text being edited
  int y;
  for (y=0; y<E.screenrows; y++) {
    int filerow = y+E.rowoff;
  if (filerow >= E.numrows) {
    // Displaying the welcome message when no file is called
    if (E.numrows==0 && y==E.screenrows/3) {
      char welcome[80];
      int welcomelen = snprintf(welcome,sizeof(welcome),
				"Codible -- version %s", CODIBLE_VERSION);
      // this will show the welcome message at 1/3 of the screen
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
	      // next spaces are filled with " " (spaces) 
        // until the message character starts
	      abAppend(ab, " ", 1);
      }
      // changing length according to the terminal size
      abAppend(ab, welcome, welcomelen);
    }
    else {
      abAppend(ab, "~", 1);
    }
  }
  else {
    int len = E.row[filerow].size;
    if (len > E.screencolumns) {
      len = E.screencolumns;
      abAppend(ab, E.row[filerow].chars, len);
    }
  }   
    abAppend(ab, "\x1b[K", 3);
    // [K escape sequence will clear each line as we redraw them
    if (y < E.screenrows-1) {
      abAppend(ab, "\r\n", 2);
    }
  }
}

void editorRefreshScreen() {
  editorScroll();
  struct abuf ab = ABUF_INIT;
  abAppend(&ab, "\x1b[?25l", 6);
  // [?25l escape sequence used for hiding the cursor
  // VT100 escape sequences will be followed
  abAppend(&ab, "\x1b[H", 3);
  // [H escape sequence for cursor positioning. By default,
  // at the top left of the editor
  editorDrawRows(&ab);
  char buf[32];
  // putting the cursor to the previous position within the 
  // visible window when scroll up
  snprintf(buf, sizeof(buf), "\x1b[%d;%dH", 
    (E.cy-E.rowoff)+1, E.cx+1);
  // add cursor to the exact position
  // E.cy+1 & E.cx+1 used to make the 0-based index to
  // 1-based index.
  // The terminal uses 1-based index but C uses 0-based index
  abAppend(&ab, buf, strlen(buf));
  abAppend(&ab, "\x1b[?25h", 6);
  // [?25h escape sequence used for showing the cursor 
  write(STDOUT_FILENO, ab.b, ab.len); 
  // writing buffer contents to standard output
  abFree(&ab); // freeing the memory used by abuf
}

/*** input ***/

void editorMoveCursor (int key) {
  switch (key) {
  case ARROW_LEFT:
    // moving the cursor left
    if (E.cx != 0) {
      E.cx--;
    }
    break;
  case ARROW_RIGHT:
    // moving the cursor right
    if (E.cx != E.screencolumns-1) {
      E.cx++;
    }
    break;
  case ARROW_UP:
    // moving the cursor up
    if (E.cy != 0) {
      E.cy--;
    }
    break;
  case ARROW_DOWN:
    // moving the cursor down
    if (E.cy < E.numrows) {
      // checking the cursor not going down after the 
      // very last line of the file
      E.cy++;
    }
    break;
  }
}

void editorProcessKeypress() {
  // process the keypress
  int c = editorReadKey();
  switch (c) {
  case CTRL_KEY('q'):
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);
    exit(0);
    break;

  case HOME_KEY:
    E.cx = 0;
    break;

  case END_KEY:
    E.cx = E.screencolumns-1;
    break;

    // page up will send the cursor at the top row
    // page down will send the cursor at the bottom row

  case PAGE_UP:
  case PAGE_DOWN:
    {
      int times = E.screenrows;
      while (times--) {
	editorMoveCursor(c==PAGE_UP ? ARROW_UP : ARROW_DOWN);
      }
    }
    break;

  case ARROW_UP:
  case ARROW_DOWN:
  case ARROW_LEFT:
  case ARROW_RIGHT:
    editorMoveCursor(c);
    break;
  }
}

/*** initialization ***/

void initialEditor() {
  E.cx = 0; 
  // horizontal coordinate of the cursor that denotes the column
  E.cy = 0; 
  // vertical coordinate of the cursor that denotes the row
  E.rowoff = 0;
  E.numrows = 0;
  E.row = NULL;
  if (getWindowSize(&E.screenrows, &E.screencolumns)==-1) {
    // exception handling
    die("getWindowSize");
  }
}

int main(int argc, char *argv[])
{
  enableRawMode();
  initialEditor(); // Initialize all fields of editorConfig
  // checking if file passed or not. If no file is called from 
  // command-line, then codible will open blank file just like
  // Emacs
  if (argc >= 2) {
    editorOpen(argv[1]);
  }
  while (1) {
    editorRefreshScreen();
    editorProcessKeypress();
  }
  return 0;
}