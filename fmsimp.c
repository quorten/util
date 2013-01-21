/*

Alright, let's try to keep it simple:

touch
cp
mv
rm

Either files or directories can be used for these commands.  Each
command implies -R.  Paths are always absolute.

Here's how the algorithm works:

One data structure keeps track of net file management operations.

The list of file management commands is processed as a stream on
standard input.

Each command modifies the relevant net command.

One the end of the input stream is reached, the net commands are
written to standard output.

The net command list starts out empty.  New commands are always added
to the end.  Previous commands may be removed from the middle.

Switch among the possible input commands.  Signal an error if an
input command uses an unknown switch.

If a touch command is seen, check if there is already a touch
command in the net command list with the given file.  If there is
not, then add a new touch command to the end.

If a cp command is seen, just add a new touch command with the
destination file for now.

If an mv command is seen, check for an existing mv command with a
destination name that is the same as the source name.  If there is
one, then delete the old command and replace it with the new command.
If there is a touch command, then change the touch command.

Note that these commands are meant to update a remote destination, not
just be pending file management commands.  For that, there is a
different algorithm.

A touch in the input can never be simplified.  Ony cp, mv, and rm can
be simplified.

Move commands are the primary candidate for simplification.  Copy
commands are only simplified by redirecting the source to the primary
source.  Remove commands are simplified by deleting tree branches.

When the input is processed, touch and copy commands will be on top,
move commands will be in the middle, and remove commands will be on
the bottom.

If a remove command is found, delete the entire tree touches, copies,
and move for that file.

Okay, I think I have it.

Switch among the file management command.

If a touch command is found, check for copy commands in the net
command list.  If there is one such command with the same destination
as the touch command's file, then delete the existing copy command*.
Also check for moves.  If any moves are found that involve the file's
name as the destination, modify the touch command to apply for the
head file name instead.  If a remove command exists for the source
file name, then delete that remove command.  Note that if rsyncing is
used, this may be disadvantagous.

If a copy command is found, check if there are any move commands with
the destination the same as the source.  If there are, then change the
copy command to point to the most original file name.  Also check if
there are any copy commands with the same name.  If there are, then
change the copy command to copy from the head file.  If a remove
command exists for the destination file name, then delete that remove
command.  If a touch command exists for the destination file name,
then remove that command.  Copy commands can always be written in
terms of file management on the remove machine only.  Only touch
commands need to traverse the network.

If a move command is found, check if there are any move commands with
the destination the same as the source.  If there are, then change the
existing move command to be the net move command of the move command
chain.  If a copy command is found that has the same destination name,
then change the existing copy command to use the new destination*.  If
a touch command is found, then change the existing touch command to
use the new destination*.  If a move is found that stomps out an
existing file name, then check if that file is touched or copied.  If
it is, then delete the relevant touch or copy command.  *Note that
these should only be done if moves are to happen before such commands.

If a remove command is found, check if there are any touch commands
with the same file name.  If there are, then delete those commands.
Do likewise for copy commands.  If a move command is found to be
unnecessary, then an equivalent remove command for the source file
name must be added.  This should be done by doing all of the same
processing as if the new command were found in the list of commands to
be simplified.

How about a variation that involves specific shell commands not yet
executed.  In this case, never modify existing touch commands;
instead, only add and modify copies, moves, and removes.

Trial run:

Input:
touch f1
touch f2
touch f3
cp f1 f5
mv f1 f4
cp f4 f6
touch f6
mv f4 f7
rm f6

Output:
touch f1
touch f2
touch f3
cp f1 f5
mv f1 f7
rm f6

Okay, so my algorithm almost works, except that it needs to do move
safety checking for potentially clashing move commands.  No move
command can be clash safe unless the destination directory contents
are manually checked.  I will have to look more into this special
logic.  If a move command is used that is both clashing and the source
file exists on the target computer, the source file will have to be
explicitly deleted if touch commands and copy commands are modified in
such a way as to eliminate the move command.  Thus, move commands only
need to be listed for unlisted files.

Wait, remove commands should come first in the output, touch and copy
commands second, then move commands come last.  Actually, removes
should always happen last for data safety, in case the user wants to
cancel a sync operation in an emergency.  This of course means that
the move mechanism will have to rename delete commands and delete
commands are always `rm -f'.

Create a graphical user interface that lets you select from subject
categories, most recently created and modified categories, and a
journaling file manager.  Then the graphical user interface
facilitates managing backup status and copying to backups.  The UI
also keeps track of media types, media characteristics, and
redundancy.  The user interface may also calculate the probability of
data loss for specific kinds of media.

*/

