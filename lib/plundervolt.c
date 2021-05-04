/**
 * @brief Library for undervolting; Version 6 - Documentation
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

int fd_teensy = 0, fd_trigger = 0, fd = 0; // Files for voltage control.
int initialised = 0; // Variable indicating the correct initialisation of the library (in terms of its specification).
plundervolt_specification_t u_spec; // Specification of the library.
uint64_t current_undervoltage; // Used in Software undervolting.
int loop_finished = 0; // When the user wishes to stop all loops of undervolting, they set this to 1. See plundervolt_set_loop_finished().
int DTR_flag = TIOCM_DTR; // Used in Hardware undervolting.

/**
 * @brief Run function given in u_spec.function only once.
 * 
 * @param arguments Arguments to pass to the function. Will most likely be u_spec.arguments.
 * @return void* Whatever the function returns, return a pointer to it.
 */
void* run_function(void *arguments);
/**
 * @brief Run function given in u_spec.function in a loop. If u_spec.integrated_loop_check = 1,
 * stop the loop when loop_finished = 1 (as set by the user in that function). When u_spec.integrated_loop_check = 0,
 * stop loop when u_spec.stop_loop return 1 and stop the undervolting process (this library calls u_spec.stop_loop() itself).
 * 
 * @param arguments Arguments to pass to the function. Will most likely be u_spec.arguments.
 * 
 * @return void* Whatever the function returns, return a pointer to it.
 */
void* run_function_loop(void *arguments);
/**
 * @brief Check if /dev/cpu/0/msr is accessible.
 * Attemps to open the file and gives feedback if fails.
 * Sets fd.
 * @return plundervolt_error_t PLUNDERVOLT_NO_ERROR if msr is accessible, PLUNDERVOLT_CANNOT_ACCESS_MSR_ERROR if not.
 */
plundervolt_error_t msr_accessible_check();
/**
 * @brief Run function u_spec.function with arguments given number of times.
 * 
 * @param times How many times to repeat the function.
 * @param arguments Arguments to pass to the function. Will most likely be u_spec.arguments.
 * 
 * @return Whatever the function returns.
 */
void* run_function_times(int times, void *arguments);

uint64_t plundervolt_get_current_undervoltage() {
    return current_undervoltage;
}

void plundervolt_set_loop_finished() {
    loop_finished = 1;
}

plundervolt_error_t msr_accessible_check() {
    // Only open the file if it has not been open before.
    if (fd == 0) {
        fd = open("/dev/cpu/0/msr", O_RDWR);
    }
    if (fd == -1) { // msr file failed to open
        return PLUNDERVOLT_CANNOT_ACCESS_MSR_ERROR;
    }
    return PLUNDERVOLT_NO_ERROR;
}

uint64_t plundervolt_compute_msr_value(int64_t value, uint64_t plane) {
    value=(value*1.024)-0.5; // -0.5 to deal with rounding issues
	value=0xFFE00000&((value&0xFFF)<<21);
	value=value|0x8000001100000000;
	value=value|(plane<<40);
	return (uint64_t)value;
}

double plundervolt_read_voltage() {
    if (msr_accessible_check() != PLUNDERVOLT_NO_ERROR) {
        return PLUNDERVOLT_CANNOT_ACCESS_MSR_ERROR;
    }
    uint64_t msr;
    __off_t offset = 0x198;
    uint64_t number = 0xFFFF00000000;
    double magic = 8192.0;
    int shift_by = 32;
    pread(fd, &msr, sizeof msr, offset);
    double res = (double)((msr & number)>>shift_by);
    return res / magic;
}

void plundervolt_set_undervolting(uint64_t value) {
    // 0x150 is the offset of the Plane Index buffer in msr (see Plundervolt paper).
    off_t offset = 0x150;
    pwrite(fd, &value, sizeof(value), offset);
}

