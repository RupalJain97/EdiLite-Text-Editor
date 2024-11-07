/** Libraries */
#include <iostream>  // For input/output in C++
#include <unistd.h>  // For read() and STDIN_FILENO
#include <stdlib.h>  // For atexit()
#include <termios.h> // Terminal I/O attributes
#include <errno.h>
#include <cstring>     // For strerror
#include <sys/ioctl.h> // To get terminal dimensions
#include <stdio.h>
#include <string>
#include <vector>

/** Data */
struct erow
{
    int size;
    char *chars;
};

struct editorConfig
{
    int cx, cy;
    int rowoff;
    int screenrows;
    int screencols;
    int numrows;
    erow *row;
    struct termios orig_termios;
};
struct editorConfig E;

/*** defines ***/
#define CTRL_KEY(k) ((k) & 0x1f)
#define KILO_VERSION "0.0.1"

enum editorKey
{
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

/** Error Handling */
// TODO

/** Terminal */

void die(const char *s)
{
    /* Clear the screen on exit */
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);

    perror(s);
    exit(1);
}

void disableRawMode()
{
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1)
        die("tcsetattr");
}

void enableRawMode()
{
    // tcgetattr(STDIN_FILENO, &orig_termios); // Get current terminal attributes
    if (tcgetattr(STDIN_FILENO, &E.orig_termios) == -1)
        die("tcgetattr");

    atexit(disableRawMode); // Ensure original settings are restored on exit

    struct termios raw = E.orig_termios; // Make a copy of the terminal settings

    raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);          // Disable echo, canonical mode, Ctrl-C/Z signals
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON); // Disable input flags
    raw.c_oflag &= ~(OPOST);                                  // Disable output processing
    raw.c_cflag |= (CS8);                                     // Set character size to 8 bits

    // raw.c_cc[VMIN] = 0;  // Set minimum number of bytes to read
    // raw.c_cc[VTIME] = 1; // Set timeout for reading

    // Apply new settings
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1)
        die("tcsetattr");
}

int editorReadKey()
{
    int nread;
    char c;
    while ((nread = read(STDIN_FILENO, &c, 1)) != 1)
    {
        if (nread == -1 && errno != EAGAIN)
            die("read");
    }
    if (c == '\x1b')
    {
        char seq[3];
        if (read(STDIN_FILENO, &seq[0], 1) != 1)
            return '\x1b';
        if (read(STDIN_FILENO, &seq[1], 1) != 1)
            return '\x1b';

        if (seq[0] == '[')
        {
            if (seq[1] >= '0' && seq[1] <= '9')
            {
                if (read(STDIN_FILENO, &seq[2], 1) != 1)
                    return '\x1b';
                if (seq[2] == '~')
                {
                    switch (seq[1])
                    {
                    case '1':
                        return HOME_KEY;
                    case '3':
                        return DEL_KEY;
                    case '4':
                        return END_KEY;
                    case '5':
                        return PAGE_UP;
                    case '6':
                        return PAGE_DOWN;
                    case '7':
                        return HOME_KEY;
                    case '8':
                        return END_KEY;
                    }
                }
            }
            else
            {
                switch (seq[1])
                {
                case 'A':
                    return ARROW_UP;
                case 'B':
                    return ARROW_DOWN;
                case 'C':
                    return ARROW_RIGHT;
                case 'D':
                    return ARROW_LEFT;
                case 'H':
                    return HOME_KEY;
                case 'F':
                    return END_KEY;
                }
            }
        }
        else if (seq[0] == 'O')
        {
            switch (seq[1])
            {
            case 'H':
                return HOME_KEY;
            case 'F':
                return END_KEY;
            }
        }

        return '\x1b';
    }
    else
    {
        return c;
    }
}

int getCursorPosition(int *rows, int *cols)
{
    char buf[32];
    unsigned int i = 0;

    if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4)
        return -1;

    // Read the response into the buffer
    while (i < sizeof(buf) - 1)
    {
        if (read(STDIN_FILENO, &buf[i], 1) != 1)
            break;
        if (buf[i] == 'R')
            break; // 'R' signifies the end of the response
        i++;
    }
    buf[i] = '\0'; // Null-terminate the buffer

    // Print the buffer contents (response from terminal)
    std::cout << "\r\nResponse: '" << &buf[1] << "'\r\n";

    // Check response format and parse
    if (buf[0] != '\x1b' || buf[1] != '[')
        return -1;
    if (sscanf(&buf[2], "%d;%d", rows, cols) != 2)
        return -1;

    editorReadKey();
    return -1;
}

int getWindowSize(int *rows, int *cols)
{
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0)
    {
        // Move the cursor to the bottom-right corner as a fallback
        if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12)
            return -1;

        return getCursorPosition(rows, cols);
    }
    else
    {
        *cols = ws.ws_col;
        *rows = ws.ws_row;
        return 0;
    }
}

/*** row operations ***/
// Append a new row to the editor's row array
void editorAppendRow(const char *s, size_t len)
{
    E.row = (erow *)realloc(E.row, sizeof(erow) * (E.numrows + 1));
    int at = E.numrows;

    E.row[at].size = len;
    E.row[at].chars = (char *)malloc(len + 1);
    memcpy(E.row[at].chars, s, len);
    E.row[at].chars[len] = '\0';
    E.numrows++;
}

/*** file i/o ***/
// Open a file and load its contents into the editor
void editorOpen(const char *filename)
{
    FILE *fp = fopen(filename, "r");
    if (!fp)
        die("fopen");

    char *line = nullptr;
    size_t linecap = 0;
    ssize_t linelen = 13;

    while ((linelen = getline(&line, &linecap, fp)) != -1)
    {
        while (linelen > 0 && (line[linelen - 1] == '\n' || line[linelen - 1] == '\r'))
            linelen--;
        editorAppendRow(line, linelen);
    }

    free(line);
    fclose(fp);
}

