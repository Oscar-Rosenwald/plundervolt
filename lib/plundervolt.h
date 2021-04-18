/* plundervolt.h */

#ifndef PLUNDERVOLT_H
#define PLUNDERVOLT_H

#include <stdio.h>
#include <stdint.h>

/************************************************
 ********************* General ******************
 ************************************************/

/**
 * @brief Determines the type of undervolting to perform. plundervolt_specificaion_t is a type struct, which holds u_type, and that takes one of those values.
 * 
 */
typedef enum {software, hardware} undervolting_type;

/**
 * @brief Error codes for the library.
 * 
 */
typedef enum {
    PLUNDERVOLT_NO_ERROR = 0,
    PLUNDERVOLT_GENERIC_ERROR = 1,
    PLUNDERVOLT_RANGE_ERROR = 2,
    PLUNDERVOLT_NOT_INITIALISED_ERROR = 3,
    PLUNDERVOLT_NO_FUNCTION_ERROR = 4,
    PLUNDERVOLT_NO_LOOP_CHECK_ERROR = 5,
    PLUNDERVOLT_CANNOT_ACCESS_MSR_ERROR = 6,
    PLUNDERVOLT_NO_TEENSY_SERIAL_ERROR = 7,
    PLUNDERVOLT_NO_TRIGGER_SERIAL_ERROR = 8,
    PLUNDERVOLT_WRITE_TO_TEENSY_ERROR = 9,
    PLUNDERVOLT_CONNECTION_INIT_ERROR = 10
} plundervolt_error_t;

/**
 * @brief Structure which houses the undervolting specification, such as start and end voltage, 
 * number of threads or function to undervolt on.
 * 
 */
typedef struct plundervolt_specification_t {
    /* General */

    /**
     * If the function is to be repeated until fault,
     * loop will = 1.
     * 
     * If using Hardware undervolting, and if integrated_loop_check = 0, this contains the number of calls to the given function.
     * 
     * So for example, loop = 100 & integrated_loop_check = 0 means function will be called 100 times during every glitch.
     */
    int loop;
    /**
     * @brief Fuction which the user wishes to undervolt on. Its arguments are stored in a 
     * user-defined structure and passed as a void pointer. The function must cast this pointer
     * to said structure in order to use these arguments.
     */
    void (* function)(void *);
    /** 
     * @brief Pointer to the structure of arguments for the function above.
     * Must be a void pointer, as the structure holding the arguments
     * is defined by the user. The function must be implemented with this in mind.
     */
    void * arguments;
    /**
     * @brief > 0 if the user's function contains loop checks itself. i.e. if the function itself checks when to stop undervolting and calling the function in a loop.
     * NOTE: If it does, it must work with the shared variable loop_finished via plundervolt_set_loop_finished().
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
     * @brief Pointer to the user-defined structure of arguments for stop_loop.
     */
    void * loop_check_arguments;
    /**
     * @brief The user may not want to undervolt. 0 means don't, >0 means do.
     * 
     */
    int undervolt;
    /**
     * @brief Time for which the undervolting halts on each iteration, i.e. the time undervolting is given to work before a new iteration is called. Gives the system time to adjust to the new voltage. In ms.
     * 
     */
    int wait_time;
    /**
     * @brief Type of undervolting to do - Hardware or Software
     */
    undervolting_type u_type;
    
    /* Software */

    /**
     * @brief Software. Undervoltage to begin on.
     * For example: start_undervolting = -100 will start undervolting -100 mV,
     * not AT -100mV.
     * 
     */
    uint64_t start_undervoltage;
    /**
     * @brief Software. Number of threads to use for calling the function. (0 is same as 1)
     */
    int threads;
    /**
     * @brief Software. Lowest acceptable undervoltage.
     * Must be smaller than start_undervoltage. It does not mean the absolute voltage of the CPU,
     * but the ammount of undervolting.
     * 
     */
    uint64_t end_undervoltage;
    /**
     * @brief Software. How many mV we jump by when going from start_undervoltage to end_undervoltage.
     */
    int step;

    /* Hardware */

    /**
     * @brief Hardware. The port connecting to Teensy. Must be a string (char* []), as the arduino code opens the connection, and it needs a string.
     * 
     */
    char* teensy_serial;
    /**
     * @brief Hardware. The device name for the on-board trigger, which controls Teensy.
     * Only used if using_dtr is not 0.
     * 
     */
    char* trigger_serial;
    /**
     * @brief Hardware. Rate of communication with Teensy.
     * 
     */
    int teensy_baudrate;
    /**
     * @brief Hardware. >0 when using an onboard DTR trigger. Default is 1.
     * 
     */
    int using_dtr;
    /**
     * @brief Hardware. How many times to repeat the undervolting operation in one iteration.
     * 
     */
    int repeat;
    /**
     * @brief Hardware. Delay before a new glitch is fired. Im ms.
     * 
     */
    int delay_before_undervolting;
    /**
     * @brief Hardware. After initial voltage is set, how long to hold it.
     * 
     */
    int duration_start;
    /**
     * @brief Hardware. After undervolting starts, how long to do it for.
     * 
     */
    int duration_during;
    /**
     * @brief Hardware. What voltage to start the operation on.
     * 
     */
    float start_voltage;
    /**
     * @brief Hardware. What voltage to undervolt on.
     * 
     */
    float undervolting_voltage;
    /**
     * @brief Hardware. What voltage to end on.
     * 
     */
    float end_voltage;
    /**
     * @brief Hardware. How many iterations of undervolting to perfom. It acts as Hardware's version of end_voltage, i.e. it stops the undervolting after some number of iterations.
     * 
     */
    int tries;
} plundervolt_specification_t;

