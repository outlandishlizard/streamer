CFLAGS = -Wall -Wextra -std=c99 
#CFLAGS += -g
LDFLAGS = $(CFLAGS) -pthread

all: test

#server: monitor.o
#	gcc -o $@ $^ $(LDFLAGS)
frame_test	: frame_producer_text.o circular_buffer.o server.o monitor.o mythread.o
	gcc -o $@ $^ $(LDFLAGS)

monitor_test : monitor_test.o monitor.o mythread.o
	gcc -o $@ $^ $(LDFLAGS)

%.o : %.c
	gcc -c -o $@ $^ $(CFLAGS)

clean:
	rm -f *.o monitor_test frame_test
