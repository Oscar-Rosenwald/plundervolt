/**
 * @file faulty multiplication.c
 * @author your name (you@domain.com)
 * @brief This file is here to illustrate how the plundervolt library outght to be used.
 * It performs a multiplication of two numbers in several threads while lowering the voltage
 * of the system, until an error in the multiplication occurs.
 * @version 0.1
 * @date 2021-01-29
 * 
 * @copyright Copyright (c) 2021
 * 
 */
#include "../lib/plundervolt.h"
#define num_1 0xAE0000
#define num_2 0x18
#define result num_1 * num_2

int go_on = 1;
plundervolt_specification_t spec;

int check() {
    uint64_t temp_res_1, temp_res_2;
    int iterations = 0;
    int max_iter = 1000;
    do {
        iterations++;
        temp_res_1 = num_1 * num_2;
        temp_res_2 = num_1 * num_2;
    } while (temp_res_1 == temp_res_2 && iterations < max_iter
            && go_on
            && plundervolt_get_current_undervoltage() > spec.end_undervoltage);
    return temp_res_1 != temp_res_2;
}

void multiply() {
    if (check()) {
        plundervolt_set_loop_finished();
        go_on = 0;
        printf("Faulty result.\n");
    }
}

int main() {
    spec = plundervolt_init();
    spec.function = multiply;
    spec.start_undervoltage = -150;
    spec.end_undervoltage = -210;
    spec.integrated_loop_check = 1;
    spec.threads = 1;
    spec.undervolt = 1;
    printf("Plundervolt specification initialised.\n");

    plundervolt_error_t error_maybe = plundervolt_set_specification(spec);
    if (error_maybe) {
        plundervolt_print_error(error_maybe);
        return -1;
    }
    printf("Library successfully prepared.\nRunning:\n\n");
    error_maybe = plundervolt_run();
    if (error_maybe) {
        plundervolt_print_error(error_maybe);
        return -1;
    }
    printf("Library finished.\nCleaning up:\n\n");
    plundervolt_cleanup();
    return 0;
}