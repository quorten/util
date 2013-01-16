/* Gathers created dates from files specified on standard input and
   writes them to standard output.  */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdio.h>
#include <stdlib.h>
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
	char *fileName = NULL;

	/* Read a file name from standard input.  */
	while (exp_getline(stdin, &fileName) != EOF)
	{
		/* Get the file creation date.  */
		FILETIME fileTime;
		SYSTEMTIME UTCTime;
		HANDLE hFile;
		BOOL retval;
		getc(stdin); /* Skip '\n' */
		if (feof(stdin))
			break;

		hFile = CreateFile(fileName, GENERIC_READ,
			FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
			NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
		if (hFile == INVALID_HANDLE_VALUE)
		{
			fprintf(stderr, "ERROR: Could not open file: %s\n", fileName);
			free(fileName); fileName = NULL;
			continue;
		}
		retval = GetFileTime(hFile, &fileTime, NULL, NULL);
		CloseHandle(hFile);
		if (!retval)
		{
			fprintf(stderr, "ERROR: Could not read file time: %s\n",
					fileName);
			free(fileName); fileName = NULL;
			continue;
		}

		/* Write the time in UTC to standard output.  */
		FileTimeToSystemTime(&fileTime, &UTCTime);
		printf("%d-%02d-%02d %02d:%02d:%02d UTC\n",
			   UTCTime.wYear, UTCTime.wMonth, UTCTime.wDay,
			   UTCTime.wHour, UTCTime.wMinute, UTCTime.wSecond);
		free(fileName); fileName = NULL;
	}
	free(fileName); fileName = NULL;

	return 0;
}
