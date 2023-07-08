/**
 * Wrapper User Space Application
 * 
 * This is a user space application written in C that allows for inserting and removing a kernel module
 * from the operating system and passing parameters to the kernel module. It also reads information from
 * the /proc file and prints the log messages in the terminal.
 * 
 * Command line arguments:
 * - argv[1]: User space application file path.
 * - argv[2]: Argument type, which can be -pid or -pname.
 * - argv[3]: If -pid is given, it will be a non-negative integer. Otherwise, if -pname is given, it will be a string.
 * 
 * Please ensure the correct number of command line arguments is passed. It must work with only one parameter,
 * and if both -pid and -pname information is given, it should give an error.
 * 
 * -pid is the equivalent of -upid in the kernel space, and -pname is the equivalent of -upname in the kernel space.
 * 
 * The flow:
 * - Get the process ID or name argument from the terminal.
 * - Pass the parameter to the kernel while the kernel object is inserted to the OS.
 * - Read log messages to be written by the kernel module from the /proc file.
 * - Print log messages in the terminal.
 * - Remove the kernel module.
 * - Exit the program with exit value 0.
 * 
 * If an error occurs in any of the above steps, print an appropriate error message and exit the program with exit value 1.
 * 
 * Authors:
 * - [ Burak Keçeci - 290201103 ][ Berkan Gönülsever - 270201064 ]
 * 
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define BUFFER_SIZE 256
#define PROC_FILE "/proc/proc_info_module"

/**
 * Prints an error message to stderr and exits the program with a non-zero exit code.
 * @param message The error message to be printed.
 */
void display_error(const char *message);

int main(int argc, char *argv[]) {
    // Check the number of command line arguments
    if (argc != 4) {
        display_error("Invalid number of arguments. Usage: get_proc_info <app_path> <-pid|-pname> <value>");
    }

    // Parse command line arguments
    char *app_path = argv[1];
    char *arg_type = argv[2];
    char *arg_value = argv[3];

    // Check if both -pid and -pname are provided
    if ((strcmp(arg_type, "-pid") == 0 && strcmp(arg_type, "-pname") == 0) ||
        (strcmp(arg_type, "-pid") != 0 && strcmp(arg_type, "-pname") != 0)) {
        display_error("Invalid argument type. Either -pid or -pname should be provided.");
    }

    // Create the command to insert the kernel module
    char command[BUFFER_SIZE];
    
    if (strcmp(arg_type, "-pid") == 0) {
        snprintf(command, BUFFER_SIZE, "insmod %s upid=%s", app_path, arg_value);
    } else if (strcmp(arg_type, "-pname") == 0) {
        snprintf(command, BUFFER_SIZE, "insmod %s upname=%s", app_path, arg_value);
    } else {
        display_error("Invalid argument type.");
    }

    // Insert the kernel module
    if (system(command) != 0) {
        display_error("Failed to insert the kernel module.");
    }

    // Read and print log messages from the /proc file
    FILE *proc_file = fopen(PROC_FILE, "r");
    if (proc_file == NULL) {
        display_error("Failed to open the /proc file.");
    }

    char msg[BUFFER_SIZE];
    while (fgets(msg, BUFFER_SIZE, proc_file) != NULL) {
        printf("%s", msg);
    }

    fclose(proc_file);

    // Remove the kernel module
    if (system("rmmod proc_info_module") != 0) {
        display_error("Failed to remove the kernel module.");
    }

    return 0;
}

void display_error(const char *message) {
    fprintf(stderr, "Error: %s\n", message);
    exit(1);
}
