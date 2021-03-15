/**
 * @file faulty multiplication.c
 * @author your name (you@domain.com)
 * @brief This file is here to illustrate how the plundervolt library outght to be used.
 * It performs a multiplication of two numbers in several threads while lowering the voltage
 * of the system, until an error in the multiplication occurs.
 * @version 4
 * @date 2021-01-31
 */

/*
NOTE:
The following program producess an error, but some tweeks may be necessary to
    - the number of threads
    - the number of iterations in the loop-checking functions (see below)
    - the undervolting start and end
 */
#include "../lib/plundervolt.h"
#define num_1 0xAE0000
 #define num_2 0x18
#define result num_1 * num_2;

int go_on = 1;
plundervolt_specification_t spec; // This is the specification for the library.

/*  This function is the loop check. It performs an operation which the user chooses, in this
        case it is multiplying two numbers and comparing the result to the known correct result,
        and it is used to check if the undervolting should stop.
    NOTE: This function does not stop the undervolting, only returns !0 if that is to happen.
*/
int multiplication_check() {
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
        if (spec.undervolt && plundervolt_get_current_undervoltage() <= spec.end_undervoltage) {
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

/*  This is the main function. It calls the loop-checking function, and in this case stops the
        undervolting (by calling plundervolt_set_loop_finished()) if it return !0.
    NOTE: This specific function does not do anything but call the check, but that is not always
        the case. What this function does is up to the user.
*/
void multiply() {
    if (multiplication_check()) { // This line calls the loop check function.
        plundervolt_set_loop_finished(); // This line stops the undervolting process.
        go_on = 0;
    }
}

int main() {
    spec = plundervolt_init(); // Initialise the specification to default values.
                               // This is necessary!
    spec.function = multiply; // Set function to undervolt on.
    spec.start_undervoltage = -100; // Set initial undervolting.
    spec.end_undervoltage = -230; // Set maximal undervolting.
    spec.integrated_loop_check = 1; // Loop check is integrated
    spec.threads = 3; // Do not set this too high. The undervolting then happens too quickly
                      // for all the iterations of the multiplication to take place.
    spec.undervolt = 1; // We do not wish to run this function alone, but undervolt in the process.
    spec.loop = 1; // The function is to be called in a loop.
    spec.u_type = software; // We want to undervolt softward-wise
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