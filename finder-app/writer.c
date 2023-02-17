/*ref:https://blog.jaycetyle.com/2018/12/linux-fd-open-close/*/

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <syslog.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <libgen.h>
#include <errno.h>


int main(int argc, char *argv[])
{
	openlog(NULL,0,LOG_USER);
	
	if(argc != 3)
    {
        syslog(LOG_ERR, "Error: Please enter 2 arguments: 1.path of file 2.content you want to write in file\n");
	syslog(LOG_USER, "input sample: ./writer path-of-file content-be-written\n");
	exit(1);
    }

	char *path = argv[1];
    	char *text_str = argv[2];
    	
	/*open file*/
	int fd=open(path, O_CREAT | O_RDWR, S_IRWXU);
	
	if(fd == -1)
	{
		syslog(LOG_ERR, "File could not be created, error type= %s\n",strerror(errno));
		exit(1);	
	
	}



	int byteCount = write(fd, text_str, strlen(text_str));
	
	if(byteCount != strlen(text_str))
	{
		syslog(LOG_ERR, "File could not be written properly, error type = %s\n", strerror(errno));
		exit(1);
	}

	syslog(LOG_DEBUG, "Writing %s to %s\n", text_str, path);
	close(fd);

	return 0;
}