/**
 * @brief This function stops the undervolting loop in all threads.
 */
void plundervolt_set_loop_finished();

/* /**
 * @brief When the user wants to debug, they must call this function first.
 * Print statements are placed throughout the code, and the user will see
 * exactly what is happening, which methods are called with what arguments,
 * and what the library is executing.
 * NOTE: Some functions are called in loops, and would cluster the 
 * terminal. Depending on the argument LEVEL, these may only be printed once.
 * 
 * @param level int Controls how much is to be shown:
 * 
 * - 0 ... Don't print anything;
 * 
 * - 1 ... Print most relevant information (such as "now undervolting"). Default;
 * 
 * - 2 ... Print when a function is called, and with what arguments;
 * 
 * - 3 ... Print also when entering and exiting an operation (such as "glitch armed" or "opening connection to msr");
 * 
 * - 4 ... Print a statement on every iteration of every loop. WARNING: This will get messy.
 * 
 * The levels are in a hierarchy. So debug statements of level 0 are shown when level set to 1.
 
void plundervolt_debug(int level); */

/**
 * @brief Create a plundervolt_specification_t structure and fill it with default values.
 * This function must be called before calling plundervolt_set_specification and plundervolt_run.
 * 
 * @return A plundervolt_specivication_t object with default values.
 */
plundervolt_specification_t plundervolt_init();

/**
 * @brief Set the internal plundervolt specification of the library.
 * Must be called after plundervolt_init().
 * 
 * @param spec plundervolt_specification_t Specification to be set to.
 * @return plundervolt_error_t PLUNDERVOLT_NO_ERROR if plundervolt_init() was called before, PLUNDERVOLT_NOT_INITIALISED_ERROR otherwise.
 */
plundervolt_error_t plundervolt_set_specification(plundervolt_specification_t spec);

/**
 * @brief Performs the undervolting according to specification.
 * In Software undervolting, it ought to run in a separate thread.
 * 
 * @return void* Must return something, as pthread_create requires a void* return value. Returns NULL.
 */
void* plundervolt_apply_undervolting();

/**
 * @brief The main function of the undervolting library. It creates threads, runs given functions,
 * possibly in a loop, and checks the conditions for stopping the loops, all according to specifications. User may or may not wish to use this function.
 * 
 * @return plundervolt_error_t Appropriate error code, or PLUNDERVOLT_NO_ERROR
 */
plundervolt_error_t plundervolt_run();

/**
 * @brief A function to be called at the end of the library usage.
 * It closes open files, and resets voltage.
 * 
 */
void plundervolt_cleanup();

/**
 * @brief Prints the appropriate message to the plundervolt_error_t passed as argument.
 * 
 * @param error plundervolt_error_t The error to translate.
 */
