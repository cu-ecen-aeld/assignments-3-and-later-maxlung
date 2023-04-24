CC=$(CROSS_COMPILE)gcc
CFLAGS=-Werror -Wall -g

all: aesdsocket
default: aesdsocket

aesdsocket: aesdsocket.o
	$(CC) -o aesdsocket aesdsocket.o	

appl=aesdsocket

.PHONY : clean
clean :
	rm -f $(appl) *.o
