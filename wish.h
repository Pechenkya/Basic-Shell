// #include <pthread.h> // pthread_create, pthread_join
// #include <regex.h>   // regcomp, regexec, regfree
#include <stdio.h>   // fopen, fclose, fileno, getline, feof
#include <string.h>  // strlen, strsep, strcat, strdup, strcmp
#include <unistd.h>  // STDERR_FILENO, fork, exec, access, exit, chdir

#ifdef REG_ENHANCED  // macOS: man re_format
#  define REG_CFLAGS REG_EXTENDED | REG_NOSUB | REG_ENHANCED
#else
#  define REG_CFLAGS REG_EXTENDED | REG_NOSUB
#endif

#define INTERACTIVE_MODE 1
#define BATCH_MODE 2

// Print default error -- could be modified
void print_error();

// Clear current PATH array
void clear_path(char* PATH[]);

// Free any dynamically allocated commands
void free_allocated(char* PATH[], char* buff);

// Check for redirection in command
FILE* check_for_redirection(char* input);

// Check if commans is Built-In, and if it is -- execute it
int check_and_exec_builtin(int argc, char* argv[], char* PATH[], FILE* fl, char *buff_begin);

// Execute commands by finding them in PATH
void execute_cmd(short cmd_count, char* argv[16][64], char* PATH[], FILE* OUTPUT[], char *buff_begin);