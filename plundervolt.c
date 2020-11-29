/**
 * @brief Library for undervolting; Version 1 - Extra Simple
 */
/* Always run after "sudo modprobe msr" */

#define _GNU_SOURCE
#include <fcntl.h>
#include <curses.h>
#include <immintrin.h>
#include <limits.h>
#include <pthread.h>
#include <sched.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <x86intrin.h>

int in_loop; // Is the function running in a loop?
int num_threads; // 0 threads acts as a "false" boolean.
long int current_voltage; // In mV
int using_undervolting; // The user may not wish to undervolt every time they run the function.
int fd; // TODO What is this number exactly?

enum undervolting_type {software, hardware}; // Software for this version of the code
struct undervolting_specification {
    long int start_voltage; // In mV
    long int end_voltage; // In mV
    int step; // How many mV we jump by.
              // We may not use steps (later versions); 0 = "false"
    void* (*function)(); // Function to be undervolted.
} u_spec;

/**
 * @brief Check if /dev/cpu/0/msr is accessible.
 * Attemps to open the file and gives feedback if fails.
 * sets fd.
 * @return 1 if accessible; 0 if not.
 */
int msr_accessible_check() {
    fd = open("/dev/cpu/0/msr", O_RDWR);
    if (fd == -1) { // msr file failed to open
        printf("Could not open /dev/cpu/0/msr\n\
            Run sudo modprobe msr first, or run this function with sudo priviliges.\n");
        return 0; // msr NOT accessible
    }
    return 1;
}

/**
 * @brief Compute the value which will be written to cpu/0/msr.
 * 
 * @param value Value to be turned into the result
 * @param plane A plane index 
 * @return uint64_t 
 */
uint64_t compute_msr_value(int64_t value, uint64_t plane) {
    value=(value*1.024)-0.5; // -0.5 to deal with rounding issues
	value=0xFFE00000&((value&0xFFF)<<21);
	value=value|0x8000001100000000;
	value=value|(plane<<40);
	return (uint64_t)value;
}

/**
 * @brief Reads the current voltage
 * 
 * @return double Current voltage as read from fd.
 */
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

/**
 * @brief Set new voltage 
 * 
 * @param value uint64_t value of the new voltage.
 */
void set_voltage(uint64_t value) {
    __off_t offset = 0x150; // 0x150 is the offset of the Plane Index buffer in msr (see Plundervolt paper).
    pwrite(fd, &value, sizeof(value), offset);
}

/**
 * @brief Run function given in u_spec
 * 
 * @return void* Whatever the function returns, return a pointer to it.
 */
void* run_function() {
    return u_spec.function();
}

/**
 * @brief A dummy function used for testing.
 * This simulates the function to be undervolted, which will be passed by the user.
 * It must return a void pointer, as the function in u_spec has a void* return type. This is so that
 * later on, when I implement working with the return of that function, there is something to work with.
 * In other words, it's just preparation for later.
 */
void* random_function() {
    printf("Entered random function.\n");
    while (current_voltage != u_spec.end_voltage) {
        ; // Noop
    }
    printf("Random function finished.\n");
    return NULL; // Must return something, as pthread_create requires a void* return value.
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
    
    while(u_spec.end_voltage > current_voltage) {
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

// TODO This whole function - reset voltage
void reset_voltage() {
    // set_voltage()
}

int main() {
    u_spec.function = random_function;
    u_spec.end_voltage = 600;

    // The following needs access to cpu/0/msr.
    // Check it is accessible.
    if (!msr_accessible_check()) {
        return -1;
    }
    u_spec.start_voltage = 1000 * read_voltage();
    current_voltage = 1000 * read_voltage();

    // Check if read_voltage() performs correctly.
    if (u_spec.start_voltage != current_voltage) {
        printf("ERROR: u_spec.start_voltage != current_voltage!\n");
    } else {
        printf("u_spec.start_voltage and current_voltage are SAME: %ld\n", current_voltage);
    }

    // Create threads.
    // One is for running the function, the other for undervolting.
    pthread_t function_thread;
    pthread_create(&function_thread, NULL, random_function, NULL);
    pthread_t undervolting_thread;
    pthread_create(&undervolting_thread, NULL, undervolt, NULL);
    
    // Wait until both threads finish
    pthread_join(undervolting_thread, NULL);
    pthread_join(function_thread, NULL);

    // run_function();
    printf("Finished.\n");
}