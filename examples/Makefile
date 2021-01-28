all: main clean

main: main.o plundervolt.o
	gcc -o main main.o plundervolt.o -pthread -lm

main.o: main.c ../lib/plundervolt.h
	gcc -c -g main.c

plundervolt.o: plundervolt.h
	gcc -c -g ../lib/plundervolt.c

clean:
	rm *.o
