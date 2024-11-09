# Text-Editor

Kilo is a lightweight terminal-based text editor implemented in C++. This editor supports essential editing features, including syntax highlighting, line-based navigation, and file saving.

## Steps

1. **Entering raw mode.** Raw mode is a terminal setting that allows the program to read input directly from the user without buffering or processing (like echoing characters or interpreting special keys). This lets the editor respond immediately to each keypress for an interactive editing experience. It is achieved by turning off a bunch of flags.

![Raw Mode](screenshots\proj_text_editor.png)

* Turn off echoing: print characters as they are typed
* Turn off canonical mode: reading input byte-by-byte, instead of line-by-line.
* Turn off all control combinations: Ctrl-C, Ctrl-Z, Ctrl-S, Ctrl-Q, Ctrl-V, Ctrl-M, etc
* Add error handling section: print the error and exit

2. Spliting the code into sections: data/defines, terminal, input, output, file I/o, init, buffer, editor operaions, row operation, syntax, searching
3. Enable Ctrl-Q to quit the editor and clear the screen
4. Repositioning of the cursor to top-left cornner, placing cursor at the correct position when typing, erasing, moving left/right/up/down, using tabs, end, del keys. Also ensure boundary cases.
5. Fetch the size of the terminal and draw lines starting with Tidle (~)
6. Mapping PAGE UP, PAGE DOWN, HOME, END, DELETE, TAB, Backspace, ENTER keys, vertical/horizontal scrolling and Control combinations.
7. Storing and rendering lines to screen, used dynamically allocated array "E.row"
8. Added a Welcome msg, status bar in new/existing files showing number of lines, file name, and help manual.
9. Prompting user to enter file name and saving file to disk with Ctrl-S. Also, warning about existing the editor with unsaved changes
10. Implement search feature: Ctrl-F to start a search and Enter/Escape to cancel, using arrow keys to go next/previous matches
11. Implement Syntax highlighting: coloring digits, strings, special coding langugage words, comments, adding color to search results. Detecting filetype of apply highlighting.



## Features

- Basic text editing capabilities
- Syntax highlighting for C/C++ files
- Search functionality (with navigation to search matches)
- Line and column indicators
- Support for terminal resizing

## Installation

To build the Kilo editor from source, use the following commands:

```
g++ -o kilo kilo.cpp -Wall
./kilo <filename>
```

OS: Ubuntu

This command will compile and run the editor. If a filename is provided, Kilo will open the specified file; otherwise, it will create a new file.

## Usage

- Ctrl-S: Save the current file.
- Ctrl-Q: Quit the editor (requires confirmation if there are unsaved changes).
- Ctrl-F: Search for text within the file.
- Arrow keys: Move the cursor in the respective direction.
- Page Up/Page Down: Scroll through the file quickly.
- Syntax Highlighting: The Kilo editor includes syntax highlighting support for C/C++ files. It detects numbers, strings, keywords, and comments in these file types, helping to visually distinguish different code elements.
