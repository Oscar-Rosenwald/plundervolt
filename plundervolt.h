/* plundervolt.h */
#include <stdio.h>
#include <stdint.h>

enum undervolting_type {software, hardware}; // Software for this version of the code
int loop_finished; // When looping, indicate we've achieved the goal and stop the loop in every thread.
uint64_t current_voltage; // In mV

struct undervolting_specification {
    uint64_t start_voltage; // In mV
    uint64_t end_voltage; // In mV
    /**
     * @brief How many mV we jump by.
     * We may not use steps (later versions); 0 = "false"
     */
    int step;
    /**
     * If the function is to be repeated until fault,
     * loop will = 1.
     */
    int loop;
    /**
     * @brief Number of threads to use. (0 is same as 1)
     */
    int threads;
    /**
     * @brief Fuction which the user wishes to undervolt on. Its arguments are stored in a 
     * user-defined structure and passed as a void pointer. The function must cast this pointer
     * to said structure.
     */
    void (* function)(void *);
    /** 
     * @brief Pointer to the structure of arguments for the function above.
     * Must be a void pointer, as the structure holding the arguments
     * is defined by the user. The function must be implemented with this in mind.
     */
    void * arguments;
    /**
     * @brief > 0 if the user's function contains loop checks itself.
     * NOTE: If it does, it must work with the shared variable loop_finished.
     */
    int integrated_loop_check; 
    /**
     * @brief Function which checks if the loop should continue.
     * Optional. If loop = 0, it will not be used.
     * If integrated_loop_check = 1, it won't either.
     * @return 1 if the loop should stop, 0 if it should go on.
     */
    int (* stop_loop) (void *); 
    /**
     * @brief Pointer to the structure of arguments for stop_loop.
     */
    void * loop_check_arguments;
    /**
     * @brief The user may not want to undervolt. 0 means don't, >0 means do.
     * 
     */
    int undervolt;
} u_spec;

/**
 * @brief Check if /dev/cpu/0/msr is accessible.
 * Attemps to open the file and gives feedback if fails.
 * sets fd.
 * @return 1 if accessible; 0 if not.
 */
int msr_accessible_check();

/**
 * @brief Set undervolting specification to default values.
 * 
 */
void initialise_undervolting();

/**
 * @brief Check if u_spec was given in the corect format. Print any faults.
 * 
 * @return int 0 if all is OK, 1 if not.
 */
int faulty_undervolting_specification();

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