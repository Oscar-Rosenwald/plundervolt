/**
 * @brief Library for undervolting; Version 5 - Hardware undervolting, Simple with Teensy
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
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <x86intrin.h>
#include <stdarg.h>
#include "arduino/arduino-serial-lib.h"
#include "plundervolt.h"

int fd = 0;
int initialised = 0;
plundervolt_specification_t u_spec;
uint64_t current_undervoltage;
int loop_finished = 0;
int debugging_level = 1;

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
/**
 * @brief Print a formatted string for debugging according to the level.
 * 
 * @param level String is printed if level <= debugging_level. 0-4
 * @param string Formatted string to print.
 */
void debug_print(int level, const char *string, ...);

void plundervolt_debug(int level) {
    if (level < 0 || level > 4) {
        fprintf(stderr, "Argument \"level\" of function \"plundervolt_debug()\" must be in the interval <0;4>. Was %d\n. Debugging level stayed at %d\n\n", level, debugging_level);
        return;
    }
    debugging_level = level;
}

void debug_print(int level, const char *string, ...) {
    va_list arguments;
    // int format_num = 0;
    // for(int i=0; string[i]; i++) {
    //     if(s[i] == '%') {
    //         format_num++;
    //     }
    // }
    va_start(arguments, string);

    if (level <= debugging_level) {
        vprintf(string, arguments);
    }
    va_end(arguments);
}

uint64_t plundervolt_get_current_undervoltage() {
    debug_print(2, "plundervolt_get_current_voltage()\n\n");
    return current_undervoltage;
}

void plundervolt_set_loop_finished() {
    debug_print(2, "plundervolt_set_loop_finished()\n\n");
    loop_finished = 1;
}

int msr_accessible_check() {
    debug_print(2, "msr_accessible_check()\n");

    debug_print(3, "Opening file /dev/cpu/0/msr\n");
    // Only open the file if it has not been open before.
    if (fd == 0) {
        fd = open("/dev/cpu/0/msr", O_RDWR);
    }
    if (fd == -1) { // msr file failed to open
        return PLUNDERVOLT_CANNOT_ACCESS_MSR_ERROR;
    }
    debug_print(3, "/dev/cpu/0/msr opened\n\n");
    return 0;
}

uint64_t plundervolt_compute_msr_value(int64_t value, uint64_t plane) {
    debug_print(2, "plundervolt_compute_msr_value(%d, %d)\n\n", value, plane);
    value=(value*1.024)-0.5; // -0.5 to deal with rounding issues
	value=0xFFE00000&((value&0xFFF)<<21);
	value=value|0x8000001100000000;
	value=value|(plane<<40);
	return (uint64_t)value;
}

double plundervolt_read_voltage() {
    debug_print(2, "plundervolt_read_voltage()\n\n");
    if (!msr_accessible_check()) {
        // TODO Error
    }
    uint64_t msr;
    __off_t offset = 0x198; // TODO Why this number?
    uint64_t number = 0xFFFF00000000; //TODO And this is what?
    double magic = 8192.0; // TODO Ha?
    int shift_by = 32; // TODO Why >>32?
    pread(fd, &msr, sizeof msr, offset);
    double res = (double)((msr & number)>>shift_by);
    return res / magic;
}

// TODO plundervolt_set_voltage(value);

void plundervolt_set_undervolting(uint64_t value) {
    debug_print(2, "plundervolt_set_undervolting(%d)\n\n", value);
    // 0x150 is the offset of the Plane Index buffer in msr (see Plundervolt paper).
    off_t offset = 0x150;
    pwrite(fd, &value, sizeof(value), offset);
}

