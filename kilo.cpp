#include <iostream> // For input/output in C++
#include <unistd.h> // For read() and STDIN_FILENO

int main()
{
    char c; // Character to store input
    // Reading one byte at a time from standard input
    while (read(STDIN_FILENO, &c, 1) == 1)
    {
        // For now, we just loop until EOF (no specific action taken)
    }
    return 0;
}