/*** Input ***/
void editorMoveCursor(int key)
{
    switch (key)
    {
    case ARROW_LEFT:
        if (E.cx != 0)
        {
            E.cx--;
        }
        break;
    case ARROW_RIGHT:
        if (E.cx != E.screencols - 1)
        {
            E.cx++;
        }
        break;
    case ARROW_UP:
        if (E.cy != 0)
        {
            E.cy--;
        }
        break;
    case ARROW_DOWN:
        if (E.cy < E.numrows)
        {
            E.cy++;
        }
        break;
    }
}

void editorProcessKeypress()
{
    int c = editorReadKey();

    switch (c)
    {
    case CTRL_KEY('q'):
        /* Clear the screen when user presses Ctrl-Q to quit */
        std::cout << "Exiting editor...\n";
        write(STDOUT_FILENO, "\x1b[2J", 4);
        write(STDOUT_FILENO, "\x1b[H", 3);
        exit(0);
        break;

    case HOME_KEY:
        E.cx = 0;
        break;
    case END_KEY:
        E.cx = E.screencols - 1;
        break;
    case PAGE_UP:
    case PAGE_DOWN:
    {
        int times = E.screenrows;
        while (times--)
            editorMoveCursor(c == PAGE_UP ? ARROW_UP : ARROW_DOWN);
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

/*** Output ***/
void editorScroll()
{
    if (E.cy < E.rowoff)
        E.rowoff = E.cy;
    if (E.cy >= E.rowoff + E.screenrows)
        E.rowoff = E.cy - E.screenrows + 1;
}

void editorDrawRows(std::string &ab)
{
    for (int y = 0; y < E.screenrows; y++)
    {
        int filerow = y + E.rowoff;
        if (filerow >= E.numrows)
        {
            if (E.numrows == 0 && y == E.screenrows / 3)
            {
                // Display the welcome message a third of the way down
                char welcome[80];
                int welcomelen = snprintf(welcome, sizeof(welcome), "Kilo editor -- version %s", KILO_VERSION);
                if (welcomelen > E.screencols)
                    welcomelen = E.screencols;

                int padding = (E.screencols - welcomelen) / 2;
                if (padding)
                {
                    ab.append("~");
                    padding--;
                }
                while (padding--)
                    ab.append(" ");
                ab.append(welcome, welcomelen);
            }
            else
            {
                ab.append("~");
            }
        }
        else
        {
            int len = E.row[filerow].size;
            if (len > E.screencols)
                len = E.screencols;
            ab.append(E.row[filerow].chars, len);
        }
        ab.append("\x1b[K"); // clear lines as we draw
        if (y < E.screenrows - 1)
        {
            // write(STDOUT_FILENO, "\r\n", 2);
            ab.append("\r\n");
        }
    }
}

void editorRefreshScreen()
{
    editorScroll();
    /*
    write() and STDOUT_FILENO come from <unistd.h>.

    The 4 in our write() call means we are writing 4 bytes out to the terminal. The first byte is \x1b, which is the escape character, or 27 in decimal. (Try and remember \x1b, we will be using it a lot.) The other three bytes are [2J.
    */
    // write(STDOUT_FILENO, "\x1b[2J", 4);

    /*
    reposition the cursor which is at the top-left corner so that we’re ready to draw the editor interface from top to bottom.

    if you have an 80×24 size terminal and you want the cursor in the center of the screen, you could use the command <esc>[12;40H. (Multiple arguments are separated by a ; character.) The default arguments for H both happen to be 1.
    */
    // write(STDOUT_FILENO, "\x1b[H", 3);

    std::string ab;

    ab.append("\x1b[?25l"); // Hide the cursor

    // Append escape sequences to clear the screen and move cursor to top-left
    // ab.append("\x1b[2J");  // Clear the screen
    ab.append("\x1b[H"); // Move cursor to the top-left corner

    editorDrawRows(ab);

    // write(STDOUT_FILENO, "\x1b[H", 3);
    // Move the cursor back to the top-left corner
    ab.append("\x1b[H");

    char buf[32];
    int welcomelen = snprintf(buf, sizeof(buf), "\x1b[%d;%dH", (E.cy - E.rowoff) + 1, E.cx + 1);
    ab.append(buf);

    ab.append("\x1b[?25h"); // Hide the cursor

    // Write the buffer contents to standard output
    write(STDOUT_FILENO, ab.c_str(), ab.size());
}

/*** Init ***/
//  initialize all the fields in the E struct.
void initEditor()
{
    E.cx = 0;
    E.cy = 0;
    E.rowoff = 0;
    E.numrows = 0;
    E.row = nullptr;
    if (getWindowSize(&E.screenrows, &E.screencols) == -1)
        die("getWindowSize");
}

int main(int argc, char *argv[])
{
    std::cout << "Welcome to the text Editor\n";
    // std::cout << "This is the raw mode.\n";
    // std::cout << "Raw mode is a terminal setting that allows the program to read input directly from the user without buffering or processing (like echoing characters or interpreting special keys). This lets the editor respond immediately to each keypress for an interactive editing experience.\n";
    // std::cout << "Enter 'q' to exit.\n";

    enableRawMode();
    initEditor();

    if (argc >= 2)
    {
        editorOpen(argv[1]);
    }

    while (1)
    {
        editorRefreshScreen();
        editorProcessKeypress();
    }
    return 0;
}