void* run_function_loop (void* arguments) {
    debug_print(2, "run_function_loop(void arguments)\n");

    debug_print(3, "Running given function in a loop\n");
    while (true) {
        debug_print(4, "run_function_loop new iteration\n");
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
    debug_print(3, "Finished running given function in a loop\n");
    return NULL;
}

void* run_function (void * arguments) {
    debug_print(2, "run_function(void* arguments)\n");

    debug_print(3, "Running given function once\n");
    (u_spec.function)(arguments);
    debug_print(3, "Finished running given function once\n");
    return NULL;
}

void* plundervolt_apply_undervolting() {
    debug_print(2, "plundervolt_apply_undervolting()\n");

    cpu_set_t cpuset;
    pthread_t thread = pthread_self();

    // TODO What is this?
    CPU_ZERO(&cpuset);
    CPU_SET(0, &cpuset);

    debug_print(3, "Setting affinity\n");
    int set_affinity = pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset);
    if (set_affinity != 0) {
        debug_print(0, "Pthread set affinity error\n."); // This must be printed every time.
        // TODO Better error
    }
    debug_print(3, "Affinity set\n");

    current_undervoltage = u_spec.start_undervoltage;

    if (u_spec.u_type == software) {
        // SOFTWARE undervolting
        debug_print(1, "Software undervolting\n\n");

        debug_print(3, "Starting to undervolt\n");
        while(u_spec.end_undervoltage <= current_undervoltage && !loop_finished) {
            debug_print(1, "Undervolting: %ld\n", current_undervoltage); // This is important information, so debugging_level is 1.

            debug_print(3, "Setting new voltage\n");
            plundervolt_set_undervolting(plundervolt_compute_msr_value(current_undervoltage, 0));
            plundervolt_set_undervolting(plundervolt_compute_msr_value(current_undervoltage, 2));
            (debug_print(3, "New voltage set\n"));

            msleep(u_spec.wait_time);

            current_undervoltage -= u_spec.step;
        }
        debug_print(3, "Undervolting ended\n");
    } else {
        // HARDWARE undervolting
        debug_print(1, "Hardware undervolting\n\n");

        int error_check;
        int iterations = 1;

        debug_print(3, "Starting to undervolt\n");
        while (!loop_finished && iterations != u_spec.tries) {
            debug_print(4, "Hardware undervolting iteration\n\n");

            iterations++;

            debug_print(3, "Configuring, arming, and firing the glitch\n");
            for (int i = 0; i < u_spec.repeat; i++) {
                debug_print(4, "\nRepeat iteration %d\n", i);
                // error_check = plundervolt_configure_glitch(u_spec.delay_before_undervolting, u_spec.repeat, u_spec.start_voltage, u_spec.duration_start, u_spec.undervolting_voltage, u_spec.duration_during, u_spec.end_voltage);
                error_check = plundervolt_configure_glitch(u_spec.delay_before_undervolting, 1, u_spec.start_voltage, u_spec.duration_start, u_spec.undervolting_voltage, u_spec.duration_during, u_spec.end_voltage);
                if (error_check) { // If not 0
                    plundervolt_set_loop_finished(); // Stops this loop
                    // TODO Handle error
                }

                error_check = plundervolt_arm_glitch();
                if (error_check) { // If not 0
                    plundervolt_set_loop_finished(); // Stops this loop
                    // TODO Handle error
                }

                error_check = plundervolt_fire_glitch();
                if (error_check) { // If not 0
                    plundervolt_set_loop_finished(); // Stops this loop
                    // TODO Handle error
                }

                // Later, when DTR is introduced, and I find out what
                // it is, we'll have to reset the voltage here.


                msleep(u_spec.wait_time); // Give the machine time to work.
            }
            debug_print(3, "Glitch executed\n");
            debug_print(1, "\n"); // Just to better separate the glitches in the terminal

            sleep(5);
        }
        debug_print(3, "Undervolting loop ended\n");
    }

    debug_print(1, "\nUndervolting finished.\n\n");
    plundervolt_set_loop_finished();
    return NULL; // Must return something, as pthread_create requires a void* return value.
}

void plundervolt_reset_voltage() {
    debug_print(2, "plundervolt_reset_voltage()\n");

    debug_print(1, "Resetting voltage...\n");
    plundervolt_set_undervolting(plundervolt_compute_msr_value(0, 0));
    plundervolt_set_undervolting(plundervolt_compute_msr_value(0, 2));
    sleep(3);
    debug_print(1, "Current voltage: %f\n", 1000 * plundervolt_read_voltage());
}

