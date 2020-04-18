/* Break apart a Mohawk archive into its components parts.  */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "utstdint.h"
#include "utendian.h"

#ifdef _WINDOWS
#define MKDIR_PERMS
#else
#define MKDIR_PERMS , S_IRWXU | S_IRWXG | S_IRWXO
#endif

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

#ifndef __GNUC__
#define __attribute__(params)
#endif

/********************************************************************/
/* Structure definitions */

struct iff_header_tag
{
	char signature[4];
	UTuint32 file_size; /* file size excluding the header (8 bytes) */
} __attribute__((packed));
typedef struct iff_header_tag iff_header;

struct rsrc_header_tag
{
	char signature[4];
	UTuint16 version;
	UTuint16 compaction;
	UTuint32 file_size;
	UTuint32 rsrc_dir_offset;
	UTuint16 file_tbl_offset; /* relative from rsrc_dir_offset */
	UTuint16 file_tbl_size;
} __attribute__((packed));
typedef struct rsrc_header_tag rsrc_header;

struct type_tbl_header_tag
{
	UTuint16 name_list_offset; /* relative from rsrc_dir start */
	UTuint16 num_types;
} __attribute__((packed));
typedef struct type_tbl_header_tag type_tbl_header;

struct type_tbl_entry_tag
{
	char signature[4];
	UTuint16 rsrc_tbl_offset; /* relative from rsrc_dir start */
	UTuint16 name_tbl_offset; /* relative from rsrc_dir start */
} __attribute__((packed));
typedef struct type_tbl_entry_tag type_tbl_entry;

struct name_tbl_header_tag
{
	UTuint16 num_entries;
} __attribute__((packed));
typedef struct name_tbl_header_tag name_tbl_header;

struct name_tbl_entry_tag
{
	UTuint16 name_str_offset; /* relative to name_list */
	UTuint16 rsrc_file_tbl_index;
} __attribute__((packed));
typedef struct name_tbl_entry_tag name_tbl_entry;

struct rsrc_tbl_header_tag
{
	UTuint16 num_entries;
} __attribute__((packed));
typedef struct rsrc_tbl_header_tag rsrc_tbl_header;

struct rsrc_tbl_entry_tag
{
	UTuint16 rsrc_id;
	UTuint16 index;
} __attribute__((packed));
typedef struct rsrc_tbl_entry_tag rsrc_tbl_entry;

struct file_tbl_header_tag
{
	UTuint32 num_entries;
} __attribute__((packed));
typedef struct file_tbl_header_tag file_tbl_header;

struct file_tbl_entry_tag
{
	UTuint32 offset;
	UTuint16 size_low;
	UTuint8 size_high;
	UTuint8 flags;
	UTuint16 unknown;
} __attribute__((packed));
typedef struct file_tbl_entry_tag file_tbl_entry;

/********************************************************************/

struct type_tbl_entry_data_tag
{
	rsrc_tbl_header drsrc_tbl_header;
	rsrc_tbl_entry *drsrc_tbl_entries;
	name_tbl_header dname_tbl_header;
	name_tbl_entry *dname_tbl_entries;
};
typedef struct type_tbl_entry_data_tag type_tbl_entry_data;

