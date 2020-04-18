#include <stdio.h>
#include <stdlib.h>
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
	UTuint8 blue;
	UTuint8 *curPix;
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

/* Note: TGA pixels are stored in BGR order.  It is up to the user to
   swap to RGB order if they need the image channels in that
   order.  */
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
	tga->imageData = (UTuint8 *)malloc(tga->imageSize);
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
		UTuint8 id;
		UTuint8 length;
		UTuint8 luma;
		RgbaPix color = { 0, 0, 0, 0 };
		unsigned int i = 0;

		while (i < tga->imageSize)
		{
			id = fgetc(fp);

			/* See if this is run length data.  */
			if (id >= 128)
			{
				/* Find the run length.  */
				length = (UTuint8)(id - 127);

				/* The next few bytes are the repeated values.  */
				if (tgaHeader.imageTypeCode == TGA_GRAYSCALE_RLE)
					luma = (UTuint8)fgetc(fp);
				else
				{
					color.b = (UTuint8)fgetc(fp);
					color.g = (UTuint8)fgetc(fp);
					color.r = (UTuint8)fgetc(fp);
					if (bytesPerPix == 4)
						color.a = (UTuint8)fgetc(fp);
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
					length--;
				}
			}
			else
			{
				/* Get the number of non RLE pixels.  */
				length = (UTuint8)(id + 1);

				while (length > 0)
				{
					if (tgaHeader.imageTypeCode == TGA_GRAYSCALE_RLE)
					{
						tga->imageData[i++] =
							(UTuint8)fgetc(fp);
					}
					else
					{
						tga->imageData[i++] =
							(UTuint8)fgetc(fp); /* Blue */
						tga->imageData[i++] =
							(UTuint8)fgetc(fp); /* Green */
						tga->imageData[i++] =
							(UTuint8)fgetc(fp); /* Red */
						if (bytesPerPix == 4)
							tga->imageData[i++] =
								(UTuint8)fgetc(fp); /* Alpha */
					}

					length--;
				}
			}
		}
	}

	fclose(fp);

	if ((tgaHeader.imageDesc & TOP_LEFT) == TOP_LEFT)
		TgaFlipVertical(tga);

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

	lineWidth = tga->width * (tga->colorDepth / 8);

	tmpBits = (RgbaPix *)malloc(sizeof(RgbaPix) * tga->width);
	if (!tmpBits)
		return false;

	top = (RgbaPix *)tga->imageData;
	bottom = (RgbaPix *)(tga->imageData + lineWidth * (tga->height - 1));

	for (i = 0; i < (tga->height / 2); i++)
	{
		memcpy(tmpBits, top, lineWidth);
		memcpy(top, bottom, lineWidth);
		memcpy(bottom, tmpBits, lineWidth);

		top = (RgbaPix *)((UTuint8 *)top + lineWidth);
		bottom = (RgbaPix * )((UTuint8 *)bottom - lineWidth);
	}

	free(tmpBits); tmpBits = NULL;

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
	UTuint8 *pRLEImage;
	unsigned int rleSize = 0;
	FILE* fp;

	header.idLength        = idStrLen;
	header.colorMapType    = 0;
	/* Perform color stuff after other header values are set.  */
	header.colorMapSpec.firstEntry = 0;
	header.colorMapSpec.numEntries = 0;
	header.colorMapSpec.bitsPerEntry = 0;
	header.xOrigin         = 0;
	header.yOrigin         = 0;
	header.width           = tga->width;
	header.height          = tga->height;
	header.bpp             = tga->colorDepth;
	header.imageDesc       = BOTTOM_LEFT;

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
	if (RLE)
	{
		/* Perform RLE data compression.  */
		int bytesPerPix = tga->colorDepth / 8;
		unsigned int i = 0; /* Source iterator */
		unsigned int scanlEndIdx = tga->width * bytesPerPix;
		header.imageTypeCode |= 0x8; /* Set the RLE bit */
		/* Note: We must account for the worst case expansion
		   factor when allocating the buffer.  */
		pRLEImage = (UTuint8 *)malloc(tga->imageSize +
									(tga->imageSize / bytesPerPix) / 128);
		if (pRLEImage == NULL)
			return false;

		/* Note: RLE packets must not cross scan line boundaries for
		   TGA v2.0, but TGA v1.0 images may have scan lines cross
		   line boundaries.  */
		while (i < tga->imageSize /* scanlEndIdx */)
		{
			UTuint8 length = 1; /* id length for RLE */
			/* Get first reference color.  */
			UTuint8 *bufStart = &tga->imageData[i];
			const UTuint8 bufSize = 3;
			i += bytesPerPix;

			/* First try to fill up the buffer with RLE pixels.  */
			while(&tga->imageData[i] - bufStart < bufSize * bytesPerPix &&
				  i < tga->imageSize /* scanlEndIdx */)
			{
				if (memcmp(bufStart, &tga->imageData[i],
						   bytesPerPix) != 0)
					break;
				length++;
				i += bytesPerPix;
			}

			/* Note: The minimum length for an RLE run depends on how
			   many bytes are wasted encoding a non-RLE chunk.  The
			   idea is that whenever one RLE chunk is encoded, it must
			   save at least as many bytes as are wasted when
			   specifying a non-RLE chunk.  In this case, one byte is
			   wasted specifying a non-RLE chunk, so at least one byte
			   must be saved (length >= 3) for an encoded RLE
			   chunk.  */
			if (length >= bufSize)
			{
				/* Look max 128 pixels into the file.  */
				while (length < 128 && i < tga->imageSize /* scanlEndIdx */)
				{
					/* Break if a pixel differs.  */
					if (memcmp(bufStart, &tga->imageData[i],
							   bytesPerPix) != 0)
						break;
					length++;
					bufStart += bytesPerPix; /* Technically unnecessary */
					i += bytesPerPix;
				}
				/* This is the end of an RLE chunk.  */
				pRLEImage[rleSize++] = length + 127;
				memcpy(&pRLEImage[rleSize], bufStart, bytesPerPix);
				rleSize += bytesPerPix;
			}
			else
			{
				/* Initialize the id and save its address.  */
				UTuint8 *id = &pRLEImage[rleSize++];
				*id = (UTuint8)-1;

				/* Empty the first pixel (known not to be part of an
				   RLE sequence) from the buffer.  */
				(*id)++;
				memcpy(&pRLEImage[rleSize], bufStart, bytesPerPix);
				rleSize += bytesPerPix;
				bufStart += bytesPerPix;

				/* Fill up the buffer with pixels.  */
				while(&tga->imageData[i] - bufStart <
					  bufSize * bytesPerPix &&
					  i < tga->imageSize /* scanlEndIdx */)
					i += bytesPerPix;

				/* Loop until encounterance of RLE data or 128 pixel
				   limit.  The buffer is used as a FIFO queue.  */
				while (*id < 127 && i < tga->imageSize /* scanlEndIdx */)
				{
					bool rleFound = true;
					unsigned int j;
					for (j = bytesPerPix; j < bufSize * bytesPerPix;
						 j += bytesPerPix)
					{
						if (memcmp(bufStart, bufStart + j,
								   bytesPerPix) != 0)
						{
							rleFound = false;
							break;
						}
					}
					if (rleFound)
					{
						/* This is the beginning of an RLE chunk, go
						   back three pixels.  */
						i -= bufSize * bytesPerPix;
						break;
					}

					/* Store non-RLE data.  */
					(*id)++;
					memcpy(&pRLEImage[rleSize], bufStart, bytesPerPix);
					rleSize += bytesPerPix;
					bufStart += bytesPerPix;
					i += bytesPerPix;
				}

				/* The buffer should be empty before starting a new
				   cycle.  If it is not, then we are at the end of the
				   image.  */
				while (*id < 127 && &tga->imageData[i] - bufStart > 0)
				{
					/* Store non-RLE data.  */
					(*id)++;
					memcpy(&pRLEImage[rleSize], bufStart, bytesPerPix);
					rleSize += bytesPerPix;
					bufStart += bytesPerPix;
				}
				if (*id == 127)
				{
					/* We must back off so that we can store the
					   subsequent pixels in a different chunk.  */
					i = bufStart - tga->imageData;
				}
			}

			/* Advance to the next scan line.  */
			/* scanlEndIdx += tga->width * bytesPerPix;
			if (scanlEndIdx > tga->imageSize)
				scanlEndIdx = tga->imageSize; */
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

void TgaCreateImage(TargaImage *tga, UTuint16 width,
					UTuint16 height, UTuint8 bpp,
					UTuint8* data)
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

bool TgaConvertRGBToRGBA(TargaImage *tga, UTuint8 alphaValue)
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
		tga->imageData = (UTuint8 *)newImage;
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
		tga->imageData = (UTuint8 *)newImage;
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
	UTuint32 newSize = tga->width * tga->height;
	UTuint8* newImage = (UTuint8 *)malloc(newSize);
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
				UTint16 val = 0;
				val += tga->imageData[i++];
				val += tga->imageData[i++];
				val += tga->imageData[i++];
				newImage[j] = (UTuint8)(val / 3);
				j++;
			}
		}
		else /* if (tga->colorDepth == 32) WARNING: untested */
		{
			while (i < tga->imageSize)
			{
				UTint16 val = 0;
				val += tga->imageData[i++];
				val += tga->imageData[i++];
				val += tga->imageData[i++];
				newImage[j] = (UTuint8)(val / 3);
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
	UTuint32 newSize = tga->width * tga->height * 3;
	UTuint8* newImage = (UTuint8 *)malloc(newSize);
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
	if (tga->height % 2 != 0)
		return false;
	tga->deinter1 = (UTuint8 *)malloc(tga->imageSize);
	tga->deinter2 = (UTuint8 *)malloc(tga->imageSize);
	if (tga->deinter1 == NULL || tga->deinter2 == NULL)
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
			tga->deinter1[lineWidth * (i+1) + j] = (UTuint8)
				(((UTint16)tga->deinter1[lineWidth * i + j] +
				  (UTint16)tga->imageData[lineWidth * (i+2) + j]) / 2);
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
			tga->deinter2[lineWidth * (i+1) + j] = (UTuint8)
				(((UTint16)tga->deinter2[lineWidth * i + j] +
				  (UTint16)tga->imageData[lineWidth * (i+2) + j]) / 2);
	}
	/* Copy last field twice.  */
	memcpy(tga->deinter2 + lineWidth * (tga->height - 2),
		   tga->imageData + lineWidth * (tga->height - 2), lineWidth);
	memcpy(tga->deinter2 + lineWidth * (tga->height - 1),
		   tga->imageData + lineWidth * (tga->height - 2), lineWidth);
	return true;
}
