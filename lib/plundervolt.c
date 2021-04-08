/**
 * @brief Library for undervolting; Version 5 - Hardware undervolting, Simple (single thread) with Teensy
 */
/* Always compile with "-pthread".
Always run after "sudo modprobe msr" */

#define _GNU_SOURCE
#define BUFMAX 1024
#define EOL '\n'
#define msleep(tms) ({usleep(tms * 1000);})

#include <fcntl.h>
#include <curses.h>
#include <immintrin.h>
#include <limits.h>
#include <pthread.h>
#include <sched.h>
#include <errno.h>
#include <fcntl.h>   	 // File Control Definitions
#include <linux/serial.h>
#include <termios.h>	 // POSIX Terminal Control Definitions
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <x86intrin.h>
#include <stdarg.h>
#include <sys/ioctl.h>
#include "arduino/arduino-serial-lib.h"
#include "plundervolt.h"

int fd_teensy = 0, fd_trigger = 0;
int initialised = 0;
plundervolt_specification_t u_spec;
uint64_t current_undervoltage;
int loop_finished = 0; // When the user wishes to stop all loops of undervolting, they set this to 1. See plundervolt_set_loop_finished().
int debugging_level = 1; // Controlls how many print statements are used. See plundervolt_debug().
int teensy_response_level = 3; // At what debugging_level do we show Teensy responses to the sent commands.
int DTR_flag = TIOCM_DTR;


/**
 * @brief Run function given in u_spec only once.
 * 
 * @return void* Whatever the function returns, return a pointer to it.
 */
void* run_function(void *);
/**
 * @brief Run function given in u_spec in a loop. If u_spec.integrated_loop_check = 1,
 * stop the loop when loop_finished = 1. When u_spec.integrated_loop_check = 0,
 * stop loop when u_spec.stop_loop return 1 and stop the undervolting process.
 * 
 * @return void* Whatever the function returns, return a pointer to it.
 */
void* run_function_loop(void *);
/**
 * @brief Check if u_spec was given in the corect format.
 * 
 * @return int 0 if all is OK. If not, return an error code.
 */
int faulty_undervolting_specification();
/**
 * @brief Check if /dev/cpu/0/msr is accessible.
 * Attemps to open the file and gives feedback if fails.
 * sets fd.
 * @return 1 if accessible; 0 if not.
 */
int msr_accessible_check();
/**
 * @brief Sets fd. If Software undervolting, connect to /dev/cpu/0/msr. If Hardware undervolting, connect to Teensy.
 * 
 * @return int PLUNDERVOLT_NO_ERROR if connection opened. If not, returns the appropriate error for what happened.
 */
int open_file();
/* /\** */
/*  * @brief Print a formatted string for debugging according to the level. */
/*  *  */
/*  * @param level String is printed if level <= debugging_level. 0-4 */
/*  * @param string Formatted string to print. */
/*  *\/ */
/* void debug_print(int level, const char *string, ...); */
/**
 * @brief Run function u_spec.function with arguments u_spec.arguments given number of times.
 * 
 * @return Whatever the function returns.
 */
void* run_function_times(int, void *);
/**
 * @brief Sends the delay information to Teensy.
 * 
 * @param delay Delay in ms.
 * @return int Error code.
 */
int set_delay(int delay);

/* void plundervolt_set_teensy_response_level(int level) { */
/*     teensy_response_level = level; */
/* } */

/* void plundervolt_debug(int level) { */
/*     if (level < 0 || level > 4) { */
/*         fprintf(stderr, "Argument \"level\" of function \"plundervolt_debug()\" must be in the interval <0;4>. Was %d\n. Debugging level stayed at %d\n\n", level, debugging_level); */
/*         return; */
/*     } */
/*     debugging_level = level; */
/* } */

/* void debug_print(int level, const char *string, ...) { */
/*     va_list arguments; */
/*     // int format_num = 0; */
/*     // for(int i=0; string[i]; i++) { */
/*     //     if(s[i] == '%') { */
/*     //         format_num++; */
/*     //     } */
/*     // } */
/*     va_start(arguments, string); */

/*     if (level <= debugging_level) { */
/*         vprintf(string, arguments); */
/*     } */
/*     va_end(arguments); */
/* } */

uint64_t plundervolt_get_current_undervoltage() {
    return current_undervoltage;
}

void plundervolt_set_loop_finished() {
    loop_finished = 1;
}

