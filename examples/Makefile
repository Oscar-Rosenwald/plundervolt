all: main multiply clean

main: main.o plundervolt.o
	gcc -o main main.o plundervolt.o -pthread -lm

main.o: main.c ../lib/plundervolt.h
	gcc -c -g main.c

plundervolt.o: ../lib/plundervolt.h
	gcc -c -g ../lib/plundervolt.c

multiply: faulty\ multiplication.o plundervolt.o
	gcc -o faulty\ multiplication faulty\ multiplication.o plundervolt.o -pthread -lm

faulty\ multiplication.o: faulty\ multiplication.c ../lib/plundervolt.h
	gcc -c -g faulty\ multiplication.c

clean:
	rm *.o
