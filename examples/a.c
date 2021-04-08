#define num_1 0xAE0000
#define num_2 0x18
#define msleep(tms) ({usleep(tms * 1000);})
#define BUFMAX 1024
#define EOLCHAR '\n'
#define TIMEOUT 10

#define TRIGGER_SET     ioctl(fd_trigger,TIOCMBIS,&DTR_flag);    // set -> LOW
#define TRIGGER_RST     ioctl(fd_trigger,TIOCMBIC,&DTR_flag);    // clear -> HIGH

#include <sys/ioctl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <errno.h>   	 // ERROR Number Definitions
#include <fcntl.h>   	 // File Control Definitions
#include <linux/serial.h>
#include <string.h>
#include <stdlib.h>
#include <termios.h>	 // POSIX Terminal Control Definitions
#include "../lib/arduino/arduino-serial-lib.h"

// uint64_t temp_res_1, temp_res_2;
// uint64_t operand1 = num_1;
// uint64_t operand2 = num_2;
int fd_teensy = -1, fd_trigger = -1;
int DTR_flag = TIOCM_DTR;

int hwvolt_arm(){
	int rc = serialport_write(fd_teensy, "arm\n");
	if(rc==-1) {
		printf("error writing");
		return -1;
	}
    char buf[BUFMAX];
    memset(buf,0,BUFMAX);  //
	serialport_read_lines(fd_teensy, buf, EOLCHAR, BUFMAX, TIMEOUT,2);
	printf("resp:%s",buf);
	return 0;
}

int configure_glitch(int repeat, float v1, int d1, float v2, int d2, float v3) {

	char buf[BUFMAX];
	memset(buf,0,BUFMAX);  //Set buf={0,0,0,0,...}
	if( fd_teensy == -1 ) printf("serial port not opened");
	sprintf(buf, ("%i %1.4f %i %1.4f %i %1.4f\n"), repeat, v1, d1, v2, d2, v3);
	printf("send: %s", buf);
	int rc = serialport_write(fd_teensy, buf);
	if(rc==-1) {
		printf("error writing");
		return -1;
	}
	memset(buf,0,BUFMAX);  //
	serialport_read_lines(fd_teensy, buf, EOLCHAR, BUFMAX, TIMEOUT, 3);
	printf("resp: %s", buf);
	return 0;
}

int fire() {
	ioctl(fd_trigger,TIOCMBIS,&DTR_flag);
}

int reset() {
	ioctl(fd_trigger,TIOCMBIC,&DTR_flag);
}

int multiply() {
    int max_iter = 100000;
    int iterations = 0;
    int fault = 0;

    typedef struct calc_info {
        uint64_t operand1;
        uint64_t operand2;
        uint64_t correct_a;
        uint64_t correct_b;
    } calc_info;

    calc_info* in = malloc(sizeof(calc_info));

    in->operand1 = num_1;
    in->operand2 = num_2;

    fire();
    do {
        iterations++;

        in->correct_a = in->operand1 * in->operand2;
        in->correct_b = in->operand1 * in->operand2;
        
        if (in->correct_a != in->correct_b) {
            fault = 1;
        }
    } while (iterations < max_iter && fault == 0);
    reset();

    if (fault) {
        printf("Fault: occured.\nMultiplication 1: %016lx\nMultiplication 2: %016lx\n", in->correct_a, in->correct_b);
    }
    free(in);
    return fault;
}

