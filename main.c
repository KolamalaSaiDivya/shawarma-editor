#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

enum editorKey {
    ARROW_LEFT = 1000,
    ARROW_RIGHT,
    ARROW_UP,
    ARROW_DOWN
};

typedef struct erow {
    int size;
    char *chars;
} erow;

struct editorConfig {
    int cx;
    int cy;

    int screenrows;
    int screencols;

    int numrows;
    erow *row;
};

struct editorConfig E;

struct termios original_termios;

void disableRawMode() {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &original_termios);
}

void enableRawMode() {
    if (tcgetattr(STDIN_FILENO, &original_termios) == -1) {
        perror("tcgetattr");
        exit(1);
    }

    atexit(disableRawMode);

    struct termios raw = original_termios;

    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);

    raw.c_oflag &= ~(OPOST);

    raw.c_cflag |= (CS8);

    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);

    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) {
        perror("tcsetattr");
        exit(1);
    }
}

int editorReadKey() {
    int nread;
    char c;

    while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
        if (nread == -1 && errno != EAGAIN) {
            perror("read");
            exit(1);
        }
    }

    if (c == '\x1b') {
        char seq[3];

        if (read(STDIN_FILENO, &seq[0], 1) != 1) return '\x1b';
        if (read(STDIN_FILENO, &seq[1], 1) != 1) return '\x1b';

        if (seq[0] == '[') {
            switch (seq[1]) {
                case 'A': return ARROW_UP;
                case 'B': return ARROW_DOWN;
                case 'C': return ARROW_RIGHT;
                case 'D': return ARROW_LEFT;
            }
        }

        return '\x1b';
    }

    return c;
}

void editorAppendRow(char *s, size_t len) {
    E.row = realloc(E.row, sizeof(erow) * (E.numrows + 1));

    int at = E.numrows;

    E.row[at].size = len;

    E.row[at].chars = malloc(len + 1);

    memcpy(E.row[at].chars, s, len);

    E.row[at].chars[len] = '\0';

    E.numrows++;
}

void editorRowInsertChar(erow *row, int at, int c) {
    if (at < 0 || at > row->size) {
        at = row->size;
    }

    row->chars = realloc(row->chars, row->size + 2);

    memmove(&row->chars[at + 1],
            &row->chars[at],
            row->size - at + 1);

    row->size++;

    row->chars[at] = c;
}

void editorInsertChar(int c) {
    if (E.cy == E.numrows) {
        editorAppendRow("", 0);
    }

    editorRowInsertChar(&E.row[E.cy], E.cx, c);

    E.cx++;
}

void editorDrawRows() {
    for (int y = 0; y < E.screenrows; y++) {

        if (y >= E.numrows) {

            write(STDOUT_FILENO, "~", 1);

        } else {

            write(STDOUT_FILENO,
                  E.row[y].chars,
                  E.row[y].size);
        }

        write(STDOUT_FILENO, "\x1b[K", 3);

        write(STDOUT_FILENO, "\r\n", 2);
    }
}

void editorRefreshScreen() {
    write(STDOUT_FILENO, "\x1b[2J", 4);

    write(STDOUT_FILENO, "\x1b[H", 3);

    editorDrawRows();

    char buf[32];

    snprintf(buf, sizeof(buf), "\x1b[%d;%dH", E.cy + 1, E.cx + 1);

    write(STDOUT_FILENO, buf, strlen(buf));
}

void editorMoveCursor(int key) {
    switch (key) {
        case ARROW_LEFT:
            if (E.cx > 0) {
                E.cx--;
            }
            break;

        case ARROW_RIGHT:
            if (E.cx < E.screencols - 1) {
                E.cx++;
            }
            break;

        case ARROW_UP:
            if (E.cy > 0) {
                E.cy--;
            }
            break;

        case ARROW_DOWN:
            if (E.cy < E.screenrows - 1) {
                E.cy++;
            }
            break;
    }
}

int main() {
    enableRawMode();

    E.cx = 0;
    E.cy = 0;

    E.screenrows = 24;
    E.screencols = 80;

    E.numrows = 0;
    E.row = NULL;

    while (1) {
        editorRefreshScreen();

        int c = editorReadKey();

        if (c == 17) {
            break;
        }

        switch (c) {
            case ARROW_UP:
            case ARROW_DOWN:
            case ARROW_LEFT:
            case ARROW_RIGHT:
                editorMoveCursor(c);
                break;

            default:
                editorInsertChar(c);
                break;
        }
    }

    return 0;
}