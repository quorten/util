/* For a Make Your Own Storybook, extract a specific tOFF from the
   tOFF collection.  */

#include <stdio.h>
#include <stdlib.h>

/* Note: the drawing OFFSET is stored in little endian.  */
struct {
	short dpadX; short dpadY;
	short scrX; short scrY; } __attribute__((packed)) tOFF;

int main(int argc, char *argv[])
{
	int number;
	FILE *fin, *fout;

	if (argc != 4)
		return 1;

	number = atoi(argv[1]);
	fin = fopen(argv[2], "rb");
	if (fin == NULL)
		return 1;
	fout = fopen(argv[3], "wb");
	if (fout == NULL)
		return 1;

	fseek(fin, 4 + number * 4, SEEK_SET);
	fread(&tOFF.dpadX, sizeof(tOFF.dpadX), 1, fin);
	fread(&tOFF.dpadY, sizeof(tOFF.dpadX), 1, fin);
	fseek(fin, 404 + number * 4, SEEK_SET);
	fread(&tOFF.scrX, sizeof(tOFF.dpadX), 1, fin);
	fread(&tOFF.scrY, sizeof(tOFF.dpadX), 1, fin);
	putc(0, fout);
	putc(0, fout);
	putc(0, fout);
	putc(0, fout);
	fwrite(&tOFF, sizeof(tOFF), 1, fout);

	fclose(fin);
	fclose(fout);
	return 0;
}
