#include <stdio.h>
#include <string.h>
#include "targa.h"

void TgaInit(TargaImage *tga)
{
	tga->imageData = NULL;
	tga->deinter1 = NULL;
	tga->deinter2 = NULL;
}

void TgaSwapRedBlue(TargaImage *tga)
{
	unsigned pixSize;
	unsigned char blue;
	unsigned char *curPix;
	int pixel;

	/* Simply get the pixel byte size and cast to a RgbPix.  */
	switch (tga->colorDepth)
    {
    case 32: pixSize = sizeof(RgbaPix); break;
    case 24: pixSize = sizeof(RgbPix); break;
    default:
		/* Ignore other color depths.  */
		return;
    }

	curPix = tga->imageData;
	for (pixel = 0; pixel < (tga->width * tga->height); pixel++)
    {
		blue = *(curPix + 2);
		*(curPix + 2) = *curPix; /* blue = red */
		*curPix = blue; /* red = blue */
		curPix += pixSize;
    }
}

bool TgaLoad(TargaImage *tga, const char *filename)
{
	FILE *fp;
	TgaHeader tgaHeader;
	int bytesPerPix;

	fp = fopen(filename, "rb");
	if (!fp)
		return false;

	/* Read the TGA header.  */
	fread(&tgaHeader, 1, sizeof(TgaHeader), fp);

	/* See if the image type is one that we support (RGB, RGB RLE,
	   GRAYSCALE, GRAYSCALE RLE).  */
	if (((tgaHeader.imageTypeCode != TGA_RGB) &&
		 (tgaHeader.imageTypeCode != TGA_GRAYSCALE) &&
		 (tgaHeader.imageTypeCode != TGA_RGB_RLE) &&
		 (tgaHeader.imageTypeCode != TGA_GRAYSCALE_RLE)) ||
		tgaHeader.colorMapType != 0)
    {
		fclose(fp);
		return false;
    }

	/* Get image width and height.  */
	tga->width = tgaHeader.width;
	tga->height = tgaHeader.height;
	/* bytesPerPix -> 3 = BGR, 4 = BGRA */
	bytesPerPix = tgaHeader.bpp / 8;
	switch(tgaHeader.imageTypeCode)
	{
	case TGA_RGB:
	case TGA_RGB_RLE:
		if (bytesPerPix == 3)
		{
			tga->imageDataFormat = IMAGE_RGB;
			tga->imageDataType = IMAGE_DATA_UNSIGNED_BYTE;
			tga->colorDepth = 24;
		}
		else
		{
			tga->imageDataFormat = IMAGE_RGBA;
			tga->imageDataType = IMAGE_DATA_UNSIGNED_BYTE;
			tga->colorDepth = 32;
		}
		break;
	case TGA_GRAYSCALE:
	case TGA_GRAYSCALE_RLE:
		tga->imageDataFormat = IMAGE_LUMINANCE;
		tga->imageDataType = IMAGE_DATA_UNSIGNED_BYTE;
		tga->colorDepth = 8;
		break;
	}

	/* Allocate memory for TGA image data.  */
	tga->imageSize = tga->width * tga->height * bytesPerPix;
	tga->imageData = (unsigned char *)malloc(tga->imageSize);
	if (tga->imageData == NULL)
		return false;

	/* Skip past the id if there is one.  */
	if (tgaHeader.idLength > 0)
		fseek(fp, tgaHeader.idLength, SEEK_CUR);

	/* Read the image data.  */
	if (tgaHeader.imageTypeCode == TGA_RGB ||
		tgaHeader.imageTypeCode == TGA_GRAYSCALE)
    {
		fread(tga->imageData, 1, tga->imageSize, fp);
    }
	else
    {
		/* This is an RLE compressed image.  */
		unsigned char id;
		unsigned char length;
		unsigned char luma;
		RgbaPix color = { 0, 0, 0, 0 };
		unsigned int i = 0;

		while (i < tga->imageSize)
		{
			id = fgetc(fp);

			/* See if this is run length data.  */
			if (id >= 128)
			{
				/* Find the run length.  */
				length = (unsigned char)(id - 127);

				/* The next few bytes are the repeated values.  */
				if (tgaHeader.imageTypeCode == TGA_GRAYSCALE_RLE)
					luma = (unsigned char)fgetc(fp);
				else
				{
					color.b = (unsigned char)fgetc(fp);
					color.g = (unsigned char)fgetc(fp);
					color.r = (unsigned char)fgetc(fp);
					if (bytesPerPix == 4)
						color.a = (unsigned char)fgetc(fp);
				}

				/* Save everything in this run.  */
				while (length > 0)
				{
					if (tgaHeader.imageTypeCode == TGA_GRAYSCALE_RLE)
						tga->imageData[i++] = luma;
					else
					{
						tga->imageData[i++] = color.b;
						tga->imageData[i++] = color.g;
						tga->imageData[i++] = color.r;
						if (bytesPerPix == 4)
							tga->imageData[i++] = color.a;
					}
					--length;
				}
			}
			else
			{
				/* Get the number of non RLE pixels.  */
				length = (unsigned char)(id + 1);

				while (length > 0)
				{
					if (tgaHeader.imageTypeCode == TGA_GRAYSCALE_RLE)
					{
						tga->imageData[i++] =
							(unsigned char)fgetc(fp);
					}
					else
					{
						tga->imageData[i++] =
							(unsigned char)fgetc(fp); /* Blue */
						tga->imageData[i++] =
							(unsigned char)fgetc(fp); /* Green */
						tga->imageData[i++] =
							(unsigned char)fgetc(fp); /* Red */
						if (bytesPerPix == 4)
							tga->imageData[i++] =
								(unsigned char)fgetc(fp); /* Alpha */
					}

					--length;
				}
			}
		}
    }

	fclose(fp);

	if ((tgaHeader.imageDesc & TOP_LEFT) == TOP_LEFT)
		TgaFlipVertical(tga);
	/* TgaSwapRedBlue(tga); */

	return (tga->imageData != NULL);
}

