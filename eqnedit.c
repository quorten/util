/* eqnedit -- Easily create LaTeX equations */

#include <stdio.h>
#include <ctype.h>
#include <process.h>

enum bool_t {false, true};
typedef enum bool_t bool;

int main()
{
	FILE* fp;
	char inputBuffer[1024];
	unsigned curFile;
	bool exiting;

	curFile = 1;
	exiting = false;
	printf("First equation number: ");
	fflush(stdout);
	scanf("%U", &curFile);
	fgets(inputBuffer, 1024, stdin);
	while (!exiting)
	{
		char answer[3];
		char filename[19];
		char outname[19];
		puts("Enter your equation:");
		fflush(stdout);
		fgets(inputBuffer, 1024, stdin);
		sprintf(filename, "math%U.tex", curFile);
		fp = fopen(filename, "wt");
		fprintf(fp, "\\documentclass{article}\n\\usepackage{amsfonts}\n\\usepackage{amsmath}\n\\pagestyle{empty}\n\\begin{document}\n%s\\end{document}\n", inputBuffer);
		fclose(fp);
		/* Execute the necessary processes */
		spawnlp(P_WAIT, "latex", "latex", filename, NULL);
		sprintf(filename, "math%U.dvi", curFile);
		sprintf(outname, "math%U.ps", curFile);
		spawnlp(P_WAIT, "dvips", "dvips", filename, "-o", outname, NULL);
		sprintf(filename, "math%U.ps", curFile);
		sprintf(outname, "math%U.png", curFile);
		spawnlp(P_WAIT, "convert", "convert", "-density", "161x161", "-trim", filename, outname, NULL);
		printf("Would you like to typeset more? (y/n) ");
		fflush(stdout);
		fgets(answer, 3, stdin);
		if (toupper((int)answer[0]) == 'N')
			exiting = true;
		curFile++;
	}
	return 0;
}
