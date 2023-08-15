/*
 * @file   : main.c
 * @date   : Ago, 2023
 * @author : Francesco Cavina <francescocavina98@gmail.com>
 */

/********************** Inclusions *******************************************/
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <pthread.h>
#include <signal.h>

#include "main.h"
#include "SerialManager.h"

/********************** Macros and Definitions *******************************/
#define INTERFACE_SERVICE_SOCKET_IP		("127.0.0.1")
#define INTERFACE_SERVICE_SOCKET_PORT		(10000)
#define	COMM_BUFFER_SIZE			(16)	
#define MUTEX_INIT_KEY				('X')

/********************** Internal Data Declaration ****************************/
typedef enum
{
	RESET = 0,
	SET = 1
} buffersFlag_t;

/********************** Internal Functions Declaration ***********************/
static void* thread_controllerEmulator_tx(void* arg);
static void* thread_controllerEmulator_rx(void* arg);
static void* thread_interfaceService_tx(void* arg);
static void* thread_interfaceService_rx(void* arg);

static void threadsInit(void);
static void serialRead(void);
static void serialWrite(void);
static void socketInit(char* ip, int port);
static void socketRead(void);
static void socketWrite(void);
static void mutexInit(void);
static void signalHandlersInit(void);
static void signalHandlerSIGINT(void);
static void signalHandlerSIGTERM(void);
static void signalBlock(void);
static void signalUnblock(void);

/********************** Internal Data Definition *****************************/
static pthread_t ThreadHandle_controllerEmulator_tx;
static pthread_t ThreadHandle_controllerEmulator_rx;
static pthread_t ThreadHandle_interfaceService_tx;
static pthread_t ThreadHandle_interfaceService_rx;
static int socket_fd;
static char buffer_right[COMM_BUFFER_SIZE];
static char buffer_left[COMM_BUFFER_SIZE];
static buffersFlag_t bufferRightFlag = RESET;
static buffersFlag_t bufferLeftFlag = RESET;
static pthread_mutex_t mutexData = PTHREAD_MUTEX_INITIALIZER;

/********************** External Data Definition *****************************/

/********************** Internal Functions Definition ************************/
static void threadsInit(void)
{
	/* Create both threads: 1 for communication with Controller Emulator and 1 for communication with Interface Service */
	pthread_create(&ThreadHandle_controllerEmulator_tx, NULL, thread_controllerEmulator_tx, NULL);
	pthread_create(&ThreadHandle_controllerEmulator_rx, NULL, thread_controllerEmulator_rx, NULL);
	pthread_create(&ThreadHandle_interfaceService_tx, NULL, thread_interfaceService_tx, NULL);
	pthread_create(&ThreadHandle_interfaceService_rx, NULL, thread_interfaceService_rx, NULL);
}

static void* thread_controllerEmulator_tx(void* arg)
{
	while(1)
	{
		/* Write to Controller Emulator */
		serialWrite();
		
		/* Blocking delay */
		usleep(100000);
	}
	
	return NULL;
}

static void* thread_controllerEmulator_rx(void* arg)
{
	while(1)
	{
		/* Read from Controller Emulator */
		serialRead();
		
		/* Blocking delay */
		usleep(100000);
	}
	
	return NULL;
}

static void* thread_interfaceService_tx(void* arg)
{
	while(1)
  	{
		/* Write to Interface Service */
		socketWrite();
		
		/* Blocking delay */
		usleep(100000);
	}	
	
	return NULL;
}

static void* thread_interfaceService_rx(void* arg)
{
	while(1)
  	{
		/* Read from Interface Service */
		socketRead();
		
		/* Blocking delay */
		usleep(100000);		
	}	
	
	return NULL;
}

static void serialRead(void)
{
	int bytes = 0;
	
	/* Write serial port */
	bytes = serial_receive(buffer_right, sizeof(buffer_right));
	
	if(bytes > 0)
	{
		/* Lock mutex for shared resource */
		pthread_mutex_lock(&mutexData);
		{
			bufferRightFlag = SET;
		}
		/* Unlock mutex for shared resource */
		pthread_mutex_unlock(&mutexData);	
			
		printf("RECEIVED from CONTROLLER EMULATOR: %ld bytes: %s", sizeof(buffer_right), buffer_right);
	}
}

