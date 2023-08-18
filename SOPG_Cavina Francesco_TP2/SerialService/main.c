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

/********************** Internal Data Declaration ****************************/
typedef enum
{
	RESET = 0,
	SET = 1
} buffersFlag_t;

typedef enum
{
	CLIENT_DISCONNECTED = 0,
	CLIENT_CONNECTED = 1
} clientStatus_t;

typedef enum
{	
	EXIT = 0,
	RUNNING = 1
} systemStatus_t;	

/********************** Internal Functions Declaration ***********************/
static void* thread_controllerEmulator_tx(void* arg);
static void* thread_controllerEmulator_rx(void* arg);
static void* thread_interfaceService_tx(void* arg);
static void* thread_interfaceService_rx(void* arg);

static void threadsInit(void);
static void serialRead(void);
static void serialWrite(void);
static int socketInit(char* ip, int port);
static void socketRead(void);
static void socketWrite(void);
static void mutexInit(void);
static void signalHandlersInit(void);
static void signalHandlerSIGINT(void);
static void signalHandlerSIGTERM(void);
static void signalBlock(void);
static void signalUnblock(void);

/********************** Internal Data Definition *****************************/
static systemStatus_t systemStatus = RUNNING;					// System

static int socket_fd;								// Socket connection	
socklen_t addr_len;								// Socket connection
struct sockaddr_in clientaddr;							// Socket connection	
struct sockaddr_in serveraddr;							// Socket connection
static clientStatus_t clientStatus = CLIENT_DISCONNECTED;			// Socket connection
	
static pthread_t ThreadHandle_controllerEmulator_tx;				// Thread handler
static pthread_t ThreadHandle_controllerEmulator_rx;				// Thread handler
static pthread_t ThreadHandle_interfaceService_tx;				// Thread handler
static pthread_t ThreadHandle_interfaceService_rx;				// Thread handler 

static pthread_mutex_t mutexData_comm = PTHREAD_MUTEX_INITIALIZER;		// Mutex
static pthread_mutex_t mutexData_systemStatus = PTHREAD_MUTEX_INITIALIZER;	// Mutex

static char buffer_right[COMM_BUFFER_SIZE];					// Cross-communication
static char buffer_left[COMM_BUFFER_SIZE];					// Cross-communication
static buffersFlag_t bufferRightFlag = RESET;					// Cross-communication
static buffersFlag_t bufferLeftFlag = RESET;					// Cross-communication
	
/********************** External Data Definition *****************************/

/********************** Internal Functions Definition ************************/
static void threadsInit(void)
{
	/* Create threads: 2 for communication with Controller Emulator and 2 for communication with Interface Service */
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
	int bytes;
	
	/* Write serial port */
	if(bufferRightFlag != SET)
	{
		bytes = serial_receive(buffer_right, sizeof(buffer_right));
	
		if(bytes > 0)
		{
			/* Lock mutex for shared resource */
			pthread_mutex_lock(&mutexData_comm);
			{
				bufferRightFlag = SET;
			}
			/* Unlock mutex for shared resource */
			pthread_mutex_unlock(&mutexData_comm);	
				
			printf("RECEIVED from CONTROLLER EMULATOR: %ld bytes: %s", sizeof(buffer_right), buffer_right);
		}
	}	
}

static void serialWrite(void)
{
	/* Read serial port */
	if(SET == bufferLeftFlag)
	{
		serial_send(buffer_left, sizeof(buffer_left));

		/* Lock mutex for shared resource */
		pthread_mutex_lock(&mutexData_comm);
		{		
			bufferLeftFlag = RESET;
		}	
		/* Unlock mutex for shared resource */
		pthread_mutex_unlock(&mutexData_comm);
					
		printf("WROTE to CONTROLLER EMULATOR: %ld bytes: %s\n", sizeof(buffer_left), buffer_left);
	}
}

static int socketInit(char* ip, int port)
{
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

	return fd;
}

static void socketRead(void)
{
	int bytes;
	
	if((clientStatus != CLIENT_DISCONNECTED) && (bufferLeftFlag != SET))
	{
		/* Read socket */
		bytes = read(socket_fd, buffer_left, sizeof(buffer_left));

		if(bytes == -1)
		{
			perror("ERROR while listening socket");
			exit(1);
		}
	
		if(bytes > 0)
		{
			/* Lock mutex for shared resource */
			pthread_mutex_lock(&mutexData_comm);
			{
				bufferLeftFlag = SET;
			}
			/* Unlock mutex for shared resource */
			pthread_mutex_unlock(&mutexData_comm);			

			printf("RECEIVED from INTERFACE SERVICE: %ld bytes: %s", sizeof(buffer_left), buffer_left);
		}
		
		if(bytes == 0)
		{
			clientStatus = CLIENT_DISCONNECTED;
		}
	}	
}

