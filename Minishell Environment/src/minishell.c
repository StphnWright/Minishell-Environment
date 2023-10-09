#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <pwd.h>
#include <signal.h>
#include <sys/wait.h>

#define BUF_SIZE 4096
#define MAX_ARGS 2048
#define BRIGHTBLUE "\x1b[34;1m"
#define DEFAULT    "\x1b[0m"

// Flag to track interrupt signal, cannot be cached
volatile sig_atomic_t interrupted = false;

// Signal handler for interrupt signal
void sigint_handler(int signal) {
    //printf("\n");
    interrupted = true;
}

// Function to get the current working directory
char *get_cwd() {
    // Define a static buffer with BUF_SIZE to store the current working directory
    static char cwd[BUF_SIZE];

    // Call getcwd() to obtain the current working directory and store it in cwd
    // If the function returns NULL, there is an error
    if (getcwd(cwd, BUF_SIZE) == NULL) {
        // Print an error message to stderr with the error details from errno
        fprintf(stderr, "Error: Cannot get current working directory. %s.\n", strerror(errno));
        // Return NULL to indicate an error
        return NULL;
    }
    // Return the current working directory stored in cwd
    return cwd;
}

// Function to get the user's home directory
char *get_home_dir() {
    // Call getpwuid() with the user's ID (from getuid()) to get the password entry structure
    struct passwd *password_entry = getpwuid(getuid());

    // If password_entry is NULL, there is an error
    if (password_entry == NULL) {
        // Print an error message to stderr with the error details from errno
        fprintf(stderr, "Error: Cannot get passwd entry. %s.\n", strerror(errno));
        // Return NULL to indicate an error
        return NULL;
    }
    // Return the home directory path from the password entry structure
    return password_entry->pw_dir;
}

// Helper function for Extra Credit Option (assuming this function handles quotes in the input)
int handle_quotes(char *dir) {
    // Initialize status variable to 0 (no error)
    int status = 0;

    // Check if the dir pointer is not NULL
    if (dir != NULL) {
        // Get the length of the string in dir
        size_t len = strlen(dir);

        // Check if the first character of the string is a double quote
        if (dir[0] == '"') {
            // If the string has only one character or the last character is not a double quote
            if (len == 1 || dir[len - 1] != '"') {
                // Print an error message to stderr
                fprintf(stderr, "Error: Malformed command.\n");
                // Set the status to 1 (error)
                status = 1;
            } else {
                // Initialize a counter variable (j) to 0
                size_t j = 0;

                // Iterate through the characters of the string from the second to the penultimate character
                for (size_t i = 1; i < len - 1; i++) {
                    // If the current character is not a double quote
                    if (dir[i] != '"') {
                        // Copy the current character to the position indicated by j
                        dir[j] = dir[i];
                        // Increment the counter variable (j)
                        j++;
                    }
                }
                // Null-terminate the string after removing quotes
                dir[j] = '\0';
            }
        }
    }
    // Return the status (0 for success, 1 for error)
    return status;
}

// Function to change the current working directory to the specified directory
int change_dir(char *dir) {
    // Call get_home_dir() to get the user's home directory
    char *home_dir = get_home_dir();

    // If home_dir is NULL, there is an error, return -1
    if (home_dir == NULL) {
        return -1;
    }

    // Declare a new buffer with BUF_SIZE to store the new directory path
    char new_dir[BUF_SIZE];

    // Call handle_quotes() to remove quotes from the input directory string and get the error status
    int status = handle_quotes(dir);
    // If status is not 0 (error), return the status
    if (status != 0) {
        return status;
    }

    // If dir is NULL (no directory provided), use the user's home directory
    if (dir == NULL) {
        strcpy(new_dir, home_dir);
    }
    // If the first character of the dir string is a tilde (~), replace it with the home directory
    else if (dir[0] == '~') {
        // Concatenate the home directory and the rest of the dir string, skipping the tilde
        snprintf(new_dir, BUF_SIZE, "%s%s", home_dir, dir + 1);
    }
    // If the directory is provided without a tilde, use the provided directory
    else {
        strcpy(new_dir, dir);
    }

    // Call chdir() to change the current working directory to the new directory (new_dir)
    // If chdir() returns -1, there is an error
    if (chdir(new_dir) == -1) {
        // Print an error message to stderr with the error details from errno
        fprintf(stderr, "Error: Cannot change directory to '%s'. %s.\n", new_dir, strerror(errno));
        // Return -1 to indicate an error
        return -1;
    }
    // Return 0 to indicate success
    return 0;
}

