# Plundervolt library #

This library is a continuation of ideas proposed in other papers, discussed below in section [Original code](#original-code). Its aim is to allow a smooth application of the concepts described in those papers by providing a well documented, simple to use, and as intuitive as possible catalogue of functions.

While a potential user will still require a level of understanding of what undervolting is, the idea is to abstract the process to a point where only superficial consideration of it is necessary to write code which makes use of it. The details are handled by the library.

## Original code ##

There are two papers which this library draws heavy inspiration from. Both provide a proof of concept, whose code was used in numerous functions, sometimes almost verbatim.

These papers are:
* *Plundervolt: Software-based Fault Injection Attacks against Intel SGX* - code available in [this link](https://github.com/KitMurdock/plundervolt).
* *VoltPillager: Hardware-based fault injection attacks against Intel
SGX Enclaves using the SVID voltage scaling interface* - available [here](https://github.com/zt-chen/voltpillager).

It is important to note a subdirectory of this library - arduino - was lifted from the latter entirely, and used without change. However, the user ought not to be in need of using this themselves.

## Broad usage ##

The library operates on two principles:

  * A single structure is necessary to provide all parameters.
	In many cases, a public function will ask for the specification again, so that the user may be able to call it with whatever parameters they wish, but a system is in place to make this step automated. Read on for more in [Usage](#usage).
  * The user has access to any and all functions necessary to perform the undervolting, or a larger function with a limited, predefined functionality, which does it for them.
	In other words, only a limited number of functions is needed to use this library, and this number can be decreased in some cases. Also, every function, which the user may wish to use for greater control over the process, is accessible to them.

The user must fill in values of this structure, and subsequently can call all public functions to achieve whatever result they want, or instruct the library to perform the default operation (see [below](#default-operation), over which they have a degree of control as well.

# Structure #

```
├── lib										// Library files
    ├── arduino								// Arduino library files
├── examples								// Provided examples of usage
    ├── faulty_multiplication_software.c	// Usage of software undervolting
	├── faulty_multiplication_hardware.c	// Usage of hardware undervolting
```


# Usage #

As stated above, the library gives two levels of control to the user. One is lesser, but easier to work with due to the smaller number of functions the user has to call themselves. The other is simply access to all necessary functions, which the user may call whenever and however they wish.

In both cases, defining the specification is essential. A range of checks is performed to ensure it is well-defined, or defined at all.

## Plundervolt specification ##

First of all, the specification **must** be initialised. Use `plundervolt_init()` to do so. This returns a structure with default values.

Then, the user has access to all variables in this structure, and is free to define them according to their desire.

Afterwards, these changes **must** be saved by calling `plundervolt_set_specification()` with the structure as the argument.

Example:
```
plundervolt_spec_t spec = plundervolt_init();
spec.u_type = software; // Tell the library to use software undervolting
plundervolt_set_specification(spec);
```

### Variables ###
	
#### General ####

  * `int loop` Run a user-defined function in loop.
  * `void (* function)(void *)` User-defined function.
  * `void *arguments` Arguments for the above function. See [notes](#passing-arguments).
  * `int integrated_loop_check` 1 if the above function stops all loops by calling `plundervolt_set_loop_finished()`; 0 if there is another function which checks that.
  * `int (* stop_loop)(void *)` The (optional) function which stops all loops if `integrated_loop_check` is 0).
  * `void *loop_check_arguments` Arguments for `stop_loop()`. See [notes](#passing-arguments).
  * `int undervolt` 1 if undervolting is to happen, i.e. not just simply running of provided functions. NOTE: This has meaning only if the [default operation](#default-operation) is used.
  * `int wait_time` In various places, the library sleeps. This tells in how long to do so, in ms.
  * `undervolting_type u_type` Either `hardware` or `software`. **Must be set**.

#### Software ####

  * `int threads` Number of threads to run the `function()` in.
  * `uint64_t start_undervoltage` Undervoltage to start on. Must be negative, otherwise is overvoltage.
  * `uint64_t end_undervoltage` Undervoltage to end on. Must be smaller than `start_undervoltage`.
  * `int step` When lowering the undervoltage from `start_` to `end_undervoltage`, by how many mV do we lower it.

#### Hardware ####

  * `char* teensy_serial` The name of the device communicating with Teensy.
  * `char* trigger_serial` The name of the device communicating with the onboard trigger, which sets Teensy off. NOTE: Only relevant it using_dtr is 1.
  * `int teensy_boudrate` Rate of communication with Teensy.
  * `int using_dtr` 1 if we are using the onboard trigger to set Teensy off, i.e. start undervolting.
  * `int repeat` Teensy can repeat the given instruction. How many times to do so?
  * `int delay_before_undervolting` How many ms to wait before undervolting starts *after* it has been triggered.
  * `int duration_start` How long to stay on `start_voltage` (see below).
  * `int duration_during` How long to hold the undervolting at `undervolting_voltage` (see below).
  * `int start_voltage` What *voltage* (not undervolting) we start at once Teensy is set off.
  * `int undervolting_voltage` What *voltage* (not undervolting) we hold in the main part of the operation (the lowest point).
  * `int end_voltage` What *voltage* (not undervolting) we end the operation on. Can be same as `start_voltage`.
  * `int tries` How many iterations of the same configuration to run.

## Public functions ##

Some functions are private to the library. They are not necessary to use the library, only made it easier for the library to be written.

Other functions are public, so the user may dictate what is happening with great control.

### General ###

  * `plundervolt_init()` Initialise the specification.
  * `plundervolt_set_specification()` Send new specification to the library.
  * `plundervolt_apply_undervolting()` Start undervolting according to specification.
  * `plundervolt_run()` Run the [default operation](#default-operation).
  * `plundervolt_cleanup()` Close open files. **Must be called** at the end of the program to avoid memory leakage.
  * `plundervolt_print_error()` The library returns a host of error codes. This function prints the appropriate string when passed that error code.
  * `plundervolt_error2str()` When passed an error code, returns the string to describe it.
  * `plundervolt_reset_voltage()` Reset the voltage to the original value. If software-undervolting, do just that. If hardware-undervolting, reset the pin, i.e. the onboard trigger.
  * `plundervolt_open_file()` Opens appropriate files depending on what type of undervolting (hard-/software) we are using.
  * `plundervolt_loop_is_running()` Returns 1 if `function` is running in a loop.
  * `plundervolt_faulty_undervolting_specification()` Checks if the specification is sensible.

### Software ###

  * `plundervolt_compute_msr_value()` Compute the value which will be written to cpu/0/msr.
  * `plundervolt_read_voltage()` Reads the current voltage.
  * `plundervolt_set_undervolting()` Set new undervoltage, i.e. change the current one to a new one.
  * `plundervolt_software_undervolt()` Perform software undervolting. The argument is the new undervoltage value.
  * `plundervolt_get_current_undervoltage()` Read current undervoltage.

### Hardware ###

  * `plundervolt_init_hardware_undervolting()` Initialise the files to given devices `teensy_serial` and `trigger_serial`.
  * `plundervolt_teensy_read_response()` Sometimes, Teensy gives a response. Read and print it.
  * `plundervolt_configure_glitch()` Send specification of a "glitch", i.e. the undervolting operation, to Teensy.
  * `plundervolt_arm_glitch()` Prepare Teensy to start undervolting.
  * `plundervolt_fire_glitch()` Start undervolting.

## Errors ##

The library functions return error codes. Almost every function does this. Use `plundervolt_print_error()` to read what happened.

However, `PLUNDERVOLT_NO_ERROR` is an int of value 0, so one can use it as a condition `if (any_function_returning_error()) {...`.

## Passing arguments ##

There are two functions which the library runs, but are defined by the user. They are both in the specification, and must be identified by a function pointer. They are `function` (the main function to run while undervolting), and `stop_loop` (checks if the loops - in all threads - should stop; only applicable if `function` doesn't check itself, or `loop` doesn't specify clearly how many times to run the function).

Their arguments must also be defined by the user as a structure. This structure must be also identified by a pointer (a void pointer), so A) The values can be changed in real time; and B) The library can typecheck.

`function` gets pointer `arguments`, and must cast it to the structure pointer, which it can work with.

`stop_loop` gets `loop_check_arguments` and must do the same.

```
typedef struct argument {
	int a;
} argument;

void function(void *arguments) {
	argument *ar = (argument *) arguments;
	int a = ar->a;
	// Work with a.
}

int main() {
	argument arg;
	specification.function = function;
	specification.arguments = &arg;
}
```

## Default operation ##

The library offers a default operation invoked by calling `plundervolt_run()` after setting the specification. This does many things for the user. It opens the files (`plundervolt_open_file()`); creates threads; calls the undervolting (`plundervolt_apply_undervolting()`); and thus runs the function `function`, possibly with `stop_loop`.

When performing hardware undervolting, there is little change beyond that. But with software, it lowers the undervotlage by defined `step` until the user-defined functions tell it to stop, or it hits `end_undervoltage`.

The user must still close the files (`plundervolt_cleanup()`), because they may wish to continue working after this, so the library doesn't presume to know better.

# Compilation note #
	
If you change the library code and need to compile it as a dependency, always use the `-pthread` option.