static void socketWrite(void) 
{
	/* Write socket */
	if(SET == bufferRightFlag)
	{
		write(socket_fd, buffer_right, sizeof(buffer_right));

		/* Lock mutex for shared resource */
		pthread_mutex_lock(&mutexData_comm);
		{
			bufferRightFlag = RESET;
		}
		/* Unlock mutex for shared resource */
		pthread_mutex_unlock(&mutexData_comm);				
	
		printf("WROTE to INTERFACE SERVICE: %ld bytes: %s\n", sizeof(buffer_right), buffer_right);
	}
}

static void mutexInit(void)
{
	if (pthread_mutex_init(&mutexData_comm, NULL) != 0)
	{
		perror("ERROR pthread_mutex_init() API");
		exit(1);
	}
	
	if (pthread_mutex_init(&mutexData_systemStatus, NULL) != 0)
	{
		perror("ERROR pthread_mutex_init() API");
		exit(1);
	}
}

static void signalHandlersInit(void)
{
	/* Set config for replacing the default handler of signal SIGINT */
	struct sigaction signalSIGINT;
	signalSIGINT.sa_handler = (void *) &signalHandlerSIGINT;
	signalSIGINT.sa_flags = SA_RESTART;
	sigemptyset(&signalSIGINT.sa_mask);
	
	if(sigaction(SIGINT, &signalSIGINT, NULL) == -1)
	{
		perror("ERROR sigaction(SIGINT) API");
		exit(1);
	}
	
	/* Set config for replacing the default handler of signal SIGTERM */
	struct sigaction signalSIGTERM;
	signalSIGTERM.sa_handler = (void *) &signalHandlerSIGTERM;
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
	/* Signal handler for SIGINT */
	write(STDOUT_FILENO, "\nSIGINT signal received.\r\n\n", 26);
	
	/* Lock mutex for shared resource */
	pthread_mutex_lock(&mutexData_systemStatus);
	{
		systemStatus = EXIT;
	}
	/* Unlock mutex for shared resource */
	pthread_mutex_unlock(&mutexData_systemStatus);	
}

static void signalHandlerSIGTERM(void)
{
	/* Signal handler for SIGTERM */
	write(STDOUT_FILENO, "\nSIGTERM signal received.\r\n\n", 27);
	
	/* Lock mutex for shared resource */
	pthread_mutex_lock(&mutexData_systemStatus);
	{
		systemStatus = EXIT;
	}
	/* Unlock mutex for shared resource */
	pthread_mutex_unlock(&mutexData_systemStatus);
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
	int socket_base_fd;	// To open socket for communication with Interface Service
	void* ret;		// For pthread_join() for freeing resources after canceling the threads

	printf("\n-=-=-=- Starting Serial Service -=-=-=-\r\n\n");
	
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
	socket_base_fd = socketInit(INTERFACE_SERVICE_SOCKET_IP, INTERFACE_SERVICE_SOCKET_PORT);
	
	/* Init mutex */
	mutexInit();
	
	/* Init threads */
	threadsInit();	
	
	/* Unblock signals for the main thread */
	signalUnblock();
	
	while(systemStatus != EXIT)
	{
		/* Program won't finish until SIGINT or SIGTERM signal is received */
		
		/* Accept socket incoming connections from client */
		addr_len = sizeof(struct sockaddr_in);
		if((socket_fd = accept(socket_base_fd, (struct sockaddr *) &clientaddr, &addr_len)) == -1)
		{
		      perror("ERROR accept() API");
		      exit(1);
	    	}
	 	
	 	/* Connection established */
		char ipClient[32];
		inet_ntop(AF_INET, &(clientaddr.sin_addr), ipClient, sizeof(ipClient));
		printf("SERVER: connection from: %s\n\n", ipClient);
		
		/* Lock mutex for shared resource */
		pthread_mutex_lock(&mutexData_systemStatus);
		{
			clientStatus = CLIENT_CONNECTED;
		}	
		/* Unlock mutex for shared resource */
		pthread_mutex_unlock(&mutexData_systemStatus);		
		
		printf("-=-=-=- Serial Service Running -=-=-=-\r\n\n");
		
		while(systemStatus != EXIT)
		{
			if(clientStatus == CLIENT_DISCONNECTED)
			{
				break;		
			}
			
			usleep(10000);
		}
		
		printf("-=-=-=- Serial Service Stopped -=-=-=-\r\n\n");
	
		/* Close socket */
		close(socket_fd);
		
		usleep(10000);
	}
	
	printf("-=-=-=- Serial Service Closed -=-=-=-\r\n\n");
	
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
	// -> already closed before
	
	exit(EXIT_SUCCESS);
	return 0;
}

/********************** End of File ******************************************/

