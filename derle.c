/* Decode an RLE sequence.  This can also double de-RLE the detail
   layer bitmap from an Orly picture.  This can also extract the tBMPs
   from a sprite.  */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "utstdint.h"
#include "utendian.h"

/* NOTE: MHK files are in big endian.  */
#ifdef LITTLE_ENDIAN
#define ntohs(s) (((s & 0xff) << 8) | ((s & 0xff00) >> 8))
#define ntohl(s) (((s & 0xff) << 24) | ((s & 0xff000000) >> 24) | \
				  ((s & 0xff00) << 8) | ((s & 0xff0000) >> 8))
#define c_ntohs(s) s = ntohs(s)
#define c_ntohl(s) s = ntohl(s)
#else
#define ntohs(s) (s)
#define ntohl(s) (s)
#define c_ntohs(s)
#define c_ntohl(s)
#endif

/* NOTE: For some odd reason, sprites store their fields in little
   endian, whereas Mohawk archives and bitmaps store their fields in
   big endian.  It's probably because sprites were introduced later,
   and the programmer who was extending the software on their Win32
   computer didn't know any better...  */
#ifdef LITTLE_ENDIAN
#define stohs(s) (s)
#define stohl(s) (s)
#define c_stohs(s)
#define c_stohl(s)
#else
#define stohs(s) (((s & 0xff) << 8) | ((s & 0xff00) >> 8))
#define stohl(s) (((s & 0xff) << 24) | ((s & 0xff000000) >> 24) | \
				  ((s & 0xff00) << 8) | ((s & 0xff0000) >> 8))
#define c_stohs(s) s = ntohs(s)
#define c_stohl(s) s = ntohl(s)
#endif

#ifndef __GNUC__
#define __attribute__(params)
#endif

struct bitmap_header_tag
{
	UTuint16 width;
	UTuint16 height;
	UTuint16 bytes_per_row;
	union
	{
		UTuint16 compression;
		/* NOTE: Verify that your compiler packs the following to a UTint16: */
		struct
		{
			UTuint16 bpp : 3;
			UTuint16 has_palette : 1;
			UTuint16 second_cmp : 4;
			UTuint16 prim_cmp : 4;
		};
	};
} __attribute__((packed));
typedef struct bitmap_header_tag bitmap_header;

struct sprite_header_tag
{
	UTuint16 num_entries;
	UTuint32 offset_to_entries;
} __attribute__((packed));
typedef struct sprite_header_tag sprite_header;

struct se_head_tag
{
	/* Sprites are defined as a series of tBMP pieces that are
	   composed to form the final sprite.  */
	UTuint16 num_pieces;
	/* These define the bounding rectangle of the sprite.  */
	UTint16 left;
	UTint16 right;
	UTint16 top;
	UTint16 bottom;
} __attribute__((packed));
typedef struct se_head_tag se_head;

struct se_tbmp_piece_tag
{
	UTint16 x; /* X coordinate to place piece */
	UTint16 y; /* Y coordinate to place piece */
	UTuint32 offset;
} __attribute__((packed));
typedef struct se_tbmp_piece_tag se_tbmp_piece;

struct sprite_entry_data_tag
{
	se_head head;
	se_tbmp_piece *pieces;
} __attribute__((packed));
typedef struct sprite_entry_data_tag sprite_entry_data;

int double_duty = 0;
int allow_overflow = 0;
int sprite_head = 0;
int doodle_sprite = 0;

int derle_bitmap(FILE *fin, FILE *fout);
int delz_bitmap(FILE *fin, FILE *fout);

