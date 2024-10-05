#include <iostream>  // For input/output in C++
#include <unistd.h>  // For read() and STDIN_FILENO
#include <stdlib.h>  // For atexit()
#include <termios.h> // Terminal I/O attributes

struct termios orig_termios;

void disableRawMode()
{
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

void enableRawMode()
{
    tcgetattr(STDIN_FILENO, &orig_termios); // Get current terminal attributes
    atexit(disableRawMode);                 // Ensure original settings are restored on exit

    struct termios raw = orig_termios; // Make a copy of the terminal settings

    raw.c_lflag &= ~(ECHO | ICANON);          // Disable echo, canonical mode
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw); // Apply new settings
}

int main()
{
    enableRawMode();

    char c; // Character to store input

    // Reading one byte at a time from standard input
    while (read(STDIN_FILENO, &c, 1) == 1 && c != 'q')
    {
        // iscntrl() comes from <ctype.h>, and printf() comes from <stdio.h>
        if (iscntrl(c))
        {
            std::cout << int(c) << "\n"; // Print control characters as integers
        }
        else
        {
            std::cout << int(c) << " ('" << c << "')\n"; // Print printable characters
        }
    }
    return 0;
}