void* run_function_loop (void* arguments) {
    while (true) {
        if (loop_finished){
            break;
        }
        if (!u_spec.integrated_loop_check &&
            (u_spec.stop_loop)(u_spec.loop_check_arguments)) {
                plundervolt_set_loop_finished(); // Stop all other loops, and stop the undervolting.
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

void plundervolt_software_undervolt(uint64_t new_undervoltage) {
    plundervolt_set_undervolting(plundervolt_compute_msr_value(new_undervoltage, 0));
    plundervolt_set_undervolting(plundervolt_compute_msr_value(new_undervoltage, 2));
}

void* plundervolt_apply_undervolting(void *error_maybe) {
    plundervolt_error_t *error_check_thread = (plundervolt_error_t *) error_maybe; // Used to send errors from a thread.

    if (u_spec.u_type == software) {
        // SOFTWARE undervolting

        cpu_set_t cpuset;
        pthread_t thread = pthread_self();
        CPU_ZERO(&cpuset);
        CPU_SET(0, &cpuset);

        int set_affinity = pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset);
        if (set_affinity != 0) {
            *error_check_thread = PLUNDERVOLT_GENERIC_ERROR;
            plundervolt_set_loop_finished();
            pthread_exit(NULL);
        }

        // Start with the undervolting on the specified value.
        current_undervoltage = u_spec.start_undervoltage;

        while(u_spec.end_undervoltage <= current_undervoltage && !loop_finished) {
            // Both lines are necessary.
            plundervolt_software_undervolt(current_undervoltage);
            msleep(u_spec.wait_time);
            current_undervoltage -= u_spec.step;
        }
    } else {
        // HARDWARE undervolting

        int error_check;
        int iterations = 0;

        plundervolt_reset_voltage();
        plundervolt_fire_glitch();
        plundervolt_reset_voltage();
        
        while (!loop_finished && iterations < u_spec.tries) {
            // This will make the system respond faster

            iterations++;

            // First configure the system.
            error_check = plundervolt_configure_glitch();
            if (error_check) { // If not 0
                plundervolt_set_loop_finished(); // Stops this loop
                *error_check_thread = error_check;
                pthread_exit(NULL);
            }

            // Second, "arm" the glitch - get it ready.
            error_check = plundervolt_arm_glitch();
            if (error_check) { // If not 0
                plundervolt_set_loop_finished(); // Stops this loop
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
    if (u_spec.u_type == hardware && u_spec.using_dtr) { // If using_dtr = 0, nothing is to be done.
        ioctl(fd_trigger,TIOCMBIC,&DTR_flag);
    } else if (u_spec.u_type == software) {
        // Both lines are necessary.
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
    spec.wait_time = 300;
    spec.u_type = software;

    spec.teensy_baudrate = 115200;
    spec.teensy_serial = "";
    spec.trigger_serial = "";
    spec.using_dtr = 1;
    spec.repeat = 1;
    spec.delay_before_undervolting = 0;
    spec.duration_start = 35;
    spec.duration_during = -25;
    spec.start_voltage = 0.900;
    spec.undervolting_voltage = 0.900;
    spec.end_voltage = 0.900;
    spec.tries = 1;
    spec.using_dtr = 1;

    initialised = 1;

    return spec;
}

plundervolt_error_t plundervolt_set_specification(plundervolt_specification_t spec) {
    if (!initialised) {
        return PLUNDERVOLT_NOT_INITIALISED_ERROR;
    }
    u_spec = spec;
    plundervolt_error_t error_check = plundervolt_faulty_undervolting_specification();
    if (error_check) {
        return error_check;
    }
    return PLUNDERVOLT_NO_ERROR;
}

plundervolt_error_t plundervolt_faulty_undervolting_specification() {
    if (!initialised) {
        return PLUNDERVOLT_NOT_INITIALISED_ERROR;
    }
    if (u_spec.undervolt) {
        if (u_spec.u_type == software && u_spec.start_undervoltage <= u_spec.end_undervoltage) {
            return PLUNDERVOLT_RANGE_ERROR;
        }
    }
    if (u_spec.function == NULL) {
        return PLUNDERVOLT_NO_FUNCTION_ERROR;
    }
    if (u_spec.loop && !u_spec.integrated_loop_check && u_spec.stop_loop == NULL) {
        return PLUNDERVOLT_NO_LOOP_CHECK_ERROR;
    }
    if (u_spec.u_type == hardware && u_spec.teensy_serial == "") {
        return PLUNDERVOLT_NO_TEENSY_SERIAL_ERROR;
    }
    if (u_spec.u_type == hardware && u_spec.trigger_serial == "") {
        return PLUNDERVOLT_NO_TRIGGER_SERIAL_ERROR;
    }

    return PLUNDERVOLT_NO_ERROR;
}

plundervolt_error_t plundervolt_init_hardware_undervolting() {

    if (!initialised) {
        return PLUNDERVOLT_NOT_INITIALISED_ERROR;
    }

    if (u_spec.using_dtr) {
        fd_trigger = open(u_spec.trigger_serial, O_RDWR | O_NOCTTY);
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
    fd_teensy = serialport_init(u_spec.teensy_serial, u_spec.teensy_baudrate);
    if (fd_teensy == -1) { // Connection failed to open.
        return PLUNDERVOLT_CONNECTION_INIT_ERROR;
    }
    serialport_flush(fd_teensy);

    return PLUNDERVOLT_NO_ERROR;
}

plundervolt_error_t plundervolt_arm_glitch() {
    int error_check = serialport_write(fd_teensy, "arm\n"); // Send Teensy the command to arm itself.
    if (error_check == -1) { // Write to Teensy failed
        return PLUNDERVOLT_WRITE_TO_TEENSY_ERROR;
    }
    char buf[BUFMAX];
    memset(buf,0,BUFMAX);
	serialport_read_lines(fd_teensy, buf, EOL, BUFMAX, 10,2);
	printf("Teensy response: %s\n", buf);
    return PLUNDERVOLT_NO_ERROR;
}

plundervolt_error_t plundervolt_fire_glitch() {
    if (u_spec.using_dtr) {
        ioctl(fd_trigger, TIOCMBIS, &DTR_flag);
    } else {
        int error_check = write(fd_teensy, "\n", 1); // Send Teensy the symbol for "end of input", i.e. "start working".
        if (error_check != 1) { // Write to Teensy failed
            return PLUNDERVOLT_WRITE_TO_TEENSY_ERROR;
        }
    }
    return PLUNDERVOLT_NO_ERROR;
}

void plundervolt_teensy_read_response() {
    char buffer[BUFMAX];
    memset(buffer, 0, BUFMAX); // Wipe buffer
    serialport_read_lines(fd_teensy, buffer, EOL, BUFMAX, 10, 3); // Read response
    printf("Teensy response: %s\n", buffer);
}

plundervolt_error_t plundervolt_configure_glitch() {
    if (fd_teensy == -1) { // Teensy not opened properly
        return PLUNDERVOLT_CONNECTION_INIT_ERROR;
    }

    char buffer[BUFMAX];
    memset(buffer, 0, BUFMAX); // Wipe buffer

    // Send delay before undervolting
    sprintf(buffer, ("delay %i\n"), u_spec.delay_before_undervolting);
    
    int error_check = serialport_write(fd_teensy, buffer);
    if (error_check == -1) { // Write to Teensy failed
        return PLUNDERVOLT_WRITE_TO_TEENSY_ERROR;
    }
    plundervolt_teensy_read_response();

    memset(buffer, 0, BUFMAX); // Wipe buffer
    
    // Send glitch specification
    sprintf(buffer, ("%i %1.4f %i %1.4f %i %1.4f\n"), u_spec.repeat, u_spec.start_voltage, u_spec.duration_start, u_spec.undervolting_voltage, u_spec.duration_during, u_spec.end_voltage);
    error_check = serialport_write(fd_teensy, buffer);
    if (error_check == -1) { // Write to Teensy failed
        return PLUNDERVOLT_WRITE_TO_TEENSY_ERROR;
    }
    plundervolt_teensy_read_response();

    return PLUNDERVOLT_NO_ERROR;
}

void plundervolt_print_error(plundervolt_error_t error) {
    fprintf(stderr, "%s\n", plundervolt_error2str(error));
}

const char* plundervolt_error2str(plundervolt_error_t error) {
    switch (error)
    {
    case PLUNDERVOLT_RANGE_ERROR:
        return "Start undervolting is smaller than end undervolting.";
    case PLUNDERVOLT_CANNOT_ACCESS_MSR_ERROR:
        return "Could not open /dev/cpu/0/msr\n\
            Run sudo modprobe msr first, and run this function with sudo priviliges.";
    case PLUNDERVOLT_NO_FUNCTION_ERROR:
        return "No function to undervolt on is provided.";
    case PLUNDERVOLT_NO_LOOP_CHECK_ERROR:
        return "No function to stop undervolting is provided, and integrated_loop_check is set to 0.";
    case PLUNDERVOLT_NOT_INITIALISED_ERROR:
        return "Plundervolt specification was not initialised properly.";
    case PLUNDERVOLT_WRITE_TO_TEENSY_ERROR:
        return "Cannot write to Teensy for some reason.";
    case PLUNDERVOLT_CONNECTION_INIT_ERROR:
        return "Could not initialise Hardware undervolting correctly.";
    case PLUNDERVOLT_NO_TEENSY_SERIAL_ERROR:
        return "No Teensy serialport provided.";
    case PLUNDERVOLT_NO_TRIGGER_SERIAL_ERROR:
        return "No trigger serialport provided.";
    default:
        return "Generic error occured.";
    }
}

plundervolt_error_t plundervolt_open_file() {
    plundervolt_error_t error_check;
    if (u_spec.u_type == software) { // Software undervolting
        error_check = msr_accessible_check();
    } else { // Hardware undervolting
        error_check = plundervolt_init_hardware_undervolting();
    }
    return error_check; // May be PLUNDERVOLT_NO_ERROR
}

plundervolt_error_t plundervolt_run() {
    if (!initialised) {
        return PLUNDERVOLT_NOT_INITIALISED_ERROR;
    }
    // Open file connedtion
    // Either access /dev/cpu/0/msr, or open Teensy connection
    plundervolt_error_t error_check = plundervolt_open_file();
    if (error_check) { // If msr_file != 0, it is an error code.
        return error_check;
    }

    // Check if specification is of the correct format.
    error_check = plundervolt_faulty_undervolting_specification();
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
            // Create undervolting thread.
            pthread_t undervolting_thread;
            pthread_create(&undervolting_thread, NULL, plundervolt_apply_undervolting, (void *) &thread_error);

            // Wait until both threads finish
            pthread_join(undervolting_thread, NULL);
        }
        for (int i = 0; i < u_spec.threads; i++) {
            pthread_join(function_thread[i], NULL); // Wait for all threads to end.
        }
    } else {
        // Since apply_undervolting calls u_spec.function itself when doing HARDWARE undervolting, we don't need to do anything else here.
        if (u_spec.undervolt) {
            plundervolt_apply_undervolting((void *) &thread_error);
        }
    }

    if (thread_error != PLUNDERVOLT_NO_ERROR) {
        return thread_error;
    }
    return PLUNDERVOLT_NO_ERROR;
}

void plundervolt_cleanup() {
    if (u_spec.u_type == software) {
        close(fd);
        if (u_spec.undervolt) {
            plundervolt_reset_voltage();
        }
    }
    close(fd_teensy);
    if (u_spec.using_dtr) {
        close(fd_trigger);
    }

}