int main(int argc, char *argv[])
{
	int retval;
	FILE *fin;
	const char *filename;
	sprite_header fsprite_header;
	sprite_entry_data *fsprite_entries = NULL;

	if (argc == 3)
	{
		filename = argv[2];
		if (!strcmp(argv[1], "-d"))
			double_duty = 1;
		if (!strcmp(argv[1], "-do"))
		{
			double_duty = 1;
			allow_overflow = 1;
		}
		if (!strcmp(argv[1], "-ds"))
		{
			double_duty = 1;
			sprite_head = 1;
		}
		if (!strcmp(argv[1], "-dds"))
		{
			doodle_sprite = 1;
			double_duty = 1;
			sprite_head = 1;
		}
	}
	else if (argc == 2)
		filename = argv[1];
	else
		return 1;

	fin = fopen(filename, "rb");
	if (fin == NULL)
		return 1;

	if (sprite_head)
	{
		unsigned i;
		int file_error = 0;

		if (fread(&fsprite_header, sizeof(sprite_header), 1, fin) != 1)
		{
			fputs("error: could not read sprite header.\n", stderr);
			retval = 1; goto cleanup;
		}
		c_stohs(fsprite_header.num_entries);
		c_stohs(fsprite_header.offset_to_entries);
		fseek(fin, fsprite_header.offset_to_entries, SEEK_SET);

		printf("Sprite information:\nNumber of entries: %d\n",
			   fsprite_header.num_entries);
		printf("Offset to entries: 0x%08x\n",
			   fsprite_header.offset_to_entries);

		fsprite_entries = (sprite_entry_data*)malloc(
			sizeof(sprite_entry_data) * fsprite_header.num_entries);
		if (fsprite_entries == NULL)
		{
			fputs("error: could not allocate memory.\n", stderr);
			retval = 1; goto cleanup;
		}
		fputs("", stderr); /* ???  Fixes an obscure Windows bug.  */
		for (i = 0; i < fsprite_header.num_entries; i++)
		{
			size_t read_size;
			unsigned num_pieces;
			unsigned j;
			if (fread(&fsprite_entries[i].head, sizeof(se_head), 1, fin) != 1)
			{
				fputs("error: could not read sprite header.\n", stderr);
				retval = 1; goto cleanup;
			}
			c_stohs(fsprite_entries[i].head.num_pieces);
			c_stohs(fsprite_entries[i].head.left);
			c_stohs(fsprite_entries[i].head.right);
			c_stohs(fsprite_entries[i].head.top);
			c_stohs(fsprite_entries[i].head.bottom);

			num_pieces = fsprite_entries[i].head.num_pieces;
			if (num_pieces > 10)
			{
				fprintf(stderr,
						"warning: number of pieces is %u at index %d\n",
						(unsigned)num_pieces, i);
				fputs("Assuming this a bad entry and marking this "
					  "as the end of the header.\n", stderr);
				fsprite_header.num_entries = i;
				break;
			}
			fsprite_entries[i].pieces = (se_tbmp_piece*)
				malloc(sizeof(se_tbmp_piece) * num_pieces);
			if (fsprite_entries[i].pieces == NULL)
			{
				fputs("error: could not allocate memory.\n", stderr);
				retval = 1; goto cleanup;
			}
			if (fread(fsprite_entries[i].pieces, sizeof(se_tbmp_piece),
					  num_pieces, fin) != num_pieces)
			{
				fputs("error: could not read sprite header.\n", stderr);
				retval = 1; goto cleanup;
			}

			for (j = 0; j < num_pieces; j++)
			{
				c_stohs(fsprite_entries[i].pieces[j].x);
				c_stohs(fsprite_entries[i].pieces[j].y);
				c_stohl(fsprite_entries[i].pieces[j].offset);
			}
		}

		for (i = 0; i < fsprite_header.num_entries; i++)
		{
			bitmap_header tbmp_head;
			int org_x, org_y;
			unsigned padding;
			UTuint8 *tbmp_bits;
			unsigned j;

			printf("\nSprite %u information:\n", i);
			printf("Number of pieces: %hu\n",
				   fsprite_entries[i].head.num_pieces);
			printf("Left: %hd\n", fsprite_entries[i].head.left);
			printf("Right: %hd\n", fsprite_entries[i].head.right);
			printf("Top: %hd\n", fsprite_entries[i].head.top);
			printf("Bottom: %hd\n", fsprite_entries[i].head.bottom);

			/* Create a tBMP that will hold the result.  */
			if (doodle_sprite)
			{ org_x = -9; org_y = -183; }
			else
			{
				org_x = fsprite_entries[i].head.left;
				org_y = fsprite_entries[i].head.top;
			}
			tbmp_head.width = fsprite_entries[i].head.right + 1 - org_x;
			tbmp_head.height = fsprite_entries[i].head.bottom + 1 - org_y;
			padding = 4 - (tbmp_head.width % 4);
			if (padding == 4) padding = 0;
			tbmp_head.bytes_per_row = tbmp_head.width + padding;
			tbmp_head.compression = 0x2;
			tbmp_bits = (UTuint8*)malloc(tbmp_head.bytes_per_row *
										 tbmp_head.height);
			if (tbmp_bits == NULL)
			{
				fputs("error: could not allocate memory.\n", stderr);
				retval = 1; goto cleanup;
			}
			memset(tbmp_bits, 0xff,
				   tbmp_head.bytes_per_row * tbmp_head.height);

			for (j = 0; j < fsprite_entries[i].head.num_pieces; j++)
			{
				char filename[5+10+1];
				FILE *fout;
				bitmap_header piece_head;
				unsigned row;
				printf("\ntBMP piece %u:\n", j);
				printf("Bitmap position: (%hd, %hd)\n",
					   fsprite_entries[i].pieces[j].x,
					   fsprite_entries[i].pieces[j].y);
				printf("Offset: 0x%08x\n",
					   fsprite_entries[i].pieces[j].offset);

				fseek(fin, fsprite_entries[i].pieces[j].offset, SEEK_SET);

				sprintf(filename, "piece%d", j);
				fout = fopen(filename, "wb+");
				if (fout == NULL)
				{
					fprintf(stderr, "error: could not open file: %s\n",
							filename);
					file_error = 1;
					continue;
				}
				retval = derle_bitmap(fin, fout);
				if (retval != 0)
				{ fclose(fout); goto cleanup; }

				/* Read the uncompressed tBMP and blt it into
				   position.  */
				fseek(fout, 0, SEEK_SET);
				fread(&piece_head, sizeof(bitmap_header), 1, fout);
				for (row = 0; row < piece_head.height; row++)
				{
					int c;
					unsigned col;
					for (col = 0; col < piece_head.bytes_per_row; col++)
					{
						int x, y;
						unsigned addr;
						c = getc(fout);
						if (c == EOF)
							break;
						if (col >= piece_head.width)
							continue;
						x = -org_x + fsprite_entries[i].pieces[j].x + (int)col;
						y = -org_y + fsprite_entries[i].pieces[j].y + (int)row;
						if (x < 0 || y < 0 ||
							x >= tbmp_head.width || y >= tbmp_head.height)
							continue;
						addr = y * tbmp_head.bytes_per_row + x;
						tbmp_bits[addr] = (UTuint8)c;
					}
					if (c == EOF)
						break;
				}
				fclose(fout);
				remove(filename);
			}

			{ /* Save the composite tBMP.  */
				char filename[5+10+1];
				FILE *fout;
				sprintf(filename, "gdata%d", i + 1);
				fout = fopen(filename, "wb");
				if (fout == NULL)
				{
					fprintf(stderr, "error: could not open file: %s\n",
							filename);
					file_error = 1;
					continue;
				}
				fwrite(&tbmp_head, sizeof(bitmap_header), 1, fout);
				fwrite(tbmp_bits,
					   tbmp_head.bytes_per_row * tbmp_head.height, 1, fout);
				fclose(fout);
				free(tbmp_bits);
			}
		}
		if (!file_error)
			retval = 0;
		else
			retval = 1;
	}
	else
	{
		FILE *fout = fopen("gdata", "wb");
		if (fout == NULL)
			return 1;
		retval = derle_bitmap(fin, fout);
		fclose(fout);
	}

cleanup:
	/* if (fsprite_entries != NULL)
	{
		unsigned i;
		for (i = 0; i < fsprite_header.num_entries; i++)
			free(fsprite_entries[i].data);
	} */
	free(fsprite_entries);
	fclose(fin);

	return retval;
}

