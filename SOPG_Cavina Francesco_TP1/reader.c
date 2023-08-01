/**
*	File: "reader.c"
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
#define FIFO_NAME	"fifo"
#define BUFFER_SIZE	300


/* private typedefs --------------------------------------------------------- */
typedef enum
{
	DATA = 0,
	SIGNAL = 1,
	ERROR = -1,
} messageType_t;


/* private function prototypes ---------------------------------------------- */
static int8_t validateMessage(uint8_t *message);
static uint8_t* getLog(uint8_t *fullLog);
static uint8_t* getSign(uint8_t *fullSign);


/* private data definition -------------------------------------------------- */
static FILE *signsFile;


/* public function definitions ---------------------------------------------- */
int main(void)
{
	uint8_t inputBuffer[BUFFER_SIZE];
	uint8_t inputLog[BUFFER_SIZE];
	uint8_t inputSignal[BUFFER_SIZE];
	int32_t bytesRead, fd;
	int8_t messageType;
	
	FILE *logFile;

	/* create named FIFO */
	createNamedFifo(FIFO_NAME);
	
	/* open named FIFO */
	fd = openNamedFifo(FIFO_NAME, O_RDONLY);
	
	/* open sign file */
	signsFile = fopen("Sign.txt", "w");
	if(NULL == signsFile) 
	{
		perror("Sign.txt file couldn't be opened");
		exit(EXIT_FAILURE);
	}
	
	/* open log file */
	logFile = fopen("Log.txt", "w");
	if(NULL == logFile) 
	{
		perror("Log.txt file couldn't be opened");
		exit(EXIT_FAILURE);
	}
	
	
	/* Loop until read syscall returns a value <= 0 */
	do
	{
		/* clean input buffer */
		memset(inputBuffer, 0, sizeof(inputBuffer));
		
		/* read data into local buffer */
		if ((bytesRead = read(fd, inputBuffer, BUFFER_SIZE)) == -1)
		{
			perror("read");
			exit(EXIT_FAILURE);
		}
		else
		{
			inputBuffer[bytesRead] = '\0';
			
			/* validate message to check if the format is valid */
			messageType = validateMessage(inputBuffer);
			
			if(messageType == DATA)
			{	
				/* clean input log buffer */
				memset(inputLog, 0, sizeof(inputLog));
				
				/* get useful data */
				memcpy(inputLog, getLog(inputBuffer), sizeof(inputLog));
				
				printf("Reader: read %d bytes: \"%s\".\n\n", (int) strlen((const char *)inputLog), inputLog);
				
				/* write on log file */
				fputs((const char *) inputLog, logFile);
				fputs("\n", logFile);
			}
			else if(messageType == SIGNAL)
			{
				/* clean input signal buffer */
				memset(inputSignal, 0, sizeof(inputSignal));
				
				/* get useful data */
				memcpy(inputSignal, getSign(inputBuffer), sizeof(inputSignal));
				
				printf("Reader: SIGUSR%s received.\n", inputSignal);
				
				/* write on signals file */
				fputs((const char *) inputSignal, signsFile);
				fputs("\n", signsFile);
			}
		}
	}
	while (bytesRead > 0);
	
	
	/* close sign file */
	fclose(signsFile);
	
	/* close log file */
	fclose(logFile);
}


/* private function definitions --------------------------------------------- */
int8_t validateMessage(uint8_t *message)
{
	printf("Message received: %s.\n", message);

	/* if the message format is wrong, then return the message type, else return ERROR */
	if(strncmp((const char *) message, "DATA:", 5) == 0)
	{
		return DATA;
	}
	else if(strncmp((const char *) message, "SIGN:", 5) == 0)
	{
		return SIGNAL;
	}
	else
	{
		printf("Message read has wrong format!\n\n");
		return ERROR;
	}
}

uint8_t* getLog(uint8_t *fullLog)
{
	uint8_t *log;
	
	/* get the data after ":" and return the pointer to it */
	log = (uint8_t *) strtok((char *) fullLog, ":");
	log = (uint8_t *) strtok(NULL, ":");
	
	return log;
}

uint8_t* getSign(uint8_t *fullSign)
{
	uint8_t *sign;
	
	/* get the signal after ":" and return the pointer to it */
	sign = (uint8_t *) strtok((char *) fullSign, ":");
	sign = (uint8_t *) strtok(NULL, ":");
	
	return sign;
}