/* fmsimp.c -- File Management Simplifier.

   The file management simplifier processes a list of "touches",
   copies, moves and deletes on standard input and writes out a
   corresponding list to standard output, with redundant move chains
   and touches removed from the list.  "Touch" is an abstract term for
   updating a file.  This program is meant to be used as part of a
   backup management system.  Note that only the simplest input syntax
   is supported: backslash line continuations are not supported, and
   all copies, moves, and deletes must be recursive.  No other
   switches are supported, and touches are always assumed to be
   recursive.  */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bool.h"
#include "exparray.h"

struct RefCountStr_tag
{
	char *d;
	unsigned count;
};
typedef struct RefCountStr_tag RefCountStr;

typedef int (*CmdFunc)(int, char *[]);

typedef char* char_ptr;
typedef RefCountStr* RefCountStr_ptr;
EA_TYPE(char);
EA_TYPE(char_ptr);
EA_TYPE(RefCountStr_ptr);
EA_TYPE(RefCountStr_ptr_array);

/* This is the sorted list of reference-counted strings.  */
RefCountStr_ptr_array str_heap;

/* `net_cmds' is an array of argument vector arrays.  Each argument
   vector is a reference-counted string.  It is easier to check
   previously processed commands when the command lines are stored as
   argument vectors rather than a string.  */
RefCountStr_ptr_array_array net_cmds;

RefCountStr *str_add(char *string);
void str_ref(RefCountStr *obj);
void str_unref(RefCountStr *obj);
bool vectorize_cmdline(const char *cmdLine, int *argc, char ***argv);
void remove_cmd(RefCountStr_ptr_array_array *list, unsigned index);
int proc_null(int argc, char *argv[]);
int proc_touch(int argc, char *argv[]);
int proc_cp(int argc, char *argv[]);
int proc_mv(int argc, char *argv[]);
int proc_rm(int argc, char *argv[]);

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

int main()
{
	char *curLine = NULL;
	EA_INIT(RefCountStr_ptr_array, net_cmds, 16);
	EA_INIT(RefCountStr_ptr, str_heap, 16);

	/* Process the commands on standard input.  */
	while (exp_getline(stdin, &curLine) != EOF)
	{
		const char *commands[6] = { "touch", "cp", "mv", "rm", "mkdir",
									"rmdir" };
		const unsigned cmdLens[6] = { 5, 2, 2, 2, 5, 5 };
		const CmdFunc cmdfuncs[6] = { proc_touch, proc_cp, proc_mv,
									  proc_rm, proc_null, proc_null };
		char *cmdNameEnd;
		unsigned cmdLen;
		unsigned i;

		getc(stdin); /* Skip '\n' */
		/* Read the command name, then dispatch to the proper function
		   with the command-line arguments.  */
		cmdNameEnd = strchr(curLine, (int)' ');
		cmdLen = cmdNameEnd - curLine;
		for (i = 0; i < 6; i++)
		{
			if (cmdLen == cmdLens[i] && !strncmp(curLine, commands[i], cmdLen))
			{
				int new_argc; char **new_argv;
				unsigned j;
				vectorize_cmdline(curLine, &new_argc, &new_argv);
				(*cmdfuncs[i])(new_argc, new_argv);
				/* `argv' is freed by the processing function.  */
				break;
			}
		}
		if (i == 6)
		{
			*cmdNameEnd = '\0';
			fprintf(stderr, "ERROR: Unknown command: %s\n", curLine);
		}
		free(curLine); curLine = NULL;
	}
	free(curLine); curLine = NULL;

	/* The list of net commands always stays sorted so that touches
	   and copies come first, moves come next, and removes come
	   last.  */

	{ /* Write the net commands to standard output.  */
		unsigned i;
		for (i = 0; i < net_cmds.len; i++)
		{
			unsigned j;
			for (j = 0; j < net_cmds.d[i].len - 1; j++)
			{
				/* TODO: Add better quoting and escaping to this
				   output routine.  */
				if (j == 0)
					fputs(net_cmds.d[i].d[j]->d, stdout);
				else
					printf("\"%s\"", net_cmds.d[i].d[j]->d);
				fputs(" ", stdout);
			}
			printf("\"%s\"", net_cmds.d[i].d[j]->d);
			fputs("\n", stdout);
			EA_DESTROY(RefCountStr_ptr, net_cmds.d[i]);
		}
	}

	/* Cleanup */
	{
		unsigned i;
		for (i = 0; i < str_heap.len; i++)
		{
			free(str_heap.d[i]->d);
			free(str_heap.d[i]);
		}
	}
	EA_DESTROY(RefCountStr, str_heap);
	EA_DESTROY(RefCountStr_ptr_array, net_cmds);
	return 0;
}