int derle_bitmap(FILE *fin, FILE *fout)
{
	bitmap_header head;
	int bpp;
	const char *secondCmpDesc;
	const char *primCmpDesc;
	int delz = 0;

	unsigned padding;
	int c;
	int y_pos = 0;

	if (fread(&head, sizeof(bitmap_header), 1, fin) != 1)
	{
		fputs("error: could not read the bitmap header.\n", stderr);
		return 1;
	}
	c_ntohs(head.width);
	c_ntohs(head.height);
	c_ntohs(head.bytes_per_row);
	c_ntohs(head.compression);

	switch (head.bpp)
	{
	case 0: bpp = 1; break;
	case 1: bpp = 4; break;
    case 2: bpp = 8; break;
	case 3: bpp = 16; break;
	case 4: bpp = 24; break;
	default: bpp = 0; break;
	}

	switch (head.second_cmp)
	{
	case 0: secondCmpDesc = "None"; break;
	case 1: secondCmpDesc = "RLE8"; break;
	case 3: secondCmpDesc = "RLE unknown"; break;
	default: secondCmpDesc = "unknown"; break;
	}

	switch (head.prim_cmp)
	{
	case 0: primCmpDesc = "None"; break;
	case 1: primCmpDesc = "LZ"; break;
	case 2: primCmpDesc = "LZ unknown"; break;
	case 4: primCmpDesc = "Riven"; break;
	default: primCmpDesc = "unknown"; break;
	}

	puts("\nBitmap information:");
	printf("width: %d; height: %d\n", (int)head.width, (int)head.height);
	printf("bytes per row: %d\ncompression: 0x%x\n",
		   (int)head.bytes_per_row, (int)head.compression);

	printf("bits per pixel: %d\n", bpp);
	printf("has palette? %d\n", (int)head.has_palette);
	printf("secondary compression: %s\n", secondCmpDesc);
	printf("primary compression: %s\n", primCmpDesc);

	padding = 4 - (head.width % 4);
	if (padding == 4) padding = 0;

	if (head.prim_cmp == 1) /* LZ */
	{
		FILE *lzout = fopen("lztemp", "wb");
		int retval;
		/* We'll uncompress the LZ compression, write the intermediate
		   results to a temporary file, then continue with the
		   secondary compression as normal.  */
		fputs("Decompressing LZ primary compression...\n", stdout);
		if (lzout == NULL)
		{
			fputs("error: could not open LZ temp file for writing.\n", stderr);
			return 1;
		}

		retval = delz_bitmap(fin, lzout);
		if (retval != 0)
			return retval;
		fclose(lzout);

		fin = fopen("lztemp", "rb");
		if (fin == NULL)
		{
			fputs("error: could not open LZ temp file for reading.\n", stderr);
			return 1;
		}
		fputs("Done decompressing LZ layer.\n", stdout);
		head.prim_cmp = 0;
		delz = 1;
	}

	if (head.compression == 0x2)
	{
		unsigned old_bytes_per_row = (unsigned)head.bytes_per_row;
		unsigned i;
		fputs("No compression found, outputting data unchanged.\n", stdout);
		head.bytes_per_row = head.width + padding;
		fwrite(&head, sizeof(bitmap_header), 1, fout);
		for (i = 0; i < head.height; i++)
		{
			unsigned j;
			for (j = 0; j < head.width; j++)
			{
				int c = getc(fin);
				if (c == EOF)
				{
					fputs("error: input ended prematurely.\n", stderr);
					if (delz) fclose(fin);
					return 1;
				}
				if (c == 0x00) /* ad-hoc transparency */
					c = 0xff;
				putc(c, fout);
			}
			for (; j < old_bytes_per_row; j++)
			{
				int c = getc(fin);
			}
			for (j = 0; j < padding; j++)
				putc('\0', fout);
		}
		if (delz) fclose(fin);
		return 0;
	}

	if (head.compression != 0x12)
	{
		fputs("error: cannot handle this kind of compression, quitting.\n",
			  stderr);
		if (delz) fclose(fin);
		return 1;
	}

	/* Write out the header for the uncompressed bitmap.  */
	head.compression = 0x2;
	head.bytes_per_row = head.width + padding;
	fwrite(&head, sizeof(bitmap_header), 1, fout);

	do
	{
		int x_pos = 0;
		int id;
		int repeat;
		int run_length;

		c = getc(fin);
		if (c == EOF)
			break;
		id = c << 8;

		c = getc(fin);
		if (c == EOF)
			break;
		id = id | c;

		repeat = id & 0x8000;
		run_length = (id & 0x7fff);

		/* { /\* Horizontally center the scan line.  *\/
			unsigned i;
			for (i = 0; i < (head.width - run_length) / 2; i++)
			{
				putc(0xff, fout); /\* ad-hoc transparency *\/
				x_pos++;
			}
		} */

		if (repeat)
		{
			int color = getc(fin);
			if (color == EOF)
				break;
			printf("Row %d: Found a repeat.\n", y_pos);
			while (run_length > 0 && x_pos < head.width)
			{
				putc(color, fout);
				x_pos++;
				run_length--;
			}
		}
		else
		{
			while (run_length > 0)
			{
				if (!double_duty)
				{
					int color = getc(fin);
					if (color == EOF)
						break;
					if (x_pos < head.width)
					{
						putc(color, fout);
						x_pos++;
					}
					run_length--;
				}
				else
				{
					int len2;
					int repeat2;

					len2 = getc(fin);
					if (len2 == EOF)
						break;
					run_length--;
					if (run_length == 0)
						break;
					repeat2 = len2 & 0x80;
					len2 = (len2 & 0x7f) + 1;

					if (repeat2)
					{
						int color = getc(fin);
						if (color == EOF)
							break;
						if (color == 0x00) /* ad-hoc transparency */
							color = 0xff;

						while (x_pos < head.width && len2 > 0)
						{
							putc(color, fout);
							x_pos++;
							len2--;
						}
						run_length--;
					}
					else
					{
						while (run_length > 0 && len2 > 0)
						{
							int color = getc(fin);
							if (color == EOF)
								break;
							if (color == 0x00) /* ad-hoc transparency */
								color = 0xff;

							if (x_pos < head.width)
							{
								putc(color, fout);
								x_pos++;
							}
							len2--;
							run_length--;
						}
					}
				}
			}
		}

		while (x_pos < head.width)
		{
			putc(0xff, fout); /* ad-hoc transparency */
			x_pos++;
		}
		while (x_pos < head.bytes_per_row)
		{
			putc('\0', fout);
			x_pos++;
		}
		y_pos++;

		if (!allow_overflow && y_pos == head.height)
		{
			/* fputs("error: image data overflow\n", stderr); */
			break;
		}
	} while (c != EOF);

	if (delz) fclose(fin);
	return 0;
}

