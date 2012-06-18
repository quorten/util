/* Read files in sdirlist.txt and rename them to files in dirlist */

#include <stdio.h>
#include <string.h>

int main()
{
	char shrtName[1024];
	char longName[1024];
	FILE* fp1;
	FILE* fp2;
	fp1 = fopen("sdirlist.txt", "rt");
	fp2 = fopen("dirlist.txt", "rt");

	while (fscanf(fp1, "%1023[^\n]", shrtName) != EOF)
	{
		char srcName[1024];
		unsigned pathEnd;
		char shrtFilName[1024];
		unsigned shrtBegin;
		unsigned shrtLen;
		int c;
		fscanf(fp2, "%1023[^\n]", longName);
		/* Find the long path */
		for (pathEnd = strlen(longName) - 1; longName[pathEnd] != '\\'; pathEnd--);
		pathEnd++;
		strncpy(srcName, longName, pathEnd);
		/* Put the short name on that */
		for (shrtBegin = strlen(shrtName) - 1; shrtName[shrtBegin] != '\\'; shrtBegin--);
		shrtBegin++;
		shrtLen = strlen(shrtName) - shrtBegin;
		strncpy(srcName + pathEnd, shrtName + shrtBegin, shrtLen);
		srcName[pathEnd+shrtLen] = '\0';
		rename(srcName, longName);
		c = fgetc(fp1); /* Ignore the newline */
		c = fgetc(fp2);
	}

	/* Shutdown */
	fclose(fp1);
	fclose(fp2);
	return 0;
}
