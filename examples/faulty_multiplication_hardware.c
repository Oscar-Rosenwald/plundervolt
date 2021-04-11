#include "../lib/plundervolt.h"
#include <unistd.h>
#include <stdlib.h>

#define num_1 0xAE0000
#define num_2 0x18
#define result num_1 * num_2;

int go_on = 1;
plundervolt_specification_t spec;
int fault = 0;

void multiply() {
    int max_iter = 300000;
    int iterations = 0;

    typedef struct calc_info {
        uint64_t operand1;
        uint64_t operand2;
        uint64_t correct_a;
        uint64_t correct_b;
    } calc_info;

    calc_info* in = malloc(sizeof(calc_info));

    in->operand1 = num_1;
    in->operand2 = num_2;

    plundervolt_fire_glitch(); // This tells Teensy to start the operation.
    // NOTE: We must use this in this function, because we are using HARDWARE undervolting.
    // In SOFTWARE undervolting, the library calls these itself in another thread.
    do {
        iterations++;

        in->correct_a = in->operand1 * in->operand2;
        in->correct_b = in->operand1 * in->operand2;
        
        if (in->correct_a != in->correct_b) {
            fault = 1;
            go_on = 0;
        }
    } while (iterations < max_iter && fault == 0
            && go_on); // Other threads (if there are any) will stop now.
    plundervolt_reset_voltage(); // Set the voltage to what it was before.
    // This is a misnomer. We aren't resetting the voltage here, but the pins connected to Teensy.
    // The voltage is reset automatically.
    // This function is used anyway, as the same must be called after SOFTWARE undervolting, so it makes sense
    // it would be called after HARDWARE as well.

    if (fault) {
        printf("Fault: occured.\nMultiplication 1: %016lx\nMultiplication 2: %016lx\n", in->correct_a, in->correct_b);
    }
    free(in);
}

void setup() {
    spec = plundervolt_init();
    spec.loop = 0; // The loop happens inside the multiply() function, so we don't need the library to do it.
    spec.threads = 1;
    spec.function = multiply;
    spec.arguments = NULL; // multiply() has no arguments.
    spec.integrated_loop_check = 1; // multiply() checks itself if the loop in it should stop. No other functions are needed for it.
    // spec.stop_loop is not set --> see intergrated_loop_check
    // spec.loop_check_arguments is not set --> No stop_loop, so no arguments to pass to it.
    spec.undervolt = 1; // We are definitely undervolting.
    spec.wait_time = 300; // Experiamentally chosen. It is time to let the PC work before we call functions, i.e. time in which the undervolting is prepared.
    spec.u_type = hardware;

    // The following are not set, as they relate only to SOFTWARE undervolting.
    // spec.start_undervoltage
    // spec.end_undervoltage 
    // spec.step

    spec.teensy_serial = "/dev/ttyACM0";
    spec.trigger_serial = "/dev/ttyS0";
    spec.teensy_baudrate = 115200; // Same as default.
    spec.using_dtr = 1; // We assume the on-board trigger.
    spec.repeat = 2; // We want two tries per specification, just to make sure we don't skip the right one.
    spec.delay_before_undervolting = 200;
    spec.duration_start = 35;
    spec.duration_during = -30;
    spec.start_voltage = 1.05;
    spec.undervolting_voltage = 0.815; // This value is to be changed. Main() decrements it, until the right voltage is found.
    spec.end_voltage = spec.start_voltage; // Return to the same voltage as you started.
    spec.tries = 1; // Since we loop ourselves, we don't need more than one try per plundervolt_run() call.
}

int main() {
    setup();
    plundervolt_error_t error_maybe;

    // This finds the right voltage to undervolt on. Parameters are largly arbitrary, more precisely tuned for our test PC's
    for (int i = 0; i < 10; i++) {
        spec.undervolting_voltage -= 0.002; // Change the voltage during undervolting
        plundervolt_set_specification(spec); // Make library see the changes.

        printf("Iteration. Voltage: %f\n", spec.undervolting_voltage);

        error_maybe = plundervolt_run();
        if (error_maybe) {
            plundervolt_print_error(error_maybe);
            return -1;
        }

        // If the function in this iteration found a fault, stop the loop early and exit.
        // The voltage needed to fault will be printed out.
        if (fault) {
            return 0;
        }
    }

    plundervolt_cleanup();
    printf("End. No fault.\n");
    return 0;
}