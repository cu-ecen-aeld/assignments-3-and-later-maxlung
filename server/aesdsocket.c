#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <syslog.h>

#define PORT "9000"  // the port users will be connecting to

#define BUFSIZE 2000

#define BACKLOG 10   // how many pending connections queue will hold

const char *OUTPUT = "/var/tmp/aesdsocketdata";

int sock_fd= 0;
int new_fd = 0;
ssize_t count = 0;
struct addrinfo *servinfo = NULL;
FILE *f_out = NULL;	 // open output file
FILE *client_socket_fh = NULL; // open socket for buffered reading


void sigchld_handler(int s)
{
	syslog(LOG_INFO, "%s\n", "Caught signal, exiting");
	if (client_socket_fh)
		fclose(client_socket_fh);
	if (new_fd)
		close(new_fd);
	if (f_out)
		fclose(f_out);
	if (sock_fd)
		close(sock_fd);
	if (servinfo)
		freeaddrinfo(servinfo);
	remove(OUTPUT);
	exit(0);
}

int main(int argc, char *argv[])
{
    //int sockfd, new_fd;  // listen on sock_fd, new connection on new_fd
    int status;
    struct addrinfo hints;
    struct sockaddr their_addr; // connector's address information
    socklen_t sin_size;
    struct sigaction sa = {0};
    const int yes=1;
    char buf[BUFSIZE];
    char *input = NULL;
    size_t len = 0;
    size_t size;



    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    sa.sa_handler = sigchld_handler; 
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);


    getaddrinfo("0.0.0.0", PORT, &hints, &servinfo);

    if ((sock_fd = socket(PF_INET, SOCK_STREAM, 0)) == -1)
		return (-1);


    setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

	if ((status = bind(sock_fd, servinfo->ai_addr, servinfo->ai_addrlen)) != 0)
		return (-1);


	if (argc == 2 && strcmp(argv[1], "-d") == 0 && fork()) {
		exit(0);
	}
    if (listen(sock_fd, BACKLOG) == -1) {
        perror("listen");
        exit(1);
    }


    printf("server: waiting for connections...\n");

    while(1) {  // main accept() loop
        sin_size = sizeof their_addr;
        new_fd = accept(sock_fd, (struct sockaddr *)&their_addr, &sin_size);
        if (new_fd == -1) {
            perror("accept");
            continue;
        }

        struct sockaddr_in *client_addr = (struct sockaddr_in *)&their_addr;
        syslog(LOG_INFO, "Accepted connection from %s\n", inet_ntoa(client_addr->sin_addr));


		client_socket_fh = fdopen(new_fd, "rb");

        if(client_socket_fh)
        {
            input = NULL;
			len = 0;
            if ((count = getline(&input, &len, client_socket_fh)) != -1)
			{
				f_out = fopen(OUTPUT, "a+");

				fputs(input, f_out);
				fflush(f_out);
				free(input);	
                   
				size = 0;

				fseek(f_out, 0, SEEK_SET);
				while ((size = fread(buf, sizeof(char), BUFSIZE, f_out)) > 0)
					send(new_fd, buf, size, 0);
				fclose(f_out);
				f_out = NULL;
					

				fclose(client_socket_fh);
				client_socket_fh = NULL;
				close(new_fd);
				new_fd = 0;
				syslog(LOG_INFO, "Closed connection from %s\n", inet_ntoa(client_addr->sin_addr));
				
            }
        }
    }

    return 0;
}
