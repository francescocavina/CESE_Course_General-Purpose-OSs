CC = gcc

reader: reader.o fifo.o
	gcc -o reader reader.o fifo.o

reader.o: reader.c
	gcc -Wall -c reader.c

fifo.o: fifo.c
	gcc -Wall -c fifo.c