bool TgaFlipVertical(TargaImage *tga)
{
	int lineWidth;
	RgbaPix *tmpBits;
	RgbaPix *top;
	RgbaPix *bottom;
	int i;

	if (!tga || !tga->imageData)
		return false;

	lineWidth = tga->width;

	switch (tga->colorDepth)
    {
    case 32: lineWidth *= 4; break;
    case 24: lineWidth *= 3; break;
    case 16: lineWidth *= 2; break;
    case 8: /* Do nothing */ break;
    default:
		return false;
    }

	tmpBits = (RgbaPix *)malloc(sizeof(RgbaPix) * tga->width);
	if (!tmpBits)
		return false;

	top = (RgbaPix *)tga->imageData;
	bottom = (RgbaPix *)(tga->imageData + lineWidth*(tga->height-1));

	for (i = 0; i < (tga->height / 2); i++)
    {
		memcpy(tmpBits, top, lineWidth);
		memcpy(top, bottom, lineWidth);
		memcpy(bottom, tmpBits, lineWidth);

		top = (RgbaPix *)((unsigned char *)top + lineWidth);
		bottom = (RgbaPix * )((unsigned char *)bottom - lineWidth);
    }

	free(tmpBits);
	tmpBits = NULL;

	return true;
}

void TgaRelease(TargaImage *tga)
{
	if (tga->imageData != NULL)
		free(tga->imageData);
	tga->imageData = NULL;
	if (tga->deinter1 != NULL)
		free(tga->deinter1);
	tga->deinter1 = NULL;
	if (tga->deinter2 != NULL)
		free(tga->deinter2);
	tga->deinter2 = NULL;
}