int msr_accessible_check() {

    // Only open the file if it has not been open before.
    if (fd_teensy == 0) {
        fd_teensy = open("/dev/cpu/0/msr", O_RDWR);
    }
    if (fd_teensy == -1) { // msr file failed to open
        return PLUNDERVOLT_CANNOT_ACCESS_MSR_ERROR;
    }
    return 0;
}

uint64_t plundervolt_compute_msr_value(int64_t value, uint64_t plane) {
    value=(value*1.024)-0.5; // -0.5 to deal with rounding issues
	value=0xFFE00000&((value&0xFFF)<<21);
	value=value|0x8000001100000000;
	value=value|(plane<<40);
	return (uint64_t)value;
}

double plundervolt_read_voltage() {
    if (!msr_accessible_check()) {
        // TODO Error
        return PLUNDERVOLT_GENERIC_ERROR;
    }
    uint64_t msr;
    __off_t offset = 0x198; // TODO Why this number?
    uint64_t number = 0xFFFF00000000; //TODO And this is what?
    double magic = 8192.0; // TODO Ha?
    int shift_by = 32; // TODO Why >>32?
    pread(fd_teensy, &msr, sizeof msr, offset);
    double res = (double)((msr & number)>>shift_by);
    return res / magic;
}

// TODO plundervolt_set_voltage(value);

void plundervolt_set_undervolting(uint64_t value) {
    // 0x150 is the offset of the Plane Index buffer in msr (see Plundervolt paper).
    off_t offset = 0x150;
    pwrite(fd_teensy, &value, sizeof(value), offset);
}

void* run_function_loop (void* arguments) {

    while (true) {
        if (loop_finished){
            break;
        }
        if (!u_spec.integrated_loop_check &&
            (u_spec.stop_loop)(u_spec.loop_check_arguments)) {
                plundervolt_set_loop_finished();
                break;
        }
        (u_spec.function)(arguments);
    }
    return NULL;
}

void* run_function (void * arguments) {

    (u_spec.function)(arguments);
    return NULL;
}

void* run_function_times (int times, void * arguments) {
    for (int i = 0; i < times; i++) {
        (u_spec.function)(arguments);
    }
}

void* plundervolt_apply_undervolting(void *error_maybe) {
    plundervolt_error_t *error_check_thread = (plundervolt_error_t *) error_maybe;

    if (u_spec.u_type == software) {
        // SOFTWARE undervolting

        cpu_set_t cpuset;
        pthread_t thread = pthread_self();
        // TODO What is this?
        CPU_ZERO(&cpuset);
        CPU_SET(0, &cpuset);

        int set_affinity = pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset);
        if (set_affinity != 0) {
            // TODO Better error
            *error_check_thread = PLUNDERVOLT_GENERIC_ERROR;
            pthread_exit(NULL);
        }

        current_undervoltage = u_spec.start_undervoltage;

        while(u_spec.end_undervoltage <= current_undervoltage && !loop_finished) {

            plundervolt_set_undervolting(plundervolt_compute_msr_value(current_undervoltage, 0));
            plundervolt_set_undervolting(plundervolt_compute_msr_value(current_undervoltage, 2));

            msleep(u_spec.wait_time);

            current_undervoltage -= u_spec.step;
        }
    } else {
        // HARDWARE undervolting

        int error_check;
        int iterations = 0;

        while (!loop_finished && iterations < u_spec.tries) {
            // This will make the system respond faster
            plundervolt_reset_voltage();
            plundervolt_fire_glitch();
            plundervolt_reset_voltage();

            iterations++;

            error_check = plundervolt_configure_glitch(u_spec.delay_before_undervolting, u_spec.repeat, u_spec.start_voltage, u_spec.duration_start, u_spec.undervolting_voltage, u_spec.duration_during, u_spec.end_voltage);
            // error_check = plundervolt_configure_glitch(u_spec.delay_before_undervolting, 1, u_spec.start_voltage, u_spec.duration_start, u_spec.undervolting_voltage, u_spec.duration_during, u_spec.end_voltage);
            if (error_check) { // If not 0
                plundervolt_set_loop_finished(); // Stops this loop
                // TODO Handle error
                *error_check_thread = error_check;
                pthread_exit(NULL);
            }

            error_check = plundervolt_arm_glitch();
            if (error_check) { // If not 0
                plundervolt_set_loop_finished(); // Stops this loop
                // TODO Handle error
                *error_check_thread = error_check;
                pthread_exit(NULL);
            }

            msleep(u_spec.wait_time); // Give the machine time to work.

            // The function must call plundervolt_fire_glitch() itself.
            // This is done because of the timing of Teensy. We wouldn't want to undervolt
            // too soon, so we let the user decide when to run the function.
            // WARNING: The user must also reset the voltage with plundervolt_reset_voltage()!
            if (u_spec.loop) {
                if (u_spec.integrated_loop_check) {
                    run_function_loop(u_spec.arguments);
                } else {
                    run_function_times(u_spec.loop, u_spec.arguments);
                }
            } else {
                run_function(u_spec.arguments);
            }
            msleep(u_spec.wait_time);
        }
    }

    plundervolt_set_loop_finished();
    return NULL; // Must return something, as pthread_create requires a void* return value.
}

