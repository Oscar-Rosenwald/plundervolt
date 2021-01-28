/**
 * @brief Library for undervolting; Version 4 - Refactoring
 */
/* Always compile with "-pthread".
Always run after "sudo modprobe msr" */

#define _GNU_SOURCE
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
#include "plundervolt.h"

int fd = NULL;
int initialised = 0;
plundervolt_specification_t u_spec;
uint64_t current_voltage;
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
 * stop loop when u_spec.stop_loop return 1.
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

uint64_t plundervolt_get_current_voltage() {
    return current_voltage;
}

void plundervolt_set_loop_finished() {
    loop_finished = 1;
}

int msr_accessible_check() {
    // Only open the file if it has not been open before.
    if (fd = NULL) {
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
    __off_t offset = 0x150;
    pwrite(fd, &value, sizeof(value), offset);
}

void* run_function_loop (void* arguments) {
    while ((u_spec.integrated_loop_check)
            ? !loop_finished
            : !(u_spec.stop_loop)(u_spec.loop_check_arguments)) {
        (u_spec.function)(arguments);
    }
    printf("Loop function finished.\n");
    return NULL;
}

void* run_function (void * arguments) {
    (u_spec.function)(arguments);
    printf("Function finished.");
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

    current_voltage = u_spec.start_undervolting;
    int milli_seconds=2000000; // TODO Why this number?
    printf("Started undervolting.\nCurrent voltage: %ld\nDesired voltage: %ld\n\n",
            current_voltage, u_spec.end_undervolting);
    
    while(u_spec.end_undervolting < current_voltage) {
        printf("Before change:\nCurrent undervolate = %ld\nDesired voltage: %ld\n\n",
                current_voltage, u_spec.end_undervolting);
        current_voltage -= u_spec.step;

        plundervolt_set_undervolting(plundervolt_compute_msr_value(current_voltage, 0));
        plundervolt_set_undervolting(plundervolt_compute_msr_value(current_voltage, 2));

        printf("After change:\nCurrent voltage: %ld\nDesired voltage: %ld\n\n",
                current_voltage, u_spec.end_undervolting);

        clock_t time = clock();
        while ((clock() < time+milli_seconds)) {
            ; // Noop
            // Just waiting
            // -> Will wait for 2 000 000 milliseconds before going further
        }
    }
    return NULL; // Must return something, as pthread_create requires a void* return value.
}

void plundervolt_reset_voltage() {
    plundervolt_set_undervolting(plundervolt_compute_msr_value(0, 0));
    plundervolt_set_undervolting(plundervolt_compute_msr_value(0, 2));
    printf("Resetting voltage.\n");
    sleep(3);
    printf("Current voltage: %f\n", 1000 * plundervolt_read_voltage());
}

plundervolt_specification_t plundervolt_init () {
    plundervolt_specification_t spec;
    spec.arguments = NULL;
    spec.step = 1;
    spec.loop = 1;
    spec.threads = 1;
    spec.start_undervolting = 0;
    spec.end_undervolting = 0;
    spec.function = NULL;
    spec.integrated_loop_check = 0;
    spec.stop_loop = NULL;
    spec.loop_check_arguments = NULL;
    spec.undervolt = 1;

    initialised = 1;

    return spec;
}

int plundervolt_set_specification(plundervolt_specification_t spec) {
    if (!initialised) {
        return PLUNDERVOLT_NOT_INITIALISED_ERROR;
    }
    u_spec = spec;
    return 0;
}

int faulty_undervolting_specification() {
    if (!initialised) {
        return PLUNDERVOLT_NOT_INITIALISED_ERROR;
    }
    if (u_spec.undervolt) {
        if (u_spec.start_undervolting <= u_spec.end_undervolting) {
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

void plundervolt_print_error(plundervolt_error_t error) {
    switch (error)
    {
    case PLUNDERVOLT_RANGE_ERROR:
        printf(stderr, "Start undervolting is smaller than end undervolting.\n");
        break;
    case PLUNDERVOLT_CANNOT_ACCESS_MSR_ERROR:
        printf(stderr, "Could not open /dev/cpu/0/msr\n\
            Run sudo modprobe msr first, and run this function with sudo priviliges.\n");
        break;
    case PLUNDERVOLT_NO_FUNCTION_ERROR:
        printf(stderr, "No function to undervolt on is provided.\n");
        break;
    case PLUNDERVOLT_NO_LOOP_CHECK_ERROR:
        printf(stderr, "No function to stop undervolting is provided, and integrated_loop_check is set to 0.\n");
        break;
    case PLUNDERVOLT_NOT_INITIALISED_ERROR:
        printf(stderr, "Plundervolt specification was not initialised properly.\n");
    default:
        printf(stderr, "Generic error occured.\n");
        break;
    }
}

int plundervolt_run() {
    // The following needs access to cpu/0/msr.
    // Check it is accessible.
    int error_check = msr_accessible_check();
    if (error_check) { // If msr_file != 0, it is an error code.
        return error_check;
    }
    
    current_voltage = 1000 * plundervolt_read_voltage();

    error_check = faulty_undervolting_specification();
    if (error_check) {
        return error_check;
    }

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
        pthread_create(&undervolting_thread, NULL, plundervolt_apply_undervolting, NULL);

        // Wait until both threads finish
        pthread_join(undervolting_thread, NULL);
    }
    for (int i = 0; i < u_spec.threads; i++) {
        pthread_join(function_thread[i], NULL);
        printf("Thread %d joined\n", i);
    }

    printf("Finished plundervolt run.\n");
    return 0;
}

void plundervolt_cleanup() {
    plundervolt_reset_voltage();
    close(fd);

    printf("Cleaned up.\n");
}