/* plundervolt.h */
#include <stdio.h>
#include <stdint.h>

enum undervolting_type {software, hardware}; // Software for this version of the code

struct undervolting_specification {
    uint64_t start_voltage; // In mV
    uint64_t end_voltage; // In mV
    int step; // How many mV we jump by.
              // We may not use steps (later versions); 0 = "false"
    int loop; // If the function is to be repeated until fault (provided in some other version),
              // loop will = 1.
    int threads; // Number of threads to use. (0 is same as 1)
    void (* function)(void *);
    void * arguments; // Pointer to the structure of arguments for the function above.
} u_spec;

/**
 * @brief Check if /dev/cpu/0/msr is accessible.
 * Attemps to open the file and gives feedback if fails.
 * sets fd.
 * @return 1 if accessible; 0 if not.
 */
int msr_accessible_check();

/**
 * @brief Compute the value which will be written to cpu/0/msr.
 * 
 * @param value Value to be turned into the result
 * @param plane A plane index 
 * @return uint64_t 
 */
uint64_t compute_msr_value(int64_t, uint64_t);

/**
 * @brief Reads the current voltage
 * 
 * @return double Current voltage as read from fd.
 */
double read_voltage();

/**
 * @brief Set new voltage 
 * 
 * @param value uint64_t value of the new voltage.
 */
void set_voltage(uint64_t);

/**
 * @brief Run function given in u_spec
 * 
 * @return void* Whatever the function returns, return a pointer to it.
 */
void* run_function(void *);

/**
 * @brief Performs the undervolting according to specification in u_spec.
 * It ought to run in a separate thread
 * 
 * @return void* Must return something, as pthread_create requires a void* return value.
 */
void* undervolt();

/**
 * @brief Resets the voltage to normal levels
 * 
 */
void reset_voltage();

/**
 * @brief The main function of the undervolting library. It uses u_specs to create threads
 * and run the given function with the given arguments, while undervolting (according to specifications).
 * 
 * @return int Negative, if fault, 0 otherwise.
 */
int plundervolt();