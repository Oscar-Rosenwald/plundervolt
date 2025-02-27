/*
NOTE:
The following program producess an error, but some tweeks may be necessary to
    - the number of threads
    - the number of iterations in the loop-checking functions (see below)
    - the undervolting start and end
 */
#include "../lib/plundervolt.h"
#include <unistd.h>
//#define HARDWARE // This will perform Hardware undervolting. Comment the line out is you want Software instead.
//#define TESTING // This allows us to find the right voltage for the PC.
#define num_1 0xAE0000
#define num_2 0x18
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
int multiplication_check() {
    uint64_t volt = plundervolt_get_current_undervoltage();
    printf("Current undervoltage: %ld\n", volt);
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
        if (volt <= spec.end_undervoltage || !spec.undervolt) {
            break;
        }
    } while (temp_res_1 == check && temp_res_2 == check // Fault hasn't occured.
            && iterations < max_iter
            && go_on); // Other threads haven't stopped the loop.
    
    fault = temp_res_1 != check || temp_res_2 != check;
    if (fault) {
        printf("Fault occured.\nMultiplication 1: %016lx\nMultiplication 2: %016lx\n\
Original result:  %016lx\nundervoltage: %ld mV\n\n", temp_res_1, temp_res_2, check, plundervolt_get_current_undervoltage());
    }
    return fault;
}

/*  This is the main function. It calls the loop-checking function, and in this case stops the
        undervolting (by calling plundervolt_set_loop_finished()) if it return !0.
    NOTE: This specific function does not do anything but call the check, but that is not always
        the case. What this function does is up to the user.
*/
void multiply() {
    int loop_running = plundervolt_loop_is_running();
    if (multiplication_check() || !loop_running) { // This line calls the loop check function.
        plundervolt_set_loop_finished(); // This line stops the undervolting process.
        go_on = 0; // This is for threads. It stops all loops defined here, which plundervolt_set_loop_finished() doesn't have access to.
    }
    sleep(0.3); // This is necessary due to (we believe) some assembly-level pre-computation missteps.
}

void setup() {
    spec = plundervolt_init(); // Initialise the specification to default values.
                               // This is necessary!
    spec.function = multiply; // Set function to undervolt on.
    spec.integrated_loop_check = 1; // Loop check is integrated
    spec.threads = 4; // Do not set this too high. The undervolting then happens too quickly
                      // for all the iterations of the multiplication to take place.
    spec.undervolt = 1; // We do not wish to run this function alone, but undervolt in the process.
    spec.loop = 1; // The function is to be called in a loop.

    spec.start_undervoltage = -130; // Set initial undervolting.
    spec.end_undervoltage = -230; // Set maximal undervolting.
    spec.wait_time = 2000;
    spec.u_type = software; // We want to undervolt softward-wise
}

int main() {
    setup();
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
