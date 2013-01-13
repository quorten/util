/* Break continuous output into lines, read from standard input and
   written to standard output. */

#include <stdio.h>

int main(int argc, char* argv[])
{
	char c;
	int maxLine;
	int curCol;
	if (argc > 1)
		sscanf(argv[1], "%d", &maxLine);
	else
		return 1;
	curCol = 0;
	while (1)
	{
		c = (char)getchar();
		if (feof(stdin))
			break;
		putchar(c);
		curCol++;
		if (curCol == maxLine)
		{
			putchar('\n');
			curCol = 0;
		}
	}
	return 0;
}