static void serialWrite(void)
{
	/* Read serial port */
	if(SET == bufferLeftFlag)
	{
		serial_send(buffer_left, sizeof(buffer_left));

		/* Lock mutex for shared resource */
		pthread_mutex_lock(&mutexData);
		{		
			bufferLeftFlag = RESET;
		}	
		/* Unlock mutex for shared resource */
		pthread_mutex_unlock(&mutexData);
					
		printf("WROTE to CONTROLLER EMULATOR: %ld bytes: %s\n", sizeof(buffer_left), buffer_left);
	}
}

static void socketInit(char* ip, int port)
{
	socklen_t addr_len;
	struct sockaddr_in clientaddr;
	struct sockaddr_in serveraddr;
		
	/* Create socket */
	int fd = socket(AF_INET,SOCK_STREAM, 0);
	
	/* Load server IP:PORT data */
	bzero((char*) &serveraddr, sizeof(serveraddr));
    	serveraddr.sin_family = AF_INET;
    	serveraddr.sin_port = htons(port);
    	if(inet_pton(AF_INET, ip, &(serveraddr.sin_addr)) <= 0)
    	{
        	fprintf(stderr,"ERROR invalid server IP.\r\n");
        	exit(1);
    	}
    	
    	/* Open TCP port */
	if (bind(fd, (struct sockaddr*)&serveraddr, sizeof(serveraddr)) == -1) 
	{
		close(fd);
		perror("ERROR bind() API");
		exit(1);
	}
	
	/* Set socket in listening mode */
	if (listen (fd, 10) == -1)
  	{
    	    	perror("ERROR listen() API");
    		exit(1);
  	}

 	/* Accept incoming connections */
	addr_len = sizeof(struct sockaddr_in);
	if ((socket_fd = accept(fd, (struct sockaddr *) &clientaddr, &addr_len)) == -1)
	{
	      perror("ERROR accept() API");
	      exit(1);
    	}
 	
 	/* Connection established */
	char ipClient[32];
	inet_ntop(AF_INET, &(clientaddr.sin_addr), ipClient, sizeof(ipClient));
	printf("SERVER: connection from: %s\n\n", ipClient);
}

static void socketRead(void)
{
	int bytes = 0;
	
	/* Read socket */
	bytes  = read(socket_fd, buffer_left, sizeof(buffer_left));	
	if(bytes == -1)
	{
		perror("ERROR while listening socket");
		exit(1);
	}
	
	if(bytes > 0)
	{
		/* Lock mutex for shared resource */
		pthread_mutex_lock(&mutexData);
		{
			bufferLeftFlag = SET;
		}
		/* Unlock mutex for shared resource */
		pthread_mutex_unlock(&mutexData);			

		printf("RECEIVED from INTERFACE SERVICE: %ld bytes: %s", sizeof(buffer_left), buffer_left);
	}
}

static void socketWrite(void) 
{
	/* Write socket */
	if(SET == bufferRightFlag)
	{
		write(socket_fd, buffer_right, sizeof(buffer_right));

		/* Lock mutex for shared resource */
		pthread_mutex_lock(&mutexData);
		{
			bufferRightFlag = RESET;
		}
		/* Unlock mutex for shared resource */
		pthread_mutex_unlock(&mutexData);				
	
		printf("WROTE to INTERFACE SERVICE: %ld bytes: %s\n", sizeof(buffer_right), buffer_right);
	}
}

static void mutexInit(void)
{
	if (pthread_mutex_init(&mutexData, NULL) != 0)
	{
		perror("ERROR pthread_mutex_init() API");
		exit(1);
	}
}

static void signalHandlersInit(void)
{
	/* Set config for replacing the default handler of signal SIGINT */
	struct sigaction signalSIGINT;
	signalSIGINT.sa_handler = &signalHandlerSIGINT;
	signalSIGINT.sa_flags = SA_RESTART;
	sigemptyset(&signalSIGINT.sa_mask);
	
	if(sigaction(SIGINT, &signalSIGINT, NULL) == -1)
	{
		perror("ERROR sigaction(SIGINT) API");
		exit(1);
	}
	
	/* Set config for replacing the default handler of signal SIGTERM */
	struct sigaction signalSIGTERM;
	signalSIGTERM.sa_handler = &signalHandlerSIGTERM;
	signalSIGTERM.sa_flags = SA_RESTART;
	sigemptyset(&signalSIGTERM.sa_mask);
	
	if(sigaction(SIGTERM, &signalSIGTERM, NULL) == -1)
	{
		perror("ERROR sigaction(SIGTERM) API");
		exit(1);
	}
}

