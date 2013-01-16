/* Checks your pi with the "correct" pi */

#include <stdio.h>
#include <malloc.h>
#include <string.h>

/* Get a line and store it in a dynamically-allocated memory block.
   The pointer to this block is returned in `out_ptr', and the newline
   character is pushed back onto the input stream.  */
int exp_getline(FILE* fp, char** out_ptr)
{
	char buffer[1024];
	char* lineData;
	unsigned lineLen;
	lineData = NULL;
	lineLen = 0;
	do
	{
		lineData = (char*)realloc(lineData, lineLen + 1024);
		buffer[0] = '\0';
		if (fgets(buffer, 1024, fp) == NULL)
		{
			if (lineLen == 0)
				lineData[0] = '\0';
			break;
		}
		strcpy(&lineData[lineLen], buffer);
		lineLen += strlen(buffer);
	}
	while (!feof(fp) && (lineLen == 0 || lineData[lineLen-1] != '\n'));

	(*out_ptr) = lineData;
	if (lineLen == 0 || lineData[lineLen-1] != '\n')
		return EOF;
	if (lineData[lineLen-1] == '\n')
	{
		ungetc((int)'\n', fp);
		lineData[lineLen-1] = '\0';
	}
	return 1;
}

int main()
{
	char c;
	char* piTest;
	const char pi[] = "3.14159265358979323846264338327950288419716939937510";

	printf("Pi is approximately equal to: ");
	fflush(stdout);
	exp_getline(stdin, &piTest); c = (char)getc(stdin);

	if (strcmp(pi, piTest) == 0)
		puts("true");
	else
		puts("false");

	return 0;
}
