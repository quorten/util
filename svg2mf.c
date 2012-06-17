/* Convert SVG path commands to Metafont/MetaPost path commands.
   Requires expat for XML parsing.

   Note that this has only been tested on Inkscape 0.46 SVGs.  Looking
   at the SVG specification, it will definitely fail when processing
   SVG images from other programs, optimizer programs in particular.
   Right now, this converter is written to be extremely simple and
   only performs the bare minimum during conversion: only Inkscape
   plain paths, ellipses, and rectangles are converted.  Also,
   transformations (usually) must have already been applied.  These
   limitations can be easily overcome by expanding this
   proof-of-concept code.  */
#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <expat.h>

#include <math.h>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

enum bool_t {false, true};
typedef enum bool_t bool;

#define BUFFER_SIZE 8192

char *buffer[BUFFER_SIZE];

/* Note that SVG coordinates measure from the top-left, but Metafont
   coordinates measure from the bottom-left.  Thus, y-coordinate
   inversion is necessary.  */
float imgHeight;
char *charCode = NULL;
char *charDesc = NULL;
char *charDepth = NULL;
char *charIDPI = NULL;
unsigned groupStack = 0;

char *substr(const char *src, unsigned len)
{
	char *newstr;
	newstr = (char *)malloc(len + 1);
	if (newstr == NULL)
		return NULL;
	strncpy(newstr, src, len);
	return newstr;
}

/*********************************************************************
 Math helper functions */

/* An SVG matrix is column-major, like this:
   [ a c e ]
   [ b d f ]
   [ 0 0 1 ] */
struct SvgMatrix_t
{
	float a, b, c, d, e, f;
};
typedef struct SvgMatrix_t SvgMatrix;

/* Return the identity matix.  */
void matrix_ident(SvgMatrix *m1)
{
	m1->a = 1; m1->c = 0; m1->e = 0;
	m1->b = 0; m1->d = 1; m1->f = 0;
}

/* Multiply `m1' by `m2', and store the result in `m3'.  */
void matrix_mult(const SvgMatrix *m1, const SvgMatrix *m2, SvgMatrix *m3)
{
	m3->a = m1->a * m2->a + m1->c * m2->b; /* + m1->e * 0; */
	m3->b = m1->b * m2->a + m1->d * m2->b; /* + m1->f * 0; */
	m3->c = m1->a * m2->c + m1->c * m2->d; /* + m1->e * 0; */
	m3->d = m1->b * m2->c + m1->d * m2->d; /* + m1->f * 0; */
	m3->e = m1->a * m2->e + m1->c * m2->f + m1->e * 1;
	m3->f = m1->b * m2->e + m1->d * m2->f + m1->f * 1;
}

/* Transform a point by a matrix, store the results in `x2' and
 * `y2'.  */
void matrix_transf(const SvgMatrix *m1, float x1, float y1, float *x2,
				   float *y2)
{
	(*x2) = m1->a * x1 + m1->c * y1; /* + 0 * 1; */
	(*y2) = m1->b * x1 + m1->d * y1; /* + 0 * 1; */
}

/* Rotate an SVG point clockwise by the given angle in degrees.  */
void rotate_point(float angle, float x1, float y1, float *x2, float *y2)
{
	(*x2) = x1 * cos(angle * M_PI / 180) - y1 * sin(angle * M_PI / 180);
	(*y2) = x1 * sin(angle * M_PI / 180) + y1 * cos(angle * M_PI / 180);
}

/* Note: you are strongly recommended not to use this function in any
   way in the process of building your Metafont.  After all, Metafonts
   are supposed to specify high-level intent, so if you are not even
   aware of what transformations you have made to your drawings, then
   that's a higher level problem that you need to solve.  Anyways,
   this function doesn't work either.

   Here's the matrix format of the separation:
   (translate * rotate * scale) * orig_coords

   The previous matrix separated format corresponds to the following
   intuitive format:
   orig_coords xscaled xs yscaled ys rotated theta shifted (x,y) */
