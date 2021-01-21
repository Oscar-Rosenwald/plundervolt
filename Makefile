all: main

main: main.o plundervolt.o
	gcc -o main main.o plundervolt.o -pthread -lm

main.o: main.c plundervolt.h
	gcc -c -g main.c

plundervolt.o: plundervolt.h
	gcc -c -g plundervolt.c

clean:
	rm *.o
