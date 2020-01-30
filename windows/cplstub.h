#ifndef CPLFUNC_H
#define CPLFUNC_H

#define WIN32_LEAN_AND_MEAN /* This might not be neccisary since it
			       excludes things that we need to include */
#include <windows.h>
#include <cpl.h>
#include <shellapi.h>

#include "../bool.h"

HANDLE hInst = NULL;

#define NUM_SUBPROGS 1

bool bCallProgram[NUM_SUBPROGS] = {1}; /* either call external program
					  or call internal function */
typedef void(* CplFunction)(LPARAM);
CplFunction ResponseFunc[NUM_SUBPROGS] = {NULL};

#endif
