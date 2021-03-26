all: main multiply f_m clean

main: main.o plundervolt.o arduino-serial-lib.o
	gcc -o main main.o plundervolt.o arduino-serial-lib.o -pthread -lm

main.o: main.c ../lib/plundervolt.h ../lib/arduino/arduino-serial-lib.h
	gcc -c -g main.c

arduino-serial-lib.o: ../lib/arduino/arduino-serial-lib.h
	gcc -c -g ../lib/arduino/arduino-serial-lib.c

plundervolt.o: ../lib/plundervolt.h
	gcc -c -g ../lib/plundervolt.c

multiply: faulty_multiplication.o plundervolt.o arduino-serial-lib.o
	gcc -o faulty_multiplication faulty_multiplication.o plundervolt.o arduino-serial-lib.o -pthread -lm

faulty_multiplication.o: faulty\ multiplication.c ../lib/plundervolt.h ../lib/arduino/arduino-serial-lib.h
	gcc -c -g faulty\ multiplication.c -o faulty_multiplication.o

f_m: f_m.o plundervolt.o arduino-serial-lib.o
	gcc -o f_m f_m.o plundervolt.o arduino-serial-lib.o -pthread -lm

f_m.o: faulty_mul2.c ../lib/plundervolt.h ../lib/arduino/arduino-serial-lib.h
	gcc -c -g faulty_mul2.c -o f_m.o

clean:
	rm *.o
