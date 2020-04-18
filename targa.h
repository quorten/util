#ifndef TARGA_H
#define TARGA_H

#include "bool.h"
#include "utstdint.h"

#pragma pack(push,1)

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

#ifndef __GNUC__
#define __attribute__(params)
#endif

/* TGA header */
struct TgaHeader_tag
{
	UTuint8 idLength;
	UTuint8 colorMapType;
	UTuint8 imageTypeCode;
	struct
	{
		/* `firstEntry' is actually "the palette index that the first
		   entry of the targa's color table maps to."  For example, if
		   `firstEntry' is 2, that means that a pixel in the image
		   with a color index value of 0x02 will map to inbdex 0 of the
		   TGA's color table, and a pixel in the image with index 0x00
		   or 0x01 is undefined.  */
		UTuint16 firstEntry;
		UTuint16 numEntries;
		UTuint8 bitsPerEntry;
	} colorMapSpec;
	UTuint16 xOrigin;
	UTuint16 yOrigin;
	UTuint16 width;
	UTuint16 height;
	UTuint8 bpp;
	UTuint8 imageDesc;
};
typedef struct TgaHeader_tag TgaHeader;

struct RgbaPix_tag
{
	UTuint8 b;
	UTuint8 g;
	UTuint8 r;
	UTuint8 a;
};
typedef struct RgbaPix_tag RgbaPix;

struct RgbPix_tag
{
	UTuint8 b;
	UTuint8 g;
	UTuint8 r;
};
typedef struct RgbPix_tag RgbPix;

struct TargaImage_tag
{
	UTuint8 colorDepth;
	UTuint8 imageDataType;
	UTuint8 imageDataFormat;
	UTuint8 *imageData;
	UTuint8 *deinter1;
	UTuint8 *deinter2;
	UTuint16 width;
	UTuint16 height;
	UTuint32 imageSize;
};
typedef struct TargaImage_tag TargaImage;

#pragma pack(pop)

void TgaInit(TargaImage *tga);
void TgaSwapRedBlue(TargaImage *tga);
bool TgaLoad(TargaImage *tga, const char *filename);
bool TgaFlipVertical(TargaImage *tga);
void TgaRelease(TargaImage *tga);
bool TgaSave(TargaImage *tga, const char *filename, bool RLE);
void TgaCreateImage(TargaImage *tga, UTuint16 width,
					UTuint16 height, UTuint8 bpp,
					UTuint8* data);
bool TgaConvertRGBToRGBA(TargaImage *tga, UTuint8 alphaValue);
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
