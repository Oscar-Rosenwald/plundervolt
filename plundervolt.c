/**
 * @brief Library for undervolting; Version 1 - Extra Simple
 */

#include<inttypes.h>

int in_loop; // Is the function running in a loop?
int num_threads; // 0 threads acts as a "false" boolean.
int current_voltage; // In mV
int using_undervolting; // The user may not wish to undervolt every time they run the function.
int fd; // TODO What is this number exactly?

enum undervolting_type {software, hardware}; // Software for this version of the code
typedef struct undervolting_specification {
    int start_voltage; // In mV
    int end_boltage; // In mV
    int step; // How many mV we jump by.
              // We may not use steps (later versions); 0 = "false"
    void* (*function)(); // Function to be undervolted.
} u_spec;

/**
 * @brief Check if /dev/cpu/0/msr is accessible.
 * Attemps to open the file and gives feedback if fails.
 * sets fd.
 * @return int 1 if yes, 0 if no.
 */
int msr_accessible() {
    fd = open("/dev/cpu/0/msr", O_RDWR);
    if (fd == -1) { // msr file failed to open
        printf("Could not open /dev/cpu/0/msr\n\
            Run sudo modprobe msr first, or run this function with sudo priviliges.\n");
        return 0; // msr NOT accessible
    }
    return 1; // Msr accessible
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
    __off_t offset = 0x150; // TODO why is the offset different than when reading the voltage? It's the same fd.
    pwrite(fd, &value, sizeof(value), offset);
}

// TODO probably doesn't work
/**
 * @brief Run function given in u_spec
 * 
 * @return void* Whatever the function returns, return a pointer to it.
 */
void* run_function() {
    void* function_result = u_spec.function;
    return function_result;
}