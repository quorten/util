/* Convert an Orly tWAV to a RIFF WAVE.  */

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

struct twav_head_tag
{
	char mhwk_magic[4]; /* MHWK */
	UTuint32 size; /* effective resource size */
	char wave_magic[4]; /* WAVE */
} __attribute__((packed));
typedef struct twav_head_tag twav_head;

struct twav_data_hdr_tag
{
	char chunk_type[4]; /* Data */
	UTuint32 chunk_size;
	UTuint16 sample_rate;
	UTuint32 sample_count;
	UTuint8 bits_per_sample;
	UTuint8 channels;
	UTuint16 encoding;
	UTuint16 loop;
	UTuint16 loop_start;
	UTuint16 loop_end;
} __attribute__((packed));
typedef struct twav_data_hdr_tag twav_data_hdr;

int main(int argc, char *argv[])
{
	FILE *fin, *fout;
	twav_head main_head;
	twav_data_hdr data_head;
	UTuint8 *samples;
	UTuint32 samples_size;
	unsigned new_rate = 0;

	struct {
		char chunk_id[4];
		UTuint32 chunk_size;
		char format[4];
	} __attribute__((packed)) riff_head;
	struct {
		char chunk_id[4];
		UTuint32 chunk_size;
		UTuint16 audio_format;
		UTuint16 num_channels;
		UTuint32 sample_rate;
		UTuint32 byte_rate;
		UTuint16 block_align;
		UTuint16 bits_per_sample;
	} __attribute__((packed)) fmt_chunk;
	struct {
		char chunk_id[4];
		UTuint32 chunk_size;
	} __attribute__((packed)) data_chunk_head;

	if (argc == 4)
	{
		new_rate = atoi(argv[1]);
		argc--; argv++;
	}

	if (argc != 3)
		return 1;
	fin = fopen(argv[1], "rb");
	fout = fopen(argv[2], "wb");
	if (fin == NULL || fout == NULL)
		return 1;

	fread(&main_head, sizeof(twav_head), 1, fin);
	fread(&data_head, sizeof(twav_data_hdr), 1, fin);

	c_ntohl(main_head.size);
	c_ntohl(data_head.chunk_size);
	c_ntohs(data_head.sample_rate);
	c_ntohl(data_head.sample_count);
	c_ntohs(data_head.encoding);
	c_ntohs(data_head.loop);
	c_ntohs(data_head.loop_start);
	c_ntohs(data_head.loop_end);

	if (data_head.encoding != 0)
	{
		fprintf(stderr,
				"error: cannot handle this kind of encoding: %hu\n",
				data_head.encoding);
		return 1;
	}

	samples_size = ((UTuint32)data_head.bits_per_sample *
					data_head.channels * data_head.sample_count);
	{
		UTuint32 remainder = samples_size & 0x7;
		samples_size = samples_size >> 3; /* / 8 */
		if (remainder)
			samples_size++;
	}
	samples = (UTuint8*)malloc(samples_size);
	if (samples == NULL)
		return 1;
	fread(samples, samples_size, 1, fin);

	/* NOTE: For sounds with a low sample rate, we need to use this
	   special "cheap" resampler in order to get the sound to sound
	   "correct", as would be played by the Mohawk engine.  */
	if (new_rate != 0)
	{
		unsigned dividend = data_head.sample_rate;
		unsigned quotient = new_rate / dividend;
		unsigned remainder = new_rate % dividend;
		/* Initialize rem_accum so that when the actual accumulated
		   remainders is greater than or equal to 1/2, a new sample
		   will be plotted.  */
		unsigned rem_accum = dividend / 2 + dividend % 2;

		UTuint8 *new_samples;
		UTuint32 new_samples_size;
		unsigned i = 0, j = 0;

		data_head.sample_rate = new_rate;
		data_head.sample_count = data_head.sample_count * quotient +
			(data_head.sample_count * remainder) / dividend;
		new_samples_size = samples_size * quotient +
			(samples_size * remainder) / dividend;
		/* `+ dividend' shouldn't be necessary, but at least it
		   provides a worst case safety margin.  `+ 1' is necessary
		   due to the nonzero initialization of rem_accum above.  */
		new_samples = (UTuint8*)malloc(new_samples_size + dividend + 1);
		if (new_samples == NULL)
			return 1;

		while (i < samples_size)
		{
			unsigned k;
			for (k = 0; k < quotient; k++)
				new_samples[j++] = samples[i];
			rem_accum += remainder;
			if (rem_accum >= dividend)
			{
				new_samples[j++] = samples[i];
				rem_accum -= dividend;
			}
			i++;
		}
		free(samples);
		samples = new_samples;
		samples_size = new_samples_size;
	}

	printf("sample rate: %hu\n", data_head.sample_rate);
	printf("sample count: %u\n", data_head.sample_count);
	printf("bits per sample: %u\n", (unsigned)data_head.bits_per_sample);
	printf("channels: %u\n", (unsigned)data_head.channels);
	printf("encoding: %hu\n", data_head.encoding);
	printf("loop: 0x%04hx\n", data_head.loop);
	printf("loop start: %hu\n", data_head.loop_start);
	printf("loop end: %hu\n", data_head.loop_end);

	memcpy(riff_head.chunk_id, "RIFF", 4);
	riff_head.chunk_size = 4 + sizeof(fmt_chunk) +
		sizeof(data_chunk_head) + samples_size;
	memcpy(riff_head.format, "WAVE", 4);
	memcpy(fmt_chunk.chunk_id, "fmt ", 4);
	fmt_chunk.chunk_size = 16;
	fmt_chunk.audio_format = 1; /* PCM */
	fmt_chunk.num_channels = data_head.channels;
	fmt_chunk.sample_rate = data_head.sample_rate;
	fmt_chunk.bits_per_sample = data_head.bits_per_sample;
	fmt_chunk.byte_rate = fmt_chunk.sample_rate *
		fmt_chunk.num_channels * fmt_chunk.bits_per_sample / 8;
	fmt_chunk.block_align = fmt_chunk.num_channels *
		fmt_chunk.bits_per_sample / 8;

	memcpy(data_chunk_head.chunk_id, "data", 4);
	data_chunk_head.chunk_size = samples_size;

	fwrite(&riff_head, sizeof(riff_head), 1, fout);
	fwrite(&fmt_chunk, sizeof(fmt_chunk), 1, fout);
	fwrite(&data_chunk_head, sizeof(data_chunk_head), 1, fout);
	fwrite(samples, samples_size, 1, fout);

	free(samples);
	fclose(fin);
	fclose(fout);
	return 0;
}
