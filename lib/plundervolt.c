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
#include "arduino/arduino-serial-lib.h"
#include "plundervolt.h"

int fd = 0;
int initialised = 0;
plundervolt_specification_t u_spec;
uint64_t current_undervoltage;
int loop_finished;

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

uint64_t plundervolt_get_current_undervoltage() {
    return current_undervoltage;
}

void plundervolt_set_loop_finished() {
    loop_finished = 1;
}

int msr_accessible_check() {
    // Only open the file if it has not been open before.
    if (fd == 0) {
        fd = open("/dev/cpu/0/msr", O_RDWR);
    }
    if (fd == -1) { // msr file failed to open
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

void* plundervolt_apply_undervolting() {
    cpu_set_t cpuset;
    pthread_t thread = pthread_self();

    // TODO What is this?
    CPU_ZERO(&cpuset);
    CPU_SET(0, &cpuset);

    int set_affinity = pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset);
    if (set_affinity != 0) {
        printf("Pthread set affinity error\n.");
    }

    current_undervoltage = u_spec.start_undervoltage;

    if (u_spec.u_type == software) {
        // SOFTWARE undervolting
        
        printf("Software undervolting.\n\n");
        while(u_spec.end_undervoltage <= current_undervoltage && !loop_finished) {
            printf("Undervolting: %ld\n", current_undervoltage);

            plundervolt_set_undervolting(plundervolt_compute_msr_value(current_undervoltage, 0));
            plundervolt_set_undervolting(plundervolt_compute_msr_value(current_undervoltage, 2));

            msleep(u_spec.wait_time);

            current_undervoltage -= u_spec.step;
        }
    } else {
        // HARDWARE undervolting

        printf("Hardware undervolting.\n\n");
        int error_check;
        int iterations;

        while (!loop_finished && iterations != u_spec.tries) {
            error_check = plundervolt_configure_glitch(u_spec.delay_before_undervolting, u_spec.repeat, u_spec.start_voltage, u_spec.duration_start, u_spec.undervolting_voltage, u_spec.duration_during, u_spec.end_voltage);
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

            printf("End of iteration. Waiting for another.\n\n");
            msleep(u_spec.wait_time); // Give the machine time to work.
        }
    }

    printf("Undervolting finished.\n\n");
    plundervolt_set_loop_finished();
    return NULL; // Must return something, as pthread_create requires a void* return value.
}

void plundervolt_reset_voltage() {
    plundervolt_set_undervolting(plundervolt_compute_msr_value(0, 0));
    plundervolt_set_undervolting(plundervolt_compute_msr_value(0, 2));
    printf("Resetting voltage...\n");
    sleep(3);
    printf("Current voltage: %f\n", 1000 * plundervolt_read_voltage());
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
    return 0;
}

int plundervolt_init_teensy_connection(char* const teensy_serial, int teensy_baudrate) {
    // If fd open, close it first - we'll restart the connection
    if (fd != -1) {
        serialport_close(fd);
    }

    // Open the connection to Teensy
    fd = serialport_init(teensy_serial, teensy_baudrate);
    if (fd == -1) { // Connection failed to open.
        return -1;
    }
    serialport_flush(fd); // TODO Why?
    printf("Glitch fired.\n");

    return PLUNDERVOLT_NO_ERROR;
}

int plundervolt_arm_glitch() {
    int error_check = serialport_write(fd, "arm\n");
    if (error_check == -1) { // Write to Teensy failed
        return -1;
    }
    printf("Glitch armed\n");
    return PLUNDERVOLT_NO_ERROR;
}

int plundervolt_fire_glitch() {
    int error_check = write(fd, "\n", 1);
    if (error_check == -1) { // Write to Teensy failed
        return -1;
    }
    return PLUNDERVOLT_NO_ERROR;
}

int plundervolt_configure_glitch(int delay_before_undervolting, int repeat, float start_voltage, int duration_start, float undervolting_voltage, int duration_during, float end_voltage) {
    if (fd == -1) { // Teensy not opened properly
        return -1;
    }

    char buffer[BUFMAX];
    memset(buffer, 0, BUFMAX); // Set buffer to all 0's

    // Send delay before undervolting
    sprintf(buffer, "delay %d\n", delay_before_undervolting);
    int error_check = serialport_write(fd, buffer);
    if (error_check == -1) { // Write to Teensy failed
        return -1;
    }

    memset(buffer, 0, BUFMAX); // Wipe buffer
    
    // Send glitch specification
    sprintf(buffer, ("%i %1.4f %i %1.4f %i %1.4f\n"), repeat, start_voltage, duration_start, undervolting_voltage, duration_during, end_voltage);
    printf("Glitch specification: repeat - %d; start - %1.4f V; duration at start - %d; undervolting - %1.4f V; duration - %d; end - %1.4f V\n", repeat, start_voltage, duration_start, undervolting_voltage, duration_during, end_voltage);
    // TODO print arguments, if DEBUGGING
    error_check = serialport_write(fd, buffer);
    if (error_check == -1) { // Write to Teensy failed
        return -1;
    }

    #ifdef DEBUGGING
    memset(buffer, 0, BUFMAX); // Wipe buffer
    serialport_read_lines(fd, buffer, EOL, BUFMAX, 10, 3); // Read response
    // TODO Why Timeout = 10?
    #endif 

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
    plundervolt_error_t error_check;
    if (u_spec.u_type == software) { // Software undervolting
        error_check = msr_accessible_check();
    } else { // Hardware undervolting
        error_check = plundervolt_init_teensy_connection(u_spec.teensy_serial, u_spec.teensy_baudrate);
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
        printf("Undervolting:\n");
        pthread_create(&undervolting_thread, NULL, plundervolt_apply_undervolting, NULL);

        // Wait until both threads finish
        pthread_join(undervolting_thread, NULL);
        printf("\nUndervolting thread joined.\n");
    }
    printf("Joining function threads.\n");
    for (int i = 0; i < u_spec.threads; i++) {
        pthread_join(function_thread[i], NULL);
        printf("Thread %d joined\n", i);
    }

    printf("\nFinished plundervolt run.\n");
    return 0;
}

void plundervolt_cleanup() {
    if (u_spec.undervolt && u_spec.u_type == software) {
        plundervolt_reset_voltage();
    }
    close(fd);

    printf("Cleaned up.\n");
}