void plundervolt_reset_voltage() {
    if (u_spec.u_type == hardware && u_spec.using_dtr) {
        ioctl(fd_trigger,TIOCMBIC,&DTR_flag);
    } else {
        plundervolt_set_undervolting(plundervolt_compute_msr_value(0, 0));
        plundervolt_set_undervolting(plundervolt_compute_msr_value(0, 2));
        sleep(3);
    }
    
}

plundervolt_specification_t plundervolt_init () {
    plundervolt_specification_t spec;
    spec.arguments = NULL;
    spec.step = 1;
    spec.loop = 1;
    spec.threads = 1;
    spec.start_undervoltage = 0;
    spec.end_undervoltage = 0;
    spec.function = NULL;
    spec.integrated_loop_check = 0;
    spec.stop_loop = NULL;
    spec.loop_check_arguments = NULL;
    spec.undervolt = 1;
    spec.wait_time = 4000000;
    spec.u_type = software;

    spec.teensy_baudrate = 115200;
    spec.teensy_serial = "";
    spec.trigger_serial = "";
    spec.using_dtr = 1;
    spec.repeat = 1;
    spec.delay_before_undervolting = 0;
    spec.duration_start = 35;
    spec.duration_during = -25; // TODO Why negative?
    spec.start_voltage = 0.900;
    spec.undervolting_voltage = 0.900;
    spec.end_voltage = 0.900;
    spec.tries = 1;
    spec.using_dtr = 1;

    initialised = 1;

    return spec;
}

int plundervolt_set_specification(plundervolt_specification_t spec) {
    if (!initialised) {
        return PLUNDERVOLT_NOT_INITIALISED_ERROR;
    }
    u_spec = spec;
    return PLUNDERVOLT_NO_ERROR;
}

int faulty_undervolting_specification() {
    if (!initialised) {
        return PLUNDERVOLT_NOT_INITIALISED_ERROR;
    }
    if (u_spec.undervolt) {
        if (u_spec.start_undervoltage <= u_spec.end_undervoltage) {
            return PLUNDERVOLT_RANGE_ERROR;
        }
    }
    if (u_spec.function == NULL) {
        return PLUNDERVOLT_NO_FUNCTION_ERROR;
    }
    if (u_spec.loop && !u_spec.integrated_loop_check && u_spec.stop_loop == NULL) {
        return PLUNDERVOLT_NO_LOOP_CHECK_ERROR;
    }
    if (u_spec.teensy_serial == "") {
        return PLUNDERVOLT_NO_TEENSY_SERIAL_ERROR;
    }
    if (u_spec.trigger_serial == "") {
        return PLUNDERVOLT_NO_TRIGGER_SERIAL_ERROR;
    }

    return 0;
}

int plundervolt_init_hardware_undervolting(char* const teensy_serial, char* const trigger_serial, int teensy_baudrate) {

    if (!initialised) {
        return PLUNDERVOLT_NOT_INITIALISED_ERROR;
    }

    if (u_spec.using_dtr) {
        fd_trigger = open(trigger_serial, O_RDWR | O_NOCTTY);
        if(fd_trigger == -1) {
            return PLUNDERVOLT_CONNECTION_INIT_ERROR;
        }

        // Create new termios struc, we call it 'tty' for convention
        struct termios tty;
        memset(&tty, 0, sizeof tty);

        // Read in existing settings, and handle any error
        if(tcgetattr(fd_trigger, &tty) != 0) {
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
        }

        tcsetattr(fd_trigger, TCSANOW, &tty);
        if( tcsetattr(fd_trigger, TCSAFLUSH, &tty) < 0) {
            perror("init_serialport: Couldn't set term attributes");
            return PLUNDERVOLT_CONNECTION_INIT_ERROR;
        }
    }

    // If fd open, close it first - we'll restart the connection
    if (fd_teensy != 0) {
        serialport_close(fd_teensy);
    }

    // Open the connection to Teensy
    fd_teensy = serialport_init(teensy_serial, teensy_baudrate);
    if (fd_teensy == -1) { // Connection failed to open.
        return PLUNDERVOLT_CONNECTION_INIT_ERROR;
    }
    serialport_flush(fd_teensy); // TODO Why?

    return PLUNDERVOLT_NO_ERROR;
}