static void signalHandlerSIGINT(void)
{
	void* ret;
	
	/* Signal handler for SIGINT */
	write(STDOUT_FILENO, "\nSIGINT signal received.\r\n", 26);
	write(STDOUT_FILENO, "Serial Service closed.\r\n\n", 25);
	
	/* Cancel threads and free resources */
	pthread_cancel(ThreadHandle_controllerEmulator_tx);
	pthread_join(ThreadHandle_controllerEmulator_tx, &ret);
	pthread_cancel(ThreadHandle_controllerEmulator_rx);
	pthread_join(ThreadHandle_controllerEmulator_rx, &ret);
	pthread_cancel(ThreadHandle_interfaceService_tx);
	pthread_join(ThreadHandle_interfaceService_tx, &ret);
	pthread_cancel(ThreadHandle_interfaceService_rx);
	pthread_join(ThreadHandle_interfaceService_rx, &ret);
	
	/* Close connection with Controller Emulator */
	serial_close();
	
	/* Close connection with Interface Service */
	close(socket_fd);
	
	exit(EXIT_SUCCESS);
}

static void signalHandlerSIGTERM(void)
{
	void* ret;

	/* Signal handler for SIGTERM */
	write(STDOUT_FILENO, "\nSIGTERM signal received.\r\n", 27);
	write(STDOUT_FILENO, "Serial Service closed.\r\n\n", 25);
	
	/* Cancel threads and free resources */
	pthread_cancel(ThreadHandle_controllerEmulator_tx);
	pthread_join(ThreadHandle_controllerEmulator_tx, &ret);
	pthread_cancel(ThreadHandle_controllerEmulator_rx);
	pthread_join(ThreadHandle_controllerEmulator_rx, &ret);
	pthread_cancel(ThreadHandle_interfaceService_tx);
	pthread_join(ThreadHandle_interfaceService_tx, &ret);
	pthread_cancel(ThreadHandle_interfaceService_rx);
	pthread_join(ThreadHandle_interfaceService_rx, &ret);
	
	/* Close connection with Controller Emulator */
	serial_close();
	
	/* Close connection with Interface Service */
	close(socket_fd);
	
	exit(EXIT_SUCCESS);
}

static void signalBlock(void)
{
	sigset_t set;
	int s;
	
	sigemptyset(&set);
	sigaddset(&set, SIGINT);
	sigaddset(&set, SIGTERM);
	
	if((pthread_sigmask(SIG_BLOCK, &set, NULL)) != 0)
	{
		perror("ERROR pthread_sigmask(SIG_BLOCK) API");
		exit(1);
	}
}

static void signalUnblock(void)
{
	sigset_t set;
	int s;
	
	sigemptyset(&set);
	sigaddset(&set, SIGINT);
	sigaddset(&set, SIGTERM);
	
	if((pthread_sigmask(SIG_UNBLOCK, &set, NULL)) != 0)
	{
		perror("ERROR pthread_sigmask(SIG_UNBLOCK) API");
		exit(1);
	}
}
	
/********************** External Functions Definition ************************/
int main(void)
{
	printf("Starting Serial Service...\r\n");
	
	/* Set signals' handlers configuration */
	signalHandlersInit();
	
	/* Block signals for main thread */
	signalBlock();
	
	/* Open serial port for communication with Controller Emulator */
	if(serial_open(1, 115200) != 0)
	{
		printf("ERROR while trying to open the serial port.\r\n");
	}
	
	/* Open TCP socket for communication with Interface Service */
	socketInit(INTERFACE_SERVICE_SOCKET_IP, INTERFACE_SERVICE_SOCKET_PORT);
	
	/* Init mutex */
	mutexInit();
	
	/* Init threads */
	threadsInit();	
	
	/* Unblock signals for the main thread */
	signalUnblock();
	
	printf("-=-=-=- Serial Service Running -=-=-=-\r\n\n");
	
	while(1)
	{
		/* Program won't finish until SIGINT or SIGTERM signal is received */
		usleep(10000);
	}
	
	exit(EXIT_SUCCESS);
	return 0;
}

/********************** End of File ******************************************/

