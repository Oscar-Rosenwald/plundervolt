#include "plundervolt.h"

typedef struct arguments_structure {
    int first;
    int second;
} arguments_structure;

void function (void* argument_list) {
    arguments_structure *args = (arguments_structure*) argument_list;
}

int main () {
    arguments_structure arguments;
    arguments.first = 1;
    arguments.second = 2;

    u_spec.loop = 1;
    u_spec.threads = 1;
    u_spec.function = function;
    u_spec.arguments = (void *) &arguments;
    u_spec.start_voltage = 700;
    u_spec.end_voltage = 695;

    return plundervolt();
}