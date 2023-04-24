#include <stdio.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <syslog.h>
#include <errno.h>
#include <stdbool.h>


#define PORT "9000"
#define FILE "/var/tmp/aesdsocketdata"
#define BUF_SIZE 1000
#define MAX_CONNECTIONS 8

/*global variables*/
static int sockfd = -1;
static int connfd = -1;
static struct addrinfo  *servaddr;

/*clean up the resources*/
void cleanup()
{
   if(sockfd > -1)
   {
      shutdown(sockfd, SHUT_RDWR);
      close(sockfd);
   }

   if(connfd > -1)
   {
      shutdown(connfd, SHUT_RDWR);
      close(connfd);
   }

   if(servaddr != NULL)
       freeaddrinfo(servaddr);
   
   remove(FILE);

   closelog();
}

/*signals*/
static void sig_handler(int sig)
{
    syslog(LOG_INFO, "Signal Caught %d\n\r", sig);
    
    if(sig == SIGINT)
    {
       cleanup();
    }
    else if(sig == SIGTERM)
    {
       cleanup();
    }
    
    exit(0);
}


int main(int argc, char **argv) 
{
   openlog("aesdsocket", 0, LOG_USER);
   
   
   sig_t retVal = signal(SIGINT, sig_handler);
    
   if (retVal == SIG_ERR) 
   {
       syslog(LOG_ERR, "could not register SIGINT, error SIG_ERR\n\r");
       cleanup();
   }

   retVal = signal(SIGTERM, sig_handler);
   if (retVal == SIG_ERR) 
   {
       syslog(LOG_ERR, "could not register SIGTERM, error SIG_ERR\n\r");
       cleanup();
   }

   bool createdaemon = false;
    
   /*check and createdaemon*/
   if (argc == 2) 
   {
      if (!strcmp(argv[1], "-d")) 
      {
         createdaemon = true;
      } 
      else 
      {
         printf("wrong arg: %s\nUse -d option for daemon", argv[1]);
         syslog(LOG_ERR, "wrong arg: %s\nUse -d option for createdaemon", argv[1]);
         return (-1);
      }
   }

    struct addrinfo addr_hints;

    memset(&addr_hints, 0, sizeof(addr_hints));

    /*initialise server address*/
    addr_hints.ai_family = AF_INET;
    addr_hints.ai_socktype = SOCK_STREAM;
    addr_hints.ai_flags = AI_PASSIVE;
    int result = getaddrinfo(NULL, (PORT), &addr_hints, &servaddr);
    if (result != 0) 
    {
       syslog(LOG_ERR, "getaddrinfo() error %s\n", gai_strerror(result));
       cleanup();
       return -1;
    }

    /*create socket connection*/
    sockfd = socket(servaddr->ai_family, SOCK_STREAM, 0);
    if (sockfd < 0) 
    {
       syslog(LOG_ERR, "socket creation failed, error number %d\n", errno);
       cleanup();
       return -1;
    }

   // Set sockopts for reuse of server socket
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) < 0) 
    {
       syslog(LOG_ERR, "set socket options failed with error number%d\n", errno);
       cleanup();
       return -1;
    }

    // Bind device address to socket
    if (bind(sockfd, servaddr->ai_addr, servaddr->ai_addrlen) < 0) 
    {
       syslog(LOG_ERR, "binding socket error num %d\n", errno);
       cleanup();
       return -1;
    }

    // Listen for connection
    if (listen(sockfd, MAX_CONNECTIONS)) 
    {
       syslog(LOG_ERR, "listening for connection error num %d\n", errno);
       cleanup();
       return -1;
    }

    printf("Listening for connections\n\r");

    if (createdaemon == true) 
    {
       int retVal = daemon(0,0);
       
       if(-1 == retVal)
       {
          syslog(LOG_ERR, "failed to create daemon\n");
          cleanup();
          return -1;
       }
    }

    while(1) 
    {
       struct sockaddr_in cli_addr;
       socklen_t cli_addr_size = sizeof(cli_addr);

       connfd = accept(sockfd, (struct sockaddr*)&cli_addr, &cli_addr_size);
        
       if(connfd < 0)
       {
          syslog(LOG_ERR, "accepting new connection error is %s", strerror(errno));
          cleanup();
          return -1;
       } 
       
       char *client_ip = inet_ntoa(cli_addr.sin_addr);

       
       syslog(LOG_INFO, "Accepted connection from %s \n\r",client_ip);
       printf("Accepted connection from %s\n\r", client_ip);

       char buf[BUF_SIZE];

       /*read the data and write into /var/tmp/aesdsocketdata file*/
       while(1)
       {
          int noOfBytesRead = read(connfd, buf, (BUF_SIZE));
            
          if (noOfBytesRead < 0) 
          {
             syslog(LOG_ERR, "Error: reading from socket errno=%d\n", errno);
             continue; 
          }
           
          
          if (noOfBytesRead == 0)
              continue;

          printf("read %d bytes\n\r", noOfBytesRead);
          
          //open the file for writing
          int fd = open(FILE,O_RDWR | O_CREAT | O_APPEND, 0766);

          if (fd < 0)
             syslog(LOG_ERR, "error opening file errno is %d\n\r", errno);

          int noOfBytesWritten = write(fd, buf, noOfBytesRead);

          if(noOfBytesWritten < 0)
          {
             syslog(LOG_ERR, "Error writing to file errno is %d\n\r", errno);
             close(fd);
             continue;
          }

          close(fd);
          printf("wrote %d bytes\n\r", noOfBytesWritten);

          if (strchr(buf, '\n')) 
          {  //check if we have recieved a newline, if so break
             // from writing into file and go ahead and send packets
             // to client
             break;
          } 

       }

       int read_offset = 0;

       /*read the data from /var/tmp/aesdsocketdata file and send to the client*/
       while(1) 
       {
          int fd = open(FILE, O_RDWR | O_CREAT | O_APPEND, 0766);
            
          if(fd < 0)
          {
             syslog(LOG_ERR, "file open error with errno=%d\n", errno);
             printf("error is %d\n\r", errno);
             continue; 
          }

          /*update file offset*/
          lseek(fd, read_offset, SEEK_SET);
          int noOfBytesRead = read(fd, buf, (BUF_SIZE));
            
          close(fd);
          if(noOfBytesRead < 0)
          {
             syslog(LOG_ERR, "Error reading from file errno %d\n", errno);
             continue;
          }

          if(noOfBytesRead == 0)
             break;

          int noOfBytesWritten = write(connfd, buf, noOfBytesRead);
          printf("wrote %d bytes to client\n\r", noOfBytesWritten);

          if(noOfBytesWritten < 0)
          {
             printf("errno is %d", errno);

             syslog(LOG_ERR, "Error writing to client fd %d\n", errno);
             continue;
          }
            
          read_offset += noOfBytesWritten;
       }

       close(connfd);
        
       connfd = -1;
       syslog(LOG_INFO, "closing client socket\n\r");
    } 

    cleanup();

    return 0;

}
