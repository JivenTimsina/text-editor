// includes
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

// defines
#define MAX_ROWS 1000
#define MAX_COLS 200
#define FILENAME_MAX_LEN 30

// structs
typedef struct {
  char data[MAX_ROWS][MAX_COLS];
  int rowLengths[MAX_ROWS];
  int numRows;
} Buffer;

// file
typedef struct {
  FILE *Pfile;
  int dirty;
  char fileName[FILENAME_MAX_LEN];
} File;

// editor
typedef struct {
  int cx, cy;
  int rows, cols;
  Buffer buffer;
  File file;
  char status[100];
  unsigned char ch[2];
} editorState;

// terminal attributes in term
struct termios term;

// initilize editorstate
editorState E = {.cx = 0, .cy = 0, .buffer.numRows = 1, .file.dirty = 0};

//  enable raw mode
void enableRawMode() {
  tcgetattr(STDIN_FILENO, &term);
  struct termios raw = term;
  raw.c_lflag &= ~(ECHO | ICANON | ISIG);
  raw.c_iflag &= ~(IXON);
  tcsetattr(0, TCSANOW, &raw);
}

// disable raw mode
void disableRawMode() { tcsetattr(STDIN_FILENO, TCSANOW, &term); }

// getch function implimentation
char getch() {
  struct termios org, raw;
  tcgetattr(0, &org);
  raw = org;
  raw.c_lflag &= ~(ICANON | ECHO);
  tcsetattr(0, TCSANOW, &raw);
  char ch;
  read(STDIN_FILENO, &ch, 1);
  tcsetattr(0, TCSANOW, &org);
  return ch;
}

// move the cursor the a specific locatoin
void move(int y, int x) {
  printf("\033[%d;%dH", y + 1, x + 1);
  fflush(stdout);
}

// Get terminal size
void getWindowSize() {
  struct winsize ws;
  ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws);
  E.rows = ws.ws_row;
  E.cols = ws.ws_col;
}

// status bar
void drawStatusBar(const char *msg) {
  getWindowSize();             // get the window size
  move(E.rows - 1, 0);         // move curosr to bottom line
  printf("\033[7m");           // white on black
  printf("%-*s", E.cols, msg); // left-align msg, fill rest with spaces
  printf("\033[0m");           // reset attributes
  fflush(stdout);
}

// save to file
void saveToFile() {
  E.file.Pfile = fopen(E.file.fileName, "w");
  if (!E.file.Pfile) {
    fprintf(stderr, "failed to open file!\n");
    exit(1);
  }
  for (int i = 0; i < E.buffer.numRows; i++) {
    fprintf(E.file.Pfile, "%s\n", E.buffer.data[i]);
  }
  fclose(E.file.Pfile);
  char savedStatus[50];
  snprintf(savedStatus, sizeof(savedStatus), "Saved %d line to %s",
           (E.buffer.numRows), E.file.fileName);
  drawStatusBar(savedStatus);
  usleep(1000000 / 2);
  E.file.dirty = 0;
}

// open file
void openFile() {
  E.file.Pfile = fopen(E.file.fileName, "r");
  if (!E.file.Pfile)
    return;
  char ch;
  while ((ch = fgetc(E.file.Pfile)) != EOF) {
    if (ch == '\n') {
      E.buffer.numRows++;
      E.cy++;
      E.cx = 0;
      move(E.cy, E.cx);
      printf("%c", ch);
    } else {
      E.buffer.data[E.cy][E.cx] = ch;
      E.cx++;
      E.buffer.rowLengths[E.cy]++;
      move(E.cy, E.cx);
      printf("%c", ch);
    }
  }
  fclose(E.file.Pfile);
  E.cx = 0;
  move(E.cy, E.cx);
}

// new line (Enter key)
void handleNewLine() {
  E.buffer.rowLengths[E.cy] = E.cx;
  E.cy++;
  E.cx = 0;
  E.buffer.rowLengths[E.cy] = 0;
  E.buffer.data[E.cy][0] = '\0';
  E.buffer.numRows++;
}

// backspace
void handleBackspace() {
  if (E.buffer.numRows == 1 && E.cx == 0) {
    return;
  }
  if (E.cx > 0) {
    for (int i = E.cx - 1; i < E.buffer.rowLengths[E.cy]; i++) {
      E.buffer.data[E.cy][i] = E.buffer.data[E.cy][i + 1];
    }
    E.buffer.rowLengths[E.cy]--;
    E.cx--;
  } else {
    E.buffer.rowLengths[E.cy] = 0;
    E.cy--;
    E.cx = E.buffer.rowLengths[E.cy];
    E.buffer.numRows--;
  }
}

// initilize editor
void initEditor() {
  memset(E.buffer.data, 0, sizeof(E.buffer.data));
  memset(E.buffer.rowLengths, 0, sizeof(E.buffer.rowLengths));

  printf("\033[2J"); // clear screen
  // format the status message
  snprintf(E.status, sizeof(E.status), "Ctrl+S = Save | Ctrl+Q = Quit | %s",
           E.file.fileName);
  drawStatusBar(E.status);
  E.cy = 0;
  E.cx = 0;
  move(E.cy, E.cx);
  openFile();
}

