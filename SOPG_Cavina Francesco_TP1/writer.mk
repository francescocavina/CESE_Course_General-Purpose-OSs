CC = gcc

writer: writer.o fifo.o
	gcc -o writer writer.o fifo.o
	
writer.o: writer.c
	gcc -Wall -c writer.c
	
fifo.o: fifo.c
	gcc -Wall -c fifo.c