bool TgaSave(TargaImage *tga, const char *filename, bool RLE)
{
	TgaHeader header;
	const char *idString = "Customary program generated image";
	unsigned idStrLen = 33;
	unsigned char *pRLEImage;
	unsigned int rleSize = 0;
	FILE* fp;

	header.idLength        = idStrLen;
	header.colorMapType    = 0;
	/* Perform color stuff after other header values are set.  */
	header.colorMapSpec[0] = 0;
	header.colorMapSpec[1] = 0;
	header.colorMapSpec[2] = 0;
	header.colorMapSpec[3] = 0;
	header.colorMapSpec[4] = 0;
	header.xOrigin         = 0;
	header.yOrigin         = 0;
	header.width           = tga->width;
	header.height          = tga->height;
	header.bpp             = tga->colorDepth;
	header.imageDesc       = BOTTOM_LEFT;

	TgaSwapRedBlue(tga); /* Convert back to BGR */

	switch (tga->imageDataFormat)
    {
    case IMAGE_RGB:
    case IMAGE_RGBA:
		header.imageTypeCode = TGA_RGB;
		break;
    case IMAGE_LUMINANCE:
		header.imageTypeCode = TGA_GRAYSCALE;
		break;
    }
	/* NOTE: RLE compressor does not properly compress 112112 patterns.  */
	if (RLE)
    {
		int bytesPerPix = tga->colorDepth / 8;
		unsigned int i = 0; /* Source iterator */
		header.imageTypeCode |= 0x8;
		/* Perform RLE data compression.  */
		pRLEImage = (unsigned char *)malloc(tga->imageSize);

		if (tga->imageDataFormat == IMAGE_LUMINANCE)
		{
			unsigned char luma;
			while (i < tga->imageSize)
			{
				unsigned char length = 1; /* id length for RLE */
				/* Get first reference color */
				luma = tga->imageData[i++];

				/* Look max 128 pixels into the file.  */
				while (length < 128 && i < tga->imageSize)
				{
					/* Break if a pixel differs.  */
					if (luma != tga->imageData[i])
						break;
					length++;
					i += bytesPerPix;
				}
				if (length == 1)
				{
					/* We have non RLE data.  */
					unsigned char luma2;
					/* Store the id and the first pixel.  */
					unsigned char *id = &pRLEImage[rleSize];
					pRLEImage[rleSize++] = 0;
					pRLEImage[rleSize++] = luma;

					/* Retrieve the pixel which broke the loop.  */
					luma = tga->imageData[i++];
					/* Loop until encounterance of RLE data or 128 pixel
					   limit.  */
					while (*id < 127 && i < tga->imageSize)
					{
						luma2 = tga->imageData[i++];

						if (luma == luma2)
						{
							/* This is the beginning of an RLE chunk, go
							   back two pixels.  */
							i -= 2 * bytesPerPix;
							break;
						}
						else
						{
							/* Store non-RLE data.  */
							(*id)++;
							pRLEImage[rleSize++] = luma;
						}
						luma = luma2;
					}
				}
				else
				{
					/* That was the end of an RLE chunk.  */
					pRLEImage[rleSize++] = length + 127;
					pRLEImage[rleSize++] = luma;
				}
			}
		}
		else
		{
			RgbaPix color = { 0, 0, 0, 0 };
			while (i < tga->imageSize)
			{
				unsigned char length = 1; /* id length for RLE */
				/* Get first reference color.  */
				color.b = tga->imageData[i++];
				color.g = tga->imageData[i++];
				color.r = tga->imageData[i++];
				if (bytesPerPix == 4)
					color.a = tga->imageData[i++];

				/* Look max 128 pixels into the file.  */
				while (length < 128 && i < tga->imageSize)
				{
					/* Break if a pixel differs.  */
					if (color.b != tga->imageData[i])
						break;
					if (color.g != tga->imageData[i+1])
						break;
					if (color.r != tga->imageData[i+2])
						break;
					if (bytesPerPix == 4)
					{
						if (color.a != tga->imageData[i+3])
							break;
					}
					length++;
					i += bytesPerPix;
				}
				if (length == 1)
				{
					/* We have non RLE data.  */
					RgbaPix color2 = {0, 0, 0, 0};
					/* Store the id and the first pixel.  */
					unsigned char *id = &pRLEImage[rleSize];
					pRLEImage[rleSize++] = 0;
					pRLEImage[rleSize++] = color.b;
					pRLEImage[rleSize++] = color.g;
					pRLEImage[rleSize++] = color.r;
					if (bytesPerPix == 4)
						pRLEImage[rleSize++] = color.a;

					/* Retrieve the pixel which broke the loop.  */
					color.b = tga->imageData[i++];
					color.g = tga->imageData[i++];
					color.r = tga->imageData[i++];
					if (bytesPerPix == 4)
						color.a = tga->imageData[i++];
					/* Loop until encounterance of RLE data or 128 pixel
					   limit.  */
					while (*id < 127 && i < tga->imageSize)
					{
						color2.b = tga->imageData[i++];
						color2.g = tga->imageData[i++];
						color2.r = tga->imageData[i++];
						if (bytesPerPix == 4)
							color2.a = tga->imageData[i++];

						if (color.b == color2.b &&
							color.g == color2.g &&
							color.r == color2.r &&
							color.a == color2.a)
						{
							/* This is the beginning of an RLE chunk, go
							   back two pixels.  */
							i -= 2 * bytesPerPix;
							break;
						}
						else
						{
							/* Store non-RLE data.  */
							(*id)++;
							pRLEImage[rleSize++] = color.b;
							pRLEImage[rleSize++] = color.g;
							pRLEImage[rleSize++] = color.r;
							if (bytesPerPix == 4)
								pRLEImage[rleSize++] = color.a;
						}
						color = color2;
					}
				}
				else
				{
					/* That was the end of an RLE chunk.  */
					pRLEImage[rleSize++] = length + 127;
					pRLEImage[rleSize++] = color.b;
					pRLEImage[rleSize++] = color.g;
					pRLEImage[rleSize++] = color.r;
					if (bytesPerPix == 4)
						pRLEImage[rleSize++] = color.a;
				}
			}
		}
    }

	fp = fopen(filename, "wb");
	if (!fp)
		return false;
	fwrite(&header, sizeof(TgaHeader), 1, fp);
	fwrite(idString, idStrLen, 1, fp);
	if (RLE)
    {
		fwrite(pRLEImage, rleSize, 1, fp);
		free(pRLEImage);
    }
	else
		fwrite(tga->imageData, tga->imageSize, 1, fp);
	fclose(fp);
	return true;
}

