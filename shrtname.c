/* Create an ISO9660 CDFS filename, one per line */
/* Note: This program does not check for duplicate filenames */

#include <stdio.h>
#include <string.h>

int main()
{
	char filename[1024];
	FILE* fp1;
	FILE* fp2;
	/* We will open two files - one for input and one for output */
	fp1 = fopen("dirlist.txt", "rt");
	fp2 = fopen("sdirlist.txt", "wt");
	/* Then we will process the input file one line at a time */

	while (fscanf(fp1, "%1023[^\n]", filename) != EOF)
	{
		unsigned i;
		unsigned pathLen;
		int c;
		if (filename[strlen(filename)-2] == '\r')
			filename[strlen(filename)-2] = '\0';
		/* Remove all spaces */
		for (i = 0; filename[i] != '\0'; i++)
		{
			if (filename[i] == ' ')
				filename[i] = '_';
		}
		/* Replace all special characters and all non-extension dots */
		for (i = 0; filename[i] != '\0'; i++)
		{
			if (filename[i] != '.' &&
				filename[i] != '\\' &&
				(filename[i] < 0x30 || filename[i] > 0x39) &&
				(filename[i] < 0x41 || filename[i] > 0x5A) &&
				(filename[i] < 0x61 || filename[i] > 0x7A))
				filename[i] = '_';
			else if (filename[i] == '.')
			{
				if (filename[i+1] == '\\' || filename[i+1] == '\0')
				{
					filename[i] = '_';
					break;
				}
				if (filename[i+2] != '\\' && filename[i+3] != '\\')
				{
					if (filename[i+2] == '\0' || filename[i+3] == '\0')
						break;
					if (filename[i+4] != '\\' && filename[i+4] != '\0')
					{
						/* Test if this is not an extension dot */
						unsigned extPos = i;
						i++;
						while (filename[i] != '\\' && filename[i] != '\0' && filename[i] != '.')
							i++;
						if (filename[i] == '.')
						{
							/* Replace the previous dot */
							filename[extPos] = '_';
						}
						else
						{
							/* Truncate this extension */
							unsigned endPos = i;
							extPos += 4;
							memmove(&(filename[extPos]), &(filename[endPos]), strlen(filename) + 1 - extPos);
							if (filename[i] == '\0')
								break;
						}
					}
				}
			}
		}
		/* Shorten each path component */
		pathLen = 0;
		for (i = 0; filename[i] != '\0'; i++)
		{
			if (pathLen == 8)
			{
				if (filename[i] == '.')
				{
					while (filename[i] != '\\' && filename[i] != '\0')
						i++;
					pathLen = 0;
					if (filename[i] == '\0')
						break;
				}
				else
				{
					unsigned endPos = i;
					while (filename[i] != '\\' && filename[i] != '\0' && filename[i] != '.')
						i++;
					memmove(&(filename[endPos]), &(filename[i]), strlen(filename) + 1 - i);
					pathLen = 0;
					i = endPos;
					if (filename[i] == '\0')
						break;
				}
			}
			if (filename[i] == '\\')
			{
				pathLen = 0;
				continue;
			}
			if (filename[i] != '.')
				pathLen++;
			else
			{
				while (filename[i] != '\\' && filename[i] != '\0')
					i++;
				if (filename[i] == '\0')
					break;
				pathLen = 0;
			}
		}
		/* Write the path */
		fprintf(fp2, "%s\n", filename);
		c = fgetc(fp1); /* Ignore the newline */
	}

	/* Shutdown */
	fclose(fp1);
	fclose(fp2);
	return 0;
}
