#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <stdbool.h>
#include <arpa/inet.h> 
#include <fcntl.h>  
#include <signal.h> 
#include <syslog.h> 
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netdb.h>
#include <getopt.h>
#include <netinet/in.h>
#include <pthread.h>
#include <sys/time.h>
#include <sys/queue.h>


#define FILE_OUT_PATH "/var/tmp/aesdsocketdata"
#define MAXSIZE 100
#define MYPORT "9000"  		
#define BACKLOG 10     	
#define TIMESPECADD 1000000000L	
#define TIMER_N_SEC 1000000
#define FD_SIZE 3
#define FD_DATA 0
#define FD_CLIENT 1
#define FD_SOCKET 2
#define FAILURE -1

int fd[FD_SIZE];
int sigFlag=0;	

pthread_mutex_t mutex;	

struct sockaddr_in connection_addr;

struct params
{
    pthread_t thread;
	bool threadFlag;
	int threadFd;
    int tid;
};

struct timerthread
{
	int timerParams;
};

typedef struct slist_data_s slist_data_t;

struct slist_data_s
{
	struct params value;
	SLIST_ENTRY(slist_data_s) entries;
};

//add timespec
static inline void timespec_add( struct timespec *returnValue, const struct timespec *timeStamp1, const struct timespec *timeStamp2)
{
    returnValue->tv_sec = timeStamp1->tv_sec + timeStamp2->tv_sec;
    returnValue->tv_nsec = timeStamp1->tv_nsec + timeStamp2->tv_nsec;

    if(returnValue->tv_nsec > TIMESPECADD) 
    {
        returnValue->tv_nsec -= TIMESPECADD;
        returnValue->tv_sec ++;
    }
}

//setup timer
static bool setup_timer( int clockID, timer_t timerID, unsigned int timer_period_s, struct timespec *startTime)
{
    bool success = false;

    if ( clock_gettime(clockID, startTime) != 0 ) 
    {
        printf("Error %d (%s) getting clock %d time\n",errno,strerror(errno),clockID);
    } 
    else 
    {
        struct itimerspec itimerspec;

        itimerspec.it_interval.tv_sec = timer_period_s;
        itimerspec.it_interval.tv_nsec = TIMER_N_SEC;

        timespec_add(&itimerspec.it_value,startTime, &itimerspec.it_interval);

        if( timer_settime(timerID, TIMER_ABSTIME, &itimerspec, NULL ) != 0 ) 
            printf("Error %d (%s) setting timer\n",errno,strerror(errno));
        else 
            success = true;
    }
    return success;
}

void timer_thread (union sigval sigval)
{
	struct timerthread* timtd = (struct timerthread*) sigval.sival_ptr;

	time_t rawtime;

	struct tm *info;

	char *myTime;

	size_t returnValue;

	time(&rawtime);
    
	info = localtime(&rawtime);

	myTime = (char*)malloc(MAXSIZE*sizeof(char));

    if(myTime == NULL)
    {
        syslog(LOG_ERR, "ERROR: Failed to allocate memory in timer_thread...");
        return;
    }

	returnValue = strftime(myTime, 80, "timestamp: %Y %b %a %H %M %S%n", info);

    if(returnValue == 0)
    {
        perror("ERROR: Failed strftime and returned 0....");
        free(myTime);
    }

	if(pthread_mutex_lock(&mutex)!=0)
    {
		perror("ERROR: Failed to lock mutex...");
		free(myTime);
	}

	int writeReturnValue = write(timtd->timerParams, myTime, returnValue);

	if(writeReturnValue != returnValue)
    {
		syslog(LOG_ERR, "ERROR: Timestamp error...");
		free(myTime);
	}

	if(pthread_mutex_unlock(&mutex)!=0)
    {
		perror("ERROR: Failed to unlock mutex...");
		free(myTime);
	}	

	free(myTime);
}


