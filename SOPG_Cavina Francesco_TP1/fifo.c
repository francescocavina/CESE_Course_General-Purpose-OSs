/**
*	File: "fifo.c"
*	Author: Francesco Cavina
*
*/

/* includes ----------------------------------------------------------------- */
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdint.h>

#include "fifo.h"


/* defines ------------------------------------------------------------------ */


/* public function prototypes ----------------------------------------------- */


/* public function definitions ---------------------------------------------- */
void createNamedFifo(const char *fifoName) 
{
	int32_t returnCode;
	
	/* Create named fifo. -1 means already exists, so there is no action if already exists */
	if((returnCode = mknod(fifoName, S_IFIFO | 0666, 0)) < -1)
	{
		printf("Error creating named fifo: %d\n", returnCode);
		exit(1);
	}
}

int32_t openNamedFifo(const char *fifoName, int8_t openFlag)
{
	int32_t fd;

	/* Open named fifo. Blocks until other process opens it */
	if(openFlag == O_RDONLY)
	{
		printf("Waiting for writers...\n");
	}
	else if(openFlag == O_WRONLY)
	{
		printf("Waiting for readers...\n");
	}
	
	if((fd = open(fifoName, openFlag)) < 0)
	{
		printf("Error opening named fifo file: %d\n", fd);
		exit(1);
	}
	
	/* open syscalls returned without error -> other process attached to named fifo */
	if(openFlag == O_RDONLY)
	{
		printf("Reader connected.\n\n");
	} 
	else if(openFlag == O_WRONLY)
	{
		printf("Writer connected.\n\n");
	}	
	
	return fd;
}
