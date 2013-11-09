CFLAGS = -Wall -Wextra -std=c99
#CFLAGS += -g
LDFLAGS = $(CFLAGS) -pthreads

#all: server

#server: monitor.o
#	gcc -o $@ $^ $(LDFLAGS)

%.o : %.c
	gcc -c -o $@ $^ $(CFLAGS)

clean:
	rm -f *.o