// Function to execute the given command
void exec_cmd(char *command) {
    // Get first token from input string
    char *cmd = strtok(command, " \t\n");

    // If cmd is NULL (empty input), return immediately
    if (cmd == NULL) {
        return;
    // If the command is "exit", exit the program with success status
    } else if (strcmp(cmd, "exit") == 0) {
        exit(EXIT_SUCCESS);
    // If the command is "cd", change the current working directory
    } else if (strcmp(cmd, "cd") == 0) {
        // Get the directory argument from the input string
        char *dir = strtok(NULL, "\t\n");
        
        // If dir is NULL (cd without arguments) or equals "~", change to the home directory
        if ((dir == NULL) || (strcmp(dir, "~") == 0)) {
            change_dir(NULL);
            return;
        }

        // If there are extra arguments to cd, print an error message and return
        // For now, do not consider spaces as delimiters
        char *extra_arg = strtok(NULL, "\t\n");
        if (extra_arg != NULL) {
            fprintf(stderr, "Error: Too many arguments to cd.\n");
            return;
        }

        // Now check that there are no spaces between quoted strings
        // Also check that the total number of quotes is even
        int q = 0;
        for (size_t i = 0; i < strlen(dir); i++) {
            if (dir[i] == '"') {
                // Quote
                q++;
            } else if ((dir[i] == ' ') && (q % 2 == 0)) {
                // Space outside argument (between an even number of quotes)
                fprintf(stderr, "Error: Too many arguments to cd.\n");
                return;
            }
        }
        if (q % 2 == 1) {
            // Odd number of quotes: malformed command
            fprintf(stderr, "Error: Malformed command.\n");
            return;
        }

        // Arguments are valid: call the change_dir function
        change_dir(dir);

    // If the command is neither "cd" nor "exit", execute the given command
    } else {
        // Command is neither cd nor exit, need to call exec
        pid_t pid = fork();

        // If fork() returns -1, there is an error
        if (pid < 0) {
            fprintf(stderr, "Error: fork() failed. %s.\n", strerror(errno));
            return;
        }

        // If fork() returns 0, this is the child process
        if (pid == 0) {
            // Child process
            struct sigaction sa_child;
            sa_child.sa_handler = SIG_IGN;
            sigemptyset(&sa_child.sa_mask);
            sa_child.sa_flags = 0;

            if (sigaction(SIGINT, &sa_child, NULL) == -1) {
                fprintf(stderr, "Error: Cannot set signal handler to ignore in child process. %s. \n", strerror(errno));
                exit(EXIT_FAILURE);
            }

            char *args[MAX_ARGS];
            args[0] = cmd;
            int i = 1;

            // Fill the array with the remaining arguments from the input string
            while ((args[i] = strtok(NULL, " \t\n")) != NULL) {
                i++;
            }

            // Execute the command with its arguments using execvp()
            execvp(args[0], args);
            fprintf(stderr, "Error: exec() failed. %s.\n", strerror(errno));
            exit(EXIT_FAILURE);
        }
        
        // If fork() returns a positive value, this is the parent process
        int status;
        waitpid(pid, &status, 0);
        if (interrupted) {
            printf("\n");
        }
        interrupted = (WIFSIGNALED(status) || WIFSTOPPED(status));
        if (interrupted) {
           printf("\n");
        }
    }
}

int main() {
    struct sigaction sa;
    sa.sa_handler = sigint_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    // Register the signal handler for SIGINT
    if(sigaction(SIGINT, &sa, NULL) == -1) {
        fprintf(stderr, "Error: Cannot register signal handler. %s.\n", strerror(errno));
        return EXIT_FAILURE;
    }

    // Prompt user for commands
    char command[BUF_SIZE];
    while (true) {
        char *cwd = get_cwd();

        if (cwd == NULL) {
            return EXIT_FAILURE;
        }
        printf("[%s%s%s]$ ", BRIGHTBLUE, cwd, DEFAULT);

        // Signal handler checks
        if (fgets(command, BUF_SIZE, stdin) == NULL) {
            if (interrupted) {
                interrupted = false;
                printf("\n");
                continue;
            } else {
                fprintf(stderr, "Error: Failed to read from stdin. %s.\n", strerror(errno));
                return EXIT_FAILURE;
            }
        }

        if (! interrupted) {
            exec_cmd(command);
        } else {
            interrupted = false;
        }
    }

    return EXIT_SUCCESS;
}