/* Add a reference-counted string to the string heap.  After the
   string is added, memory ownership of the string passes on to the
   string heap.  A pointer to the reference counting segment is
   returned.  As always with any kind of garbage collector, this
   reference counter does not make your code invulnerable to the
   problem of unused memory still being allocated.  */
RefCountStr *str_add(char *string)
{
	unsigned sub_pos = 0;
	unsigned sub_len = str_heap.len;
	unsigned offset = 0;
	/* Find where to insert the string in the heap.  */
	/* Basically, do a binary search for the string until the string
	   is known not to exist in the heap.  Then insert the string at
	   the sorted position.  */
	while (sub_len > 0)
	{
		int cmp_result;
		offset = sub_pos + sub_len / 2;
		cmp_result = strcmp(string, str_heap.d[offset]->d);
		if (cmp_result < 0)
		{
			sub_len = offset - sub_pos;
		}
		else if (cmp_result > 0)
		{
			sub_len = sub_len + sub_pos - (offset + 1);
			sub_pos = offset + 1;
		}
		else if (cmp_result == 0)
		{
			/* Do not insert the string.  Increase the reference count
			   instead.  */
			str_heap.d[offset]->count++;
			if (string != str_heap.d[offset]->d)
				free(string);
			return str_heap.d[offset];
		}
	}

	/* Check to the left and at the given offset to see whether to
	   insert before or at the given offset.  */
	if (str_heap.len > 0)
	{
		int low_cmp = 0;
		int cmp_result = strcmp(string, str_heap.d[offset]->d);
		int high_cmp = 0;
		if (offset > 0)
			low_cmp = strcmp(string, str_heap.d[offset-1]->d);
		if (offset < str_heap.len - 1)
			high_cmp = strcmp(string, str_heap.d[offset+1]->d);
		if (low_cmp < 0 && cmp_result < 0)
			offset--;
		else if (cmp_result > 0 && high_cmp < 0)
			offset++;
		/* else if (low_cmp < 0 && cmp_result > 0)
			/\* Do nothing.  *\/;
		else
			fputs("ERROR: Broken algorithm.", stderr); */
	}

	/* Insert the element.  */
	EA_INS(RefCountStr_ptr, str_heap, offset);
	str_heap.d[offset] = (RefCountStr *)malloc(sizeof(RefCountStr));
	str_heap.d[offset]->d = string;
	str_heap.d[offset]->count = 1;
	return str_heap.d[offset];
}

/* Increase the reference count of a string.  */
void str_ref(RefCountStr *obj)
{
	obj->count++;
}

/* Decrease the reference count of a string.  The string is freed and
   the tracking segment is removed if the reference count reaches
   zero.  */
void str_unref(RefCountStr *obj)
{
	obj->count--;
	if (obj->count == 0)
	{
		unsigned i;
		free(obj->d);
		/* Remove the tracking segment from the heap.  */
		for (i = 0; i < str_heap.len; i++)
		{
			if (str_heap.d[i] == obj)
			{
				EA_REMOVE(RefCountStr_ptr, str_heap, i);
				free(obj);
			}
		}
		/* EA_REMOVE(RefCountStr, str_heap, (int)(obj - str_heap.d)); */
	}
}

/* Break up a command-line into argument vectors.  This function only
   has rudimentary parsing capabilities.  In particular, it only
   recognizes backslashes, single quotes and double quotes, with
   rudimentary parsing for each.  Backslash newlines sequences are not
   supported.  */
