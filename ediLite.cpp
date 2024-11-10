/** Libraries */
#include <iostream>    // For input/output in C++
#include <unistd.h>    // For read() and STDIN_FILENO
#include <stdlib.h>    // For atexit() and memory management
#include <termios.h>   // For terminal I/O attributes
#include <errno.h>     // For error handling
#include <cstring>     // For strerror
#include <sys/ioctl.h> // To get terminal dimensions
#include <stdio.h>     // For standard I/O functions
#include <string>      // For string handling
#include <vector>      // For vector data structure
#include <time.h>      // For time handling
#include <cstdarg>     // For variadic arguments
#include <fstream>     // For file handling

/*** defines ***/
#define CTRL_KEY(k) ((k) & 0x1f)
#define EDILITE_VERSION "0.0.1"
#define EDILITE_TAB_STOP 8
#define EDILITE_QUIT_TIMES 3

#define HL_HIGHLIGHT_NUMBERS (1 << 0)
#define HLDB_ENTRIES (sizeof(HLDB) / sizeof(HLDB[0]))
#define HL_HIGHLIGHT_STRINGS (1 << 1)

/** Data */
struct erow
{
    int idx;             // Row index in the file
    int size;            // Size of the row
    int rsize;           // Rendered size (tabs expanded)
    char *chars;         // Actual characters in the row
    char *render;        // Rendered row with tabs converted to spaces
    unsigned char *hl;   // Highlight attributes for each character
    int hl_open_comment; // Indicates if the row has an open comment
};

struct editorConfig
{
    int cx, cy;                  // Cursor position in chars
    int rx;                      // Rendered x position
    int rowoff;                  // Offset for row scrolling
    int coloff;                  // Offset for column scrolling
    int screenrows;              // Number of rows on the screen
    int screencols;              // Number of columns on the screen
    int numrows;                 // Number of rows in the file
    int dirty;                   // Indicates if file has unsaved changes
    erow *row;                   // Rows of text in the editor
    char *filename;              // Opened filename
    char statusmsg[80];          // Status message displayed to the user
    time_t statusmsg_time;       // Timestamp of the last status message
    struct editorSyntax *syntax; // Syntax highlighting for the file type
    struct termios orig_termios; // Original terminal attributes
};
struct editorConfig E;

