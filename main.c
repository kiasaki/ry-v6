#define _DEFAULT_SOURCE
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"

enum KEY_ACTION {
  KEY_NULL = 0,
  CTRL_C = 3,
  CTRL_D = 4,
  CTRL_F = 6,
  CTRL_H = 8,
  TAB = 9,
  CTRL_L = 12,
  ENTER = 13,
  CTRL_Q = 17,
  CTRL_S = 19,
  CTRL_U = 21,
  ESC = 27,
  BACKSPACE =  127,
  ARROW_LEFT = 1000,
  ARROW_RIGHT,
  ARROW_UP,
  ARROW_DOWN,
  DEL_KEY,
  HOME_KEY,
  END_KEY,
  PAGE_UP,
  PAGE_DOWN
};


static int rawmode = 0;
static struct termios orig_termios;

void disable_raw_mode() {
  if (rawmode) {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
    rawmode = 0;
  }
}

int enable_raw_mode() {
  struct termios raw;
  if (rawmode) return 0;
  if (!isatty(STDIN_FILENO)) goto fatal;
  atexit(disable_raw_mode);
  if (tcgetattr(STDIN_FILENO, &orig_termios) == -1) goto fatal;
  raw = orig_termios;
  // input modes: no break, no CR to NL, no parity check, no strip char...
  raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
  raw.c_oflag &= ~(OPOST); // output: no post processing
  raw.c_cflag |= (CS8); // control modes: set 8 bit chars
  // local modes: no echo, no canonical, no extended functions, no signal char
  raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
  // control chars: set return condition: min number of bytes and timer
  raw.c_cc[VMIN] = 0; // Return each byte, or zero for timeout
  raw.c_cc[VTIME] = 1; // 100ms timeout
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) < 0) goto fatal;
  rawmode = 1;
  return 0;
fatal:
  errno = ENOTTY;
  return -1;
}

int cursor_position(int ifd, int ofd, int *rows, int *cols) {
  char buf[32];
  unsigned int i = 0;
  if (write(ofd, "\x1b[6n", 4) != 4) return -1;
  while (i < sizeof(buf)-1) {
    if (read(ifd,buf+i,1) != 1) break;
    if (buf[i] == 'R') break;
    i++;
  }
  buf[i] = '\0';
  if (buf[0] != ESC || buf[1] != '[') return -1;
  if (sscanf(buf+2, "%d;%d", rows,cols) != 2) return -1;
  return 0;
}

