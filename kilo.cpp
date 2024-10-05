/** Libraries */
#include <iostream>  // For input/output in C++
#include <unistd.h>  // For read() and STDIN_FILENO
#include <stdlib.h>  // For atexit()
#include <termios.h> // Terminal I/O attributes


/** Data */
struct termios orig_termios;

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

/** Init */
int main()
{
    enableRawMode();

    while (1)
    {
        char c; // Character to store input

        // Reading one byte at a time from standard input
        read(STDIN_FILENO, &c, 1);
        // iscntrl() comes from <ctype.h>, and printf() comes from <stdio.h>
        if (iscntrl(c))
        {
            std::cout << int(c) << "\n"; // Print control characters as integers
        }
        else
        {
            std::cout << int(c) << " ('" << c << "')\n"; // Print printable characters
        }
        if(c == 'q')
            break;
    }
    return 0;
}