/* plundervolt.h */

#ifndef PLUNDERVOLT_H
#define PLUNDERVOLT_H

#include <stdio.h>
#include <stdint.h>

// Software for this version of the code
typedef enum {software, hardware} undervolting_type;

typedef enum {
    PLUNDERVOLT_NO_ERROR = 0,
    PLUNDERVOLT_GENERIC_ERROR = 1,
    PLUNDERVOLT_RANGE_ERROR = 2,
    PLUNDERVOLT_NOT_INITIALISED_ERROR = 3,
    PLUNDERVOLT_NO_FUNCTION_ERROR = 4,
    PLUNDERVOLT_NO_LOOP_CHECK_ERROR = 5,
    PLUNDERVOLT_CANNOT_ACCESS_MSR_ERROR = 6
} plundervolt_error_t;

/**
 * @brief Structure which houses the undervolting specification, such as start and end voltage, 
 * number of threads or function to undervolt on.
 * 
 */
typedef struct plundervolt_specification_t {
    /**
     * @brief Undervoltage to begin on.
     * For example: start_undervolting = -100 will start undervolting -100 mV,
     * not at -100mV.
     * 
     */
    uint64_t start_undervoltage;
    /**
     * @brief Lowest acceptable undervoltage.
     * Must be smaller than start_undervoltage. It does not mean the absolute voltage of the CPU,
     * but the ammount of undervolting.
     * 
     */
    uint64_t end_undervoltage;
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
    /**
     * @brief Time for which the undervolting halts on each iteration. It is a rough estimate in
     * software undervolting. In ms.
     * 
     */
    int wait_time;
    /**
     * @brief Type of undervolting to do - Hardware or Software
     */
    undervolting_type u_type;
} plundervolt_specification_t;

/**
 * @return uint64_t Current undervoltage in mV.
 */
uint64_t plundervolt_get_current_undervoltage();

/**
 * @brief This function stops the undervolting loop.
 */
void plundervolt_set_loop_finished();

/**
 * @brief Create a plundervolt_specification_t structure and fill it with default values.
 * This function must be called before calling plundervolt_set_specification.
 * 
 */
plundervolt_specification_t plundervolt_init();

/**
 * @brief Set the internal plundervolt specification of the library.
 * Must be called afte plundervolt_init().
 * 
 * @param spec Specification to be set to.
 * @return int 0 if no error, PLUNDERVOLT_NOT_INITIALISED_ERROR otherwise.
 */
int plundervolt_set_specification(plundervolt_specification_t);

/**
 * @brief Compute the value which will be written to cpu/0/msr.
 * 
 * @param value Value to be turned into the result
 * @param plane A plane index 
 * @return uint64_t 
 */
uint64_t plundervolt_compute_msr_value(int64_t, uint64_t);

/**
 * @brief Reads the current voltage
 * 
 * @return double Current voltage as read from fd.
 */
double plundervolt_read_voltage();

/**
 * @brief Set new undervoltage. The parameter should be the result of plundervolt_compute_msr_value().
 * 
 * @param value uint64_t value of the new undervoltage.
 */
void plundervolt_set_undervolting(uint64_t);

/**
 * @brief Performs the undervolting according to specification.
 * It ought to run in a separate thread
 * 
 * @return void* Must return something, as pthread_create requires a void* return value. Returns NULL.
 */
void* plundervolt_apply_undervolting();

/**
 * @brief Resets the voltage to normal levels
 * 
 */
void plundervolt_reset_voltage();

/**
 * @brief The main function of the undervolting library. It creates threads, runs given functions,
 * possibly in a loop, and checks the conditions for stopping the loops, all according to specifications.
 * 
 * @return int 0 if OK, error code if otherwise.
 */
int plundervolt_run();

/**
 * @brief A function to be called at the end of the library usage.
 * It closes open files, and resets voltage.
 * 
 */
void plundervolt_cleanup();

/**
 * @brief Prints the appropriate message to the plundervolt error passed as argument.
 * 
 * @param error The error to translate.
 */
void plundervolt_print_error(plundervolt_error_t);

#endif /* PLUNDERVOLT_H */