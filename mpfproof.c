/* Generate proof-sheets for MetaPost-processed Metafonts.  The
   MetaPost transcript is given on standard input, and the proof-sheet
   TeX file is written to standard output.  Also, one argument must be
   specified that indicates the prefix of the PostScript files output
   from MetaPost.  (for `mfput.100', the prefix would be `mfput'.)

   Warning: This code's parsing mechanisms are extremely simple.  They
   may fail under certain circumstances.  */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Get a line and store it in a dynamically-allocated memory block.
   The pointer to this block is returned in `out_ptr', and the newline
   character is pushed back onto the input stream.  */
int exp_getline(FILE* fp, char** out_ptr)
{
	char buffer[1024];
	char* lineData;
	unsigned lineLen;
	lineData = NULL;
	lineLen = 0;
	do
	{
		lineData = (char*)realloc(lineData, lineLen + 1024);
		buffer[0] = '\0';
		if (fgets(buffer, 1024, fp) == NULL)
		{
			if (lineLen == 0)
				lineData[0] = '\0';
			break;
		}
		strcpy(&lineData[lineLen], buffer);
		lineLen += strlen(buffer);
	}
	while (!feof(fp) && (lineLen == 0 || lineData[lineLen-1] != '\n'));

	(*out_ptr) = lineData;
	if (lineLen == 0 || lineData[lineLen-1] != '\n')
		return EOF;
	if (lineData[lineLen-1] == '\n')
	{
		ungetc((int)'\n', fp);
		lineData[lineLen-1] = '\0';
	}
	return 1;
}

int main(int argc, char *argv[])
{
	char* filename;
	char* buffer = NULL;

	if (argc < 2)
	{
		puts("Ussage: mpfproof OUTPUT_PREFIX < transcript.log > mpfproof.tex");
		return 0;
	}
	filename = argv[1];

	/* Output the header (sorry about the illegibly-escaped TeX
	   commands).  */
	puts(
"\\input epsf\n\
\\font\\textt=cmtex10 \\textt\n\
\\font\\textrm=cmr10\n\
\\def\\okbreak{\\vfil\\penalty2000\\vfilneg}\n\
\n\
\\def\\today{\\ifcase\\month\\or\n\
  January\\or February\\or March\\or April\\or May\\or June\\or\n\
  July\\or August\\or September\\or October\\or November\\or December\\fi\n\
  \\space\\number\\day, \\number\\year}\n\
\\newcount\\m \\newcount\\n\n\
\\n=\\time \\divide\\n 60 \\m=-\\n \\multiply\\m 60 \\advance\\m \\time\n\
\\def\\hours{\\twodigits\\n\\twodigits\\m}\n\
\\def\\twodigits#1{\\ifnum #1<10 0\\fi \\number#1}\n\
\n\
\\footline={\\sevenrm\\today\\ at \\hours\\hss\\tenrm\\folio\\hss}");

	/* Read in the transcript, looking for any lines that have square
	   brackets on them.  */
	while (exp_getline(stdin, &buffer) != EOF)
	{
		char c;
		char *desc, *charcode;
		char *descDelim, *codeDelim;
		c = (char)getc(stdin);
		/* Break the line up into the explanatory text and the
		   character number.  */
		descDelim = strchr(buffer, (int)'[');
		codeDelim = strchr(buffer, (int)']');
		if (descDelim == NULL || codeDelim == NULL)
			goto cleanup;
		desc = buffer;
		if (descDelim > desc) (*(descDelim - 1)) = '\0';
		else (*descDelim) = '\0';
		charcode = descDelim + 1; (*codeDelim) = '\0';
		/* Build a PostScript file name from the character number.  */
		/* Output a line that includes both a description and the
		   inclusion statement.  */
		printf("%s.%s {\\textrm Character %s ``%s''} "
			   "$$\\epsfverbosetrue\\epsfbox{%s.%s}$$\\par\\okbreak\n",
			   filename, charcode, charcode, desc, filename, charcode);
	cleanup:
		free(buffer);
	}
	free(buffer);

	/* Output the footer.  */
	puts("\\vfill\\end%");
	return 0;
}