bool vectorize_cmdline(const char *cmdLine, int *argc, char ***argv)
{
	bool is_valid_syntax = true;
	char_ptr_array loc_argv;
	char_array cur_arg;
	unsigned arg_start = 0, curpos = 0;
	EA_INIT(char_ptr, loc_argv, 16);
	EA_INIT(char, cur_arg, 16);
	while (cmdLine[curpos] != '\0')
	{
		/* Process any quoted sequences.  */
		if (cmdLine[curpos] == '\'')
		{
			curpos++;
			while (cmdLine[curpos] != '\0' && cmdLine[curpos] != '\'')
			{
				EA_APPEND(char, cur_arg, cmdLine[curpos]);
				curpos++;
			}
			if (cmdLine[curpos] == '\'')
				curpos++;
			else
				is_valid_syntax = false;
		}
		if (cmdLine[curpos] == '"')
		{
			curpos++;
			while (cmdLine[curpos] != '\0' && cmdLine[curpos] != '"')
			{
				if (cmdLine[curpos] == '\\')
					curpos++;
				EA_APPEND(char, cur_arg, cmdLine[curpos]);
				curpos++;
			}
			if (cmdLine[curpos] == '"')
				curpos++;
			else
				is_valid_syntax = false;
		}

		/* Process unquoted sequences.  */
		while (cmdLine[curpos] != '\0' &&
			   cmdLine[curpos] != ' ' && cmdLine[curpos] != '\t')
		{
			EA_APPEND(char, cur_arg, cmdLine[curpos]);
			curpos++;
		}

		if (cmdLine[curpos] == '\0' ||
			cmdLine[curpos] == ' ' || cmdLine[curpos] == '\t')
		{
			/* Add the current argument to the argument array.  */
			EA_APPEND(char, cur_arg, '\0');
			EA_APPEND(char_ptr, loc_argv, cur_arg.d);
			cur_arg.d = NULL; /* Memory ownership has passed on.  */
			if (cmdLine[curpos] != '\0')
			{
				/* Prepare for adding the next argument vector.  */
				EA_INIT(char, cur_arg, 16);
				curpos++;
			}
			arg_start = curpos;
		}
	}
	EA_DESTROY(char, cur_arg);
	*argc = loc_argv.len;
	*argv = loc_argv.d;
	return is_valid_syntax;
}

/* Remove a command from a command list.  The command list and the
   index to the command to remove should be given.  */
void remove_cmd(RefCountStr_ptr_array_array *list, unsigned index)
{
	unsigned j;
	for (j = 0; j < list->d[index].len; j++)
		str_unref(list->d[index].d[j]);
	EA_REMOVE(RefCountStr_ptr_array, *list, index);
}

/* Pass a command through without any special processing.  */
int proc_null(int argc, char *argv[])
{
	unsigned i;
	EA_INIT(RefCountStr_ptr, net_cmds.d[net_cmds.len], 16);
	for (i = 0; i < argc; i++)
	{
		RefCountStr *segment = str_add(argv[i]);
		EA_APPEND(RefCountStr_ptr, net_cmds.d[net_cmds.len], segment);
	}
	EA_ADD(RefCountStr_ptr_array, net_cmds);
	free((void *)argv);
	return 0;
}

/* Process a `touch' command.  "Touch" is the generic term for any
   kind of command that requires creating or updating a file.  */
