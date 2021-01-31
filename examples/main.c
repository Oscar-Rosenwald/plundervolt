#include "../lib/plundervolt.h"

typedef struct arguments_structure {
    int first;
    int second;
} arguments_structure;
plundervolt_specification_t spec;

int loop_check(void * arg) {
    return plundervolt_get_current_undervoltage() == spec.end_undervoltage;
}

void function (void* argument_list) {
    arguments_structure *args = (arguments_structure*) argument_list;
    // printf("1: %d, 2: %d\n", args->first, args->second);
    // plundervolt_set_loop_finished();
}

int main () {
    arguments_structure arguments;
    arguments.first = 1;
    arguments.second = 2;

    spec = plundervolt_init();
    spec.loop = 1;
    spec.threads = 4;
    spec.function = function;
    spec.arguments = (void *) &arguments;
    spec.start_undervoltage = -100;
    spec.end_undervoltage = -105;
    spec.integrated_loop_check = 0;
    spec.stop_loop = loop_check;
    spec.loop_check_arguments = NULL;
    spec.undervolt = 1;

    int error_maybe = plundervolt_set_specification(spec);
    if (error_maybe) {
        plundervolt_print_error(error_maybe);
    }

    error_maybe = plundervolt_run();
    if (error_maybe) {
        plundervolt_print_error(error_maybe);
    }
    plundervolt_cleanup();
}