int plundervolt_arm_glitch() {
    int error_check = serialport_write(fd_teensy, "arm\n");
    if (error_check == -1) { // Write to Teensy failed
        return PLUNDERVOLT_WRITE_TO_TEENSY_ERROR;
    }
    char buf[BUFMAX];
    memset(buf,0,BUFMAX);
	serialport_read_lines(fd_teensy, buf, EOL, BUFMAX, 10,2);
	printf("Teensy response: %s",buf);
    return PLUNDERVOLT_NO_ERROR;
}

int plundervolt_fire_glitch() {
    if (u_spec.using_dtr) {
        ioctl(fd_trigger, TIOCMBIS, &DTR_flag);
    } else {
        int error_check = write(fd_teensy, "\n", 1);
        if (error_check != 1) { // Write to Teensy failed
            return PLUNDERVOLT_WRITE_TO_TEENSY_ERROR;
        }
    }
    return PLUNDERVOLT_NO_ERROR;
}

void plundervolt_teensy_read_response2(int level, int lines) {
    if (debugging_level >= level) {
        char buffer[BUFMAX];
        memset(buffer, 0, BUFMAX); // Wipe buffer
        serialport_read_lines(fd_teensy, buffer, EOL, BUFMAX, 10, lines); // Read response
        // TODO Why Timeout = 10?
    }
}

void plundervolt_teensy_read_response(int level) {
    if (debugging_level >= level) {
        char buffer[BUFMAX];
        memset(buffer, 0, BUFMAX); // Wipe buffer
        serialport_read_lines(fd_teensy, buffer, EOL, BUFMAX, 10, 3); // Read response
        // TODO Why Timeout = 10?
    }
}

int set_delay(int delay) {
    char buf[BUFMAX];
	memset(buf,0,BUFMAX);  //
	sprintf(buf, ("delay %i\n"), delay);
	printf("send: %s", buf);
	int error_check = serialport_write(fd_teensy, buf); // Wait delay_before_glitch_us useconds.
	if(error_check == -1) {
		printf("error writing");
		return PLUNDERVOLT_WRITE_TO_TEENSY_ERROR;
	}
    memset(buf,0,BUFMAX);  //
	serialport_read_lines(fd_teensy, buf, EOL, BUFMAX, 10, 2); // Clearing the global buffer
	printf("resp: %s", buf);
    // plundervolt_teensy_read_response2(1, 2);
    return 0;
}

int plundervolt_configure_glitch(int delay_before_undervolting, int repeat, float start_voltage, int duration_start, float undervolting_voltage, int duration_during, float end_voltage) {
    if (fd_teensy == -1) { // Teensy not opened properly
        return PLUNDERVOLT_CONNECTION_INIT_ERROR;
    }

    char buffer[BUFMAX];
    memset(buffer, 0, BUFMAX); // Set buffer to all 0's

    // Send delay before undervolting
    sprintf(buffer, ("delay %i\n"), delay_before_undervolting);
    
    int error_check = serialport_write(fd_teensy, buffer);
    if (error_check == -1) { // Write to Teensy failed
        return PLUNDERVOLT_WRITE_TO_TEENSY_ERROR;
    }
    plundervolt_teensy_read_response(teensy_response_level);

    memset(buffer, 0, BUFMAX); // Wipe buffer
    
    // Send glitch specification
    sprintf(buffer, ("%i %1.4f %i %1.4f %i %1.4f\n"), repeat, start_voltage, duration_start, undervolting_voltage, duration_during, end_voltage);
    // TODO print arguments, if DEBUGGING
    error_check = serialport_write(fd_teensy, buffer);
    if (error_check == -1) { // Write to Teensy failed
        return PLUNDERVOLT_WRITE_TO_TEENSY_ERROR;
    }
    plundervolt_teensy_read_response(teensy_response_level);

    return PLUNDERVOLT_NO_ERROR;
}

