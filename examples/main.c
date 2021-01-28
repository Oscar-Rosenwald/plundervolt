#include "../lib/plundervolt.h"

typedef struct arguments_structure {
    int first;
    int second;
} arguments_structure;

int loop_check(void * arg) {
    return plundervolt_get_current_voltage == u_spec.end_voltage;
}

void function (void* argument_list) {
    arguments_structure *args = (arguments_structure*) argument_list;
    printf("1: %d, 2: %d\n", args->first, args->second);
    // plundervolt_set_loop_finished();
}

int main () {
    arguments_structure arguments;
    arguments.first = 1;
    arguments.second = 2;

    plundervolt_specification_t spec = plundervolt_init();
    spec.loop = 1;
    spec.threads = 2;
    spec.function = function;
    spec.arguments = (void *) &arguments;
    spec.start_undervolting = -100;
    spec.end_undervolting = -102;
    spec.integrated_loop_check = 0;
    spec.stop_loop = loop_check;
    spec.loop_check_arguments = NULL;
    spec.undervolt = 1;

    int error_maybe = plundervolt_set_specification(spec);
    if (error_maybe) {
        plundervolt_print_error(error_maybe);
    }

    return plundervolt_run();
}