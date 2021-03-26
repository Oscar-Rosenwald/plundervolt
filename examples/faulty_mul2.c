#include "../lib/plundervolt.h"
#include <unistd.h>

#define HARDWARE // This will perform Hardware undervolting. Comment the line out is you want Software instead.
#define num_1 0xAE0000
#define num_2 0x18
#define result num_1 * num_2;
#define msleep(tms) ({usleep(tms * 1000);})

int go_on = 1;
plundervolt_specification_t spec;

void multiply() {
    uint64_t temp_res_1, temp_res_2;
    int iterations = 0;
    int max_iter = 10000000;
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

        if (temp_res_1 != check || temp_res_2 != check) {
            fault = 1;
            break;
        }
    } while (iterations <= max_iter);
    if (fault) {
        printf("Fault occured.\nMultiplication 1: %016lx\nMultiplication 2: %016lx\n\
Original result:  %016lx\n\n", temp_res_1, temp_res_2, check);
    }
}

int main() {
    plundervolt_debug(1);
    spec = plundervolt_init();
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

    plundervolt_error_t error_maybe = plundervolt_set_specification(spec);
    if (error_maybe) {
        plundervolt_print_error(error_maybe);
        return -1;
    }
    printf("Library successfully prepared.\nRunning:\n\n");

    // Open Teensy connection
    error_maybe = plundervolt_init_teensy_connection(spec.teensy_serial, spec.teensy_baudrate);
    if (error_maybe) {
        plundervolt_print_error(error_maybe);
        return -1;
    }
    

    for (int i = 0; i < spec.tries; i++) {
        plundervolt_configure_glitch(spec.delay_before_undervolting, spec.repeat, spec.start_voltage, spec.duration_start, spec.undervolting_voltage, spec.duration_during, spec.end_voltage);

        plundervolt_arm_glitch();
        plundervolt_fire_glitch();

        msleep(300);
        multiply();
        msleep(300);
    }
}