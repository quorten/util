/* Merges line-broken output into a single line. */

#include <stdio.h>

int main()
{
	char c;
	while (1)
	{
		c = (char)getchar();
		if (feof(stdin))
			break;
		if (c != '\r' && c != '\n')
			putchar(c);
	}
	return 0;
}