void* threadHandler(void* thread_param)
{

	struct params* threadParamValues = (struct params*) thread_param;
	
    int memAllocSize = MAXSIZE;
    int sendIndex = 0; 
    int position = 0;
    int endPosition = 0; 
    int nextLineSize = 0;
    int toSendSize = 0;
	char buffer[MAXSIZE];				
    char* bufferAppend = (char*)malloc(MAXSIZE*sizeof(char));										   
	size_t bufferSize = 0;									
	off_t retVal;
	
    //clear buffer
    memset(buffer, '\0', sizeof(buffer));

    int receiveReturnValue = 0;
    int writeReturnValue = 0;
    int readReturnValue = 0;
    char* newlinePosition = NULL; 
    char* newlinePosition2 =NULL;
	char* tempPtr = NULL; 
    char* tempPtr2 = NULL;

    char *IP = inet_ntoa(connection_addr.sin_addr);
    syslog(LOG_DEBUG, "Connection Accepted: %s\n", IP);
		
    do
    {
        //receive data
        receiveReturnValue = recv(threadParamValues->threadFd, buffer, sizeof(buffer), 0);

        //check for error
        if(receiveReturnValue == FAILURE)
        {
            syslog(LOG_ERR, "ERROR: Failed to receive thread param values...");
            return NULL;
        }
        else
        {
            if((memAllocSize-bufferSize) < receiveReturnValue)
            {
                memAllocSize += receiveReturnValue;

                tempPtr = (char*)realloc(bufferAppend, memAllocSize* sizeof(char));

                if(tempPtr!= NULL)
                    bufferAppend=tempPtr;

            }
            //load into buffer
            memcpy(&bufferAppend[bufferSize], buffer, receiveReturnValue);				
        }

        bufferSize+=receiveReturnValue;

        newlinePosition = strchr(buffer, '\n');	

    //keep going until new line found
    }while(newlinePosition == NULL); 


    //lock mutex while writing
    pthread_mutex_lock(&mutex);
    writeReturnValue = write(fd[FD_DATA], bufferAppend, bufferSize);
    if(writeReturnValue == -1)
    {
        syslog(LOG_ERR, "ERROR: Failed to write to file...");
        return NULL;
    }
    //unlock mutex after writing
    pthread_mutex_unlock(&mutex);	


    endPosition = lseek(fd[FD_DATA], 0, SEEK_END);

    retVal = lseek(fd[FD_DATA], 0, SEEK_SET);
    if(retVal == (off_t)FAILURE)
    {
        syslog(LOG_ERR, "ERROR: Failed lseek for end of file position");
        return NULL;
    }

    //while not at the end of file
    while(sendIndex != endPosition)
    {
        toSendSize=0;
        
        char* bufferAppend2 = (char*)malloc(MAXSIZE*sizeof(char));
        if(bufferAppend2 == NULL)
        {
            syslog(LOG_ERR,"ERROR: Failed to malloc appending buffer...");
            break;
        }

        //clear buffer
        memset(bufferAppend2, '\0', MAXSIZE);

        //get size of next line
        nextLineSize = endPosition - position;

        do
        {
            //lock mutex while reading
            pthread_mutex_lock(&mutex);										
            readReturnValue = read(fd[FD_DATA], bufferAppend2+toSendSize, sizeof(char));

            if(readReturnValue == FAILURE)
            {
                    syslog(LOG_ERR,"ERROR: Failed to read...");
                    return NULL;
            }       

            //unlock mutex after reading
            pthread_mutex_unlock(&mutex);        							

            //
            if(memAllocSize - toSendSize < endPosition)
            {
                //add next line size to the current malloc size
                memAllocSize += nextLineSize;
                tempPtr2 = (char*)realloc(bufferAppend2, memAllocSize*sizeof(char)); 	
                if(tempPtr2 != NULL)
                    bufferAppend2 = tempPtr2;

            }

            toSendSize += readReturnValue;

            if(toSendSize>1)
            {
                newlinePosition2 = strchr(bufferAppend2, '\n');    
            }
        }while(newlinePosition2 == NULL);

        position = lseek(fd[FD_DATA], 0, SEEK_CUR);
        sendIndex+=toSendSize;
        
        //send data
        int sendReturn = send(threadParamValues->threadFd, bufferAppend2, toSendSize, 0);
        if(sendReturn == FAILURE)
        {
                syslog(LOG_ERR,"ERROR: Failed to send data...");
                return NULL;
        }
	
        free(bufferAppend2);
	}

    free(bufferAppend);

    //close fd
    close(fd[FD_CLIENT]);	
    syslog(LOG_INFO,"Connection Closed: %s",IP);	   

    //set thread flag
    threadParamValues->threadFlag = true;

    return NULL;
}

