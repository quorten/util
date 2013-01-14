/* An "exponential gradient" generator.

   This program generates a non-linear gradient that has its color
   change accelerate to a maximum rate then decelerate to no change by
   the end of the gradient.  */

#include "bool.h"
#include "exparray.h"
#include "targa.h"

typedef unsigned char uchar;
typedef unsigned short ushort;
EA_TYPE(uchar);
EA_TYPE(ushort);

void ColorInterleave(uchar_array* data);

int main()
{
	TargaImage image;
	uchar_array data;
	unsigned imageWidth = 0;

	EA_INIT(uchar, data, 16);
	ColorInterleave(&data);

	imageWidth = data.len / 4;
	/* Note: `data.d' is freed when CTargaImage is released.  */
	TgaInit(&image);
	TgaCreateImage(&image, imageWidth, 1, 32, data.d);
	TgaSave(&image, "ref.tga", false);
	TgaRelease(&image);
	return 0;
}

void ColorInterleave(uchar_array* data)
{
	
	unsigned char accel = 0;
	unsigned short curntValue = 0;
	unsigned char cellWidth; /* pixel width for each acceleration stage */
	ushort_array accelList;

	unsigned char color = 0;
	unsigned i = 1;
	/* Incrementing/Decrementing our number of similar pixels */
	bool bGoingForward = true;

	/* Build our acceleration list (it is not very big, only for
	   debugging purposes).  We only need to know the highest
	   acceleration.  */
	EA_INIT(ushort, accelList, 16);
	while (curntValue <= 127)
	{
		accel += 1;
		curntValue += accel;
		EA_APPEND(ushort, accelList, curntValue);
	}
	cellWidth = accel;
	while (accel > 1)
	{
		accel -= 1;
		curntValue += accel;
		EA_APPEND(ushort, accelList, curntValue);
	}
	EA_DESTROY(ushort, accelList);

	/* Fill the data.  */
	while (true)
	{
		/* Variables governing handling of remainder pixels -- see
		   "Distribute remainder pixels".  */
		unsigned char remainder;
		int subtractingVal = 0;
		int steppingVal = 1;
		/* Number of pixels with same color */
		unsigned char solidFill = cellWidth / i;
		unsigned j = 0;
		remainder = cellWidth % i;

		/* `remainder' may be subtracted so divisions with remainders
		   work.  */
		while (j < /*(unsigned)(*/cellWidth/* - remainder)*/)
		{
			unsigned k;
			for (k = 0; k < solidFill; k++)
			{
				EA_APPEND(uchar, *data, 255); /* Blue */
				EA_APPEND(uchar, *data, 255); /* Green */
				EA_APPEND(uchar, *data, 255); /* Red */
				EA_APPEND(uchar, *data, color); /* Alpha */
				j++;
			}
			/* Distribute the remainder pixels.  */
			/* Info: 7 / 9 remainder pixels per every quotient pixel */
			if (((remainder * steppingVal) / i) - subtractingVal >= 1)
			{
				subtractingVal++;
				EA_APPEND(uchar, *data, 255); /* Blue */
				EA_APPEND(uchar, *data, 255); /* Green */
				EA_APPEND(uchar, *data, 255); /* Red */
				EA_APPEND(uchar, *data, color); /* Alpha */
				j++;
			}
			color++;
			steppingVal++;
		}
		/* for (unsigned j = 0; j < remainder; j++)
		{
			EA_APPEND(uchar, *data, 255); /\* Blue *\/
			EA_APPEND(uchar, *data, 255); /\* Green *\/
			EA_APPEND(uchar, *data, 255); /\* Red *\/
			EA_APPEND(uchar, *data, color); /\* Alpha *\/
		} */
		if (bGoingForward == true)
			i++;
		if (bGoingForward == false)
		{
			i--;
			if (i == 0)
				break;
		}
		if (i == 17)
		{
			i -= 2;
			bGoingForward = false;
		}
	}
}

/*
Some info for better algorithim:
16 / 9
quotient: 1
remainder: 7

cell size of 16 pixels
1 pixel groupings of same color
9 different colors in cell
7 pixels added at end to fill cell width
-OR-
7 / 9 remainder pixels per every quotient pixel
*/
