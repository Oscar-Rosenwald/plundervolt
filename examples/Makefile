# all: main multiply f_m fm a clean
all: fm_hardware fm_software clean

# main: main.o plundervolt.o arduino-serial-lib.o
# 	gcc -o main main.o plundervolt.o arduino-serial-lib.o -pthread -lm
fm_hardware: fm_hardware.o plundervolt.o arduino-serial-lib.o
	gcc -o fm_hardware fm_hardware.o plundervolt.o arduino-serial-lib.o -pthread -lm

# main.o: main.c ../lib/plundervolt.h ../lib/arduino/arduino-serial-lib.h
# 	gcc -c -g main.c
fm_hardware.o: faulty_multiplication_hardware.c ../lib/plundervolt.h ../lib/arduino/arduino-serial-lib.h
	gcc -c -g faulty_multiplication_hardware.c -o fm_hardware.o

fm_software: fm_software.o plundervolt.o arduino-serial-lib.o
	gcc -o fm_software fm_software.o plundervolt.o arduino-serial-lib.o -pthread -lm

fm_software.o: faulty_multiplication_software.c ../lib/plundervolt.h ../lib/arduino/arduino-serial-lib.h
	gcc -c -g faulty_multiplication_software.c -o fm_software.o

# arduino-serial-lib.o: ../lib/arduino/arduino-serial-lib.h
# 	gcc -c -g ../lib/arduino/arduino-serial-lib.c
arduino-serial-lib.o: ../lib/arduino/arduino-serial-lib.h
	gcc -c -g ../lib/arduino/arduino-serial-lib.c

plundervolt.o: ../lib/plundervolt.h
	gcc -c -g ../lib/plundervolt.c

# multiply: faulty_multiplication.o plundervolt.o arduino-serial-lib.o
# 	gcc -o faulty_multiplication faulty_multiplication.o plundervolt.o arduino-serial-lib.o -pthread -lm

# faulty_multiplication.o: faulty\ multiplication.c ../lib/plundervolt.h ../lib/arduino/arduino-serial-lib.h
# 	gcc -c -g faulty\ multiplication.c -o faulty_multiplication.o

# f_m: f_m.o plundervolt.o arduino-serial-lib.o
# 	gcc -o f_m f_m.o plundervolt.o arduino-serial-lib.o -pthread -lm

# f_m.o: faulty_mul2.c ../lib/plundervolt.h ../lib/arduino/arduino-serial-lib.h
# 	gcc -c -g faulty_mul2.c -o f_m.o

# a: a.o arduino-serial-lib.o
# 	gcc -o a a.o arduino-serial-lib.o -lm

# a.o: a.c ../lib/arduino/arduino-serial-lib.h
# 	gcc -c -g a.c -o a.o

# fm: fm.o arduino-serial-lib.o plundervolt.o
# 	gcc -o fm fm.o plundervolt.o arduino-serial-lib.o -pthread -lm

# fm.o: faulty_mul3.c ../lib/plundervolt.h ../lib/arduino/arduino-serial-lib.h
# 	gcc -c -g faulty_mul3.c -o fm.o

clean:
	rm *.o
