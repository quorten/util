#ifndef TARGA_H
#define TARGA_H

#include "bool.h"

enum TgaTypes
{
    TGA_NODATA = 0,
    TGA_INDEXED = 1,
    TGA_RGB = 2,
    TGA_GRAYSCALE = 3,
    TGA_INDEXED_RLE = 9,
    TGA_RGB_RLE = 10,
    TGA_GRAYSCALE_RLE = 11
};

/* Image Data Formats */
#define	IMAGE_RGB       0
#define IMAGE_RGBA      1
#define IMAGE_LUMINANCE 2

/* Image data types */
#define	IMAGE_DATA_UNSIGNED_BYTE 0

/* Pixel data transfer from file to screen:
   These masks are AND'd with the imageDesc in the TGA header,
   bit 4 is left-to-right ordering
   bit 5 is top-to-bottom */
#define BOTTOM_LEFT  0x00	/* first pixel is bottom left corner */
#define BOTTOM_RIGHT 0x10	/* first pixel is bottom right corner */
#define TOP_LEFT     0x20	/* first pixel is top left corner */
#define TOP_RIGHT    0x30	/* first pixel is top right corner */

/* TGA header */
struct TgaHeader_tag
{
	unsigned char idLength;
	unsigned char colorMapType;
	unsigned char imageTypeCode;
	unsigned char colorMapSpec[5];
	unsigned short xOrigin;
	unsigned short yOrigin;
	unsigned short width;
	unsigned short height;
	unsigned char bpp;
	unsigned char imageDesc;
};
typedef struct TgaHeader_tag TgaHeader;

struct RgbaPix_tag
{
	unsigned char r;
	unsigned char g;
	unsigned char b;
	unsigned char a;
};
typedef struct RgbaPix_tag RgbaPix;

struct RgbPix_tag
{
	unsigned char r;
	unsigned char g;
	unsigned char b;
};
typedef struct RgbPix_tag RgbPix;

struct TargaImage_tag
{
	unsigned char colorDepth;
	unsigned char imageDataType;
	unsigned char imageDataFormat;
	unsigned char *imageData;
	unsigned char *deinter1;
	unsigned char *deinter2;
	unsigned short width;
	unsigned short height;
	unsigned long imageSize;
};
typedef struct TargaImage_tag TargaImage;

void TgaInit(TargaImage *tga);
void TgaSwapRedBlue(TargaImage *tga);
bool TgaLoad(TargaImage *tga, const char *filename);
bool TgaFlipVertical(TargaImage *tga);
void TgaRelease(TargaImage *tga);
bool TgaSave(TargaImage *tga, const char *filename, bool RLE);
void TgaCreateImage(TargaImage *tga, unsigned short width,
					unsigned short height, unsigned char bpp,
					unsigned char* data);
bool TgaConvertRGBToRGBA(TargaImage *tga, unsigned char alphaValue);
bool TgaConvertRGBAToRGB(TargaImage *tga);
bool TgaConvertRGBToLum(TargaImage *tga, bool fastMode);
bool TgaConvertLumToRGB(TargaImage *tga);
bool TgaDeinterlace(TargaImage *tga);

/*
Potential member accessor functions for private members variables:

GetImageFormat(); tga::imageDataFormat
GetImage(); tga::imageData
GetWidth(); tga::width
GetHeight(); tga::height
GetF1Image(); tga::deinter1
GetF2Image(); tga::deinter2
*/

#endif /* not TARGA_H */