//signal handler
void signalHandler(int signalNumber)
{
    if ((signalNumber == SIGINT) || (signalNumber == SIGTERM))
    {
        printf("Caught signal, exiting\n");

        if(shutdown(fd[FD_SOCKET], SHUT_RDWR) == FAILURE)
            perror("ERROR: Failed to shut down socket...");

        sigFlag=1;
    }
}


int main(int argc, char *argv[])
{

    int options = 1;	
    int index = 0; 		
    int clockID = CLOCK_MONOTONIC;	
													
    bool daemon = false;	
	pid_t pid; 																												
    timer_t timerID;			

    struct addrinfo hints;													
	struct addrinfo *res;	
    struct timerthread td;													
    struct sigevent sev;	
    struct timespec startTime;											
											
	//init linked list
	slist_data_t *linkedListPtr = NULL;
	SLIST_HEAD(slisthead, slist_data_s) head;
	SLIST_INIT(&head);

	//init mutex
	pthread_mutex_init(&mutex, NULL);

	//open log
	openlog(NULL,0,LOG_USER);
	
    if (signal(SIGINT, signalHandler) == SIG_ERR)		
            syslog(LOG_ERR,"Failed SIGINT");
    else if (signal(SIGTERM, signalHandler) == SIG_ERR)
            syslog(LOG_ERR,"Failed SIGTERM");
	
	if(argc == 1)
		daemon = false;

	else if(argc > 2)
    {
		syslog(LOG_ERR,"ERROR: Too many arguements...");	
		return FAILURE;
	}

	else if(argc == 2)
    {
        //compare string for -d argument for daemon flag
		if(strcmp(argv[1], "-d") == 0)
			daemon = true;
	}

    //clear memory
	memset(&hints, 0, sizeof(hints)); 

    //set options
	hints.ai_family 	= AF_INET;	
	hints.ai_socktype 	= SOCK_STREAM;	
	hints.ai_flags		= AI_PASSIVE;		
	
	//get address information
    int addressInformation;
 	if ((addressInformation = getaddrinfo(NULL, MYPORT, &hints, &res)) !=0)
     {
	    syslog(LOG_ERR, "ERROR: Failed to get address info...");
   	    return FAILURE;
	}
	
	//get fd for FD_SOCKET
	fd[FD_SOCKET] = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
	if(fd[FD_SOCKET] == FAILURE)
    {
	    syslog(LOG_ERR,"ERROR: Failed to get fd for FD_SOCKET...");
	    return FAILURE;
	}

    //attach socket to port
   	if(setsockopt(fd[FD_SOCKET], SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &options, sizeof(options)))
    	{
        	perror("ERROR: Failed to set socket options...");
        	exit(EXIT_FAILURE);
    	}

	//bind the address and check return value
    int bindReturnValue = bind(fd[FD_SOCKET], res->ai_addr , res->ai_addrlen);
	if(bindReturnValue == FAILURE)
    {
	    syslog(LOG_ERR, "ERROR: Failed to bind address...");
	    return FAILURE;
	}

	freeaddrinfo(res);
	
    //attach socket to port
    if(setsockopt(fd[FD_SOCKET], SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &options, sizeof(options)))
    {
            perror("ERROR: Failed to set socket options...");
            exit(EXIT_FAILURE);
    }

 
    //listen for a connection on SOCKET and check for failure
	int listenReturnValue = listen(fd[FD_SOCKET], BACKLOG);
	if(listenReturnValue == FAILURE)
    {
	    syslog(LOG_ERR, "ERROR: Failed to listen for a connection...");
	    return FAILURE;
	}

        //open file
        fd[FD_DATA] = open(FILE_OUT_PATH, O_CREAT | O_APPEND | O_RDWR, 0666);

        //check for successful file open
        if(fd[FD_DATA] == FAILURE)
        {
               syslog(LOG_ERR, "ERROR: Failed to open file...");
               return FAILURE;
         }
         	
        if(daemon)
        {
                pid = fork();
                if (pid == FAILURE)
                        return FAILURE;
                else if (pid != 0)
                        exit (EXIT_SUCCESS);
                /* create new session and process group */
                if (setsid () == FAILURE)
                        return FAILURE;
                /* set the working directory to the root directory */
                if (chdir ("/") == -1)
                        return -1;

                /* redirect fd's 0,1,2 to /dev/null */
                open ("/dev/null", O_RDWR); /* stdin */
                dup (0); /* stdout */
                dup (0); /* stderror */
        }

	if(daemon == false||pid == 0)
    {
		memset(&sev,0,sizeof(struct sigevent));
	    /**
        * Setup a call to timer_thread passing in the td structure as the sigev_value
        * argument
        */
		td.timerParams = fd[FD_DATA];
        sev.sigev_notify = SIGEV_THREAD;
        sev.sigev_value.sival_ptr = &td;
        sev.sigev_notify_function = timer_thread;

        if ( timer_create(clockID,&sev,&timerID) != 0 ) 
        {
            printf("Error %d (%s) creating timer!\n",errno,strerror(errno));
        }

		if(!(setup_timer(clockID, timerID, 10, &startTime)))
        {
			printf("Timer setup error!!");
		}
	}

    socklen_t addr_size = sizeof connection_addr;

	while(!sigFlag)
    {

                //accept for a connection
                fd[FD_CLIENT] = accept(fd[FD_SOCKET], (struct sockaddr*)&connection_addr, &addr_size);
                
                //if signal flag is set, leave loop
                if(sigFlag)
                	break;
                	
                //check for accept error
                if(fd[FD_CLIENT] == -1)
                {
                    syslog(LOG_ERR, "ERROR: Failed to accept connection... errno:%s", strerror(errno));
                    return FAILURE;
                }
                
				//setup the values in linked list for each entry
				linkedListPtr = malloc(sizeof(slist_data_t));

                //set linked list parameters
				(linkedListPtr->value).threadFd = fd[FD_CLIENT];
				(linkedListPtr->value).tid = index;
				(linkedListPtr->value).threadFlag = false;

				//instert head into linked list
        		SLIST_INSERT_HEAD(&head, linkedListPtr, entries);

                //increment counter
				index++;

			//create thread and check to see if thread create failed
			int returnVal = pthread_create(&((linkedListPtr->value).thread), NULL, threadHandler,(void*)&(linkedListPtr->value));
			if(returnVal != 0)
            {
				perror("ERROR, Failed to create thread..");
				return FAILURE;
			}
				
			//check through linked list
			SLIST_FOREACH(linkedListPtr, &head, entries) 
            {
                //if flag is set
    	    	if((linkedListPtr->value).threadFlag == true)
                {
                    //join threads together
					pthread_join((linkedListPtr->value).thread, NULL);
				}
    		}
	}
	
	//join threads together
	SLIST_FOREACH(linkedListPtr, &head, entries) 
    {
		pthread_join((linkedListPtr->value).thread, NULL);
    }

    //clean up linked list
	while (!SLIST_EMPTY(&head)) 
    {
        linkedListPtr = SLIST_FIRST(&head);

        SLIST_REMOVE_HEAD(&head, entries);

        //free pointer
        free(linkedListPtr);

        linkedListPtr = NULL;
    }

	//close files
	close(fd[FD_DATA]);
	close(fd[FD_SOCKET]);
	close(fd[FD_CLIENT]);

    //close log
	closelog();

    //delete timer
	timer_delete(timerID);

    //remove file
    remove(FILE_OUT_PATH);

	return 0;
}