enum editorKey
{
    BACKSPACE = 127,
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

enum editorHighlight
{
    HL_NORMAL = 0,
    HL_COMMENT,
    HL_MLCOMMENT,
    HL_KEYWORD1,
    HL_KEYWORD2,
    HL_STRING,
    HL_NUMBER,
    HL_MATCH
};

struct editorSyntax
{
    char *filetype;                 // Name of the file type
    char **filematch;               // Array of filename extensions
    char **keywords;                // Array of keywords for syntax highlighting
    char *singleline_comment_start; // Single-line comment start pattern
    char *multiline_comment_start;  // Multi-line comment start pattern
    char *multiline_comment_end;    // Multi-line comment end pattern
    int flags;                      // Flags for syntax highlighting
};

char *C_HL_extensions[] = {".c", ".h", ".cpp", NULL};
char *C_HL_keywords[] = {
    "switch", "if", "while", "for", "break", "continue", "return", "else",
    "struct", "union", "typedef", "static", "enum", "class", "case",
    "int|", "long|", "double|", "float|", "char|", "unsigned|", "signed|",
    "void|", NULL};
struct editorSyntax HLDB[] = {
    {"c",
     C_HL_extensions,
     C_HL_keywords,
     "//", "/*", "*/",
     HL_HIGHLIGHT_NUMBERS | HL_HIGHLIGHT_STRINGS},
};

/*** prototypes ***/
void editorSetStatusMessage(const char *fmt, ...);
void editorRefreshScreen();
std::string editorPrompt(const std::string &prompt, void (*callback)(const std::string &, int));

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

/*** syntax highlighting ***/
int editorSyntaxToColor(int hl)
{
    switch (hl)
    {
    case HL_COMMENT:
    case HL_MLCOMMENT:
        return 36; // Cyan
    case HL_KEYWORD1:
        return 33; // Yellow
    case HL_KEYWORD2:
        return 32; // Green
    case HL_STRING:
        return 35; // Magenta
    case HL_NUMBER:
        return 31; // Red
    case HL_MATCH:
        return 34; // Blue
    default:
        return 37; // White
    }
}

int is_separator(int c)
{
    return isspace(c) || c == '\0' || strchr(",.()+-/*=~%<>[];", c) != NULL;
}

void editorUpdateSyntax(erow *row)
{
    // Resize hl array to match the row's render size
    row->hl = (unsigned char *)realloc(row->hl, row->rsize);
    memset(row->hl, HL_NORMAL, row->rsize); // Set all to normal initially

    if (E.syntax == NULL)
        return;

    char **keywords = E.syntax->keywords;

    char *scs = E.syntax->singleline_comment_start;
    char *mcs = E.syntax->multiline_comment_start;
    char *mce = E.syntax->multiline_comment_end;

    int scs_len = scs ? strlen(scs) : 0;
    int mcs_len = mcs ? strlen(mcs) : 0;
    int mce_len = mce ? strlen(mce) : 0;

    int prev_sep = 1;
    int in_string = 0;
    int i = 0;
    int in_comment = (row->idx > 0 && E.row[row->idx - 1].hl_open_comment);

    while (i < row->rsize)
    {
        char c = row->render[i];
        unsigned char prev_hl = (i > 0) ? row->hl[i - 1] : HL_NORMAL;

        if (scs_len && !in_string && !in_comment)
        {
            if (!strncmp(&row->render[i], scs, scs_len))
            {
                memset(&row->hl[i], HL_COMMENT, row->rsize - i);
                break;
            }
        }

        if (mcs_len && mce_len && !in_string)
        {
            if (in_comment)
            {
                row->hl[i] = HL_MLCOMMENT;
                if (!strncmp(&row->render[i], mce, mce_len))
                {
                    memset(&row->hl[i], HL_MLCOMMENT, mce_len);
                    i += mce_len;
                    in_comment = 0;
                    prev_sep = 1;
                    continue;
                }
                else
                {
                    i++;
                    continue;
                }
            }
            else if (!strncmp(&row->render[i], mcs, mcs_len))
            {
                memset(&row->hl[i], HL_MLCOMMENT, mcs_len);
                i += mcs_len;
                in_comment = 1;
                continue;
            }
        }

        if (E.syntax->flags & HL_HIGHLIGHT_STRINGS)
        {
            if (in_string)
            {
                row->hl[i] = HL_STRING;
                if (c == '\\' && i + 1 < row->rsize)
                {
                    row->hl[i + 1] = HL_STRING;
                    i += 2;
                    continue;
                }
                if (c == in_string)
                    in_string = 0;
                i++;
                prev_sep = 1;
                continue;
            }
            else
            {
                if (c == '"' || c == '\'')
                {
                    in_string = c;
                    row->hl[i] = HL_STRING;
                    i++;
                    continue;
                }
            }
        }
        if (E.syntax->flags & HL_HIGHLIGHT_NUMBERS)
        {
            if ((isdigit(c) && (prev_sep || prev_hl == HL_NUMBER)) ||
                (c == '.' && prev_hl == HL_NUMBER))
            {
                row->hl[i] = HL_NUMBER;
                i++;
                prev_sep = 0;
                continue;
            }
        }

        if (prev_sep)
        {
            int j;
            for (j = 0; keywords[j]; j++)
            {
                int klen = strlen(keywords[j]);
                int kw2 = keywords[j][klen - 1] == '|';
                if (kw2)
                    klen--;
                if (!strncmp(&row->render[i], keywords[j], klen) &&
                    is_separator(row->render[i + klen]))
                {
                    memset(&row->hl[i], kw2 ? HL_KEYWORD2 : HL_KEYWORD1, klen);
                    i += klen;
                    break;
                }
            }
            if (keywords[j] != NULL)
            {
                prev_sep = 0;
                continue;
            }
        }

        // Update `prev_sep` for the next iteration
        prev_sep = is_separator(c);
        i++;
    }

    int changed = (row->hl_open_comment != in_comment);
    row->hl_open_comment = in_comment;
    if (changed && row->idx + 1 < E.numrows)
        editorUpdateSyntax(&E.row[row->idx + 1]);
}

void editorSelectSyntaxHighlight()
{
    E.syntax = NULL;
    if (E.filename == NULL)
        return;
    char *ext = strrchr(E.filename, '.');
    for (unsigned int j = 0; j < HLDB_ENTRIES; j++)
    {
        struct editorSyntax *s = &HLDB[j];
        unsigned int i = 0;
        while (s->filematch[i])
        {
            int is_ext = (s->filematch[i][0] == '.');
            if ((is_ext && ext && !strcmp(ext, s->filematch[i])) ||
                (!is_ext && strstr(E.filename, s->filematch[i])))
            {
                E.syntax = s;
                int filerow;
                for (filerow = 0; filerow < E.numrows; filerow++)
                {
                    editorUpdateSyntax(&E.row[filerow]);
                }
                return;
            }
            i++;
        }
    }
}

/*** row operations ***/
int editorRowCxToRx(erow *row, int cx)
{
    int rx = 0;
    int j;
    for (j = 0; j < cx; j++)
    {
        if (row->chars[j] == '\t')
            rx += (EDILITE_TAB_STOP - 1) - (rx % EDILITE_TAB_STOP);
        rx++;
    }
    return rx;
}

int editorRowRxToCx(erow *row, int rx)
{
    int cur_rx = 0;
    int cx;
    for (cx = 0; cx < row->size; cx++)
    {
        if (row->chars[cx] == '\t')
            cur_rx += (EDILITE_TAB_STOP - 1) - (cur_rx % EDILITE_TAB_STOP);
        cur_rx++;
        if (cur_rx > rx)
            return cx;
    }
    return cx;
}

void editorUpdateRow(erow *row)
{
    free(row->render);

    // For each tab, we may need up to 8 spaces, so allocate accordingly
    int tabs = 0;
    for (int j = 0; j < row->size; j++)
    {
        if (row->chars[j] == '\t')
            tabs++;
    }

    row->render = (char *)malloc(row->size + tabs * (EDILITE_TAB_STOP - 1) + 1);
    int idx = 0;

    for (int j = 0; j < row->size; j++)
    {
        if (row->chars[j] == '\t')
        {
            row->render[idx++] = ' ';
            while (idx % (EDILITE_TAB_STOP) != 0)
                row->render[idx++] = ' ';
        }
        else
        {
            row->render[idx++] = row->chars[j];
        }
    }

    row->render[idx] = '\0';
    row->rsize = idx;

    editorUpdateSyntax(row);
}

void editorRowInsertChar(erow *row, int at, char c)
{
    // Ensure 'at' is within bounds
    if (at < 0 || at > row->size)
        at = row->size;

    row->chars = (char *)realloc(row->chars, row->size + 2); // Adjust size for new char + null byte
    memmove(&row->chars[at + 1], &row->chars[at], row->size - at);
    row->chars[at] = c;
    row->size++;

    // Update render vector and rsize accordingly
    editorUpdateRow(row);
    E.dirty++;
}

// Append a new row to the editor's row array
void editorInsertRow(int at, const char *s, size_t len)
{
    if (at < 0 || at > E.numrows)
        return;

    E.row = (erow *)realloc(E.row, sizeof(erow) * (E.numrows + 1));
    std::memmove(&E.row[at + 1], &E.row[at], sizeof(erow) * (E.numrows - at));

    for (int j = at + 1; j <= E.numrows; j++)
        E.row[j].idx++;

    E.row[at].idx = at;

    E.row[at].size = len;
    E.row[at].chars = (char *)malloc(len + 1);
    memcpy(E.row[at].chars, s, len);
    E.row[at].chars[len] = '\0';

    E.row[at].rsize = 0;
    E.row[at].render = nullptr;
    E.row[at].hl = nullptr;
    E.row[at].hl_open_comment = 0;

    editorUpdateRow(&E.row[at]);
    E.numrows++;
    E.dirty++;
}

void editorRowDelChar(erow *row, int at)
{
    if (at < 0 || at >= row->size)
        return;

    std::memmove(&row->chars[at], &row->chars[at + 1], row->size - at);
    row->size--;
    editorUpdateRow(row);
    E.dirty++;
}

void editorFreeRow(erow *row)
{
    free(row->render);
    free(row->chars);
    free(row->hl);
}

void editorDelRow(int at)
{
    if (at < 0 || at >= E.numrows)
        return;

    editorFreeRow(&E.row[at]);
    std::memmove(&E.row[at], &E.row[at + 1], sizeof(erow) * (E.numrows - at - 1));
    for (int j = at; j < E.numrows - 1; j++)
        E.row[j].idx--;

    E.numrows--;
    E.dirty++;
}

void editorRowAppendString(erow *row, const char *s, size_t len)
{
    row->chars = (char *)realloc(row->chars, row->size + len + 1);
    std::memcpy(&row->chars[row->size], s, len);
    row->size += len;
    row->chars[row->size] = '\0';
    editorUpdateRow(row);
    E.dirty++;
}

/*** editor operations ***/
void editorInsertChar(int c)
{
    if (E.cy == E.numrows)
    {
        editorInsertRow(E.numrows, "", 0); // Append a new empty row if needed
    }
    editorRowInsertChar(&E.row[E.cy], E.cx, c);
    E.cx++;
}

void editorDelChar()
{
    if (E.cy == E.numrows)
        return;

    if (E.cx == 0 && E.cy == 0)
        return;
    erow *row = &E.row[E.cy];
    if (E.cx > 0)
    {
        editorRowDelChar(row, E.cx - 1);
        E.cx--;
    }
    else
    {
        E.cx = E.row[E.cy - 1].size;
        editorRowAppendString(&E.row[E.cy - 1], row->chars, row->size);
        editorDelRow(E.cy);
        E.cy--;
    }
}

void editorInsertNewline()
{
    if (E.cy == E.numrows)
    {
        editorInsertRow(E.numrows, "", 0); // Insert an empty row if at the end of file
    }
    else if (E.cx == 0)
    {
        editorInsertRow(E.cy, "", 0);
    }
    else
    {
        erow *row = &E.row[E.cy];
        editorInsertRow(E.cy + 1, &row->chars[E.cx], row->size - E.cx);

        row = &E.row[E.cy];
        row->size = E.cx;
        row->chars[row->size] = '\0';
        editorUpdateRow(row);
    }

    // Move cursor to the beginning of the new row
    E.cy++;
    E.cx = 0;
    E.dirty++;
}

/*** file i/o ***/
std::string editorRowsToString(int &buflen)
{
    buflen = 0;
    for (int j = 0; j < E.numrows; j++)
    {
        buflen += E.row[j].size + 1; // +1 for newline character
    }

    std::string buffer;
    buffer.reserve(buflen); // Reserve exact space to avoid reallocations

    for (int j = 0; j < E.numrows; j++)
    {
        buffer.append(E.row[j].chars);
        buffer.append("\n"); // Append newline
    }
    return buffer;
}

void editorSave()
{
    if (E.filename == nullptr)
    {
        E.filename = strdup(editorPrompt("Save as: %s (ESC to cancel)", NULL).c_str());
        if (E.filename == nullptr || E.filename[0] == '\0')
        { // Check for cancel
            editorSetStatusMessage("Save aborted");
            return;
        }
        editorSelectSyntaxHighlight();
    }

    int len;
    std::string buffer = editorRowsToString(len);

    std::ofstream file(E.filename, std::ios::out | std::ios::trunc);
    if (file)
    {
        file.write(buffer.c_str(), len);
        E.dirty = 0;
        editorSetStatusMessage("%d bytes written to disk... File saved successfully", len);
    }
    else
    {
        editorSetStatusMessage("Can't save! I/O error");
    }
}

/*** find ***/
void editorFindCallback(const std::string &query, int key)
{
    static int last_match = -1;
    static int direction = 1;
    static int saved_hl_line = -1;
    static unsigned char *saved_hl = nullptr;

    // Restore previous hl if needed
    if (saved_hl)
    {
        memcpy(E.row[saved_hl_line].hl, saved_hl, E.row[saved_hl_line].rsize);
        free(saved_hl);
        saved_hl = nullptr;
        // saved_hl_line = -1;
    }

    if (key == '\r' || key == '\x1b')
    {
        last_match = -1;
        direction = 1;
        return;
    }
    else if (key == ARROW_RIGHT || key == ARROW_DOWN)
    {
        direction = 1;
    }
    else if (key == ARROW_LEFT || key == ARROW_UP)
    {
        direction = -1;
    }
    else
    {
        last_match = -1;
        direction = 1;
    }

    if (last_match == -1)
        direction = 1;
    int current = last_match;
    for (int i = 0; i < E.numrows; i++)
    {
        current += direction;
        if (current == -1)
            current = E.numrows - 1;
        else if (current == E.numrows)
            current = 0;

        erow *row = &E.row[current];
        const char *match = strstr(row->render, query.c_str());
        if (match)
        {
            last_match = current;
            E.cy = current;
            E.cx = editorRowRxToCx(row, match - row->render);
            E.rowoff = E.numrows;

            // Save current hl state for restoration later
            saved_hl_line = current;
            saved_hl = (unsigned char *)malloc(row->rsize);
            memcpy(saved_hl, row->hl, row->rsize);

            // Highlight the search match in blue
            memset(&row->hl[match - row->render], HL_MATCH, query.length());
            break;
        }
    }
}

void editorFind()
{
    int saved_cx = E.cx;
    int saved_cy = E.cy;
    int saved_coloff = E.coloff;
    int saved_rowoff = E.rowoff;

    std::string query = editorPrompt("Search: %s (Use ESC/Arrows/Enter)", editorFindCallback);
    if (query.empty())
        return;
    else
    {
        E.cx = saved_cx;
        E.cy = saved_cy;
        E.coloff = saved_coloff;
        E.rowoff = saved_rowoff;
    }
}

// Open a file and load its contents into the editor
void editorOpen(const char *filename)
{
    // free(E.filename);
    E.filename = strdup(filename);

    editorSelectSyntaxHighlight();

    FILE *fp = fopen(filename, "r");
    if (!fp)
        die("fopen");

    char *line = nullptr;
    size_t linecap = 0;
    ssize_t linelen;

    while ((linelen = getline(&line, &linecap, fp)) != -1)
    {
        while (linelen > 0 && (line[linelen - 1] == '\n' || line[linelen - 1] == '\r'))
            linelen--;
        editorInsertRow(E.numrows, line, linelen);
    }

    free(line);
    fclose(fp);
    E.dirty = 0;
}

/*** Input ***/
void editorMoveCursor(int key)
{
    // Check if the cursor is within a valid line; otherwise, set `row` to `nullptr`
    erow *row = (E.cy >= E.numrows) ? nullptr : &E.row[E.cy];

    switch (key)
    {
    case ARROW_LEFT:
        if (E.cx != 0)
        {
            E.cx--;
        }
        else if (E.cy > 0)
        {
            E.cy--;
            E.cx = E.row[E.cy].size;
        }
        break;
    case ARROW_RIGHT:
        if (row && E.cx < row->size)
        {
            E.cx++;
        }
        else if (row && E.cx == row->size)
        {
            E.cy++;
            E.cx = 0;
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

    row = (E.cy >= E.numrows) ? nullptr : &E.row[E.cy];
    int rowlen = row ? row->size : 0;
    if (E.cx > rowlen)
    {
        E.cx = rowlen;
    }
}

void editorProcessKeypress()
{
    static int quit_times = EDILITE_QUIT_TIMES;
    int c = editorReadKey();

    switch (c)
    {
    case '\r':
        editorInsertNewline();
        break;

    case CTRL_KEY('q'):
        if (E.dirty && quit_times > 0)
        {
            editorSetStatusMessage("WARNING!!! File has unsaved changes. "
                                   "Press Ctrl-Q %d more times to quit.",
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
        if (E.cy < E.numrows)
            E.cx = E.row[E.cy].size;
        break;

    case BACKSPACE:
    case CTRL_KEY('h'):
    case DEL_KEY:
        if (c == DEL_KEY)
            editorMoveCursor(ARROW_RIGHT);
        editorDelChar();
        break;

    case PAGE_UP:
    case PAGE_DOWN:
    {
        if (c == PAGE_UP)
        {
            E.cy = E.rowoff;
        }
        else if (c == PAGE_DOWN)
        {
            E.cy = E.rowoff + E.screenrows - 1;
            if (E.cy > E.numrows)
                E.cy = E.numrows;
        }
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

    case CTRL_KEY('f'):
        editorFind();
        break;

    case CTRL_KEY('l'):
    case '\x1b':
        break;

    default:
        editorInsertChar(c);
        break;
    }
    quit_times = EDILITE_QUIT_TIMES;
}

std::string editorPrompt(const std::string &prompt, void (*callback)(const std::string &, int) = nullptr)
{
    size_t buflen = 0;
    std::string buf;
    buf.reserve(128);

    while (true)
    {
        editorSetStatusMessage(prompt.c_str(), buf.c_str());
        editorRefreshScreen();

        int c = editorReadKey();

        if (c == '\x1b')
        { // ESC to cancel
            editorSetStatusMessage("");
            if (callback)
                callback(buf, c);
            return "";
        }
        else if (c == '\r')
        { // Enter to confirm
            if (!buf.empty())
            {
                editorSetStatusMessage("");
                if (callback)
                    callback(buf, c);
                return buf;
            }
        }
        else if (c == DEL_KEY || c == CTRL_KEY('h') || c == BACKSPACE)
        { // Backspace
            if (!buf.empty())
                buf.pop_back();
        }
        else if (!iscntrl(c) && c < 128)
        { // Add printable character
            buf += c;
            buflen++;
        }

        if (callback)
            callback(buf, c);
    }
}

/*** Output ***/
void editorScroll()
{
    E.rx = 0;

    if (E.cy < E.numrows)
        E.rx = editorRowCxToRx(&E.row[E.cy], E.cx);

    if (E.cy < E.rowoff)
        E.rowoff = E.cy;
    if (E.cy >= E.rowoff + E.screenrows)
        E.rowoff = E.cy - E.screenrows + 1;

    int lineNumberWidth = std::to_string(E.numrows).length() + 1;

    if (E.rx < E.coloff)
        E.coloff = E.rx;
    if (E.rx >= E.coloff + E.screencols + lineNumberWidth)
        E.coloff = E.rx - E.screencols + lineNumberWidth + 1;
}

void editorDrawRows(std::string &ab)
{
    // Calculate line number width based on total lines
    int lineNumberWidth = std::to_string(E.numrows).length() + 1;

    for (int y = 0; y < E.screenrows; y++)
    {
        int filerow = y + E.rowoff + 1;
        if (filerow >= E.numrows)
        {
            ab.append("~");
        }
        else
        {
            // Display the line number with padding to keep alignment
            char lineNumber[8];
            snprintf(lineNumber, sizeof(lineNumber), "%*d ", lineNumberWidth, filerow); // Line number with padding

            ab.append("\x1b[93m"); // Set color to bright yellow
            ab.append(lineNumber); // Append line number to the left of each line
            ab.append("\x1b[39m"); // Reset color to default

            int len = E.row[filerow].rsize - E.coloff;
            if (len < 0)
                len = 0;
            if (len > E.screencols - lineNumberWidth)
                len = E.screencols - lineNumberWidth - 1;

            char *c = &E.row[filerow].render[E.coloff];
            unsigned char *hl = &E.row[filerow].hl[E.coloff];

            int current_color = -1;
            for (int j = 0; j < len; j++)
            {
                if (iscntrl(c[j]))
                {
                    char sym = (c[j] <= 26) ? '@' + c[j] : '?';
                    ab.append("\x1b[7m");
                    ab.append(sym, 1);
                    ab.append("\x1b[m");
                    if (current_color != -1)
                    {
                        char buf[16];
                        int clen = snprintf(buf, sizeof(buf), "\x1b[%dm", current_color);
                        ab.append(buf, clen);
                    }
                }
                else if (hl[j] == HL_NORMAL)
                {
                    if (current_color != -1)
                    {
                        ab.append("\x1b[39m"); // Reset color
                        current_color = -1;
                    }
                    ab.append(1, c[j]);
                }
                else
                {
                    int color = editorSyntaxToColor(hl[j]);
                    if (color != current_color)
                    {
                        char buf[16];
                        int clen = snprintf(buf, sizeof(buf), "\x1b[%dm", color);
                        ab.append(buf, clen); // Apply new color
                        current_color = color;
                    }
                    ab.append(1, c[j]);
                }
            }
            ab.append("\x1b[39m");
        }
        ab.append("\x1b[K");
        ab.append("\r\n");
    }
}

void editorDrawTopStatusBar(std::string &ab)
{
    ab.append("\x1b[7m"); // Invert colors for the status bar
    std::string editor_name = std::string("EdiLite Text Editor -- version ") + EDILITE_VERSION;
    // const char *editor_name = editor_name.c_str();

    int len = editor_name.length();
    int padding = (E.screencols - len) / 2; // Center-align the text
    if (padding > 0)
    {
        for (int i = 0; i < padding; i++)
        {
            ab.append(" ");
        }
    }
    ab.append(editor_name);

    // Fill the remaining space
    while (len + padding < E.screencols)
    {
        ab.append(" ");
        len++;
    }

    ab.append("\x1b[m\r\n"); // Reset formatting and move to the next line
}

void editorDrawStatusBar(std::string &ab)
{
    ab.append("\x1b[7m"); // Invert colors
    char status[80], rstatus[80];
    int len = snprintf(status, sizeof(status), "%.20s - %d lines",
                       E.filename ? E.filename : "[No Name]", E.numrows, E.dirty ? "(modified)" : "");

    int rlen = snprintf(rstatus, sizeof(rstatus), "%s | %d/%d", E.syntax ? E.syntax->filetype : "no ft", E.cy + 1, E.numrows);
    if (len > E.screencols)
        len = E.screencols;
    ab.append(status);
    while (len < E.screencols)
    {
        if (E.screencols - len == rlen)
        {
            ab.append(rstatus);
            break;
        }
        else
        {
            ab.append(" ");
            len++;
        }
    }
    ab.append("\x1b[m"); // Reset to normal formatting
    ab.append("\r\n");
}

void editorDrawMessageBar(std::string &ab)
{
    ab.append("\x1b[K"); // Clear the line
    int msglen = strlen(E.statusmsg);
    if (msglen > E.screencols)
        msglen = E.screencols;
    if (msglen && time(nullptr) - E.statusmsg_time < 5)
        ab.append(E.statusmsg, msglen);
}

void editorDrawHelpLine(std::string &ab)
{
    ab.append("\x1b[7m"); // Invert colors for emphasis
    std::string helpText = "HELP: Ctrl-F = find | Ctrl-S = save | Ctrl-Q = quit";
    int helpTextLen = helpText.length();
    if (helpTextLen > E.screencols)
        helpTextLen = E.screencols;
    ab.append(helpText); // Add the help text
    while (helpTextLen < E.screencols)
    {
        ab.append(" "); // Fill the rest with spaces
        helpTextLen++;
    }
    ab.append("\x1b[m"); // Reset colors
    ab.append("\r\n");
}

void editorRefreshScreen()
{
    editorScroll();

    std::string ab;

    ab.append("\x1b[?25l"); // Hide the cursor

    // Append escape sequences to clear the screen and move cursor to top-left
    // ab.append("\x1b[2J");  // Clear the screen
    ab.append("\x1b[H"); // Move cursor to the top-left corner

    editorDrawTopStatusBar(ab);
    editorDrawRows(ab);
    editorDrawStatusBar(ab);
    editorDrawHelpLine(ab);
    editorDrawMessageBar(ab);

    // Calculate line number width dynamically
    int lineNumberWidth = std::to_string(E.numrows).length() + 1;

    // Move the cursor back to the top-left corner
    // ab.append("\x1b[H");

    char buf[32];
    int welcomelen = snprintf(buf, sizeof(buf), "\x1b[%d;%dH", (E.cy - E.rowoff) + 2, (E.rx - E.coloff) + lineNumberWidth + 2);
    ab.append(buf);

    ab.append("\x1b[?25h"); // Hide the cursor

    // Write the buffer contents to standard output
    write(STDOUT_FILENO, ab.c_str(), ab.size());
}

void editorSetStatusMessage(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(E.statusmsg, sizeof(E.statusmsg), fmt, ap);
    va_end(ap);
    E.statusmsg_time = time(nullptr);
}

/*** Init ***/
void initEditor()
{
    E.cx = 0;
    E.cy = 0;
    E.rx = 0;
    E.rowoff = 0;
    E.coloff = 0;
    E.numrows = 0;
    E.row = nullptr;
    E.filename = nullptr;
    E.statusmsg[0] = '\0';
    E.statusmsg_time = 0;
    E.dirty = 0;
    E.syntax = nullptr;

    if (getWindowSize(&E.screenrows, &E.screencols) == -1)
        die("getWindowSize");

    E.screenrows -= 4; // Reserve 3 rows for the status bar
}

int main(int argc, char *argv[])
{
    std::cout << "Welcome to the text Editor\n";

    enableRawMode();
    initEditor();

    if (argc >= 2)
    {
        editorOpen(argv[1]);
    }

    editorSetStatusMessage("Welcome to EdiLite, Use Arrow keys to navigate.");
    while (1)
    {
        editorRefreshScreen();
        editorProcessKeypress();
    }
    return 0;
}
