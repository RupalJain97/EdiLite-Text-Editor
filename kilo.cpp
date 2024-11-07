/** Libraries */
#include <iostream>  // For input/output in C++
#include <unistd.h>  // For read() and STDIN_FILENO
#include <stdlib.h>  // For atexit()
#include <termios.h> // Terminal I/O attributes
#include <errno.h>
#include <cstring>     // For strerror
#include <sys/ioctl.h> // To get terminal dimensions
#include <stdio.h>

/** Data */
// struct termios orig_termios;
struct editorConfig
{
    int screenrows;
    int screencols;
    struct termios orig_termios;
};
struct editorConfig E;

/*** defines ***/
#define CTRL_KEY(k) ((k) & 0x1f)

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

char editorReadKey()
{
    int nread;
    char c;
    while ((nread = read(STDIN_FILENO, &c, 1)) != 1)
    {
        if (nread == -1 && errno != EAGAIN)
            die("read");
    }
    return c;
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
    if (sscanf(&buf[2], "%d;%d", rows, cols) != 2) return -1;

    // Extract rows and columns from the response
    // try
    // {
    //     size_t semicolon;
    //     *rows = std::stoi(buf.substr(2), &semicolon);
    //     *cols = std::stoi(buf.substr(semicolon + 3));
    // }
    // catch (e)
    // {
    //     return -1;
    // }

    // std::cout << "\r\n";
    // char c;
    // while (read(STDIN_FILENO, &c, 1) == 1)
    // {
    //     if (iscntrl(c))
    //     {
    //         std::cout << static_cast<int>(c) << "\r\n"; // Print control characters as integers
    //     }
    //     else
    //     {
    //         std::cout << static_cast<int>(c) << " ('" << c << "')\r\n"; // Print printable characters
    //     }
    // }

    editorReadKey();
    return -1;
}

int getWindowSize(int *rows, int *cols)
{
    struct winsize ws;
    if (1 || ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0)
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

/*** Input ***/
void editorProcessKeypress()
{
    char c = editorReadKey();

    switch (c)
    {
    case CTRL_KEY('q'):
        /* Clear the screen when user presses Ctrl-Q to quit */
        std::cout << "Exiting editor...\n";
        write(STDOUT_FILENO, "\x1b[2J", 4);
        write(STDOUT_FILENO, "\x1b[H", 3);
        exit(0);
        break;
    }
}

/*** Output ***/
void editorDrawRows()
{
    for (int y = 0; y < E.screenrows; y++)
    {
        write(STDOUT_FILENO, "~\r\n", 3); // Draw a tilde at the start of each line
    }
}

void editorRefreshScreen()
{
    /*
    write() and STDOUT_FILENO come from <unistd.h>.

    The 4 in our write() call means we are writing 4 bytes out to the terminal. The first byte is \x1b, which is the escape character, or 27 in decimal. (Try and remember \x1b, we will be using it a lot.) The other three bytes are [2J.
    */
    write(STDOUT_FILENO, "\x1b[2J", 4);

    /*
    reposition the cursor which is at the top-left corner so that we’re ready to draw the editor interface from top to bottom.

    if you have an 80×24 size terminal and you want the cursor in the center of the screen, you could use the command <esc>[12;40H. (Multiple arguments are separated by a ; character.) The default arguments for H both happen to be 1.
    */
    write(STDOUT_FILENO, "\x1b[H", 3);

    editorDrawRows();
    write(STDOUT_FILENO, "\x1b[H", 3);
}

/*** Init ***/
//  initialize all the fields in the E struct.
void initEditor()
{
    if (getWindowSize(&E.screenrows, &E.screencols) == -1)
        die("getWindowSize");
}

int main()
{
    std::cout << "Welcome to the text Editor\n";
    // std::cout << "This is the raw mode.\n";
    // std::cout << "Raw mode is a terminal setting that allows the program to read input directly from the user without buffering or processing (like echoing characters or interpreting special keys). This lets the editor respond immediately to each keypress for an interactive editing experience.\n";
    // std::cout << "Enter 'q' to exit.\n";

    enableRawMode();
    initEditor();

    while (1)
    {
        editorRefreshScreen();
        editorProcessKeypress();
    }
    return 0;
}
