CFLAGS=-c -Wall -O2 -D_LINUX_ -DSPI_BUS_NUMBER=0
LIBS = -lpthread -larmbianio -lpigpio
all: spi_demo

spi_demo: main.o
	$(CC) main.o $(LIBS) -o spi_demo 

main.o: main.c ../../src/tbd.inl ../../src/TwoBitDisplay.cpp ../../src/TwoBitDisplay.h
	$(CC) $(CFLAGS) main.c

clean:
	rm -rf *.o spi_demo
