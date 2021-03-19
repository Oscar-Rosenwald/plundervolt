all: main multiply clean

main: main.o plundervolt.o arduino-serial-lib.o
	gcc -o main main.o plundervolt.o arduino-serial-lib.o -pthread -lm

main.o: main.c ../lib/plundervolt.h ../lib/arduino/arduino-serial-lib.h
	gcc -c -g main.c

arduino-serial-lib.o: ../lib/arduino/arduino-serial-lib.h
	gcc -c -g ../lib/arduino/arduino-serial-lib.c

plundervolt.o: ../lib/plundervolt.h
	gcc -c -g ../lib/plundervolt.c

multiply: faulty\ multiplication.o plundervolt.o arduino-serial-lib.o
	gcc -o faulty\ multiplication faulty\ multiplication.o plundervolt.o arduino-serial-lib.o -pthread -lm

faulty\ multiplication.o: faulty\ multiplication.c ../lib/plundervolt.h
	gcc -c -g faulty\ multiplication.c

clean:
	rm *.o
