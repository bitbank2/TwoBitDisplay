CFLAGS=-c -Wall -O2 -DSPI_BUS_NUMBER=0 -D_LINUX_
LIBS = -lpthread -lcurl -laccel -larmbianio
all: spi_clock

spi_clock: main.o
	$(CC) main.o $(LIBS) -o spi_clock 

main.o: main.c ../../src/tbd.inl ../../src/TwoBitDisplay.cpp ../../src/TwoBitDisplay.h
	$(CC) $(CFLAGS) main.c

clean:
	rm -rf *.o spi_clock
