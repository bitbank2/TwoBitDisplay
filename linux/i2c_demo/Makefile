CFLAGS=-c -Wall -O2 -D_LINUX_
LIBS = -lpthread -larmbianio -lpigpio
all: i2c_demo

i2c_demo: main.o
	$(CC) main.o $(LIBS) -o i2c_demo 

main.o: main.c ../../src/tbd.inl ../../src/TwoBitDisplay.cpp ../../src/TwoBitDisplay.h
	$(CC) $(CFLAGS) main.c

clean:
	rm -rf *.o i2c_demo