int main(int argc, char *argv[])
{
	int retval;
	FILE *farch = NULL;
	iff_header mhwk_header;
	rsrc_header frsrc_header;
	type_tbl_header ftype_tbl_header;
	type_tbl_entry *ftype_tbl_entries = NULL;
	type_tbl_entry_data *ftype_tbl_data = NULL;
	file_tbl_header ffile_tbl_header;
	file_tbl_entry *ffile_tbl_entries = NULL;

	if (argc != 3)
		return 1;

	farch = fopen(argv[1], "rb");
	if (!farch)
		return 1;

	if (mkdir(argv[2] MKDIR_PERMS) == -1)
	{
		fputs("error: could not create the output directories", stderr);
		retval = 1; goto cleanup;
	}
	if (chdir(argv[2]))
	{
		fputs("error: could not change to output directory", stderr);
		retval = 1; goto cleanup;
	}

	if (fread(&mhwk_header, sizeof(iff_header), 1, farch) != 1 ||
		fread(&frsrc_header, sizeof(rsrc_header), 1, farch) != 1)
	{
		fputs("error: could not read the headers", stderr);
		retval = 1; goto cleanup;
	}

	c_ntohl(mhwk_header.file_size);
	c_ntohs(frsrc_header.version);
	c_ntohs(frsrc_header.compaction);
	c_ntohl(frsrc_header.file_size);
	c_ntohl(frsrc_header.rsrc_dir_offset);
	c_ntohs(frsrc_header.file_tbl_offset);
	c_ntohs(frsrc_header.file_tbl_size);

	if (fseek(farch, frsrc_header.rsrc_dir_offset, SEEK_SET) ||
		fread(&ftype_tbl_header, sizeof(type_tbl_header), 1, farch) != 1)
	{
		fputs("error: could not read the headers", stderr);
		retval = 1; goto cleanup;
	}

	c_ntohs(ftype_tbl_header.name_list_offset);
	c_ntohs(ftype_tbl_header.num_types);
	ftype_tbl_entries = (type_tbl_entry*)malloc(sizeof(type_tbl_entry) *
												ftype_tbl_header.num_types);
	ftype_tbl_data = (type_tbl_entry_data*)
		malloc(sizeof(type_tbl_entry_data) * ftype_tbl_header.num_types);
	if (ftype_tbl_entries == NULL || ftype_tbl_data == NULL)
	{
		fputs("error: could not allocate memory", stderr);
		retval = 1; goto cleanup;
	}
	{ /* Nullify the contained pointers.  */
		unsigned i;
		for (i = 0; i < ftype_tbl_header.num_types; i++)
		{
			ftype_tbl_data[i].drsrc_tbl_entries = NULL;
			ftype_tbl_data[i].dname_tbl_entries = NULL;
		}
	}
	if (fread(ftype_tbl_entries, sizeof(type_tbl_entry),
			  ftype_tbl_header.num_types, farch) !=
		ftype_tbl_header.num_types)
	{
		fputs("error: could not read the type table", stderr);
		retval = 1; goto cleanup;
	}

	/* check error */
	fseek(farch, frsrc_header.rsrc_dir_offset, SEEK_SET);
	fseek(farch, frsrc_header.file_tbl_offset, SEEK_CUR);
	printf("File tbl: 0x%08x\n", (UTuint32)ftell(farch));
	fread(&ffile_tbl_header, sizeof(ffile_tbl_header), 1, farch);
	c_ntohl(ffile_tbl_header.num_entries);
	ffile_tbl_entries = (file_tbl_entry*)malloc(sizeof(file_tbl_entry) *
												ffile_tbl_header.num_entries);
	if (ffile_tbl_entries == NULL)
	{
		fputs("error: could not allocate memory", stderr);
		retval = 1; goto cleanup;
	}
	fread(ffile_tbl_entries, sizeof(file_tbl_entry),
		  ffile_tbl_header.num_entries, farch);

	{
		unsigned i;
		for (i = 0; i < ffile_tbl_header.num_entries; i++)
		{
			c_ntohl(ffile_tbl_entries[i].offset);
			c_ntohs(ffile_tbl_entries[i].size_low);
		}
	}

	{
		unsigned i;
		for (i = 0; i < ftype_tbl_header.num_types; i++)
		{
			char type_dir[5];
			unsigned j;

			strncpy(type_dir, ftype_tbl_entries[i].signature, 4);
			type_dir[4] = '\0';
			c_ntohs(ftype_tbl_entries[i].rsrc_tbl_offset);
			c_ntohs(ftype_tbl_entries[i].name_tbl_offset);

			/* check error */
			fseek(farch, frsrc_header.rsrc_dir_offset, SEEK_SET);
			fseek(farch, ftype_tbl_entries[i].rsrc_tbl_offset, SEEK_CUR);
			fread(&(ftype_tbl_data[i].drsrc_tbl_header),
				  sizeof(rsrc_tbl_header), 1, farch);
			c_ntohs(ftype_tbl_data[i].drsrc_tbl_header.num_entries);
			ftype_tbl_data[i].drsrc_tbl_entries = (rsrc_tbl_entry*)
				malloc(sizeof(rsrc_tbl_entry) *
					   ftype_tbl_data[i].drsrc_tbl_header.num_entries);
			if (ftype_tbl_data[i].drsrc_tbl_entries == NULL)
			{
				fputs("error: could not allocate memory", stderr);
				retval = 1; goto cleanup;
			}
			fread(ftype_tbl_data[i].drsrc_tbl_entries, sizeof(rsrc_tbl_entry),
				  ftype_tbl_data[i].drsrc_tbl_header.num_entries, farch);

			for (j = 0;
				 j < ftype_tbl_data[i].drsrc_tbl_header.num_entries; j++)
			{
				c_ntohs(ftype_tbl_data[i].drsrc_tbl_entries[j].rsrc_id);
				c_ntohs(ftype_tbl_data[i].drsrc_tbl_entries[j].index);
			}

			/* check error */
			fseek(farch, frsrc_header.rsrc_dir_offset, SEEK_SET);
			fseek(farch, ftype_tbl_entries[i].name_tbl_offset, SEEK_CUR);
			fread(&(ftype_tbl_data[i].dname_tbl_header),
				  sizeof(name_tbl_header), 1, farch);
			c_ntohs(ftype_tbl_data[i].dname_tbl_header.num_entries);
			ftype_tbl_data[i].dname_tbl_entries = (name_tbl_entry*)
				malloc(sizeof(name_tbl_entry) *
					   ftype_tbl_data[i].dname_tbl_header.num_entries);
			if (ftype_tbl_data[i].dname_tbl_entries == NULL)
			{
				fputs("error: could not allocate memory", stderr);
				retval = 1; goto cleanup;
			}
			fread(ftype_tbl_data[i].dname_tbl_entries, sizeof(name_tbl_entry),
				  ftype_tbl_data[i].dname_tbl_header.num_entries, farch);

			mkdir(type_dir MKDIR_PERMS);

			for (j = 0;
				 j < ftype_tbl_data[i].dname_tbl_header.num_entries; j++)
			{
				c_ntohs(ftype_tbl_data[i].dname_tbl_entries[j].
						name_str_offset);
				c_ntohs(ftype_tbl_data[i].dname_tbl_entries[j].
						rsrc_file_tbl_index);
			}

			for (j = 0;
				 j < ftype_tbl_data[i].drsrc_tbl_header.num_entries; j++)
			{
				/* NOW in this inner most loop, we can read each entry
				   in the Mohawk archive and write it out to an
				   individual file.  */
				int index = ftype_tbl_data[i].drsrc_tbl_entries[j].index - 1;
				int id = ftype_tbl_data[i].drsrc_tbl_entries[j].rsrc_id;
				int offset = ffile_tbl_entries[index].offset;
				int size = ffile_tbl_entries[index].size_low |
					ffile_tbl_entries[index].size_high << 16;
				char id_name[4+1+11+1];
				char real_name[205];
				char *out_name = id_name;
				FILE *fout;
				int unexp_eof = 0;
				printf("\nResource type: %s\n", type_dir);
				printf("Resource id: %d\n", id);
				printf("Resource index: %d\n", index);
				printf("Resource address: 0x%08x\n", offset);
				printf("Resource size: 0x%08x\n", size);

				/* Check if there's a valid name table entry.  */
				if (ftype_tbl_data[i].dname_tbl_header.num_entries > 0)
				{
					unsigned k;
					for (k = 0; k < ftype_tbl_data[i].
							 dname_tbl_header.num_entries; k++)
					{
						int name_offset = ftype_tbl_data[i].
							dname_tbl_entries[k].name_str_offset;
						int n_index = ftype_tbl_data[i].dname_tbl_entries[k].
							rsrc_file_tbl_index - 1;
						int c;
						char *curpos = real_name + 5;

						if (n_index != index)
							continue;

						fseek(farch, frsrc_header.rsrc_dir_offset, SEEK_SET);
						fseek(farch, ftype_tbl_header.name_list_offset,
							  SEEK_CUR);
						fseek(farch, name_offset, SEEK_CUR);
						while ((c = getc(farch)) != EOF &&
							   c != '\0' && curpos != real_name + 204)
							*curpos++ = (char)c;
						*curpos++ = '\0';

						printf("Resource name: %s\n", real_name + 5);
						printf("Index (resource name): %d\n", n_index);
						out_name = real_name;
					}

				}

				strcpy(id_name, type_dir); strcpy(real_name, type_dir);
				id_name[4] = '/'; real_name[4] = '/';
				sprintf(&id_name[5], "%d", id);
				printf("Output name: %s\n", out_name);

				fout = fopen(out_name, "wb");
				if (fout == NULL)
				{
					fputs("Error opening output file.\n", stderr);
					continue;
				}
				fseek(farch, offset, SEEK_SET);
				{
					unsigned k;
					for (k = 0; k < size; k++)
					{
						int c = getc(farch);
						if (c == EOF)
						{
							fputs("Unexpected end of file.\n", stderr);
							unexp_eof = 1;
							break;
						}
						putc(c, fout);
					}
				}
				fclose(fout);

				if (!unexp_eof)
					puts("Resource output successfully.");
			}
		}
	}

	retval = 0;
cleanup:
	fclose(farch);
	free(ftype_tbl_entries);
	if (ftype_tbl_data != NULL)
	{
		unsigned i;
		for (i = 0; i < ftype_tbl_header.num_types; i++)
		{
			free(ftype_tbl_data[i].drsrc_tbl_entries);
			free(ftype_tbl_data[i].dname_tbl_entries);
		}
	}
	free(ftype_tbl_data);
	free(ffile_tbl_entries);
	return retval;
}
