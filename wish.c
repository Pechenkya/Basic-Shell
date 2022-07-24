#include "wish.h"
#include <ctype.h>  // isspace
#include <regex.h>  // regcomp, regexec, regfree
#include <stdio.h>  // fopen, fclose, fileno, getline, feof
#include <stdlib.h> // exit
#include <sys/types.h>
#include <sys/wait.h> // waitpid

void print_error()
{
  char error_message[30] = "An error has occurred\n";
  write(STDERR_FILENO, error_message, strlen(error_message)); 
}

void clear_path(char* PATH[])
{
  for(int i = 1; PATH[i] != NULL; ++i)
  {
    free(PATH[i]);
  }
  PATH[1] = NULL;
}

void free_allocated(char* PATH[], char* buff)
{
  free(buff);
  clear_path(PATH);
}

FILE* check_for_redirection(char* input)
{
  char* red_begin = strchr(input, '>');
  char* tmp_buff = NULL;
  char* file_name = NULL;
    if(red_begin == NULL)
    {
      return stdout;
    }
    else
    {
      *red_begin = '\0';
      red_begin++;
      do
      {
        tmp_buff = strsep(&red_begin, " ");
        if(tmp_buff != NULL && strcmp(tmp_buff, ""))
        {
          if(file_name)     // IF filename already found -> too much parameters
            return NULL;
          else
            file_name = tmp_buff;
        }
      } while (tmp_buff != NULL);
      
      return fopen(file_name, "w");
    }
}

int check_and_exec_buildin(int argc, char* argv[], 
      char* PATH[], FILE* fl, char *buff_begin)
{
  // Check if 'exit' command -- then exit //
  if(!strcmp(argv[0], "exit"))
  {
    if(argc > 1)
      fprintf(stderr, "Error: exit must have no parameters\n");
    else
    {
      if(fl != NULL)
        fclose(fl);

      free_allocated(PATH, buff_begin);
      exit(0);
    }

    return 1;
  }
  else if(!strcmp(argv[0], "cd"))
  {
    if(argc != 2)
      fprintf(stderr, "cd must have one parameter\n");
    else if(chdir(argv[1]))
      fprintf(stderr, "No such directory: %s\n", argv[1]);
    
    return 1;
  }
  else if(!strcmp(argv[0], "path"))
  {
    clear_path(PATH);
    if(argc == 1)
      PATH[1] = NULL;
    else
    {
      for(int i = 1; i < argc; ++i)
      {
        // PATH[i - 1] <- argv[i] //
        PATH[i] = (char *) malloc(strlen(argv[i]));
        strcpy(PATH[i], argv[i]);
      }
      PATH[argc] = NULL;
    }

    return 1;  
  }

  return 0;
} 

void execute_cmd(int argc, char* argv[], 
      char* PATH[], FILE* OUTPUT, char *buff_begin)
{
  // First -- we fork process
  int child_proccess = fork();

  if(child_proccess == 0)
  {
    // Executing process on new thread
    // Redirecting output if needed
    if(OUTPUT != stdout)
    {
      int file = fileno(OUTPUT);
      dup2(file, STDOUT_FILENO);    // standart out
      dup2(file, STDERR_FILENO);    // standart error
    } 

    // Find first path with existing command and execute it
    char dest[256];
    dest[0] = '\0';
    for(int i = 0; PATH[i] != NULL; ++i)
    {
      // Building path
      dest[0] = '\0';
      strcat(dest, PATH[i]);
      strcat(dest, "/");
      strcat(dest, argv[0]);
      //

      // Executing command on first found path
      if(!access(dest, X_OK))
        execv(dest, argv);
      //
    }
    fprintf(stderr, "No such command %s found in PATH\n", argv[0]);

    free_allocated(PATH, buff_begin);
    exit(0);
  } 
  else if(child_proccess > 0)
  {
    // Wait for process to complete
    wait(NULL);
  }
  else
  {
    fprintf(stderr, "Error while starting new proccess from %d\n", getpid());
  }
}


int main(int argc, char *argv[]) {
  // Input handling //
  char *input = NULL;
  char *parse_token = NULL;
  size_t len = 0;
  ssize_t input_sz = 0;
  int argsc;
  char *argsv[255];
  char *arg_buff = NULL;
  //

  // System parameters
  char *PATH[32];
  PATH[0] = "";
  PATH[1] = NULL;

  FILE* OUTPUT = stdout;
  //

  if(argc == INTERACTIVE_MODE)   // Interactive mode -> 1
  {
    while(1)
    {
      argsc = 0;

      printf("wish> ");
      input_sz = getline(&input, &len, stdin);

      // Empty line handling //
      if(input[0] == '\n')
        continue;

      input[input_sz - 1] = '\0';

      // Removing tabulations //
      for(int i = 0; i < input_sz; ++i)
        if(input[i] == '\t')
          input[i] = ' ';

      // Checking for redirection //
      OUTPUT = check_for_redirection(input);
      if(OUTPUT == NULL)    // Error while processing filename
      {
        OUTPUT = stdout;
        fprintf(stderr, "Error with redirection\n");
        free(input);
        input = NULL;
        continue;
      }
    

      // Parsing input // 
      parse_token = input;
      do
      {
        arg_buff = strsep(&parse_token, " ");
        if(arg_buff == NULL || strcmp(arg_buff, ""))
          argsv[argsc++] = arg_buff;
      } while (argsc == 0 || argsv[argsc - 1] != NULL);
      argsc--;  // NULL is not a parameter
      
      // Executing commant and free input //
      if(!check_and_exec_buildin(argsc, argsv, PATH, NULL, input))
      {
        execute_cmd(argsc, argsv, PATH, OUTPUT, input);
      }
      
      if(OUTPUT != stdout)
      {
        fclose(OUTPUT);
        OUTPUT = stdout;
      }

      free(input);
      input = NULL;
    }
  }
  else if(argc == BATCH_MODE)  // Batch mode -> 2
  {
    FILE* file = fopen(argv[1], "r");
    if(!file)
    {
      exit(1);
    }

    while(!feof(file))
    {
      argsc = 0;
      input_sz = getline(&input, &len, file);
      // Empty line handling //
      if(input_sz == -1 || input[0] == '\n')
        continue;

      // Removing tabulations //
      for(int i = 0; i < input_sz; ++i)
        if(input[i] == '\t' || input[i] == '\n')
          input[i] = ' ';

      
      // Parsing input // 
      parse_token = input;
      do
      {
        arg_buff = strsep(&parse_token, " ");
        if(arg_buff == NULL || strcmp(arg_buff, ""))
          argsv[argsc++] = arg_buff;
      } while (argsc == 0 || argsv[argsc - 1] != NULL);
      argsc--;  // NULL is not a parameter

      // Executing commant and free input //
      if(!check_and_exec_buildin(argsc, argsv, PATH, file, input))
      {
        execute_cmd(argsc, argsv, PATH, OUTPUT, input);
      }

      free(input);
      input = NULL;
    }
    clear_path(PATH);
    fclose(file);
  }
  else
  {
    exit(1);
  }

  return 0;
}