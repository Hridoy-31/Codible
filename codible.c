/*** includes ***/

#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE

#include <ctype.h> // iscntrl() resides in it
#include <stdio.h> 
// printf(), perror(), sscanf(), snprintf(), FILE,
// fopen(), getline(), vsnprintf() reside in it
#include <stdlib.h> 
// atexit(), exit(), realloc(), free(), malloc() reside in it 
#include <termios.h> 
// struct termios, tcgetattr(), tcsetattr(), ECHO, TCSAFLUSH, 
// ICANON, ISIG, IXON. IEXTEN, ICRNL, OPOST, BRKINT, INPCK, 
// ISTRIP, CS8, VMIN, VTIME reside in it
#include <unistd.h> 
// read(), STDIN_FILENO, write(), STDOUT_FILENO
// ftruncate(), close() reside in it
#include <errno.h> // errno, EAGAIN reside in it
#include <sys/ioctl.h> 
// ioctl(), TIOCGWINSZ, struct winsize reside in it.
#include <string.h> // memcpy(), strlen(), 
// strdup(), memmove(), strerror(), strstr(), memset()
// strchr(), strrchr(), strcmp() reside in it
#include <sys/types.h> // ssize_t resides in it
#include <time.h> // time_t, time() reside in it
#include <stdarg.h> // va_list, va_start(), va_end() reside in it
#include <fcntl.h> // open(), O_RDWR, O_CREAT reside in it 

/*** defines ***/

#define CODIBLE_VERSION "0.0.1"
#define CTRL_KEY(k) ((k) & 0x1f)
#define CODIBLE_TAB_STOP 8
// press Ctrl-Q 3 more times to quit the editor without saving
#define CODIBLE_QUIT_TIMES 3 