plundervolt_specification_t plundervolt_init () {
    debug_print(2, "plundervolt_init()\n");
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
    spec.repeat = 1;
    spec.delay_before_undervolting = 0;
    spec.duration_start = 35;
    spec.duration_during = -25; // TODO Why negative?
    spec.start_undervoltage = 700;
    spec.undervolting_voltage = 600;
    spec.end_voltage = 700;
    spec.tries = 1;

    initialised = 1;

    return spec;
}

int plundervolt_set_specification(plundervolt_specification_t spec) {
    debug_print(2, "plundervolt_set_specification(plundervolt_specification_t spec)\n");
    if (!initialised) {
        return PLUNDERVOLT_NOT_INITIALISED_ERROR;
    }
    u_spec = spec;
    return PLUNDERVOLT_NO_ERROR;
}

int faulty_undervolting_specification() {
    debug_print(2, "fautly_undervolting_specification()\n");

    debug_print(3, "Checking if specification initialised\n");
    if (!initialised) {
        return PLUNDERVOLT_NOT_INITIALISED_ERROR;
    }
    debug_print(3, "Specification initialised. Checking if undervolting\n");
    if (u_spec.undervolt) {
        debug_print(1, "We are undervolting\n");
        if (u_spec.start_undervoltage <= u_spec.end_undervoltage) {
            return PLUNDERVOLT_RANGE_ERROR;
        }
    }
    debug_print(3, "Checking if function is provided\n");
    if (u_spec.function == NULL) {
        return PLUNDERVOLT_NO_FUNCTION_ERROR;
    }
    debug_print(3, "Function provided. Checking for running in a loop, and which function will stop it\n");
    if (u_spec.loop && !u_spec.integrated_loop_check && u_spec.stop_loop == NULL) {
        return PLUNDERVOLT_NO_LOOP_CHECK_ERROR;
    }
    return 0;
}

int plundervolt_init_teensy_connection(char* const teensy_serial, int teensy_baudrate) {
    debug_print(2, "plndervolt_init_teensy_connection(%s, %d)\n", teensy_serial, teensy_baudrate);

    debug_print(3, "Restarting teensy connection if needed\n");
    // If fd open, close it first - we'll restart the connection
    if (fd != -1) {
        serialport_close(fd);
        debug_print(3, "Teensy connection closed\n");
    }

    debug_print(3, "Opening Teensy connection\n");
    // Open the connection to Teensy
    fd = serialport_init(teensy_serial, teensy_baudrate);
    if (fd == -1) { // Connection failed to open.
        debug_print(3, "Teensy connection failed to open\n");
        return -1;
    }
    serialport_flush(fd); // TODO Why?
    debug_print(3, "Teensy connection opened\n");

    return PLUNDERVOLT_NO_ERROR;
}

int plundervolt_arm_glitch() {
    debug_print(2, "plundervolt_arm_glitch()\n");

    debug_print(3, "Arming the glitch\n");
    int error_check = serialport_write(fd, "arm\n");
    if (error_check == -1) { // Write to Teensy failed
        return -1;
    }
    debug_print(1, "Glitch armed\n");
    return PLUNDERVOLT_NO_ERROR;
}

int plundervolt_fire_glitch() {
    debug_print(2, "plundervolt_fire_glitch()\n");

    debug_print(3, "Firing the glitch\n");
    int error_check = write(fd, "\n", 1);
    if (error_check == -1) { // Write to Teensy failed
        return -1;
    }
    debug_print(1, "Glitch fired.\n");
    return PLUNDERVOLT_NO_ERROR;
}

