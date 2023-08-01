/**
*	File: "writer.c"
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
#include <signal.h>

#include "fifo.h"


/* defines ------------------------------------------------------------------ */
#define FIFO_NAME	"fifo"
#define BUFFER_SIZE	300


/* private function prototypes ---------------------------------------------- */
static void writeFifo(char *buffer);
static void signalHandler1(void);
static void signalHandler2(void);


/* private data definition -------------------------------------------------- */
static int32_t fd;


/* public function definitions ---------------------------------------------- */
int main(void)
{
	char outputBuffer[BUFFER_SIZE];
	
	/* create named FIFO */
	createNamedFifo(FIFO_NAME);
	
	/* open named FIFO */
	fd = openNamedFifo(FIFO_NAME, O_WRONLY);
	
	/* set config for replacing the default handler of signal SIGUSR1 */
	struct sigaction signal1;
	signal1.sa_handler = signalHandler1;
	signal1.sa_flags = SA_RESTART;
	sigemptyset(&signal1.sa_mask);
	sigaction(SIGUSR1, &signal1, NULL);
	
	/* set config for replacing the default handler of signal SIGUSR2 */
	struct sigaction signal2;
	signal2.sa_handler = signalHandler2;
	signal2.sa_flags = SA_RESTART;
	sigemptyset(&signal2.sa_mask);
	sigaction(SIGUSR2, &signal2, NULL);

	/* loop forever */
	while (1)
	{
		/* get some text from console */
		fgets(outputBuffer, BUFFER_SIZE, stdin);
		outputBuffer[strcspn(outputBuffer, "\n")] = 0;
		writeFifo(outputBuffer);
	}
}


/* private function definitions --------------------------------------------- */
void writeFifo(char *buffer)
{
	uint32_t bytesWritten;

	/* write buffer to named fifo. Strlen - 1 to avoid sending \n char */
	if ((bytesWritten = write(fd, buffer, strlen(buffer))-1) == -1)
	{
		perror("write");
		exit(EXIT_FAILURE);
	}
	else
	{
		/* log or signal has been sent through the named fifo */
		printf("Writer: wrote %s, %d bytes.\n\n", buffer, bytesWritten);
	}
}

void signalHandler1(void)
{
	/* handler for SIGUSR1 signal */
	printf("Se recibió SIGUSR1.\n");
	
	writeFifo("SIGN:1");
}	

void signalHandler2(void)
{
	/* handler for SIGUSR2 signal */
	printf("Se recibió SIGUSR2.\n");
	
	writeFifo("SIGN:2");
}