// mapping WASD keys with the arrow constants
enum editorKey {
  BACKSPACE = 127,
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

enum editorHighlight {
  HL_NORMAL = 0,
  HL_NUMBER,
  HL_MATCH
};

#define HL_HIGHLIGHT_NUMBERS (1<<0)

/*** data ***/

struct editorSyntax {
  char *filetype;
  char **filematch;
  int flags;
};

// editor row
typedef struct erow {
  // the size of rendering characters
  int size;
  int rsize;
  // store a line of text as a pointer
  char *chars;
  // for rendering the non-printable characters;
  char *render;
  // highlighted array
  unsigned char *highlight;

} erow;

struct editorConfig {
  int cx, cy;
  int rx;
  int rowoff;
  int coloff;
  int screenrows;
  int screencolumns;
  int numrows;
  // number of rows to be displayed
  // making erow arrays for taking multiple lines
  erow *row;
  // identify if the buffer is changed
  int dirty;
  char *filename;
  char statusmessage[80];
  time_t statusmessage_time;
  struct editorSyntax *syntax;
  struct termios original;
};

struct editorConfig E;

/*** filetypes ***/

char *C_HL_extensions[] = {".c", ".h", ".cpp", NULL};
// highlight database;
struct editorSyntax HLDB[] = {
  {
    "c",
    C_HL_extensions,
    HL_HIGHLIGHT_NUMBERS
  },
};

#define HLDB_ENTRIES (sizeof(HLDB) / sizeof(HLDB[0]))

/*** prototypes ***/

void editorSetStatusMessage(const char *fmt, ...);
void editorRefreshScreen();
char *editorPrompt(char *prompt, void (*callback)(char *, int));

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

/*** syntax highlighting ***/

int is_separator (int c) {
  // if the string doesn't contain the characters
  // provided in the strchr(), the function will
  // return NULL
  return (isspace(c) || c=='\0' || 
    strchr(",\".()+-/*=~%<>[];", c) != NULL);
}

void editorUpdateSyntax(erow *row) {
  row->highlight = realloc(row->highlight, row->rsize);
  memset(row->highlight, HL_NORMAL, row->rsize);
  if (E.syntax == NULL) {
    return;
  }
  // considering the starting of a line as a separator
  int i = 0, prev_separator = 1;
  while (i < row->rsize) {
    char c = row->render[i];
    unsigned char prev_highlight = (i>0) ? row->highlight[i-1] : 
      HL_NORMAL;
    if (E.syntax->flags && HL_HIGHLIGHT_NUMBERS) {
      if ((isdigit(c) && 
            (prev_separator||prev_highlight==HL_NUMBER)) 
            || (c == '.' && prev_highlight == HL_NUMBER)) {
        row->highlight[i] = HL_NUMBER;
        i++;
        prev_separator = 0;
        continue;
      }
    }
    
    prev_separator = is_separator(c);
    i++;
  }
}

int editorSyntaxToColor(int highlight) {
  switch (highlight) {
    case HL_NUMBER:
    // digits coloring with red
      return 31;
    case HL_MATCH:
    // matched string coloring with blue
      return 34;
    default:
      return 37;
  }
}

void editorSelectSyntaxHighlight() {
  E.syntax = NULL;
  if (E.filename == NULL) {
    return;
  }
  //strrchr() returns a pointer to the last occurance of
  // a character in a string
  char *ext = strrchr(E.filename, '.');
  for(unsigned int j=0; j<HLDB_ENTRIES; j++) {
    struct editorSyntax *s = &HLDB[j];
    unsigned int i = 0;
    while(s->filematch[i]) {
      int is_ext = (s->filematch[i][0] == '.');
      if ((is_ext && ext && !strcmp(ext, s->filematch[i])) || 
            (!is_ext && strstr(E.filename, s->filematch[i]))) {
        E.syntax = s;
        for (int filerow = 0; filerow < E.numrows; filerow++) {
          editorUpdateSyntax(&E.row[filerow]);
        }
        return;
      }
      i++;
    }
  }
}

/*** row operations ***/

int editorRowCxToRx (erow *row, int cx) {
  int rx = 0;
  for (int j=0; j<cx; j++) {
    if (row->chars[j] == '\t') {
      rx = rx + (CODIBLE_TAB_STOP - 1) - (rx % CODIBLE_TAB_STOP);
      // (rx % CODIBLE_TAB_STOP) = how many columns to the right
      // of the last tab stop
      // (CODIBLE_TAB_STOP - 1) = how many columns to the left
      // of the next tab stop
      // Added these with rx to get to the next tab stop
    }
    rx++; // gets on the next tab stop
  } 
  return rx;
}

int editorRowRxToCx(erow *row, int rx) {
  int current_rx=0,cx;
  for (cx = 0; cx < row->size; cx++) {
    if (row->chars[cx] == '\t') {
      current_rx += (CODIBLE_TAB_STOP-1)-(current_rx%CODIBLE_TAB_STOP);
    }
    current_rx++;
    if (current_rx > rx) {
      return cx;
    }
  }
  return cx;
}

void editorUpdateRow(erow *row) {
  int tabs = 0;
  for (int j=0; j<row->size; j++) {
    // counting tabs found in chars of the row
    if (row->chars[j] == '\t') {
      tabs++;
    }
  }
  free(row->render);
  // the maximum number of characters needed
  // for a tab is 8
  row->render = malloc(row->size + tabs*(CODIBLE_TAB_STOP-1) + 1);
  int index = 0;
  for (int j=0; j<row->size; j++) {
    // if a tab is found, replace it with a space
    // until the tab stop location
    if (row->chars[j] == '\t') {
      row->render[index++] = ' ';
      // tab stop location can be divided by 8
      while (index%CODIBLE_TAB_STOP != 0) {
        row->render[index++] = ' ';
      }
    }
    else {
      row->render[index++] = row->chars[j];
    }
  }
  row->render[index] = '\0';
  row->rsize = index;
  editorUpdateSyntax(row);
}

void editorInsertRow (int at, char *s, size_t len) {
  if (at<0 || at>E.numrows) {
    // validating the index
    return;
  }
  E.row = realloc(E.row, sizeof(erow)*(E.numrows + 1));
  memmove(&E.row[at+1], &E.row[at], sizeof(erow)*(E.numrows-at));
  E.row[at].size = len;
  E.row[at].chars = malloc(len + 1);
  memcpy(E.row[at].chars, s, len);
  E.row[at].chars[len] = '\0';
  // Initializing the rendering size is 0 
  // and the rendering string is NULL
  E.row[at].rsize = 0;
  E.row[at].render = NULL;
  E.row[at].highlight = NULL;
  editorUpdateRow(&E.row[at]);
  E.numrows++;
  E.dirty++;
}

void editorFreeRow(erow *row) {
  free(row->render);
  free(row->chars);
  free(row->highlight);
}

void editorDelRow(int at) {
  if (at<0 || at>=E.numrows) {
    return;
  }
  editorFreeRow(&E.row[at]);
  memmove(&E.row[at], &E.row[at+1], sizeof(erow)*(E.numrows-at-1));
  E.numrows--;
  E.dirty++;
}

void editorRowInsertChar(erow *row, int at, int c) {
  if (at<0 || at>row->size) {
    at = row->size;
  }
  row->chars = realloc(row->chars, row->size + 2);
  // memmove is safe to use than memcpy when source & 
  // destination of an array overlaps with each other 
  memmove(&row->chars[at+1], &row->chars[at], row->size - at+1);
  row->size++;
  row->chars[at] = c;
  // updating render & rsize
  editorUpdateRow(row);
  E.dirty++;
}

void editorRowAppendString(erow *row, char *s, size_t len) {
  row->chars = realloc(row->chars, row->size+len+1);
  // appending the string in the row
  memcpy(&row->chars[row->size], s, len);
  // updating the size
  row->size += len;
  row->chars[row->size] = '\0';
  editorUpdateRow(row);
  E.dirty++;
}

void editorRowDelChar(erow *row, int at) {
  if (at < 0 || at >= row->size) {
    return;
  }
  // overwrite the deleted character with the next character
  memmove(&row->chars[at], &row->chars[at+1], row->size - at);
  // then decrement the size of the row
  row->size--;
  editorUpdateRow(row);
  E.dirty++;
}

/*** editor operations ***/

void editorInsertChar (int c) {
  if (E.cy == E.numrows) {
    // appending a blank row after the end of a line to take 
    // the character from the user
    editorInsertRow(E.numrows,"", 0);
  }
  editorRowInsertChar(&E.row[E.cy], E.cx, c);
  // after taking the character, moving forward the cursor
  // to take the next character right after the previous one
  E.cx++;
}

void editorInsertNewLine() {
  if (E.cx == 0) {
    // if the cursor at the beginning of a line, then 
    // pressing Enter will create a blank line before
    // that line
    editorInsertRow(E.cy, "", 0);
  }
  else {
    erow *row = &E.row[E.cy];
    // passing the characters of the right of cursor
    // to the new line
    editorInsertRow(E.cy+1, &row->chars[E.cx], row->size - E.cx);
    row = &E.row[E.cy];
    row->size = E.cx;
    row->chars[row->size] = '\0';
    editorUpdateRow(row);
  }
  E.cy++;
  E.cx=0;
}

void editorDelChar() {
  if (E.cy == E.numrows) {
    // return immediately if the cursor gets to 
    // the end of the file
    return;
  }
  if (E.cx == 0 && E.cy == 0) {
    // return immediately if the cursor is at the 
    // beginning of the first line
    return;
  }
  erow *row = &E.row[E.cy];
  if (E.cx > 0) {
    editorRowDelChar(row, E.cx - 1);
    E.cx--;
  }
  else {
    // setting the cursor at the end of the previous row
    // before appending
    E.cx = E.row[E.cy-1].size;
    editorRowAppendString(&E.row[E.cy-1], row->chars, row->size);
    editorDelRow(E.cy);
    E.cy--;
  }
}

/*** file i/o ***/

char *editorRowsToString(int *buflen) {
  int totallen=0;
  // getting the total size to be copied
  for (int j=0; j<E.numrows; j++) {
    totallen = totallen + E.row[j].size + 1;
  }
  *buflen = totallen;
  char *buf = malloc(totallen);
  char *p = buf;
  for (int j=0; j<E.numrows; j++) {
    memcpy(p, E.row[j].chars, E.row[j].size);
    p = p + E.row[j].size;
    // appending the new line character after each row
    *p = '\n';
    p++;
  }
  return buf;
  // the caller function will free the buf
}

void editorOpen(char *filename) {
  free(E.filename);
  E.filename = strdup(filename);
  editorSelectSyntaxHighlight();
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
    editorInsertRow(E.numrows, line, len);
  }
  free(line);
  fclose(fp);
  E.dirty = 0;
}

