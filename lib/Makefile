all: libplundervolt.a clean

libplundervolt.a: plundervolt.o arduino-serial-lib.o
	ar -rc libplundervolt.a plundervolt.o arduino-serial-lib.o

arduino-serial-lib.o: arduino/arduino-serial-lib.h
	gcc -c -g arduino/arduino-serial-lib.c

plundervolt.o: plundervolt.h
	gcc -c -g plundervolt.c

clean:
	rm *.o
