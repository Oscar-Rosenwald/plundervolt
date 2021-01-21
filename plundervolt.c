/**
 * @brief Library for undervolting; Version 2 - Library stage
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
int num_threads; // 0 threads acts as a "false" boolean.
uint64_t current_voltage; // In mV
int using_undervolting; // The user may not wish to undervolt every time they run the function.
int fd; // TODO What is this number exactly?

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

void* run_function(void* arguments) {
    while (current_voltage != u_spec.end_voltage) {
        (u_spec.function)(arguments);
    }
    printf("Random function finished.\n");
    return NULL;
}

// /**
//  * @brief A dummy function used for testing.
//  * This simulates the function to be undervolted, which will be passed by the user.
//  * It must return a void pointer, as the function in u_spec has a void* return type. This is so that
//  * later on, when I implement working with the return of that function, there is something to work with.
//  * In other words, it's just preparation for later.
//  */
// void random_function(void *arguments_struc) {
//     arguments_structure *arguments = (arguments_structure *) arguments_struc;
// }

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
    sleep(3);
    printf("Resetting voltage.\nCurrent voltage: %f\n", 1000 * read_voltage());
}

int plundervolt () {
    // The following needs access to cpu/0/msr.
    // Check it is accessible.
    if (!msr_accessible_check()) {
        return -1;
    }
    
    // u_spec.start_voltage = 1000 * read_voltage();
    current_voltage = 1000 * read_voltage();
    // u_spec.end_voltage = current_voltage - 5;

    // Check if read_voltage() performs correctly.
    /* if (u_spec.start_voltage != current_voltage) {
        printf("ERROR: u_spec.start_voltage != current_voltage!\nstart = %ld\ncurrent = %ld\n", u_spec.start_voltage, current_voltage);
        return -1;
    } else {
        printf("u_spec.start_voltage and current_voltage are SAME: %ld\n", current_voltage);
    } */

    // Create threads
    // One is for running the function, the other for undervolting.
    pthread_t function_thread;
    pthread_create(&function_thread, NULL, run_function, u_spec.arguments);
    pthread_t undervolting_thread;
    pthread_create(&undervolting_thread, NULL, undervolt, NULL);

    // Wait until both threads finish
    pthread_join(undervolting_thread, NULL);
    pthread_join(function_thread, NULL);
    reset_voltage();

    // run_function();
    printf("Finished.\n");
    return 0;
}

// int main(int argc, char *(*argv)) {    
//     // TODO The following is for testing purposes only. It will be deleted in the final version.
//     if (argc > 1) { // We have command line arguments
//     // First command - sudo
//     // Second command - ./<file>.out
//     // Third command - arguments
//         if (strcmp(argv[1] + 2, "reset") == 0) {
//             set_voltage(compute_msr_value(0, 0));
//             set_voltage(compute_msr_value(0, 2));
//             printf("Only reset voltage to SAME: %f\n", 1000 * read_voltage());
//         } else if (strcmp(argv[1] + 2, "read") == 0) {
//             printf("Current voltage: %f\n", 1000 * read_voltage());    
//         } else if (strcmp(argv[1] + 2, "set") == 0) {
//             if (argc < 3) {
//                 printf("Wrong number of arguments. You set --set, "
//                 "which is supposed to be followed by a number.\n");
//             } else {
//                 int64_t third_argument;
//                 third_argument = strtol(argv[2], NULL, 10);
//                 if (third_argument == 0) {
//                     printf("The third argument must be a number: %s.\n", argv[2]);
//                 } else {
//                     printf("Voltage: %f\nVoltage to: %ld\nTest: %ld\n\n", 1000 * read_voltage(), third_argument, strtol("700", NULL, 10));  
//                     sleep(2); // Wait for 2 seconds
//                     set_voltage(compute_msr_value(third_argument, 0));
//                     set_voltage(compute_msr_value(third_argument, 2));
//                     printf("Voltage set to: %ld\nVoltage: %f\n", third_argument, 1000 * read_voltage());
//                 }
//             }
//         } else {
//             printf("Wrong arguments: ");
//             for (int i = 1; i < argc; i++) {
//                 printf("%s  ", argv[i]);
//             }
//             printf("\n");
//         }       
//         return 1;
//     }
//     u_spec.function = random_function;
//}