void TgaCreateImage(TargaImage *tga, unsigned short width,
					unsigned short height, unsigned char bpp,
					unsigned char* data)
{
	tga->width = width;
	tga->height =  height;
	tga->colorDepth = bpp;
	tga->imageSize = width * height * (bpp / 8);
	tga->imageData = data;
	tga->imageDataType = IMAGE_DATA_UNSIGNED_BYTE;
	switch (bpp)
    {
    case 8:
		tga->imageDataFormat = IMAGE_LUMINANCE;
		break;
    case 24:
		tga->imageDataFormat = IMAGE_RGB;
		break;
    case 32:
		tga->imageDataFormat = IMAGE_RGBA;
		break;
    }
}

bool TgaConvertRGBToRGBA(TargaImage *tga, unsigned char alphaValue)
{
	if (tga->colorDepth == 24 && tga->imageDataFormat == IMAGE_RGB)
    {
		RgbaPix *newImage = (RgbaPix *)malloc(sizeof(RgbaPix) *
											  tga->width * tga->height);
		RgbaPix *dest = newImage;
		RgbPix *src = (RgbPix *)tga->imageData;
		int x;

		if (!newImage)
			return false;

		for (x = 0; x < tga->height; x++)
		{
			int y;
			for (y = 0; y < tga->width; y++)
			{
				dest->r = src->r;
				dest->g = src->g;
				dest->b = src->b;
				dest->a = alphaValue;

				src++;
				dest++;
			}
		}

		free(tga->imageData);
		tga->imageData = (unsigned char *)newImage;
		tga->colorDepth = 32;
		/* tga->imageDataType = IMAGE_DATA_UNSIGNED_BYTE; */
		tga->imageDataFormat = IMAGE_RGBA;
		tga->imageSize = tga->width * tga->height * 4;

		return true;
    }

	return false;
}

bool TgaConvertRGBAToRGB(TargaImage *tga)
{
	if (tga->colorDepth == 32 && tga->imageDataFormat == IMAGE_RGBA)
    {
		RgbPix *newImage = (RgbPix *)malloc(sizeof(RgbPix) *
											tga->width * tga->height);
		RgbPix *dest = newImage;
		RgbaPix *src = (RgbaPix *)tga->imageData;
		int x;

		if (!newImage)
			return false;

		for (x = 0; x < tga->height; x++)
		{
			int y;
			for (y = 0; y < tga->width; y++)
			{
				dest->r = src->r;
				dest->g = src->g;
				dest->b = src->b;

				src++;
				dest++;
			}
		}

		free(tga->imageData);
		tga->imageData = (unsigned char *)newImage;
		tga->colorDepth = 24;
		/* tga->imageDataType = IMAGE_DATA_UNSIGNED_BYTE; */
		tga->imageDataFormat = IMAGE_RGB;
		tga->imageSize = tga->width * tga->height * 3;

		return true;
    }

	return false;
}

/* Works for RGBA, but alpha is ignored
   fastMode: Just copies first color channel,
   otherwise averages RGB values to find luminance */