void plundervolt_print_error(plundervolt_error_t error);

/**
 * @brief Turns the given error into a string.
 * 
 * @param error Error to convert to string.
 * @return char* Pointer to the return string.
 */
const char* plundervolt_error2str(plundervolt_error_t error);

/**
 * @brief Resets the voltage to normal levels. Use if Software undervolting, or using onboard DTR Trigger when Hardware-undervolting.
 * 
 */
void plundervolt_reset_voltage();
/**
 * @brief Sets fd, fd_teensy, and fd_trigger. If Software undervolting (u_spec.u_type = software), connect fd to /dev/cpu/0/msr. If Hardware undervolting (u_spec.u_type = hardware), connect fd_teensy to Teensy. If Hardware undervolting and also using Trigger (u_spec.using_dtr = 1), also connect fd_trigger to the onboard DTR trigger.
 * 
 * @return plundervolt_error_t PLUNDERVOLT_NO_ERROR if connection(s) opened. If not, returns the appropriate error for what happened.
 */
plundervolt_error_t plundervolt_open_file();

/************************************************
 ************* Software undervolting ************
 ************************************************/

/**
 * @brief Compute the value which will be written to cpu/0/msr.
 * 
 * @param value Value to be turned into the result
 * @param plane A plane index 
 * @return uint64_t The value to write to cpu/0/msr
 */
uint64_t plundervolt_compute_msr_value(int64_t value, uint64_t plane);

/**
 * @brief Reads the current voltage if using Software undervolting.
 * 
 * @return double Current voltage as read from fd.
 */
double plundervolt_read_voltage();

/**
 * @brief Set new undervoltage. The parameter should be the result of plundervolt_compute_msr_value().
 * 
 * @param value uint64_t value of the new undervoltage.
 */
void plundervolt_set_undervolting(uint64_t value);

/**
 * @return uint64_t Current undervoltage in mV.
 */
uint64_t plundervolt_get_current_undervoltage();

/************************************************
 ************* Hardware undervolting ************
 ************************************************/

/**
 * @brief Opens the serial port(s) fd_teensy (and fd_trigger if applicable - see plundervolt_specification_t), throwing the appropriate exceptions.
 * 
 * @param teensy_serial char* const Serial port of the Teensy system
 * @param trigger_serial char* const Serial port for the on-board trigger. Can be anything if !using_dtr.
 * @param teensy_baudrate int Boud rate of the undervolting
 * 
 * @return Error message if initialisation of connection with Teensy failed.
 */
plundervolt_error_t plundervolt_init_hardware_undervolting(char* const teensy_serial, char* const trigger_serial, int teensy_baudrate);

/**
 * @brief Read response from Teensy and print it out.
 * 
 */
void plundervolt_teensy_read_response();

/**
 * @brief Send undervolting configuration to Teensy. This does not arm the glitch. Call "plundervolt_arm_glitch()" after this.
 * It uses the following parameters in the user specification:
 * 
 * @param delay_before_undervolting How many miliseconds to wait before the operation is started
 * @param repeat How many times to repeat the undervolting
 * @param start_voltage Voltage at the beginning of the operation
 * @param duration_start Delay between start_voltage and undervolting_voltage
 * @param undervolting_voltage Voltage during undervolting
 * @param duration_during Delay between undervolting_voltage and end_voltage
 * @param end_voltage Voltage at the end of the operation
 * 
 * @return Error if connection not initialised, PLUNDERVOLT_NO_ERROR if yes.
 */
plundervolt_error_t plundervolt_configure_glitch();

/**
 * @brief Call after plundervolt_configure_glitch().
 * This function prepares the glitch in the Teensy system to be used.
 * Use before plundervolt_fire_glitch().
 * 
 * @return Error if connection not initialised, PLUNDERVOLT_NO_ERROR if yes.
 */
plundervolt_error_t plundervolt_arm_glitch();

/**
 * @brief Send an EOL character to the Teensy system to indicate end of input, or activate the onboard trigger DTR if using_dtr = 1.
 * This will start the undervolting.
 * 
 * @return Error if writing to Teensy failed.
 */
plundervolt_error_t plundervolt_fire_glitch();

#endif /* PLUNDERVOLT_H */