int proc_touch(int argc, char *argv[])
{
	char *src;
	unsigned i;
	bool change_src = false;
	RefCountStr *new_src_name;

	/* Process the command line.  */
	if (argc != 2)
	{
		fputs("touch: Invalid command line.\n", stderr);
		free(argv);
		return 1;
	}
	src = argv[1];

	/* Check existing commands in the net command list.  */
	for (i = 0; i < net_cmds.len; i++)
	{
		RefCountStr_ptr_array *cmd_line = &net_cmds.d[i];
		char *cmd_name = cmd_line->d[0]->d;
		if (!strcmp(cmd_name, "touch") ||
			!strcmp(cmd_name, "cp") ||
			!strcmp(cmd_name, "rm"))
		{
			/* Check if the destination pathname is the same as the
			   pathname we are touching.  */
			if (!strcmp(src, EA_BACK(net_cmds.d[i])->d))
				remove_cmd(&net_cmds, i);
		}
		else if (!strcmp(cmd_name, "mv"))
		{
			/* Check if the destination pathname is the same as the
			   pathname we are touching.  */
			if (!strcmp(src, EA_BACK(net_cmds.d[i])->d))
			{
				/* Modify the touch command that will be added to
				   touch the source file name of the move.  */
				new_src_name = cmd_line->d[cmd_line->len-2];
				str_ref(new_src_name);
				free(src);
				change_src = true;
			}
		}
	}

	/* Add the touch command to the net command list.  */
	/* TODO: Search downward for the insertion position, which is
	   where move or remove commands start appearing.  */
	EA_INIT(RefCountStr_ptr, net_cmds.d[net_cmds.len], 16);
	for (i = 0; i < argc; i++)
	{
		if (i == 1 && change_src)
		{
			EA_APPEND(RefCountStr_ptr, net_cmds.d[net_cmds.len], new_src_name);
		}
		else
		{
			RefCountStr *segment = str_add(argv[i]);
			EA_APPEND(RefCountStr_ptr, net_cmds.d[net_cmds.len], segment);
		}
	}
	EA_ADD(RefCountStr_ptr_array, net_cmds);
	free((void *)argv);
	return 0;
}

/* Process a copy command.  All command-line options must come first.
   Command-line options are ignored, and if a file is a directory, the
   copy command is assumed to be recursive as if an `-R' switch were
   specified.  `-p' is implied whether or not it is actually
   specified.  Only one source file may be specified.  */
int proc_cp(int argc, char *argv[])
{
	char *src, *dest;
	unsigned i;
	bool change_src = false;
	RefCountStr *new_src_name;

	/* Process the command line.  */
	if (argc < 3)
	{
		fputs("cp: Invalid command line.\n", stderr);
		free(argv);
		return 1;
	}
	src = argv[argc-2];
	dest = argv[argc-2];

	/* Check for existing command in the net command list.  */
	for (i = 0; i < net_cmds.len; i++)
	{
		RefCountStr_ptr_array *cmd_line = &net_cmds.d[i];
		char *cmd_name = cmd_line->d[0]->d;
		if (!strcmp(cmd_name, "cp") ||
			!strcmp(cmd_name, "mv"))
		{
			/* Check the source name of this copy command and the
			   destination name of the other command to see if there
			   is a link.  */
			if (!strcmp(src, EA_BACK(net_cmds.d[i])->d))
			{
				/* Change this copy command to point to the most
				   original file name.  */
				new_src_name = cmd_line->d[cmd_line->len-2];
				str_ref(new_src_name);
				free(src);
				change_src = true;
			}
		}
		/* Remove any touch or remove commands that get stomped out by
		   this copy command.  */
		else if ((!strcmp(cmd_name, "rm") || !strcmp(cmd_name, "touch")) &&
				 !strcmp(cmd_line->d[cmd_line->len-1]->d, dest))
			remove_cmd(&net_cmds, i);
	}

	/* Add the copy command to the net command list.  */
	/* TODO: Search downward for the insertion position, which is
	   where move or remove commands start appearing.  */
	EA_INIT(RefCountStr_ptr, net_cmds.d[net_cmds.len], 16);
	for (i = 0; i < argc; i++)
	{
		if (i == argc - 2 && change_src)
		{
			EA_APPEND(RefCountStr_ptr, net_cmds.d[net_cmds.len], new_src_name);
		}
		else
		{
			RefCountStr *segment = str_add(argv[i]);
			EA_APPEND(RefCountStr_ptr, net_cmds.d[net_cmds.len], segment);
		}
	}
	EA_ADD(RefCountStr_ptr_array, net_cmds);
	free((void *)argv);
	return 0;
}

/* Process a move command.  All command-line options must come first.
   Command-line options are ignored.  Only one source file may be
   specified.  */
