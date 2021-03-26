/**
 * @file faulty multiplication.c
 * @author your name (you@domain.com)
 * @brief This file is here to illustrate how the plundervolt library outght to be used.
 * It performs a multiplication of two numbers in several threads while:
 * A) lowering the voltage if doing Software undervolting;
 * or B) calling the connected Teensy system to set lower
 * voltage if doing Hardware undervolting,
 * until an error in the multiplication occurs.
 * @version 5.0
 * @date 2021-03-18
 */

/*
NOTE:
The following program producess an error, but some tweeks may be necessary to
    - the number of threads
    - the number of iterations in the loop-checking functions (see below)
    - the undervolting start and end
 */
#include "../lib/plundervolt.h"
#define HARDWARE // This will perform Hardware undervolting. Comment the line out is you want Software instead.
#ifdef HARDWARE
    #define num_1 11403264
    #define num_2 24
#else
    #define num_1 0xAE0000
    #define num_2 0x18
#endif
#define result num_1 * num_2;

/* This controls if other threads go on.
When one thread finds a fault, the others should stop, too,
so they set go_on to 0. */
int go_on = 1;
plundervolt_specification_t spec; // This is the specification for the library.

/*  This function is the loop check. It performs an operation which the user chooses, in this
        case it is multiplying two numbers and comparing the result to the known correct result,
        and it is used to check if the undervolting should stop.
    NOTE: This function does not stop the undervolting, only returns !0 if that is to happen.
*/
int multiplication_check_software() {
    uint64_t temp_res_1, temp_res_2;
    int iterations = 0;
    int max_iter = 1000000000;
    uint64_t check = result;
    int fault = 0;
    uint64_t operand1 = num_1;
    uint64_t operand2 = num_2;
    do {
        iterations++;
        temp_res_1 = operand1 * operand2;
        temp_res_2 = operand1 * operand2;

        // Stop if:
        //      - we are undervolting (spec.undervolt) and volgate
        //        hit the limit; OR
        //      - we are not undervolting.
        if (plundervolt_get_current_undervoltage() <= spec.end_undervoltage || !spec.undervolt) {
            break;
        }
    } while (temp_res_1 == check && temp_res_2 == check
            && iterations < max_iter
            && go_on);
    
    fault = temp_res_1 != check || temp_res_2 != check;
    if (fault) {
        printf("Fault occured.\nMultiplication 1: %016lx\nMultiplication 2: %016lx\n\
Original result:  %016lx\n\n", temp_res_1, temp_res_2, check);
    }
    return fault;
}

/*  This function is the loop check. It performs an operation which the user chooses, in this
        case it is multiplying two numbers and comparing the result to the known correct result,
        and it is used to check if the undervolting should stop.
    NOTE: This function does not stop the undervolting, only returns !0 if that is to happen.
*/
int multiplication_check_hardware() {
    uint64_t temp_res_1, temp_res_2;
    int iterations = 0;
    int max_iter = 1000000000;
    uint64_t check = result;
    int fault = 0;
    uint64_t operand1 = num_1;
    uint64_t operand2 = num_2;

    do {
        iterations++;
        printf("multiplication iteration: %d\n", iterations);
        temp_res_1 = operand1 * operand2;
        temp_res_2 = operand1 * operand2;
        printf("result: %ld\n\n", temp_res_1);

        // Stop if we are not undervolting.
        if (!spec.undervolt) {
            break;
        }
    } while (temp_res_1 == check && temp_res_2 == check // Fault
            && iterations < max_iter
            && go_on);
    fault = temp_res_1 != check || temp_res_2 != check;
    if (fault) {
        printf("Fault occured.\nMultiplication 1: %016lx\nMultiplication 2: %016lx\n\
Original result:  %016lx\n\n", temp_res_1, temp_res_2, check);
    }
    return fault;
}

/*  This is the main function. It calls the loop-checking function, and in this case stops the
        undervolting (by calling plundervolt_set_loop_finished()) if it return !0.
    NOTE: This specific function does not do anything but call the check, but that is not always
        the case. What this function does is up to the user.
*/
void multiply() {
    #ifdef HARDWARE
        if (multiplication_check_hardware()) { // This line calls the loop check function.
    #else
        if (multiplication_check_software()) { // This line calls the loop check function.
    #endif
        plundervolt_set_loop_finished(); // This line stops the undervolting process.
        go_on = 0;
    }
}

int main() {
    spec = plundervolt_init(); // Initialise the specification to default values.
                               // This is necessary!
    spec.function = multiply; // Set function to undervolt on.
    spec.integrated_loop_check = 1; // Loop check is integrated
    spec.threads = 1; // Do not set this too high. The undervolting then happens too quickly
                      // for all the iterations of the multiplication to take place.
    spec.undervolt = 1; // We do not wish to run this function alone, but undervolt in the process.
    spec.loop = 1; // The function is to be called in a loop.

    #ifdef HARDWARE
        spec.teensy_serial = "/dev/ttyACM0";
        // Teensy_baudrate stays the default.
        spec.repeat = 1000; // Undervolt only once per iteration.
        spec.delay_before_undervolting = 200;
        spec.duration_start = 35;
        spec.duration_during = -30;
        spec.start_voltage = 1.05;
        spec.undervolting_voltage = 0.795;
        spec.end_voltage = spec.start_voltage; // Reset the voltage to the start voltage.
                                               //This is not necessarily the case with every configuration.
        spec.tries = 50;
        spec.wait_time = 300;
        spec.u_type = hardware;
    #else
        spec.start_undervoltage = -100; // Set initial undervolting.
        spec.end_undervoltage = -230; // Set maximal undervolting.
        spec.u_type = software; // We want to undervolt softward-wise
    #endif

    plundervolt_debug(1);

    printf("Plundervolt specification initialised.\n");

    // We must take care of the possible errors during initialisation.
    plundervolt_error_t error_maybe = plundervolt_set_specification(spec);
    if (error_maybe) {
        plundervolt_print_error(error_maybe);
        return -1;
    }
    printf("Library successfully prepared.\nRunning:\n\n");

    // Now we run the function. Notice again that we check for errors.
    error_maybe = plundervolt_run();
    if (error_maybe) {
        plundervolt_print_error(error_maybe);
        return -1;
    }
    printf("Library finished.\nCleaning up:\n\n");
    plundervolt_cleanup(); // Must be called, or memory leakage may occur, and the voltage will be wrong.
    return 0;
}
