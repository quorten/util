/* Make all kinds of great ASCII art, by loading a greyscale TARGA
   image and changing pixels into characters! */

#include <stdio.h>
#include "targa.h"
#include "lumchars.h"

int main(int argc, char* argv[])
{
	TargaImage image;
	unsigned char* imgData;
	int y, x;
	int invert;

	if (argc < 2)
	{
		puts("Ussage: ascart TARGA\nOutput will be sent to standard output.");
		return 0;
	}
	TgaInit(&image);
	if (!TgaLoad(&image, argv[1]))
	{
		fputs("Error loading image.\n", stderr);
		TgaRelease(&image);
		return 1;
	}
	TgaFlipVertical(&image);

	if (argc >= 3 && argv[2][0] == '-' && argv[2][1] == '1')
		invert = 1;
	else
		invert = 0;

	if (image.imageDataFormat != IMAGE_LUMINANCE)
		TgaConvertRGBToLum(&image, false);
	imgData = image.imageData;

	for (y = 0; y < image.height; y++)
	{
		for (x = 0; x < image.width; x++)
		{
			unsigned char val;
			val = imgData[y*image.width+x];
			if (invert)
				val = 255 - val;
			putchar(ascChars[val]);
		}
		putchar('\n');
	}
	TgaRelease(&image);
	return 0;
}
