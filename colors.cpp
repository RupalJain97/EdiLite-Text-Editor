#include <iostream>

int main()
{
	for (int i = 30; i <= 37; i++)
	{
		std::cout << "\033[" << i << "m" << i << ": foreground text\033[0m   ";
		std::cout << "\033[" << i + 10 << "m" << i + 10 << ": background text\033[0m\n";
	}

	std::cout << "\nHigh-Intensity ANSI Colors (90-97) - Foreground and Background:\n";
	for (int i = 90; i <= 97; i++)
	{
		std::cout << "\033[" << i << "m" << i << ": foreground text\033[0m   ";
		std::cout << "\033[" << i + 10 << "m" << i + 10 << ": background text\033[0m\n";
	}

	std::cout << "\nExtended 256 Colors (38;5;0-255):\n";
	for (int i = 0; i < 256; i++)
	{
		std::cout << "  \033[38;5;" << i << "m" << i << ": sample \033[0m  ";
		std::cout << "\033[48;5;" << i << "m" << i << ": sample\033[0m   \t";
		if ((i + 1) % 4 == 0)
		{ // To format into columns
			std::cout << "\n";
		}
	}

	std::cout << "\n";
	return 0;
}
