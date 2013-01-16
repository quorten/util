/* Translates recursive ls output to path-prefixed output.  The
   recursive ls output is read from standard input, and the
   path-prefixed output is written to standard output.  */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "bool.h"
#include "exparray.h"

EA_TYPE(char);

int main()
{
	char_array prefix;
	int readChar;

	/* Process the input one character at a time.  */
	EA_INIT(char, prefix, 16);
	readChar = getchar();
	while (readChar != EOF)
	{
		if ((char)readChar != '\n')
		{
			/* Read the whole prefix.  */
			EA_CLEAR(prefix);
			EA_APPEND(char, prefix, (char)readChar);
			readChar = getchar();
			while (readChar != EOF && (char)readChar != ':')
			{
				EA_APPEND(char, prefix, (char)readChar);
				readChar = getchar();
			}
			if (readChar == EOF)
				break;
			EA_APPEND(char, prefix, '/');
			EA_APPEND(char, prefix, '\0');
			readChar = getchar(); /* Read the newline.  */
			/* Read until the double newline.  */
			readChar = getchar();
			while (readChar != EOF)
			{
				if ((char)readChar == '\n')
					break;
				else
				{
					fputs(prefix.d, stdout);
					while (readChar != EOF && (char)readChar != '\n')
					{
						putchar(readChar);
						readChar = getchar();
					}
					if (readChar == EOF)
						break;
					putchar((int)'\n');
				}
				readChar = getchar();
			}
			if (readChar == EOF)
				break;
		}
		readChar = getchar();
	}

	/* Shutdown */
	EA_DESTROY(char, prefix);
	return 0;
}
