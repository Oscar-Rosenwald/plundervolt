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

int check(uint64_t res) {
    uint64_t temp_res_1, temp_res_2;
    int iterations = 0;
    int max_iter = 1000;
    do {
        iterations++;
        temp_res_1 = num_1 * num_2;
        temp_res_2 = num_1 * num_2;
    } while (temp_res_1 == temp_res_2 && iterations < max_iter);
    return temp_res_1 != temp_res_2;
}

void multiply() {
    uint64_t res = num_1 * num_2;
    if (check(res)) {
        plundervolt_set_loop_finished();
    }
}

int main() {
    plundervolt_specification_t spec = plundervolt_init();
    spec.function = multiply;
    spec.start_undervoltage = -180;
    spec.end_undervoltage = -200;
    spec.integrated_loop_check = 1;
    spec.threads = 4;
    spec.undervolt = 1;

    plundervolt_error_t error_maybe = plundervolt_set_specification(spec);
    if (error_maybe) {
        plundervolt_print_error(error_maybe);
        return -1;
    }
    error_maybe = plundervolt_run();
    if (error_maybe) {
        plundervolt_print_error(error_maybe);
        return -1;
    }
    plundervolt_cleanup();
    return 0;
}