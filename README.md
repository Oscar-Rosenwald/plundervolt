# Plundervolt library #
This library is a continuation of ideas proposed in other papers, mainly that of *Plundervolt: Software-based Fault Injection Attacks against Intel SGX*. It borrows heavily from the code in [This link](https://github.com/KitMurdock/plundervolt). It is meant to simplify undervolting as a whole, and to provide a well-documented environment which enables the user to run a piece of code while undervolting without much care for the undervolting part.

`plundervolt.h` provides a range of functions which may be useful to your code. You are able to:

  * Run a function parallel to undervolting.
  * Run that function in a loop.
  * Provide your specification of when the loop should stop.
  * Specify how low the undervolting goes.
  * Run your function in as many threads as you desire.
  * Read the current voltage of the CPU.
  * Set a one-time undervolting value and reset the voltage later.
  * Perform *Hardware* or *Software* undervolting (the former is still to be implemented at this point).
  * Run your function (with thread support) in loop or as it is, with arguments specified by you (see later for [passing arguments](#Passing-Arguments)), while undervolting according to your wishes in a separate thread, and checking on every function iteration for your condition (unless your function checks for it already), all that with a simple function call and a little specification maintenance.
  * Probably something I didn't think of as well.
  
# Usage #
The library includes all of its dependencies, so you only need to include it and you are good to go.

If you want to use the all-encompassing function, you must define the specification first. It is possible to use the other functions without doing it, though.

## Plundervolt specification ##
The structure `plundervolt_specification_t` holds all the necessary information for the library to run. You must define it by calling `plundervolt_init()`, which sets the default values, then change those to your liking, and call `plundervolt_set_specification()` with this specification as an argument.

The specification holds:

  * `start_` and `end_undervoltage` are values. `End_undervoltage` must be smaller than `start_undervoltage`. They can, and in undervolting should, be negative.

	NOTE: The library does not check these values further. If you put too low a value, your computer may crash and will need to be restarted.
  * `step` is the value by which the current undervoltage is dropped on each iteration of a loop in the `plundervolt_apply_undervolting()` function.
  * `* function` is a pointer to the function you want to run while undervolting. It receives a (\* void) argument, which the function must unpack and work with.
  * `* arguments` is a pointer to the arguments for this function. It may be any data type, but the pointer must be void.
  * `loop` acts as a boolean, where non-zero means run the provided function in a loop.
  * `threads` is the number of threads the function will run in. <1 is interpreted as 1.
  * `integrated_loop_check` is a boolean, where 1 means the function holds it's own check and stops the undervolting by calling `plundervolt_set_loop_finished()`.
  * `* stop_loop` is a pointer to the function which is called on every iteration of the loop (if `loop` is >0) if `integrated_loop_check` = 0..
  * `* loop_check_arguments` is like `* arguments` but for the `stop_loop` function.
  * `undervolt` is a boolean. If >0, the undervolting will happen.
  * `wait_time` is the time for which the undervolting halts before dropping again. In the *Software* version, it is approximate.

## Errors ##

The library will check you have the essential specification right and alerts you if there is a problem. The function `plundervolt_print_error()` is provided, which translates the error code returned to a print statement and you can see what is wrong.

Both `plundervolt_set_specification()` and `plundervolt_run()` can return an error code, or return 0 if everything is OK.

## Passing Arguments ##

When using `plundervolt_run()`, arguments can be passed to two functions in the specification: `void (* function)` and `int (* stop_loop)`. Both take a void pointer as input.

If you wish to pass anything, you must set `* arguments` and `* loop_check_arguments` respectively. They are void pointers, so anything you pass must be cast to (void *) first. The functions must then unpack these pointers and work with what is inside. The library has no functionality for doing so.

Should you need to pass more arguments than one, create a structure and pass the pointer to *it*.

This approach ensures you can change the values of the arguments without changing the specification. If arguments are provided (if not, they are set to NULL), they are passed on every iteration of the loop, or once if loop = 0.

## Cleanup ##

After your code is finished, you must call `plundervolt_cleanup()`. This function closes the open file stream and resets the voltage to acceptable levels.

The library itself allocates no memory save for the threads, which are joined and freed inside, so any other memory you have allocated you must take care of yourself.

# Compilation note #
If you change the library code and need to compile it as a dependency, always use the `-pthread` option.