bool TgaConvertRGBToLum(TargaImage *tga, bool fastMode)
{
	unsigned long newSize = tga->width * tga->height;
	unsigned char* newImage = (unsigned char *)malloc(newSize);
	unsigned int i = 0, j = 0;
	if (!newImage)
		return false;

	if (fastMode)
    {
		if (tga->colorDepth == 24)
		{
			while (i < tga->imageSize)
			{
				newImage[j] = tga->imageData[i];
				i += 3; j++;
			}
		}
		else /* if (tga->colorDepth == 32) */
		{
			while (i < tga->imageSize)
			{
				newImage[j] = tga->imageData[i];
				i += 4; j++;
			}
		}
    }
	else
    {
		if (tga->colorDepth == 24)
		{
			while (i < tga->imageSize)
			{
				short val = 0;
				val += tga->imageData[i++];
				val += tga->imageData[i++];
				val += tga->imageData[i++];
				newImage[j] = (unsigned char)(val / 3);
				j++;
			}
		}
		else /* if (tga->colorDepth == 32) WARNING: untested */
		{
			while (i < tga->imageSize)
			{
				short val = 0;
				val += tga->imageData[i++];
				val += tga->imageData[i++];
				val += tga->imageData[i++];
				newImage[j] = (unsigned char)(val / 3);
				i++; j++;
			}
		}
    }

	free(tga->imageData);
	tga->imageData = newImage;
	tga->colorDepth = 8;
	/* tga->imageDataType = IMAGE_DATA_UNSIGNED_BYTE; */
	tga->imageDataFormat = IMAGE_LUMINANCE;
	tga->imageSize = newSize;

	return true;
}

bool TgaConvertLumToRGB(TargaImage *tga)
{
	unsigned long newSize = tga->width * tga->height * 3;
	unsigned char* newImage = (unsigned char *)malloc(newSize);
	unsigned int i = 0, j = 0;
	if (!newImage)
		return false;

	while (i < tga->imageSize)
    {
		newImage[j++] = tga->imageData[i];
		newImage[j++] = tga->imageData[i];
		newImage[j++] = tga->imageData[i];

		i++;
    }

	free(tga->imageData);
	tga->imageData = newImage;
	tga->colorDepth = 24;
	/* tga->imageDataType = IMAGE_DATA_UNSIGNED_BYTE; */
	tga->imageDataFormat = IMAGE_RGB;
	tga->imageSize = newSize;

	return true;
}

bool TgaDeinterlace(TargaImage *tga)
{
	int lineWidth = tga->width * tga->colorDepth / 8;
	int i;
	tga->deinter1 = (unsigned char *)malloc(tga->imageSize);
	tga->deinter2 = (unsigned char *)malloc(tga->imageSize);
	if (!tga->deinter1 || !tga->deinter2)
		return false;

	/* Odd fields first -- skip last two for normal processing.  */
	/* Copy first one to top.  */
	memcpy(tga->deinter1, tga->imageData, lineWidth);
	/* Just average for fields in between.  */
	for (i = 1; i < tga->height - 1; i += 2)
    {
		int j;
		memcpy(tga->deinter1 + lineWidth * i,
			   tga->imageData + lineWidth * i, lineWidth);
		for (j = 0; j < lineWidth; j++)
			tga->deinter1[lineWidth * (i+1) + j] =
				(unsigned char)(((short)tga->deinter1[lineWidth * i + j] + (short)tga->imageData[lineWidth * (i+2) + j]) / 2);
    }
	/* Copy the last field.  */
	memcpy(tga->deinter1 + lineWidth * (tga->height - 1),
		   tga->imageData + lineWidth * (tga->height - 1), lineWidth);

	/* Do similar for even fields.  */
	for (i = 0; i < tga->height - 2; i += 2)
    {
		int j;
		memcpy(tga->deinter2 + lineWidth * i,
			   tga->imageData + lineWidth * i, lineWidth);
		for (j = 0; j < lineWidth; j++)
			tga->deinter2[lineWidth * (i+1) + j] =
				(unsigned char)(((short)tga->deinter2[lineWidth * i + j] + (short)tga->imageData[lineWidth * (i+2) + j]) / 2);
    }
	/* Copy last field twice.  */
	memcpy(tga->deinter2 + lineWidth * (tga->height - 2),
		   tga->imageData + lineWidth * (tga->height - 2), lineWidth);
	memcpy(tga->deinter2 + lineWidth * (tga->height - 1),
		   tga->imageData + lineWidth * (tga->height - 2), lineWidth);
	return true;
}