int delz_bitmap(FILE *fin, FILE *fout)
{
	struct {
		UTuint32 uncmpr_size;
		UTuint32 cmpr_size;
		UTuint16 dictionary_size;
	} __attribute__((packed)) lz_header;
	UTuint32 cmpr_pos = 0;
	UTuint32 uncmpr_pos = 0;
	UTuint16 ring_pos = 0;
	UTuint8 ring[1024];

	if (fread(&lz_header, sizeof(lz_header), 1, fin) != 1)
	{
		fputs("error: could not read the lz header.\n", stderr);
		return 1;
	}
	c_ntohl(lz_header.uncmpr_size);
	c_ntohl(lz_header.cmpr_size);
	c_ntohs(lz_header.dictionary_size);

	printf("Uncompressed size: %d\nCompressed size: %d\n",
		   lz_header.uncmpr_size, lz_header.cmpr_size);

	if (lz_header.dictionary_size != 1024)
	{
		fprintf(stderr, "error: invalid ring buffer size: %d\n",
				(int)lz_header.dictionary_size);
		return 1;
	}

	memset(ring, 0, 1024);

	while (cmpr_pos < lz_header.cmpr_size)
	{
		/* Read the decoder byte.  */
		unsigned bit_pos = 0;
		UTuint8 decoder;
		int c = getc(fin);
		if (c == EOF)
			break;
		cmpr_pos++;
		decoder = (UTuint8)c;
		while (bit_pos < 8 &&
			   cmpr_pos < lz_header.cmpr_size)
		{
			if (decoder & (1 << bit_pos)) /* absolute byte */
			{
				c = getc(fin);
				if (c == EOF)
					break;
				cmpr_pos++;
				putc(c, fout);
				uncmpr_pos++;
				ring[ring_pos++] = (UTuint8)c;
				ring_pos %= 1024;
			}
			else /* LZ pair */
			{
				unsigned length;
				unsigned offset;
				int c2;
				c = getc(fin);
				if (c == EOF)
					break;
				cmpr_pos++;
				c2 = getc(fin);
				if (c2 == EOF)
					break;
				cmpr_pos++;
				length = (((unsigned)c & 0xfc) >> 2) + 3;
				offset = (((c & 0x03) << 8) | c2) + 0x42;
				offset %= 1024;

				while (length > 0)
				{
					putc(ring[offset], fout);
					uncmpr_pos++;
					ring[ring_pos++] = ring[offset];
					ring_pos %= 1024;
					offset = (offset + 1) % 1024;
					length--;
				}
			}
			bit_pos++;
		}
	}

	if (uncmpr_pos != lz_header.uncmpr_size)
	{
		fputs("warning: uncompressed size is not what was expected.\n",
			  stdout);
	}

	return 0;
}