int init(char* const serial, char* const trigger, int baudrate) {
    fd_trigger = open(trigger, O_RDWR | O_NOCTTY);
	if(fd_trigger == -1) {
		printf("Trigger serial: could not open port\n");
		return -1;
	}
	printf("Trigger serial: opened port %s\n", trigger);

	// Create new termios struc, we call it 'tty' for convention
	struct termios tty;
	memset(&tty, 0, sizeof tty);

	// Read in existing settings, and handle any error
	if(tcgetattr(fd_trigger, &tty) != 0) {
		printf("Error %i from tcgetattr: %s\n", errno, strerror(errno));
	}

	tty.c_cflag &= ~PARENB; // Clear parity bit, disabling parity (most common)
	tty.c_cflag &= ~CSTOPB; // Clear stop field, only one stop bit used in communication (most common)
	tty.c_cflag |= CS8; // 8 bits per byte (most common)
	tty.c_cflag &= CRTSCTS; // Disable RTS/CTS hardware flow control (most common)
	tty.c_cflag |= CREAD | CLOCAL; // Turn on READ & ignore ctrl lines (CLOCAL = 1)

	tty.c_lflag &= ~ICANON;
	tty.c_lflag &= ~ECHO; // Disable echo
	tty.c_lflag &= ~ECHOE; // Disable erasure
	tty.c_lflag &= ~ECHONL; // Disable new-line echo
	tty.c_lflag &= ~ISIG; // Disable interpretation of INTR, QUIT and SUSP
	tty.c_iflag &= ~(IXON | IXOFF | IXANY); // Turn off s/w flow ctrl
	tty.c_iflag &= ~(IGNBRK|BRKINT|PARMRK|ISTRIP|INLCR|IGNCR|ICRNL); // Disable any special handling of received bytes

	tty.c_oflag &= ~OPOST; // Prevent special interpretation of output bytes (e.g. newline chars)
	tty.c_oflag &= ~ONLCR; // Prevent conversion of newline to carriage return/line feed
	//tty.c_oflag &= ~OXTABS; // Prevent conversion of tabs to spaces (NOT PRESENT ON LINUX)
	//tty.c_oflag &= ~ONOEOT; // Prevent removal of C-d chars (0x004) in output (NOT PRESENT ON LINUX)

	tty.c_cc[VTIME] = 0;    // Wait for up to 1s (10 deciseconds), returning as soon as any data is received.
	tty.c_cc[VMIN] = 0;

	// Set in/out baud rate to be 9600
	cfsetispeed(&tty, B38400);
	cfsetospeed(&tty, B38400);


	struct serial_struct kernel_serial_settings;
	int r = ioctl(fd_trigger, TIOCGSERIAL, &kernel_serial_settings);
	if (r >= 0) {
		kernel_serial_settings.flags |= ASYNC_LOW_LATENCY;
		r = ioctl(fd_trigger, TIOCSSERIAL, &kernel_serial_settings);
		if (r >= 0) printf("set linux low latency mode\n");
	}

	tcsetattr(fd_trigger, TCSANOW, &tty);
	if( tcsetattr(fd_trigger, TCSAFLUSH, &tty) < 0) {
		perror("init_serialport: Couldn't set term attributes");
		return -1;
	}

    if( fd_teensy!=-1 ) {
		serialport_close(fd_teensy);
		printf("closed port %s\n",serial);
	}

	fd_teensy = serialport_init(serial, baudrate);
	if( fd_teensy==-1 ) {
		printf("Teensy Serial: couldn't open port\n");
		return -1;
	}
	printf("Teensy serial: opened port %s\n",serial);
	serialport_flush(fd_teensy);

	return 0;
}

int set_delay(int delay) {
    char buf[BUFMAX];
	memset(buf,0,BUFMAX);  //
	sprintf(buf, ("delay %i\n"), delay);
	printf("send: %s", buf);
	int rc = serialport_write(fd_teensy, buf); // Wait delay_before_glitch_us useconds.
	if(rc==-1) {
		printf("error writing");
		return -1;
	}
    memset(buf,0,BUFMAX);  //
	serialport_read_lines(fd_teensy, buf, EOLCHAR, BUFMAX, TIMEOUT, 2); // Clearing the global buffer
	printf("resp: %s", buf);
    return 0;
}

void v_close() {
    close(fd_teensy);
    close(fd_trigger);
    fd_trigger = -1;
    fd_teensy = -1;
}

int main () {
    char* const serial = "/dev/ttyACM0";
    char* const trigger = "/dev/ttyS0";
    int baudrate = 115200;

    int repeat = 2;
    int tries = 10;
    int delay_before = 200;
    float start_voltage = 1.05;
    int delay_start = 35;
    float undervolting_voltage = 0.815;
    int delay_during= -30;
    float end_voltage = start_voltage;

    system("~/set_freq.sh 3.6GHz");
	sleep(2);
	init(serial, trigger, baudrate);
    // To make the trigger respond faster.
    TRIGGER_RST
    TRIGGER_SET
    TRIGGER_RST

    for (int i = 0; i < tries; i++) {
        undervolting_voltage -= 0.002;
        printf("voltage %f %i\n", undervolting_voltage, i);


        set_delay(delay_before);
        configure_glitch(repeat, start_voltage, delay_start, undervolting_voltage, delay_during, end_voltage);
        hwvolt_arm();

        msleep(300);
        if (multiply()) {
            printf("Fault\n");
            v_close();
            return 1;
        }
        msleep(300);
        printf("\n");
    }
    v_close();
    printf("No fault.\n");
    return 0;
}