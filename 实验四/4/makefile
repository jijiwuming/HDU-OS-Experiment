CFLAGS +=-std=c11
msgqueue:msgqueue.o
	cc -pthread -o msgqueue msgqueue.o 
.PHONY : clean
clean:
	rm msgqueue.o
	rm msgqueue