all: fm_hardware fm_software

fm_hardware:
	gcc faulty_multiplication_hardware.c -pthread -lm -L../lib/ -lplundervolt -o fm_hardware

fm_software:
	gcc faulty_multiplication_software.c -pthread -lm -L../lib/ -lplundervolt -o fm_software
