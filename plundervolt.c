/**
 * @brief Library for undervolting; Version 3 - Threads
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

int in_loop; // Is the function running in a loop?
int using_undervolting; // The user may not wish to undervolt every time they run the function.
int fd; // TODO What is this number exactly?
int initialised = 0;

int msr_accessible_check() {
    fd = open("/dev/cpu/0/msr", O_RDWR);
    if (fd == -1) { // msr file failed to open
        printf("Could not open /dev/cpu/0/msr\n\
            Run sudo modprobe msr first, and run this function with sudo priviliges.\n");
        return 0; // msr NOT accessible
    }
    return 1;
}

uint64_t compute_msr_value(int64_t value, uint64_t plane) {
    value=(value*1.024)-0.5; // -0.5 to deal with rounding issues
	value=0xFFE00000&((value&0xFFF)<<21);
	value=value|0x8000001100000000;
	value=value|(plane<<40);
	return (uint64_t)value;
}

double read_voltage() {
    uint64_t msr;
    __off_t offset = 0x198; // TODO Why this number?
    uint64_t number = 0xFFFF00000000; //TODO And this is what?
    double magic = 8192.0; // TODO Ha?
    int shift_by = 32; // TODO Why >>32?
    pread(fd, &msr, sizeof msr, offset);
    double res = (double)((msr & number)>>shift_by);
    return res / magic;
}

void set_voltage(uint64_t value) {
    __off_t offset = 0x150; // 0x150 is the offset of the Plane Index buffer in msr (see Plundervolt paper).
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

void* undervolt() {
    cpu_set_t cpuset;
    pthread_t thread = pthread_self();

    // TODO What is this?
    CPU_ZERO(&cpuset);
    CPU_SET(0, &cpuset);

    int set_affinity = pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset);
    if (set_affinity != 0) {
        printf("Pthread set affinity error\n.");
    }

    current_voltage = u_spec.start_voltage;
    int milli_seconds=2000000; // TODO Why this number?
    printf("Started undervolting.\nCurrent voltage: %ld\nDesired voltage: %ld\n\n", current_voltage, u_spec.end_voltage);
    
    while(u_spec.end_voltage < current_voltage) {
        printf("Before change:\nCurrent undervolate = %ld\nDesired voltage: %ld\n\n", current_voltage, u_spec.end_voltage);
        current_voltage--;

        set_voltage(compute_msr_value(current_voltage, 0));
        set_voltage(compute_msr_value(current_voltage, 2));

        printf("After change:\nCurrent voltage: %ld\nDesired voltage: %ld\n\n", current_voltage, u_spec.end_voltage);

        clock_t time = clock();
        while ((clock() < time+milli_seconds)) {
            ; // Noop
            // Just waiting
            // -> Will wait for 2 000 000 milliseconds before going further
        }
    }
    return NULL; // Must return something, as pthread_create requires a void* return value.
}

void reset_voltage() {
    set_voltage(compute_msr_value(0, 0));
    set_voltage(compute_msr_value(0, 2));
    printf("Resetting voltage.\n");
    sleep(3);
    printf("Current voltage: %f\n", 1000 * read_voltage());
}

void initialise_undervolting () {
    u_spec.function = NULL;
    u_spec.stop_loop = NULL;
    u_spec.arguments = NULL;
    u_spec.loop_check_arguments = NULL;
    u_spec.loop = 1;
    u_spec.threads = 1;
    u_spec.integrated_loop_check = 0;

    initialised = 1;
}

int faulty_undervolting_specification() {
    if (!initialised) {
        printf("ERROR: u_spec not initialised properly. Use initialise_undervolting.\n");
        return 1;
    }
    int error = 0;
    if (u_spec.undervolt) {
        if (u_spec.start_voltage <= u_spec.end_voltage) {
            printf("ERROR: start voltage >= end_voltage.\n");
            error = 1;
        }
    }
    if (u_spec.function == NULL) {
        printf("ERROR: No undervolting function provided.\n");
        error = 1;
    }
    if (u_spec.loop && !u_spec.integrated_loop_check && u_spec.stop_loop == NULL) {
        printf("ERROR: Running loop and no integrated check, but no loop check function provided.\n");
        error = 1;
    }
    return error;
}

int plundervolt () {
    // The following needs access to cpu/0/msr.
    // Check it is accessible.
    if (!msr_accessible_check()) {
        return -1;
    }
    
    current_voltage = 1000 * read_voltage();

    if (faulty_undervolting_specification()) {
        return -1;
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
        pthread_create(&undervolting_thread, NULL, undervolt, NULL);

        // Wait until both threads finish
        pthread_join(undervolting_thread, NULL);
    }
    for (int i = 0; i < u_spec.threads; i++) {
        pthread_join(function_thread[i], NULL);
        printf("Thread %d joined\n", i);
    }
    reset_voltage();

    printf("Finished.\n");
    return 0;
}