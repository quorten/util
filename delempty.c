/* delempty.c -- Deletes empty directories */

#include <dirent.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "exparray.h"

typedef char* char_ptr;

EA_TYPE(char_ptr);

enum DIR_STATUSCODE {DIR_EMPTY, DIR_HAS_CONTENTS, UNKNOWN_ERROR};

enum DIR_STATUSCODE SearchDirectory(char *dirname);

int main()
{
    SearchDirectory(".");
    return 0;
}

enum DIR_STATUSCODE SearchDirectory(char *dirname)
{
    char_ptr_array dirContentList; /* Holds the directory contents */
    DIR *d;
    struct dirent *de;
    d = opendir(dirname);
    if (d == NULL)
        return UNKNOWN_ERROR; /* most likely not a directory */
    EA_INIT(char_ptr, dirContentList, 128);
    while (de = readdir(d))
    {
        if (strcmp(de->d_name, ".") != 0 && strcmp(de->d_name, "..") != 0)
        {
            dirContentList.d[dirContentList.len] =
                (char*)malloc(de->d_namlen + 1);
            strcpy(dirContentList.d[dirContentList.len], de->d_name);
            EA_ADD(char_ptr, dirContentList);
        }
    }
    closedir(d);
    if (dirContentList.len == 0)
    {
        EA_DESTROY(char_ptr, dirContentList);
        return DIR_EMPTY;
    }
    else
    {
        chdir(dirname);
        while (dirContentList.len > 0)
        {
            enum DIR_STATUSCODE status = UNKNOWN_ERROR;
            dirContentList.len--;
            status =
                SearchDirectory(dirContentList.d[dirContentList.len]);
            if (status == DIR_EMPTY)
                rmdir(dirContentList.d[dirContentList.len]);
            free(dirContentList.d[dirContentList.len]);
        }
        chdir("..");
        /* Do a recall to see if this directory is empty now.  */
        d = opendir(dirname);
        while (de = readdir(d))
        {
            if (!strcmp(de->d_name, ".") &&
                !strcmp(de->d_name, ".."))
            {
                dirContentList.d[dirContentList.len] =
                    (char*)malloc(de->d_namlen + 1);
                strcpy(dirContentList.d[dirContentList.len], de->d_name);
                EA_ADD(char_ptr, dirContentList);
            }
        }
        closedir(d);
        if (dirContentList.len == 0)
        {
            EA_DESTROY(char_ptr, dirContentList);
            return DIR_EMPTY;
        }
    }
    EA_DESTROY(char_ptr, dirContentList);
    return DIR_HAS_CONTENTS;
}
