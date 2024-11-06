/** Libraries */
#include <iostream>  // For input/output in C++
#include <unistd.h>  // For read() and STDIN_FILENO
#include <stdlib.h>  // For atexit()
#include <termios.h> // Terminal I/O attributes

/** Data */
struct termios orig_termios;

/*** defines ***/
#define CTRL_KEY(k) ((k) & 0x1f)

/** Error Handling */
// TODO

/** Terminal */
void disableRawMode()
{
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

void enableRawMode()
{
    tcgetattr(STDIN_FILENO, &orig_termios); // Get current terminal attributes
    atexit(disableRawMode);                 // Ensure original settings are restored on exit

    struct termios raw = orig_termios; // Make a copy of the terminal settings

    raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);          // Disable echo, canonical mode, Ctrl-C/Z signals
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON); // Disable input flags
    raw.c_oflag &= ~(OPOST);                                  // Disable output processing
    raw.c_cflag |= (CS8);                                     // Set character size to 8 bits

    // raw.c_cc[VMIN] = 0;  // Set minimum number of bytes to read
    // raw.c_cc[VTIME] = 1; // Set timeout for reading

    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw); // Apply new settings
}


void die(const char* s) {
    std::cerr << s << ": " << strerror(errno) << std::endl;
    exit(EXIT_FAILURE);
}

/** Init */
int main()
{
    std::cout << "Welcome to the text Editor\n";
    // std::cout << "This is the raw mode.\n";
    // std::cout << "Raw mode is a terminal setting that allows the program to read input directly from the user without buffering or processing (like echoing characters or interpreting special keys). This lets the editor respond immediately to each keypress for an interactive editing experience.\n";
    std::cout << "Enter 'q' to exit.\n";

    enableRawMode();

    while (1)
    {
        char c = '\0'; // Character to store input

        if (read(STDIN_FILENO, &c, 1) == -1 && errno != EAGAIN) die("read");

        // iscntrl() comes from <ctype.h>
        if (iscntrl(c))
        {
            std::cout << int(c) << "\r\n"; // Print control characters as integers
        }
        else
        {
            std::cout << int(c) << " ('" << c << "')\r\n"; // Print printable characters
        }
        if (c == CTRL_KEY('q'))
            break;
    }
    return 0;
}