int plundervolt_configure_glitch(int delay_before_undervolting, int repeat, float start_voltage, int duration_start, float undervolting_voltage, int duration_during, float end_voltage) {
    debug_print(2, "plundervolt_configure_glitch(""%i %1.4f %i %1.4f %i %1.4f)\n", repeat, start_voltage, duration_start, undervolting_voltage, duration_during, end_voltage);

    if (fd == -1) { // Teensy not opened properly
        return -1;
    }

    debug_print(3, "Sending delay length to Teensy\n");
    char buffer[BUFMAX];
    memset(buffer, 0, BUFMAX); // Set buffer to all 0's

    // Send delay before undervolting
    sprintf(buffer, "delay %d\n", delay_before_undervolting);
    int error_check = serialport_write(fd, buffer);
    if (error_check == -1) { // Write to Teensy failed
        return -1;
    }
    debug_print(3, "Delay sent\n");

    memset(buffer, 0, BUFMAX); // Wipe buffer
    
    debug_print(3, "Sending glitch configuration to Teensy\n");
    // Send glitch specification
    sprintf(buffer, ("%i %1.4f %i %1.4f %i %1.4f\n"), repeat, start_voltage, duration_start, undervolting_voltage, duration_during, end_voltage);
    debug_print(1, "Glitch specification: repeat - %d; start - %1.4f V; duration at start - %d; undervolting - %1.4f V; duration - %d; end - %1.4f V\n", repeat, start_voltage, duration_start, undervolting_voltage, duration_during, end_voltage);
    // TODO print arguments, if DEBUGGING
    error_check = serialport_write(fd, buffer);
    if (error_check == -1) { // Write to Teensy failed
        return -1;
    }
    debug_print(3, "Glitch configuration sent\n");

    if (debugging_level >= 3) {
        memset(buffer, 0, BUFMAX); // Wipe buffer
        serialport_read_lines(fd, buffer, EOL, BUFMAX, 10, 3); // Read response
        // TODO Why Timeout = 10?
        debug_print(3, "Teensy response: %s\n", buffer);
    }

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
    default:
        fprintf(stderr, "Generic error occured.\n");
        break;
    }
}

int open_file() {
    debug_print(2, "open_file()\n");

    debug_print(3, "Attempting to open connection\n");
    plundervolt_error_t error_check;
    if (u_spec.u_type == software) { // Software undervolting
        error_check = msr_accessible_check();
    } else { // Hardware undervolting
        error_check = plundervolt_init_teensy_connection(u_spec.teensy_serial, u_spec.teensy_baudrate);
    }
    debug_print(3, "Connection opened\n");
    return error_check;
}

int plundervolt_run() {
    debug_print(2, "plundervolt_run()\n");

    if (!initialised) {
        return PLUNDERVOLT_NOT_INITIALISED_ERROR;
    }
    // Open file connedtion
    // Either access /dev/cpu/0/msr, or open Teensy connection
    plundervolt_error_t error_check = open_file();
    if (error_check) { // If msr_file != 0, it is an error code.
        return error_check;
    }

    debug_print(3, "Checking if specification is correct\n");
    error_check = faulty_undervolting_specification();
    if (error_check) {
        return error_check;
    }
    debug_print(3, "Specification is correct\n");

    loop_finished = 0;

    // Create threads
    // One is for running the function, the other for undervolting.
    if (u_spec.threads < 1) u_spec.threads = 1;
    debug_print(3, "Creating threads. Number of threads: %d", u_spec.threads);
    pthread_t* function_thread = malloc(sizeof(pthread_t) * u_spec.threads);
    for (int i = 0; i < u_spec.threads; i++) {
        if (u_spec.loop) {
            pthread_create(&function_thread[i], NULL, run_function_loop, u_spec.arguments);
        } else {
            pthread_create(&function_thread[i], NULL, run_function, u_spec.arguments);
        }
    }
    debug_print(3, "Threads created\n");

    if (u_spec.undervolt) {
        pthread_t undervolting_thread;
        debug_print(1, "Undervolting:\n");
        pthread_create(&undervolting_thread, NULL, plundervolt_apply_undervolting, NULL);

        // Wait until both threads finish
        pthread_join(undervolting_thread, NULL);
        debug_print(1, "Undervolting thread joined\n");
    }
    debug_print(3, "Joining function threads\n");
    for (int i = 0; i < u_spec.threads; i++) {
        pthread_join(function_thread[i], NULL);
        debug_print(1, "Thread %d joined\n", i);
    }

    debug_print(1, "\nFinished plundervolt run.\n");
    return 0;
}

void plundervolt_cleanup() {
    debug_print(2, "plundervolt_cleanup()\n");

    if (u_spec.undervolt && u_spec.u_type == software) {
        debug_print(3, "Resetting voltage\n");
        plundervolt_reset_voltage();
    }
    debug_print(3, "Closing connection/file\n");
    close(fd);

    debug_print(1, "Cleaned up.\n");
}