void editorSave() {
  if (E.filename == NULL) {
    E.filename = editorPrompt("Save as: %s (ESC to cancel)", NULL);
    if (E.filename == NULL) {
      editorSetStatusMessage("Save aborted");
      return;
    }
    editorSelectSyntaxHighlight();
  }
  int len;
  char *buf = editorRowsToString(&len);
  // opening E.filename
  // if it's new, then create a file. that's why O_CREAT used
  // if it exists, then open the file for read and write.
  // Thats's why O_RDWR is used.
  // OCREAT's permission here is  0644.
  // The 0644 permits that the owner can read and write 
  // whenever they want
  // otherwise the user can only read
  int fd = open(E.filename, O_RDWR | O_CREAT, 0644);
  if (fd != -1) {
    if (ftruncate(fd, len) != -1) {
      if (write(fd, buf, len) == len) {
        close(fd);
        free(buf);
        E.dirty = 0;
        editorSetStatusMessage("%d bytes written to disk", len);
        return;
      }
    }
    close(fd);
  }
  free(buf);
  editorSetStatusMessage("Can't save !! I/O error: %s", 
    strerror(errno));
}

/*** find ***/

void editorFindCallBack(char *query, int key) {
  // last match is the index in the row that have
  // searched previous query
  static int last_match = -1;
  // direction 1 means forward search, 
  // diredtion 2 means backward search
  static int direction = 1;
  static int saved_highlight_line;
  static char *saved_highlight = NULL;
  if (saved_highlight) {
    memcpy(E.row[saved_highlight_line].highlight, saved_highlight, 
      E.row[saved_highlight_line].rsize);
    free(saved_highlight);
    saved_highlight = NULL;
  }
  // stopping incremental search if the user pressed 
  // ENTER or ESC key
  if (key == '\r' || key == '\x1b') {
    // resetting last_match and direction to get ready
    // for next search operation
    last_match = -1;
    direction = 1;
    return;
  }
  else if (key == ARROW_RIGHT || key == ARROW_DOWN) {
    direction = 1;
  }
  else if (key == ARROW_LEFT || key == ARROW_UP) {
    direction = -1;
  }
  else {
    last_match = -1;
    direction = 1;
  }
  if (last_match == -1) {
    direction = 1;
  }
  // current is the current row we are searching
  int current = last_match;
  for (int i=0; i<E.numrows; i++) {
    current = current + direction;
    if (current == -1) {
      current = E.numrows - 1;
    }
    else if (current == E.numrows) {
      current = 0;
    }
    erow *row = &E.row[current];
    // using strstr() to find if query is a substring of the 
    // current row
    char *match = strstr(row->render, query);
    if (match) {
      last_match = current;
      E.cy = current;
      E.cx = editorRowRxToCx(row, match-row->render);
      // the matching line will always be on top by setting 
      // the rowoff very bottom of the file
      E.rowoff = E.numrows;
      saved_highlight_line = current;
      saved_highlight = malloc(row->rsize);
      memcpy(saved_highlight, row->highlight, row->rsize);
      memset(&row->highlight[match - row->render], 
        HL_MATCH, strlen(query));
      break;
    }
  }
}