// refresh screen
void refreshScreen() {
  write(STDOUT_FILENO, "\x1b[2J", 4); // clear screen
  write(STDOUT_FILENO, "\x1b[H", 3);  // move cursor to (0, 0)
  // draw buffer
  for (int i = 0; i < E.buffer.numRows; i++) {
    if (i < E.buffer.numRows) {
      write(STDOUT_FILENO, E.buffer.data[i], E.buffer.rowLengths[i]);
      write(STDOUT_FILENO, "\n", 1);
    }
  }
  // show status bar
  if (E.file.dirty == 1) {
    snprintf(E.status, sizeof(E.status), "Ctrl+S = Save | Ctrl+Q = Quit | %s *",
             E.file.fileName);
  } else {
    snprintf(E.status, sizeof(E.status), "Ctrl+S = Save | Ctrl+Q = Quit | %s",
             E.file.fileName);
  }
  drawStatusBar(E.status);

  // move the cursor to the editing position
  move(E.cy, E.cx);
}

// add character to buffer
void addToBuffer() {
  if (isprint(E.ch[0])) {
    int len = E.buffer.rowLengths[E.cy];
    if (len < MAX_COLS - 1) {
      // shift right
      for (int i = len; i > E.cx; i--) {
        E.buffer.data[E.cy][i] = E.buffer.data[E.cy][i - 1];
      }
      E.buffer.data[E.cy][E.cx] = E.ch[0];
      E.buffer.rowLengths[E.cy]++;
      E.cx++;
      E.buffer.data[E.cy][E.buffer.rowLengths[E.cy]] = '\0';
    }
  }
}

// process key presses
void processKeyPresses() {
  // input key
  E.ch[0] = getch();

  switch (E.ch[0]) {

  // CTRL + Q
  case 17:
    if (E.file.dirty == 1) // file is changed but not saved
    {
      drawStatusBar("do you really want to quit without saving? (y/n)");
      char ch = getchar();
      if (ch == 'y' || ch == 'Y') {
        printf("\033[2J"); // clear the screen
        printf("\033[H");  // move the cursor to top left
        disableRawMode();
        exit(1);
      } else if (ch == 'n' || ch == 'N') {
        break;
      }
    } else {
      printf("\033[2J"); // clear the screen
      printf("\033[H");  // move the cursor to top left
      disableRawMode();
      exit(1);
      break;
    }

    // Ctrl+S
  case 19:
    saveToFile();
    break;

    // arrow keys
  case 27:
    E.ch[1] = getch();
    if (E.ch[1] == '[') {
      E.ch[2] = getch();
      switch (E.ch[2]) {
      case 'A': // arrow up
        if (E.cy > 0) {
          if (E.cx >= E.buffer.rowLengths[E.cy - 1]) {
            E.cy--;
            E.cx = E.buffer.rowLengths[E.cy];
          } else {
            E.cy--;
          }
        }
        break;

      case 'B': // Arrow down
        if (E.cy < E.buffer.numRows - 1) {
          if (E.cy != E.buffer.numRows &&
              E.cx >= E.buffer.rowLengths[E.cy + 1]) {
            E.cy++;
            E.cx = E.buffer.rowLengths[E.cy];
          } else if (E.cy == E.buffer.numRows - 2 &&
                     E.cx >= E.buffer.rowLengths[E.cy + 1]) {
            E.cy++;
            E.cx = E.buffer.rowLengths[E.cy] + 2;
          } else if (E.cx <= E.buffer.rowLengths[E.cy + 1]) {
            E.cy++;
          }
        }
        break;

      case 'C': // Arrow right
        if (E.cy != E.buffer.numRows - 1 && E.cx < E.buffer.rowLengths[E.cy]) {
          E.cx++;
        } else if (E.cy == E.buffer.numRows - 1 &&
                   E.cx < E.buffer.rowLengths[E.cy]) {
          E.cx++;
        }
        break;

      case 'D': // Arrow left
        if (E.cx > 0) {
          E.cx--;
        }
        break;
      }
    }
    break;

  default:
    if (E.ch[0] == 10) // \n
    {
      handleNewLine();
    } else if (E.ch[0] == 127) // backspace
    {
      handleBackspace();
    } else {
      addToBuffer(); // printable characters
    }
    refreshScreen();
    E.file.dirty = 1;
  }
}

// main
int main(int argc, char *argv[]) {
  if (argc != 2) {
    fprintf(stderr, "Usage: ./main <filename>\n");
    return -1;
  }
  if (strlen(argv[1]) > FILENAME_MAX_LEN) {
    fprintf(stderr, "Max FileName Size Is %d\n", FILENAME_MAX_LEN);
    return -1;
  }
  strcpy(E.file.fileName, argv[1]);

  atexit(disableRawMode);
  enableRawMode();
  initEditor();
  while (1) {
    refreshScreen();
    processKeyPresses();
  }
  return 0;
}