void separate_transforms(char *transAttr)
{
	SvgMatrix sm;
	float rotAngle;
	float scaleX, scaleY;
	float transX, transY;
	float oldX, oldY;
	float newX, newY;

	sscanf(transAttr, "matrix(%f, %f, %f, %f, %f, %f)",
		   sm.a, sm.b, sm.c, sm.d, sm.e, sm.f);

	/* Check matrix positions e and f for translation: 
	   [ 1 0 dx ]
	   [ 0 1 dy ]
	   [ 0 0  1 ] */
	if (sm.e != 0 && sm.f != 0)
	{
		transX = sm.e; transY = sm.f;
		/* Remove the translation from the matrix.  */
		sm.e = 0; sm.f = 0;
	}

	/* Check matrix positions b and c for a rotation value (note that
	   the scale values confound with the rotation values):
	   [ sx  0 0 ]   [ cos(a) -sin(a) 0 ]   [ sx*cos(a) sx*-sin(a) 0 ]
	   [  0 sy 0 ] * [ sin(a)  cos(a) 0 ] = [ sy*sin(a)  sy*cos(a) 0 ]
	   [  0  0 1 ]   [      0       0 1 ]   [         0          0 1 ]

	   sx * cos(a) == a; sx * -sin(a) == c;
	   sy * sin(a) == b; sy *  cos(a) == d;
	   sx == (a + c) / (cos(a) - sin(a));
	   sy == (b + d) / (sin(a) + cos(a));
	   (sx + sy) * cos(a) = a + d;
	   cos(a) == (a + d) / (sx + sy);

	   Sorry, but I'm not solving this problem for now.  */
	if (sm.b != 0 && sm.c != 0)
	{
		double rotRad;
		SvgMatrix rm;
		rotRad = asin(sm.b);
		/* SVG rotations measure clockwise in degrees.  */
		rotAngle = -(float)rotRad * M_PI / 180;
		/* Remove the rotations from the matrix.  */
		rm.a = (float)cos(rotRad); rm.c = -(float)sin(rotRad); rm.e = 0;
		rm.b = (float)sin(rotRad); rm.d = (float)cos(rotRad); rm.f = 0;
		sm.a /= (float)cos(rotRad); sm.b = 0;
		rm.b = 0; rm.c = 0;
	}

	/* Check matrix positions a and d for scaling:
	   [ sx  0 0 ]
	   [  0 sy 0 ]
	   [  0  0 1 ] */
	if (sm.a != 1 && sm.d != 1)
	{
		scaleX = sm.a; scaleY = sm.d;
		/* Remove the scaling from the matrix.  */
		sm.a = 1; sm.d = 1;
	}
}

/*********************************************************************
 Parser functions */

#define CHECK_SIZE()							\
	if (j >= len) break;
#define SKIP_CHAR_NOCHECK(code)					\
	if (attr[i+1][j] == code) j++;
#define SKIP_CHAR(code)							\
	SKIP_CHAR_NOCHECK(code);					\
	CHECK_SIZE();
#define SKIP_WHITESPACE()						 \
	while (j < len &&							 \
		   (attr[i+1][j] == ' ' ||				 \
			attr[i+1][j] == '\t' ||				 \
			attr[i+1][j] == '\n'))				 \
		j++;									 \
	CHECK_SIZE();
#define COPY_TO_DELIM_NOCHECK(delim)			 \
	while (j < len && attr[i+1][j] != delim)	 \
	{											 \
		putc((int)attr[i+1][j], stdout);		 \
		j++;									 \
	}
#define COPY_TO_DELIM(delim)					 \
	COPY_TO_DELIM_NOCHECK(delim);				 \
	CHECK_SIZE();
#define SKIP_TO_DELIM_NOCHECK(delim)			\
	while (j < len && attr[i+1][j] != delim)	\
		j++;
#define SKIP_TO_DELIM(delim)					\
	SKIP_TO_DELIM_NOCHECK(delim)				\
	CHECK_SIZE();
