/*
 * Dynamic Kernel Module - proc_info_module.c
 *
 * This kernel module provides process information based on the given process ID (PID) or process name.
 * It creates a /proc file named proc_info_module that can be accessed from user space to retrieve process information.
 *
 * Module Parameters:
 *  - upid: A non-negative integer that specifies the user process ID (PID).
 *  - upname: A string that specifies the user process name.
 *
 * Process Information:
 *  - Name: Process name.
 *  - PID: Process ID.
 *  - PPID: PID of the process's parent.
 *  - UID: User identifier (UID) of the process.
 *  - Path: The path of the process in /proc.
 *  - State: The process state, such as running, interruptible, uninterruptible, or stopped.
 *  - Memory Usage: Memory usage of the process in kilobytes (KB). This information is only available when the process is in a running state.
 *
 * Flow:
 *  - Acquire process ID or name as module parameters.
 *  - Open the /proc file named proc_info_module.
 *  - If no processes with the specified ID or name are found, print an error message, log the error in the /proc file, and exit the program with exit value 2.
 *  - Obtain information about the process.
 *  - If the process is in a running state, calculate the memory usage.
 *  - When the /proc file is read from the user space application, log the message to the /proc file.
 *  - Exit the program with exit value 0.
 *
 * /proc file will be automatically removed when the kernel module is unloaded.
 * If an error occurs during any of the steps, an appropriate error message will be printed, logged in the /proc file, and the program will exit with exit value 1.
 *
 * Authors:
 * - [ Burak Keçeci - 290201103 ][ Berkan Gönülsever - 270201064 ]
 *
 * License: GPL
 */

#include <linux/module.h> // Needed by all modules
#include <linux/kernel.h> // Needed for KERN_INFO
#include <linux/proc_fs.h> // Needed for the proc file system
#include <linux/sched.h> // Needed for for_each_process macro
#include <linux/slab.h> // Needed for kmalloc
#include <linux/uaccess.h> // Needed for copy_to_user

#define PROC_FILENAME "proc_info_module"

static struct proc_dir_entry *proc_file_entry;

static int upid = -1;  // User process ID
static char upname[TASK_COMM_LEN] = {0};  // User process name



/**
 * Convert the process state to string.
 * 
 * @state: The process state.
*/
static const char* get_state_string(long state);

/**
 * Check if the task matches the provided process ID or process name.
 *
 * This function compares the process ID or process name of the given task with the provided
 * process ID or process name.
 *
 * @task: Pointer to the task structure to check.
 * @found_task: Pointer to the task structure pointer to store the matched task (if found).
 *
 * @return: 0 if the task matches the provided process ID or process name, 1 otherwise.
 */
static int get_process_info(struct task_struct *task, struct task_struct **found_task);

/**
 * Read callback function for the /proc file.
 *
 * This function is called when the /proc file is read. It retrieves information about the
 * specified process ID or process name and writes it to the user buffer.
 *
 * @file: Pointer to the file structure.
 * @buffer: User buffer to write the process information to.
 * @count: Size of the user buffer.
 * @offset: Pointer to the file offset.
 *
 * @return: Number of bytes written to the user buffer, or a negative error code on failure.
 */
static ssize_t read_proc(struct file *file, char __user *buffer, size_t count, loff_t *offset);

/**
 * Initialization function for the module.
 *
 * This function is called when the module is loaded into the kernel. It creates the /proc file
 * entry and registers the read callback function.
 *
 * @return: 0 on success, or a negative error code on failure.
 */
static int proc_info_module_init(void);

/**
 * Cleanup function for the module.
 *
 * This function is called when the module is unloaded from the kernel. It removes the /proc file
 * entry.
 */
static void proc_info_module_exit(void);

// File operations structure for the /proc file
static const struct proc_ops proc_fops = {
    .proc_read = read_proc,
};

/**
 * Convert the process state to string.
 * 
 * @state: The process state.
*/
static const char* get_state_string(long state) {
    switch (state) {
        case TASK_RUNNING: // state 0
            return "Running";
        case TASK_INTERRUPTIBLE:
            return "Interruptible Sleep"; // state 1
        case TASK_UNINTERRUPTIBLE:
            return "Uninterruptible Sleep"; // state 2
        case __TASK_STOPPED:
            return "Stopped"; // state 4
        case __TASK_TRACED:
            return "Traced"; //  state 8
        case EXIT_ZOMBIE:
            return "Zombie"; // state 16
        case EXIT_DEAD:
            return "Dead (Exit)"; // state 32
        case TASK_DEAD:
            return "Dead"; // state 64
        case TASK_WAKEKILL:
            return "Wakekill"; // state 128
        case TASK_WAKING:
            return "Waking"; // state 256
        case TASK_STATE_MAX:
            return "State Max"; // state 512
        default:
            return "Unknown"; // state -1
    }
}