void editorFind() {
  // saving the cursors position before search
  int saved_cx = E.cx;
  int saved_cy = E.cy;
  int saved_coloff = E.coloff;
  int saved_rowoff = E.rowoff;
  char *query = editorPrompt("Search: %s (Use ESC/Arrows/ENTER)", editorFindCallBack);
  if (query) {
    free(query);
  }
  else {
    // restoring the cursor position before search
    E.cx = saved_cx;
    E.cy = saved_cy;
    E.coloff = saved_coloff;
    E.rowoff = saved_rowoff;
  }
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
  E.rx = 0;
  if (E.cy < E.numrows) {
    E.rx = editorRowCxToRx(&E.row[E.cy], E.cx);
  }
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
  if (E.rx < E.coloff) {
    // checking if the cursor is within the visible window
    E.coloff = E.rx;
  }
  if (E.rx >= E.coloff + E.screencolumns) {
    // if the cursor is out of visible window, then
    // scroll & show the remaining part within the
    // visible window
    E.coloff = E.rx - E.screencolumns + 1;
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
    // to show the remaining part of a line beyond the visible 
    // window
    int len = E.row[filerow].rsize - E.coloff;
    if (len < 0) {
      // nothing will be displayed on the line after scrolling
      // if the cursor beyond the end of the line
      len = 0;
    }
    if (len > E.screencolumns) {
      len = E.screencolumns;
    }
    char *c = &E.row[filerow].render[E.coloff];
    unsigned char *highlight = &E.row[filerow].highlight[E.coloff];
    int current_color = -1;
    for (int j=0; j<len; j++) {
      if (highlight[j] == HL_NORMAL) {
        if (current_color != -1) {
          abAppend(ab, "\x1b[39m", 5);
          current_color = -1;
        }
        abAppend(ab, &c[j], 1);
      }
      else {
        int color = editorSyntaxToColor(highlight[j]);
        if (color != current_color) {
          current_color = color;
          char buffer[16];
          int colorlen = snprintf(buffer, sizeof(buffer),
            "\x1b[%dm", color);
          abAppend(ab, buffer, colorlen);
        }
        abAppend(ab, &c[j], 1);
      }
    }
    // resetting the text color to default
    abAppend(ab, "\x1b[39m", 5);
  }   
  abAppend(ab, "\x1b[K", 3);
  // [K escape sequence will clear each line as we redraw them
  abAppend(ab, "\r\n", 2);
  }
}