#define WRITE_COORD()			\
	printf("(");				\
	COPY_TO_DELIM(',');			\
	printf("px");				\
	SKIP_CHAR(',');				\
	SKIP_WHITESPACE();			\
	printf(",");				\
	/* Now for the hard part: the y-coordinate must be inverted.  */ \
	{												   \
		char yStr[80];								   \
		unsigned yStrLen;							   \
		float ycoord;								   \
		unsigned lastPos;							   \
		lastPos = j;								   \
		SKIP_TO_DELIM_NOCHECK(' ');					   \
		yStrLen = j - lastPos;						   \
		if (yStrLen >= 79) yStrLen = 79;			   \
		strncpy(yStr, &attr[i+1][lastPos], yStrLen);   \
		sscanf(yStr, "%f", &ycoord);				   \
		ycoord = imgHeight - ycoord;				   \
		printf("%f", ycoord);						   \
	}							\
	SKIP_CHAR_NOCHECK(' ');		\
	printf("px");				\
	printf(")");
#define MATCH_ATTRIB(name, var)					\
	if (strcmp(attr[i], name) == 0)				\
		var = i;

void XMLCALL svg_start(void *data, const char *elmt, const char **attr)
{
	unsigned i;
	unsigned idAttr, dataAttr, styleAttr;
	unsigned typeAttr, rxAttr, ryAttr, cxAttr, cyAttr, transAttr;
	unsigned xAttr, yAttr, widthAttr, heightAttr;
	unsigned j;
	unsigned len;

	enum {WAS_LINE, WAS_CURVE} lastDraw;
	bool writeTerminator;
	enum {GT_PATH, GT_ELLIPSE, GT_RECT, GT_GROUP} geoType;

	bool draw_fill, draw_stroke, fill_white, stroke_white;
	char *render_cmd = "draw";

	/* Perform special handling for some kinds of elements.  */
	if (strcmp(elmt, "svg") == 0) /* root element */
	{
		float imgWidth = 10;
		for (i = 0; attr[i]; i += 2)
		{
			if (strcmp(attr[i], "width") == 0)
				sscanf(attr[i+1], "%f", &imgWidth);
			if (strcmp(attr[i], "height") == 0)
			{
				sscanf(attr[i+1], "%f", &imgHeight);
				printf("\
%% WARNING: This file is mostly automatically generated from raw paths\n\
%% from an SVG image.  Therefore, it actually does not benefit from the\n\
%% extended features that Metafont provides.\n\
\n\
mode_setup;\n\
px# := %sin#;\n\
define_pixels(px);\n\
%% define_blacker_pixels(); %% Please use this!\n\
pickup pencircle scaled 1px;\n\
\n\
def overdraw expr p =\n\
  erase fill p; draw p;\n\
enddef;\n\
def outlinefill expr p =\n\
  fill p; erase draw p;\n\
enddef;\n\
\n\
beginchar(\"%s\", %fpx#, %fpx#, %spx#); \"%s\";\n",
			   charIDPI, charCode, imgWidth, imgHeight, charDepth, charDesc);
				return;
			}
		}
		fprintf(stderr, "error: could not get image height.\n");
		imgHeight = 0;
		return;
	}

	if (strcmp(elmt, "g") == 0) /* group */
	{
		groupStack++;
		geoType = GT_GROUP;
		printf("begingroup; ");
	}

	/* Perform preliminary determination of the shape type.  */
	if (strcmp(elmt, "rect") == 0)
		geoType = GT_RECT;
	else if (strcmp(elmt, "path") == 0)
		geoType = GT_PATH;
	else if (geoType != GT_GROUP)
		return;

	/* Parse and match attribute names.  */
	idAttr = (unsigned)-1;
	dataAttr = (unsigned)-1;
	styleAttr = (unsigned)-1;
	typeAttr = (unsigned)-1;
	transAttr = (unsigned)-1;
	for (i = 0; attr[i]; i += 2)
	{
		MATCH_ATTRIB("id", idAttr);
		MATCH_ATTRIB("d", dataAttr);
		MATCH_ATTRIB("style", styleAttr);
		MATCH_ATTRIB("transform", transAttr);
		if (geoType == GT_RECT)
		{
			MATCH_ATTRIB("x", xAttr);
			MATCH_ATTRIB("y", yAttr);
			MATCH_ATTRIB("width", widthAttr);
			MATCH_ATTRIB("height", heightAttr);
		}
		else if (geoType == GT_PATH)
		{
			MATCH_ATTRIB("sodipodi:type", typeAttr);
			MATCH_ATTRIB("sodipodi:cx", cxAttr);
			MATCH_ATTRIB("sodipodi:cy", cyAttr);
			MATCH_ATTRIB("sodipodi:rx", rxAttr);
			MATCH_ATTRIB("sodipodi:ry", ryAttr);
		}
	}
	if (geoType != GT_GROUP && geoType != GT_RECT &&
		dataAttr == (unsigned)-1)
		return;

	/* Print the ID.  */
	if (idAttr != (unsigned)-1)
		printf("%% ID: %s\n", attr[idAttr+1]);

	if (geoType == GT_GROUP)
	{
		printf("save currenttransform; transform currenttransform;\n"
			   "currenttransform := identity;\n");
	}

	/* Parse the transform attribute.  */
	if (transAttr != (unsigned)-1)
	{
		float a, b, c, d, e, f;
		if (strncmp(attr[transAttr+1], "matrix", 6) == 0)
		{
			sscanf(attr[transAttr+1], "matrix(%f, %f, %f, %f, %f, %f)",
				   &a, &b, &c, &d, &e, &f);
			printf("transform custtrans; transform temptrans;\n"
				   "xpart custtrans = %fpx; ypart custtrans = %fpx;\n"
				   "xxpart custtrans = %f; xypart custtrans = %f;\n"
				   "yxpart custtrans = %f; yypart custtrans = %f;\n"
			   "temptrans = custtrans shifted (0px, -h) yscaled -1;\n"
				   "custtrans := temptrans;\n",
				   e, f, a, b, c, d);
		}
		if (strncmp(attr[transAttr+1], "translate", 9) == 0)
		{
			sscanf(attr[transAttr+1], "translate(%f, %f)", &e, &f);
			printf("custtrans := shifted (%fpx, %fpx);", e, -f);
		}

		if (geoType == GT_GROUP)
			printf("currenttransform := custtrans;\n");
	}

	if (geoType == GT_GROUP)
		return;

	/* Figure out the correct fill/stroke attributes.  */
	/* TODO: Transform stroke widths too.  */
	draw_fill = true, draw_stroke = false, fill_white = false;
	stroke_white = false;
	if (styleAttr != (unsigned)-1)
	{
		unsigned lastPos;

		/* Parse the style attribute.  */
		i = styleAttr;
		len = strlen(attr[i+1]);
		j = 0;
		do
		{
			const char *styleName, *styleValue;
			unsigned snLen, svLen;
			/* Read the style name.  */
			lastPos = j; styleName = &attr[i+1][lastPos];
			SKIP_TO_DELIM(':'); snLen = j - lastPos;
			/* Read the style value.  */
			SKIP_CHAR(':'); SKIP_WHITESPACE();
			lastPos = j; styleValue = &attr[i+1][lastPos];
			SKIP_TO_DELIM_NOCHECK(';'); svLen = j - lastPos;

			if (strncmp(styleName, "fill", snLen) == 0)
			{
				if (strncmp(styleValue, "none", svLen) == 0)
					draw_fill = false;
				if (strncmp(styleValue, "#ffffff", svLen) == 0)
					fill_white = true;
			}
			else if (strncmp(styleName, "stroke", snLen) == 0)
			{
				draw_stroke = true;
				if (strncmp(styleValue, "none", svLen) == 0)
					draw_stroke = false;
				if (strncmp(styleValue, "#ffffff", svLen) == 0)
					stroke_white = true;
			}
			else if (strncmp(styleName, "stroke-width", snLen) == 0)
			{
				char *scaleStr;
				scaleStr = substr(styleValue, svLen);
				printf("pickup pencircle scaled %spx;\n", scaleStr);
				free(scaleStr);
			}

			SKIP_CHAR(';');
			SKIP_WHITESPACE();
		} while (j < len);

		/* Figure out which Metafont drawing sequence to use.  */
		if (draw_fill && draw_stroke && fill_white == stroke_white)
		{
			if (fill_white)
				render_cmd = "erase";
			render_cmd = "filldraw";
		}
		else if (draw_fill && draw_stroke && fill_white != stroke_white)
		{
			if (fill_white)
				render_cmd = "overdraw";
			if (stroke_white)
				render_cmd = "outlinefill";
		}
		else if (draw_fill && !draw_stroke)
		{
			if (fill_white) render_cmd = "erase";
			render_cmd = "fill";
		}
		else if (draw_stroke && !draw_fill)
		{
			if (fill_white) render_cmd = "erase";
			render_cmd = "draw";
		}
	}

	/* Check for a special geometry type.  */
	if (geoType == GT_RECT)
	{
		float xpos, ypos, width, height;
		sscanf(attr[xAttr+1], "%f", &xpos);
		sscanf(attr[yAttr+1], "%f", &ypos);
		sscanf(attr[widthAttr+1], "%f", &width);
		sscanf(attr[heightAttr+1], "%f", &height);
		ypos = imgHeight -  height - ypos;

		printf("%s unitsquare ", render_cmd);
		if (width == height)
			printf("scaled %fpx ", width);
		else
			printf("xscaled %fpx yscaled %fpx ", width, height);
		printf("shifted (%fpx,%fpx);\n", xpos, ypos);
		return;
	}
	if (geoType == GT_PATH)
	{
		/* Find out whether this is an Inkscape path or ellipse.  */
		if (typeAttr != (unsigned)-1 && strcmp(attr[typeAttr+1], "arc") == 0)
		{
			float xpos, ypos, dx, dy;
			geoType = GT_ELLIPSE;
			sscanf(attr[cxAttr+1], "%f", &xpos);
			sscanf(attr[cyAttr+1], "%f", &ypos);
			sscanf(attr[rxAttr+1], "%f", &dx); dx *= 2;
			sscanf(attr[ryAttr+1], "%f", &dy); dy *= 2;
			/* Sodipodi coordinates measure from the bottom-left, so
			   no y-coordinate conversion is necessary.  */

			printf("%s fullcircle ", render_cmd);
			if (dx == dy)
				printf("scaled %fpx ", dx);
			else
				printf("xscaled %fpx yscaled %fpx ", dx, dy);
			printf("shifted (%fpx,%fpx) "
				   "transformed custtrans;\n",
				   xpos, ypos);
			return;
		}
	}

	/* Otherwise, write a path.  */
	/* TODO: Add more extensive path processing here so that the
	   program can handle non-Inkscape 0.46 SVGs.  */
	i = dataAttr;
	len = strlen(attr[i+1]);
	lastDraw = 0;
	writeTerminator = false;
	j = 0;
	printf("%s ", render_cmd);
	while (j < len)
	{
		switch (attr[i+1][j])
		{
		case ' ':
			SKIP_CHAR(' ');
			continue;
		case 'M': /* moveto */
			SKIP_CHAR('M');
			if (writeTerminator == true)
			{
				printf(";\n");
				printf("%s ", render_cmd);
			}
			SKIP_CHAR(' ');
			WRITE_COORD();
			writeTerminator = true;
			break;
		case 'L': /* lineto */
			SKIP_CHAR('L');
			printf("--");
			SKIP_CHAR(' ');
			WRITE_COORD();
			lastDraw = WAS_LINE;
			break;
		case 'C': /* curveto */
			SKIP_CHAR('C');
			printf("..");
			SKIP_CHAR(' ');
			printf("controls ");
			WRITE_COORD();
			printf(" and ");
			WRITE_COORD();
			printf("..");
			WRITE_COORD();
			lastDraw = WAS_CURVE;
			break;
		case 'Z': /* closepath */
		case 'z':
			if (lastDraw == WAS_LINE)
				printf("--cycle");
			if (lastDraw == WAS_CURVE)
				printf("..cycle");
			SKIP_CHAR('Z'); SKIP_CHAR('z'); /* skip it either way */
			break;
		case 'A': /* arc */
			SKIP_CHAR('A');
			fprintf(stderr, "Sorry, arcs are not supported yet.\n");
			SKIP_CHAR(' ');
			SKIP_TO_DELIM(' '); SKIP_CHAR(' ');
			SKIP_TO_DELIM(' '); SKIP_CHAR(' ');
			SKIP_TO_DELIM(' '); SKIP_CHAR(' ');
			SKIP_TO_DELIM(' '); SKIP_CHAR(' ');
			SKIP_TO_DELIM(' '); SKIP_CHAR(' ');
			fprintf(stderr, "Warning: may not recover from parse error.\n");
			break;
		case 'm':
		case 'l':
		case 'c':
		case 'h':
		case 'v':
			fprintf(stderr,
					"Sorry, relative coordinates are not supported yet.\n");
			fprintf(stderr, "Warning: may not recover from parse error.\n");
			break;
		case 'H':
		case 'V':
			fprintf(stderr,
				"Sorry, horizontal/vertical lines are not supported yet.\n");
			fprintf(stderr, "Warning: may not recover from parse error.\n");
			break;
		default:
		{
			char *errString;
			unsigned errPos;
			errPos = j;
			SKIP_TO_DELIM(' ');
			errString = substr(&attr[i+1][errPos], j - errPos);
			fprintf(stderr, "Unhandled syntax in path data: %s.\n", errString);
			free(errString);
			SKIP_CHAR(' ');
			free(errString);
			break;
		}
		}
	}
	if (writeTerminator == true)
		printf(";\n");
}

