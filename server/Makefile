ifdef CROSS_COMPILE
CC = $(CROSS_COMPILE)gcc -g -Wall -Werror
else
CC = gcc -g -Wall -Werror
endif

all: aesdsocket.c
	$(CC) aesdsocket.c -o aesdsocket

default: aesdsocket.c
	$(CC) aesdsocket.c -o aesdsocket	

.PHONY: clean
clean:
	rm -f aesdsocket
