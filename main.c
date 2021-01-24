#include "plundervolt.h"

typedef struct arguments_structure {
    int first;
    int second;
} arguments_structure;

int loop_check(void * arg) {
    return current_voltage == u_spec.end_voltage;
}

void function (void* argument_list) {
    arguments_structure *args = (arguments_structure*) argument_list;
    // loop_finished = loop_check(NULL);
}

int main () {
    arguments_structure arguments;
    arguments.first = 1;
    arguments.second = 2;

    initialise_undervolting();

    u_spec.loop = 1;
    u_spec.threads = 2;
    u_spec.function = function;
    u_spec.arguments = (void *) &arguments;
    u_spec.start_voltage = 700;
    u_spec.end_voltage = 698;
    u_spec.integrated_loop_check = 0;
    u_spec.stop_loop = loop_check;
    u_spec.loop_check_arguments = NULL;
    u_spec.undervolt = 1;

    return plundervolt();
}