int proc_mv(int argc, char *argv[])
{
	char *src;
	RefCountStr *dest_name;
	unsigned i;

	/* Process the command line.  */
	if (argc < 3)
	{
		fputs("mv: Invalid command line.\n", stderr);
		free(argv);
		return 1;
	}
	src = argv[argc-2];
	dest_name = str_add(argv[argc-1]);

	/* Check for existing command in the net command list.  */
	for (i = 0; i < net_cmds.len; i++)
	{
		RefCountStr_ptr_array *cmd_line = &net_cmds.d[i];
		char *cmd_name = cmd_line->d[0]->d;
		if (!strcmp(cmd_name, "mv") &&
			!strcmp(src, cmd_line->d[cmd_line->len-1]->d))
		{
			/* Change the existing move command to be the net move
			   command of the move command chain.  */
			str_unref(cmd_line->d[cmd_line->len-1]);
			cmd_line->d[cmd_line->len-1] = dest_name;
			/* We don't need to add a new move command anymore.  */
			return 0;
		}
		else if (!strcmp(cmd_name, "touch") ||
				 !strcmp(cmd_name, "cp"))
		{
			/* Check if the move command stomps out the file of this
			   touch or copy command.  */
			if (!strcmp(dest_name->d, cmd_line->d[cmd_line->len-1]->d))
			{
				/* Delete the relevant touch or copy command.  */
				remove_cmd(&net_cmds, i);
			}
		}
	}

	/* Add the move command to the net command list.  */
	/* TODO: Search downward for the insertion position, which is
	   where the remove commands start appearing.  */
	EA_INIT(RefCountStr_ptr, net_cmds.d[net_cmds.len], 16);
	for (i = 0; i < argc; i++)
	{
		if (i == argc - 1)
		{
			EA_APPEND(RefCountStr_ptr, net_cmds.d[net_cmds.len], dest_name);
		}
		else
		{
			RefCountStr *segment = str_add(argv[i]);
			EA_APPEND(RefCountStr_ptr, net_cmds.d[net_cmds.len], segment);
		}
	}
	EA_ADD(RefCountStr_ptr_array, net_cmds);
	free((void *)argv);
	return 0;
}

/* Process a remove command.  Command-line switches must come first.
   Only one path name may be specified on the command-line.  Recursive
   deletes are implied.  */
int proc_rm(int argc, char *argv[])
{
	char *src;
	unsigned i;

	/* Process the command line.  */
	if (argc < 2)
	{
		fputs("rm: Invalid command line.\n", stderr);
		free(argv);
		return 1;
	}
	src = argv[argc-1];

	/* Check existing commands in the net command list.  */
	for (i = 0; i < net_cmds.len; i++)
	{
		RefCountStr_ptr_array *cmd_line = &net_cmds.d[i];
		char *cmd_name = cmd_line->d[0]->d;
		if (!strcmp(cmd_name, "touch"))
		{
			/* Check if the source name is the same.  */
			char *touch_src = net_cmds.d[i].d[1]->d;
			if (!strcmp(touch_src, src))
				remove_cmd(&net_cmds, i);
		}
		else if (!strcmp(cmd_name, "cp"))
		{
			/* Check if the destination name is the same.  */
			char *cp_dest = cmd_line->d[cmd_line->len-1]->d;
			if (!strcmp(cp_dest, src))
				remove_cmd(&net_cmds, i);
		}
		else if (!strcmp(cmd_name, "mv"))
		{
			/* Check if the destination name is the name.  */
			if (!strcmp(src, EA_BACK(net_cmds.d[i])->d))
			{
				/* Add a remove command for the source of the move.  */
				char **new_argv = (char **)malloc(sizeof(char *) * 2);
				new_argv[0] = (char *)malloc(2 + 1);
				strcpy(new_argv[0], "rm");
				new_argv[1] =
					(char *)malloc(strlen(cmd_line->d[
											  cmd_line->len-2]->d) + 1);
				strcpy(new_argv[1], cmd_line->d[cmd_line->len-2]->d);
				proc_rm(2, new_argv);
				/* Delete the old move command.  */
				remove_cmd(&net_cmds, i);
			}
		}
	}

	/* Add the remove command to the net command list.  */
	EA_INIT(RefCountStr_ptr, net_cmds.d[net_cmds.len], 16);
	for (i = 0; i < argc; i++)
	{
		RefCountStr *segment = str_add(argv[i]);
		EA_APPEND(RefCountStr_ptr, net_cmds.d[net_cmds.len], segment);
	}
	EA_ADD(RefCountStr_ptr_array, net_cmds);
	free(argv);
	return 0;
}