int screen_size(int *rows, int *cols) {
  int ifd = STDIN_FILENO;
  int ofd = STDOUT_FILENO;
  struct winsize ws;

  if (ioctl(1, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
    /* ioctl() failed. Try to query the terminal itself. */
    int orig_row, orig_col, retval;

    /* Get the initial position so we can restore it later. */
    retval = cursor_position(ifd,ofd,&orig_row,&orig_col);
    if (retval == -1) goto failed;

    /* Go to right/bottom margin and get position. */
    if (write(ofd,"\x1b[999C\x1b[999B",12) != 12) goto failed;
    retval = cursor_position(ifd,ofd,rows,cols);
    if (retval == -1) goto failed;

    /* Restore position. */
    char seq[32];
    snprintf(seq,32,"\x1b[%d;%dH",orig_row,orig_col);
    if (write(ofd,seq,strlen(seq)) == -1) {
        /* Can't recover... */
    }
    return 0;
  } else {
    *cols = ws.ws_col;
    *rows = ws.ws_row;
    return 0;
  }

failed:
  return -1;
}

int read_key() {
  int fd = STDIN_FILENO;
  int nread;
  char c, seq[3];
  while ((nread = read(fd, &c, 1)) == 0);
  if (nread == -1) exit(1);

  while(1) {
    switch(c) {
    case ESC:
      /* If this is just an ESC, we'll timeout here. */
      if (read(fd, seq, 1) == 0) return ESC;
      if (read(fd, seq+1, 1) == 0) return ESC;

      /* ESC [ sequences. */
      if (seq[0] == '[') {
        if (seq[1] >= '0' && seq[1] <= '9') {
          /* Extended escape, read additional byte. */
          if (read(fd, seq+2, 1) == 0) return ESC;
          if (seq[2] == '~') {
            switch(seq[1]) {
            case '3': return DEL_KEY;
            case '5': return PAGE_UP;
            case '6': return PAGE_DOWN;
            }
          }
        } else {
          switch(seq[1]) {
          case 'A': return ARROW_UP;
          case 'B': return ARROW_DOWN;
          case 'C': return ARROW_RIGHT;
          case 'D': return ARROW_LEFT;
          case 'H': return HOME_KEY;
          case 'F': return END_KEY;
          }
        }
      }

      /* ESC O sequences. */
      else if (seq[0] == 'O') {
        switch(seq[1]) {
        case 'H': return HOME_KEY;
        case 'F': return END_KEY;
        }
      }
      break;
    default:
      return c;
    }
  }
}

static int lscreen_size(lua_State *L) {
  int w, h;
  if (screen_size(&h, &w) == -1) {
    lua_pushliteral(L, "screen_size: failed to fetch screen size");
    lua_error(L);
  }
  lua_pushnumber(L, (lua_Number)w);
  lua_pushnumber(L, (lua_Number)h);
  return 2;
}

static int lnext_keypress(lua_State *L) {
  int c = read_key();
  switch(c) {
  case ESC:
    lua_pushliteral(L, "esc");
    return 1;
  case DEL_KEY:
    lua_pushliteral(L, "del");
    return 1;
  case ENTER:
    lua_pushliteral(L, "enter");
    return 1;
  case BACKSPACE:
    lua_pushliteral(L, "backspace");
    return 1;
  case TAB:
    lua_pushliteral(L, "tab");
    return 1;
  case PAGE_UP:
    lua_pushliteral(L, "pageup");
    return 1;
  case PAGE_DOWN:
    lua_pushliteral(L, "pagedown");
    return 1;
  case ARROW_UP:
    lua_pushliteral(L, "up");
    return 1;
  case ARROW_DOWN:
    lua_pushliteral(L, "down");
    return 1;
  case ARROW_LEFT:
    lua_pushliteral(L, "left");
    return 1;
  case ARROW_RIGHT:
    lua_pushliteral(L, "right");
    return 1;
  case CTRL_C:
    exit(1);
  case CTRL_D:
    lua_pushliteral(L, "ctrl-d");
    return 1;
  case CTRL_F:
    lua_pushliteral(L, "ctrl-f");
    return 1;
  case CTRL_H:
    lua_pushliteral(L, "ctrl-h");
    return 1;
  case CTRL_Q:
    lua_pushliteral(L, "ctrl-q");
    return 1;
  case CTRL_S:
    lua_pushliteral(L, "ctrl-s");
    return 1;
  case CTRL_U:
    lua_pushliteral(L, "ctrl-u");
    return 1;
  default: {
    char s[2] = "\0\0";
    s[0] = c;
    lua_pushstring(L, s);
    return 1;
  }
  }
  lua_pushliteral(L, "next_keypress: unreachable");
  lua_error(L);
}

int main(int argc, char **argv) {
  if (enable_raw_mode() == -1) {
    fprintf(stderr, "ry: error: not a tty\n");
    return 1;
  }
  lua_State *L = luaL_newstate();
  luaL_openlibs(L);

  lua_register(L, "screen_size", lscreen_size);
  lua_register(L, "next_keypress", lnext_keypress);

  int r = luaL_dofile(L, "prelude.lua");
  if (r != 0) {
    disable_raw_mode();
    const char *err = lua_tostring(L, -1);
    fprintf(stderr, "ry: error: %s\n", err);
  }
  luaL_dostring(L, "args = {}");
  for (char **p = argv; *p != argv[argc]; p++) {
    char s[1024];
    snprintf(&s[0], 1024-1, "table.insert(args, \"%s\")", *p);
    luaL_dostring(L, s);
  }
  r = luaL_dostring(L, "run()");
  if (r != 0) {
    disable_raw_mode();
    const char *err = lua_tostring(L, -1);
    fprintf(stderr, "ry: error: %s\n", err);
  }
  return 0;
}

/*
void render(void) {
    int y;
    erow *r;
    char buf[32];
    struct abuf ab = ABUF_INIT;

    abAppend(&ab,"\x1b[?25l",6); // Hide cursor
    abAppend(&ab,"\x1b[H",3); // Go home
    for (y = 0; y < E.screenrows; y++) {
        int filerow = E.rowoff+y;

        if (filerow >= E.numrows) {
            if (E.numrows == 0 && y == E.screenrows/3) {
                char welcome[80];
                int welcomelen = snprintf(welcome,sizeof(welcome),
                    "Kilo editor -- verison %s\x1b[0K\r\n", KILO_VERSION);
                int padding = (E.screencols-welcomelen)/2;
                if (padding) {
                    abAppend(&ab,"~",1);
                    padding--;
                }
                while(padding--) abAppend(&ab," ",1);
                abAppend(&ab,welcome,welcomelen);
            } else {
                abAppend(&ab,"~\x1b[0K\r\n",7);
            }
            continue;
        }

        r = &E.row[filerow];

        int len = r->rsize - E.coloff;
        int current_color = -1;
        if (len > 0) {
            if (len > E.screencols) len = E.screencols;
            char *c = r->render+E.coloff;
            unsigned char *hl = r->hl+E.coloff;
            int j;
            for (j = 0; j < len; j++) {
                if (hl[j] == HL_NONPRINT) {
                    char sym;
                    abAppend(&ab,"\x1b[7m",4);
                    if (c[j] <= 26)
                        sym = '@'+c[j];
                    else
                        sym = '?';
                    abAppend(&ab,&sym,1);
                    abAppend(&ab,"\x1b[0m",4);
                } else if (hl[j] == HL_NORMAL) {
                    if (current_color != -1) {
                        abAppend(&ab,"\x1b[39m",5);
                        current_color = -1;
                    }
                    abAppend(&ab,c+j,1);
                } else {
                    int color = editorSyntaxToColor(hl[j]);
                    if (color != current_color) {
                        char buf[16];
                        int clen = snprintf(buf,sizeof(buf),"\x1b[%dm",color);
                        current_color = color;
                        abAppend(&ab,buf,clen);
                    }
                    abAppend(&ab,c+j,1);
                }
            }
        }
        abAppend(&ab,"\x1b[39m",5);
        abAppend(&ab,"\x1b[0K",4);
        abAppend(&ab,"\r\n",2);
    }

    // First status row
    abAppend(&ab,"\x1b[0K",4);
    abAppend(&ab,"\x1b[7m",4);
    char status[80], rstatus[80];
    int len = snprintf(status, sizeof(status), "%.20s - %d lines %s",
        E.filename, E.numrows, E.dirty ? "(modified)" : "");
    int rlen = snprintf(rstatus, sizeof(rstatus),
        "%d/%d",E.rowoff+E.cy+1,E.numrows);
    if (len > E.screencols) len = E.screencols;
    abAppend(&ab,status,len);
    while(len < E.screencols) {
        if (E.screencols - len == rlen) {
            abAppend(&ab,rstatus,rlen);
            break;
        } else {
            abAppend(&ab," ",1);
            len++;
        }
    }
    abAppend(&ab,"\x1b[0m\r\n",6);

    // Second status row
    abAppend(&ab,"\x1b[0K",4);
    int msglen = strlen(E.statusmsg);
    if (msglen && time(NULL)-E.statusmsg_time < 5)
        abAppend(&ab,E.statusmsg,msglen <= E.screencols ? msglen : E.screencols);

    // Place cursor
    int j;
    int cx = 1;
    int filerow = E.rowoff+E.cy;
    erow *row = (filerow >= E.numrows) ? NULL : &E.row[filerow];
    if (row) {
        for (j = E.coloff; j < (E.cx+E.coloff); j++) {
            if (j < row->size && row->chars[j] == TAB) cx += 7-((cx)%8);
            cx++;
        }
    }
    snprintf(buf,sizeof(buf),"\x1b[%d;%dH",E.cy+1,cx);
    abAppend(&ab,buf,strlen(buf));
    abAppend(&ab,"\x1b[?25h",6); // Show cursor
    write(STDOUT_FILENO,ab.b,ab.len);
    abFree(&ab);
}
*/