/**
 * Check if the task matches the provided process ID or process name.
 *
 * This function compares the process ID or process name of the given task with the provided
 * process ID or process name.
 *
 * @task: Pointer to the task structure to check.
 * @found_task: Pointer to the task structure pointer to store the matched task (if found).
 *
 * @return: 0 if the task matches the provided process ID or process name, 1 otherwise.
 */
static int get_process_info(struct task_struct *task, struct task_struct **found_task)
{
    if (upid != -1) {
        if (task->pid == upid) {
            *found_task = task;
            return 0;
        }
    } else {
        if (strcmp(task->comm, upname) == 0) {
            *found_task = task;
            return 0;
        }
    }
    return 1;
}

/**
 * Log the information of a process to the buffer.
 *
 * This function retrieves information about a process and appends it to the given buffer.
 *
 * @task: Pointer to the task structure of the process.
 * @buffer: Pointer to the buffer to store the process information.
 * @size: Size of the buffer.
 */
static void log_process_info(struct task_struct *task, char *buffer, size_t size)
{
    struct task_struct *parent_task = task->parent;
    unsigned long memory_usage = 0;

    if (task->mm && task->mm->total_vm)
        memory_usage = task->mm->total_vm << (PAGE_SHIFT - 10);

    sprintf(buffer + strlen(buffer), "Name: %s\n", task->comm);
    sprintf(buffer + strlen(buffer), "PID: %d\n", task->pid);
    sprintf(buffer + strlen(buffer), "PPID: %d\n", parent_task ? parent_task->pid : -1);
    sprintf(buffer + strlen(buffer), "UID: %d\n", task_uid(task).val);
    sprintf(buffer + strlen(buffer), "Path: /proc/%d\n", task->pid);
    sprintf(buffer + strlen(buffer), "State: %s\n", get_state_string(task->__state));
    if (task->__state == TASK_RUNNING) {
        sprintf(buffer + strlen(buffer), "Memory usage: %lu KB\n", memory_usage);
    } else {
        sprintf(buffer + strlen(buffer), "Memory usage: State is not running.\n");
    }
}

/**
 * Read callback function for the /proc file.
 *
 * This function is called when the /proc file is read. It retrieves information about the
 * specified process ID or process name and writes it to the user buffer.
 *
 * @file: Pointer to the file structure.
 * @buffer: User buffer to write the process information to.
 * @count: Size of the user buffer.
 * @offset: Pointer to the file offset.
 *
 * @return: Number of bytes written to the user buffer, or a negative error code on failure.
 */
static ssize_t read_proc(struct file *file, char __user *buffer, size_t count, loff_t *offset)
{
    struct task_struct *task = NULL;
    char *kbuffer;
    size_t kbuffer_size;
    ssize_t retval = 0;
    int found_process = 0;

    kbuffer_size = PAGE_SIZE;
    kbuffer = kmalloc(kbuffer_size, GFP_KERNEL);
    if (!kbuffer)
        return -ENOMEM;

    kbuffer[0] = '\0';

    if (*offset == 0) {
        rcu_read_lock();
        for_each_process(task) {
            if (get_process_info(task, &task) == 0) {
                log_process_info(task, kbuffer, kbuffer_size);
                found_process = 1;
                break;
            }
        }
        rcu_read_unlock();

        if (!found_process) {
            if (upid != -1)
                sprintf(kbuffer, "Error: Process with ID %d not found.\n", upid);
            else
                sprintf(kbuffer, "Error: Process with name %s not found.\n", upname);
            retval = -ENOENT;
        }
    }

    retval = strlen(kbuffer);
    if (copy_to_user(buffer, kbuffer, retval)) {
        kfree(kbuffer);
        return -EFAULT;
    }

    kfree(kbuffer);
    *offset += retval;
    return retval;
}

/**
 * Initialization function for the module.
 *
 * This function is called when the module is loaded into the kernel. It creates the /proc file
 * entry and registers the read callback function.
 *
 * @return: 0 on success, or a negative error code on failure.
 */
static int proc_info_module_init(void)
{
    proc_file_entry = proc_create(PROC_FILENAME, 0, NULL, &proc_fops);
    if (!proc_file_entry) {
        printk(KERN_ERR "Failed to create /proc/%s entry\n", PROC_FILENAME);
        return -ENOMEM;
    }

    printk(KERN_INFO "proc_info_module loaded\n");
    return 0;
}

/**
 * Cleanup function for the module.
 *
 * This function is called when the module is unloaded from the kernel. It removes the /proc file
 * entry.
 */
static void proc_info_module_exit(void)
{
    remove_proc_entry(PROC_FILENAME, NULL);
    printk(KERN_INFO "proc_info_module unloaded\n");
}

// Module initialization and cleanup macros
module_init(proc_info_module_init);
module_exit(proc_info_module_exit);

// Module parameters and their descriptions
module_param(upid, int, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
MODULE_PARM_DESC(upid, "User process ID");

module_param_string(upname, upname, TASK_COMM_LEN, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
MODULE_PARM_DESC(upname, "User process name");

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Dynamic Kernel Module");
MODULE_AUTHOR("Burak Keçeci & Berkan Gönülsever");