void editorDrawStatusBar (struct abuf *ab) {
  // making the status bar in inverted colors
  // "\x1b[7m" switches to inverted color formatting
  abAppend(ab, "\x1b[7m", 4);
  char status[80], rstatus[80];
  int len = snprintf(status, sizeof(status), "%.20s - %d lines %s",
    E.filename ? E.filename : "[No Name]", E.numrows,
    E.dirty ? "(modified)" : "");
  int rlen = snprintf(rstatus, sizeof(rstatus), "%s | %d/%d",
    E.syntax ? E.syntax->filetype: "no filetype", 
    E.cy+1, E.numrows);
  // restricting the characters to remain in status bar
  if (len > E.screencolumns) {
    len = E.screencolumns;
  }
  abAppend(ab, status, len);
  // filling the status bar with blank spaces
  while (len < E.screencolumns) {
    // for printing the current line number at the very
    // right side of the status bar
    if (E.screencolumns - len == rlen) {
      abAppend(ab, rstatus, rlen);
      break;
    }
    else {
    abAppend(ab, " ", 1);
    len++;
    }
  }
  // "\x1b[m" switches (back) to the normal text formatting
  abAppend(ab, "\x1b[m", 3);
  abAppend(ab, "\r\n", 2);
}

void editorDrawMessageBar(struct abuf *ab) {
  // clearing the message bar
  abAppend(ab, "\x1b[K", 3);
  int msglen = strlen(E.statusmessage);
  // fitting the message within the column space
  if (msglen > E.screencolumns) {
    msglen = E.screencolumns;
  }
  // displaying the message if it's less than 5 second's old
  if (msglen && time(NULL) - E.statusmessage_time < 5) {
    abAppend(ab, E.statusmessage, msglen);
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
  editorDrawStatusBar(&ab);
  editorDrawMessageBar(&ab);
  char buf[32];
  // putting the cursor to the previous position within the 
  // visible window when scroll up
  snprintf(buf, sizeof(buf), "\x1b[%d;%dH", 
    (E.cy-E.rowoff)+1, (E.rx-E.coloff)+1);
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

// A thorough knowledge of VARIADIC FUNCTION is needed !!!
// the ... denotes the function can take any number of arguments
void editorSetStatusMessage (const char *fmt, ...) {
  // there will be va_start() to denote the start 
  // of the operation and va_end() to denote the end
  // of the operation of a value type va_list.
  va_list ap;
  // the last argument before the ... must be passed
  // to va_start() to get the address of the next argument.
  // Between va_start() and va_end(), there will be va_arg()
  // which will be executed on the type of the next argument.
  va_start(ap, fmt);
  // storing the resulting string in E.statusmessage 
  // Here, vsnprintf() works as a va_arg();
  vsnprintf(E.statusmessage, sizeof(E.statusmessage), fmt, ap);
  va_end(ap);
  // setting the current time in status message time
  E.statusmessage_time = time(NULL);
  // current time can be gotten by passing NULL inside the time()
  // it will calculate the seconds have past since 
  // January 1, 1970 midnight.
}

/*** input ***/

char *editorPrompt(char *prompt, void (*callback)(char *, int)) {
  size_t buffersize = 128;
  char *buffer = malloc(buffersize);
  // initially the prompt is initialized as an empty string
  size_t bufferlen = 0;
  buffer[0] =  '\0';

  while (1) {
    editorSetStatusMessage(prompt, buffer);
    editorRefreshScreen();
    int c = editorReadKey();
    if (c==DEL_KEY || c==CTRL_KEY('h') || c==BACKSPACE) {
      if (bufferlen != 0) {
        buffer[--bufferlen] = '\0';
      }
    }
    // checking for Esc key, if pressed, then the input 
    // prompt will disappear
    else if (c == '\x1b') {
      editorSetStatusMessage("");
      if (callback) {
        callback(buffer,c);
      }
      free(buffer);
      return NULL;
    }
    else if (c == '\r') {
      if (bufferlen != 0) {
        // when input is not empty and Enter key has been
        // pressed, then the status message gets clear
        editorSetStatusMessage("");
        if (callback) {
          callback(buffer,c);
        }
        return buffer;
      }
    }
    // checking the input is a printable character or not
    else if (!iscntrl(c) && c<128) {
      if (bufferlen == buffersize-1) {
        // if bufferlen reaches the maximum capacity,
        // we double the capacity and reallocating it
        buffersize *= 2;
        buffer = realloc(buffer, buffersize);
      }
      buffer[bufferlen++] = c;
      buffer[bufferlen] = '\0';
    }
    if (callback) {
      callback(buffer,c);
    }
  }
}

void editorMoveCursor (int key) {
  // checking whether the cursor is in last line or not
  erow *row = (E.cy >= E.numrows) ? NULL : &E.row[E.cy];
  switch (key) {
  case ARROW_LEFT:
    // moving the cursor left
    if (E.cx != 0) {
      E.cx--;
    }
    // implementing the feature of pressing left arrow to
    // go to the end of the previous line
    // and checking it's not the very first line
    else if (E.cy > 0) {
      E.cy--;
      E.cx = E.row[E.cy].size;
    }
    break;
  case ARROW_RIGHT:
    // moving the cursor right
    // checking if the cursor is in left of the last
    // character or not
    if (row && E.cx < row->size) {
      E.cx++;
    }
    // implementing the feature of pressing right arrow
    // to go to the beginning of the next line
    else if (row && E.cx == row->size) {
      E.cy++;
      E.cx = 0;
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
  row = (E.cy >= E.numrows) ? NULL : &E.row[E.cy];
  int rowlen = row ? row->size : 0;
  if (E.cx > rowlen) {
    // limiting the cursor for not to go beyond the
    // endline;
    E.cx = rowlen;
  }
}

void editorProcessKeypress() {
  static int quit_times = CODIBLE_QUIT_TIMES;
  // process the keypress
  int c = editorReadKey();
  switch (c) {
    // case handling for "Enter" key
    case '\r':
      editorInsertNewLine();
      break;

    case CTRL_KEY('q'):
      if (E.dirty && quit_times > 0) {
        editorSetStatusMessage("WARNING!!! File has unsaved"
          " changes. Press Ctrl-Q %d more times to quit.", 
          quit_times);
        quit_times--;
        return;
      }
      write(STDOUT_FILENO, "\x1b[2J", 4);
      write(STDOUT_FILENO, "\x1b[H", 3);
      exit(0);
      break;

    case CTRL_KEY('s'):
      editorSave();
      break;

    case HOME_KEY:
      E.cx = 0;
      break;

    case END_KEY:
      // moves the cursor at the end of the current file
      if (E.cy < E.numrows) {
        E.cx = E.row[E.cy].size;
      }
      break;

    case CTRL_KEY('f'):
      // search feature prompt
      editorFind();
      break;

    case BACKSPACE:
    case CTRL_KEY('h'):
    case DEL_KEY:
      // delete key implementation
      if (c == DEL_KEY) {
        editorMoveCursor(ARROW_RIGHT);
      }
      editorDelChar();
      break;

      // page up will send the cursor at the top row
      // page down will send the cursor at the bottom row

    case PAGE_UP:
    case PAGE_DOWN:
      {
      // scrolling up a page by PAGE_UP
      if (c == PAGE_UP) {
        E.cy = E.rowoff;
      }
      // scrolling down a page by PAGE_DOWN
      else if (c == PAGE_DOWN) {
        E.cy = E.rowoff + E.screenrows - 1;
        if (E.cy > E.numrows) {
          E.cy = E.numrows;
        }
      }
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

    // Ctrl-L used to refresh the terminal window
    case CTRL_KEY('l'):
    // case handling for "Esc" key
    case '\x1b':
      break;

    default:
      editorInsertChar(c);
      break;
  }
  // by pressing any key other than Ctrl-Q, the quit_times
  // resets back to 3
  quit_times = CODIBLE_QUIT_TIMES;
}

/*** initialization ***/

void initialEditor() {
  E.cx = 0; 
  // horizontal coordinate of the cursor that denotes the column
  E.cy = 0; 
  // vertical coordinate of the cursor that denotes the row
  E.rx = 0; // horizontal coordinate used for rendering
  E.rowoff = 0; // row offset
  E.coloff = 0; // column offset
  E.numrows = 0;
  E.row = NULL;
  E.dirty = 0;
  E.filename = NULL;
  E.statusmessage[0] = '\0';
  E.statusmessage_time = 0;
  E.syntax = NULL;
  if (getWindowSize(&E.screenrows, &E.screencolumns)==-1) {
    // exception handling
    die("getWindowSize");
  }
  E.screenrows = E.screenrows - 2;
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
  editorSetStatusMessage(
    "HELP: Ctrl-S = Save | Ctrl-Q = Quit | Ctrl-F = Find");
  while (1) {
    editorRefreshScreen();
    editorProcessKeypress();
  }
  return 0;
}