void plundervolt_print_error(plundervolt_error_t error) {
    switch (error)
    {
    case PLUNDERVOLT_RANGE_ERROR:
        fprintf(stderr, "Start undervolting is smaller than end undervolting.\n");
        break;
    case PLUNDERVOLT_CANNOT_ACCESS_MSR_ERROR:
        fprintf(stderr, "Could not open /dev/cpu/0/msr\n\
            Run sudo modprobe msr first, and run this function with sudo priviliges.\n");
        break;
    case PLUNDERVOLT_NO_FUNCTION_ERROR:
        fprintf(stderr, "No function to undervolt on is provided.\n");
        break;
    case PLUNDERVOLT_NO_LOOP_CHECK_ERROR:
        fprintf(stderr, "No function to stop undervolting is provided, and integrated_loop_check is set to 0.\n");
        break;
    case PLUNDERVOLT_NOT_INITIALISED_ERROR:
        fprintf(stderr, "Plundervolt specification was not initialised properly.\n");
    case PLUNDERVOLT_NO_LOOP_CHECK_ERROR:
        fprintf(stderr, "The specification of who stops the function loop is faulty.\n");
    case PLUNDERVOLT_WRITE_TO_TEENSY_ERROR:
        fprintf(stderr, "Cannot write to Teensy for some reason.\n");
    case PLUNDERVOLT_CONNECTION_INIT_ERROR:
        fprintf(stderr, "Could not initialise Hardware undervolting correctly.\n");
    case PLUNDERVOLT_NO_TEENSY_SERIAL_ERROR:
        fprintf(stderr, "No Teensy serialport provided.\n");
    case PLUNDERVOLT_NO_TRIGGER_SERIAL_ERROR:
        fprintf(stderr, "No trigger serialport provided.\n");
    default:
        fprintf(stderr, "Generic error occured.\n");
        break;
    }
}

int open_file() {
    plundervolt_error_t error_check;
    if (u_spec.u_type == software) { // Software undervolting
        error_check = msr_accessible_check();
    } else { // Hardware undervolting
        error_check = plundervolt_init_hardware_undervolting(u_spec.teensy_serial, u_spec.trigger_serial, u_spec.teensy_baudrate);
    }
    return error_check;
}

int plundervolt_run() {
    if (!initialised) {
        return PLUNDERVOLT_NOT_INITIALISED_ERROR;
    }
    // Open file connedtion
    // Either access /dev/cpu/0/msr, or open Teensy connection
    plundervolt_error_t error_check = open_file();
    if (error_check) { // If msr_file != 0, it is an error code.
        return error_check;
    }

    error_check = faulty_undervolting_specification();
    if (error_check) {
        return error_check;
    }

    loop_finished = 0;

    plundervolt_error_t thread_error = PLUNDERVOLT_NO_ERROR;

    if (u_spec.u_type == software) {
        // Create threads
        // One is for running the function, the other for undervolting.
        if (u_spec.threads < 1) u_spec.threads = 1;
        pthread_t* function_thread = malloc(sizeof(pthread_t) * u_spec.threads);
        for (int i = 0; i < u_spec.threads; i++) {
            if (u_spec.loop) {
                pthread_create(&function_thread[i], NULL, run_function_loop, u_spec.arguments);
            } else {
                pthread_create(&function_thread[i], NULL, run_function, u_spec.arguments);
            }
        }

        if (u_spec.undervolt) {
            pthread_t undervolting_thread;
            pthread_create(&undervolting_thread, NULL, plundervolt_apply_undervolting, (void *) &thread_error);

            // Wait until both threads finish
            pthread_join(undervolting_thread, NULL);
        }
        for (int i = 0; i < u_spec.threads; i++) {
            pthread_join(function_thread[i], NULL);
        }
    } else {
        // Since apply_undervolting calls u_spec.function itself when doing HARDWARE undervolting, we don't need to do anything else here.
        // TODO Threads - later version.
        plundervolt_apply_undervolting((void *) &thread_error);
    }

    if (thread_error != PLUNDERVOLT_NO_ERROR) {
        return thread_error;
    }
    return PLUNDERVOLT_NO_ERROR;
}

void plundervolt_cleanup() {

    if (u_spec.undervolt && u_spec.u_type == software) {
        plundervolt_reset_voltage();
    }
    close(fd_teensy);
    if (u_spec.using_dtr) {
        close(fd_trigger);
    }

}