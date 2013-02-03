/* Applies dates to files specified in dirlist.txt and dates specified
   from standard input.  */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

void DateParse(const char* timeStr, FILETIME* fileTime);
void AltDateParse(const char* timeStr, FILETIME* fileTime);

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
	char* fileName = NULL;
	char* fileTimeStr = NULL;
	FILE* dirListFile = fopen("dirlist.txt", "r");
	if (dirListFile == NULL)
	{
		fputs("ERROR: Could not open file: dirlist.txt\n", stderr);
		return 1;
	}

	/* Get the creation date.  */
	while (exp_getline(stdin, &fileTimeStr) != EOF)
	{
		FILETIME fileTime;
		HANDLE hFile;
		getc(stdin); /* Skip '\n' */
		if (feof(stdin))
			break;
		/* Read the file name.  */
		exp_getline(dirListFile, &fileName);
		getc(dirListFile); /* Skip '\n' */

		/* Parse the creation date.  */
		if (isdigit((int)fileTimeStr[0]))
			DateParse(fileTimeStr, &fileTime);
		else
			AltDateParse(fileTimeStr, &fileTime);

		/* Apply the changes to our files.  */
		hFile = CreateFile(fileName, GENERIC_WRITE,
			FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
			OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
		SetFileTime(hFile, &fileTime, NULL, NULL);
		CloseHandle(hFile);

		free(fileName); fileName = NULL;
		free(fileTimeStr); fileTimeStr = NULL;
	}
	free(fileName); fileName = NULL;
	free(fileTimeStr); fileTimeStr = NULL;
	fclose(dirListFile);

	return 0;
}

/* Parse a UTC date in ISO 8601 format.  */
void DateParse(const char* timeStr, FILETIME* fileTime)
{
	SYSTEMTIME systemTime;
	sscanf(timeStr, "%hd-%hd-%hd %hd:%hd:%hd UTC",
		   &systemTime.wYear, &systemTime.wMonth, &systemTime.wDay,
		   &systemTime.wHour, &systemTime.wMinute, &systemTime.wSecond);
	systemTime.wDayOfWeek = 3;
	systemTime.wMilliseconds = 0;
	SystemTimeToFileTime(&systemTime, fileTime);
}

/* Parse a date that is in a format similar to the format that the
   Windows file properties dialog reports file times in.  This is how
   the date should look like: "January 08, 2013 5:38:49 AM".  */
void AltDateParse(const char* timeStr, FILETIME* fileTime)
{
	SYSTEMTIME st, lt; /* lt = localTime */
	TIME_ZONE_INFORMATION tz;
	char* monthNames[12] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun",
							 "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
	unsigned monthLens[12] = { 7, 8, 5, 5, 3, 4, 4, 6, 9, 7, 8, 8 };
	unsigned timeStrLen = strlen(timeStr);
	unsigned curPos = 0;
	unsigned i;

	/* Parse the month name.  */
	for (i = 0; i < 12; i++)
	{
		if (strstr(timeStr, monthNames[i]))
		{
			lt.wMonth = i + 1;
			curPos = monthLens[i] + 1;
			break;
		}
	}
	if (curPos >= timeStrLen)
		return;

	/* Parse the day, year, and time.  */
	sscanf(timeStr + curPos, "%hd, %hd %hd:%hd:%hd",
		   &lt.wDay, &lt.wYear, &lt.wHour, &lt.wMinute, &lt.wSecond);
	lt.wDayOfWeek = 3;
	lt.wMilliseconds = 0;

	/* Read either "AM" or "PM" off of the end of the string.  */
	curPos = timeStrLen;
	curPos -= 2;
	if (timeStr[curPos] == 'A')
	{
		if (lt.wHour == 12)
			lt.wHour = 0;
	}
	else
	{
		if (lt.wHour != 12)
			lt.wHour += 12;
	}

	/* Convert and store the file time.  */
	GetTimeZoneInformation(&tz);
	TzSpecificLocalTimeToSystemTime(&tz, &lt, &st);
	SystemTimeToFileTime(&st, fileTime);
}
