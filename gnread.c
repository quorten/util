/* Read a GNAM chunk file and write its contents to standard
   output.  */

#include <stdio.h>

int main(int argc, char *argv[])
{
	char *filename;
	FILE *fp;

	if (argc != 2)
		return 1;

	filename = argv[1];
	fp = fopen(filename, "rb");
	if (fp == NULL)
		return 1;

	{ /* Read the story type.  */
		char *story_type = "Unknown";
		int c = getc(fp);
		if (c == EOF)
			return 1;
		switch (c)
		{
		case 0: story_type = "Doodle"; break;
		case 1:
			story_type = "Funny Little Man (Strange Princess) Story"; break;
		case 2: story_type = "Ugly Troll People Story"; break;
		case 3: story_type = "Bug Eater Story"; break;
		case 4: story_type = "One Big Wish Story"; break;
		case 0xff: story_type = "Make Your Own Story"; break;
		}
		fputs("Story type: ", stdout);
		puts(story_type);
		/* Ignore the next two story type characters.  */
		c = getc(fp);
		c = getc(fp);
		if (c == EOF)
			return 1;
	}

	{ /* Read the lines of the story name.  There can be up to three
		 lines in the story name.  */
		unsigned i;
		puts("Story name:");
		for (i = 0; i < 3; i++)
		{
			char record[80];
			if (fread(record, sizeof(record), 1, fp) != 1)
				return 1;
			puts(record);
		}
	}

	return 0;
}
