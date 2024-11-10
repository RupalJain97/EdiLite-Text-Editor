#include <iostream>

int main()
{
	std::cout << "Basic ANSI Colors (30-37):\n";
	for (int i = 30; i <= 37; i++)
	{
		std::cout << "\033[" << i << "m" << i << ": sample text\033[0m\n";
	}

	std::cout << "\nHigh-Intensity ANSI Colors (90-97):\n";
	for (int i = 90; i <= 97; i++)
	{
		std::cout << "\033[" << i << "m" << i << ": sample text\033[0m\n";
	}

	std::cout << "\nExtended 256 Colors (38;5;0-255):\n";
	for (int i = 0; i < 256; i++)
	{
		std::cout << "  \033[38;5;" << i << "m" << i << ": sample text\033[0m\t  ";
		if ((i + 1) % 8 == 0)
		{ // To format into columns
			std::cout << "\n";
		}
	}

	std::cout << "\n";
	return 0;
}