void XMLCALL svg_end(void *data, const char *elmt)
{
	if (groupStack > 0 && strcmp(elmt, "g") == 0)
	{
		groupStack--;
		printf("endgroup;\n");
	}
}

int main(int argc, char *argv[])
{
	int retval;
	XML_Parser xmlp = NULL;
	int done;

	if (argc > 1 && (strcmp(argv[1], "-h") == 0 ||
					 strcmp(argv[1], "--help") == 0))
	{
		puts(
"Usage: svg2mf [char] [chardesc] [depth] [inv_dpi] < INPUT > OUTPUT");
		retval = 0; goto cleanup;
	}
	else
	{
		if (argc >= 2) charCode = strdup(argv[1]);
		else charCode = strdup("A");
		if (argc >= 3) charDesc = strdup(argv[2]);
		else charDesc = strdup("Unknown character (A).");
		if (argc >= 4) charDepth = strdup(argv[3]);
		else charDepth = strdup("0");
		if (argc >= 5) charIDPI = strdup(argv[4]);
		else charIDPI = strdup("1/100");
	}

	xmlp = XML_ParserCreate(NULL);
	if (!xmlp)
	{
		fprintf(stderr, "Could not allocate memory for XML parser.\n");
		retval = 1; goto cleanup;
	}
	XML_SetElementHandler(xmlp, svg_start, svg_end);

	done = 0;
	while (!done)
	{
		int len;

		len = (int)fread(buffer, 1, BUFFER_SIZE, stdin);
		if (ferror(stdin)) {
			fprintf(stderr, "Read error\n");
			XML_ParserFree(xmlp);
			retval = 1; goto cleanup;
		}
		done = feof(stdin);

		if (XML_Parse(xmlp, buffer, len, done) == XML_STATUS_ERROR) {
			fprintf(stderr, "Parse error at line %lu:\n%s\n",
					XML_GetCurrentLineNumber(xmlp),
					XML_ErrorString(XML_GetErrorCode(xmlp)));
			XML_ParserFree(xmlp);
			retval = 1; goto cleanup;
		}
	}

	/* Write the end of the Metafont file.  */
	printf("endchar;\n\nend\n");
	retval = 0; goto cleanup;

cleanup:
	free(charCode);
	free(charDesc);
	free(charDepth);
	free(charIDPI);
	if (xmlp)
		XML_ParserFree(xmlp);
	return retval;
}
