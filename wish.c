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
  for(int i = 2; PATH[i] != NULL; ++i)
  {
    free(PATH[i]);
  }
  PATH[2] = NULL;
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
      print_error();
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
      print_error();
    else if(chdir(argv[1]))
      print_error();
    
    return 1;
  }
  else if(!strcmp(argv[0], "path"))
  {
    clear_path(PATH);
    if(argc == 1)
      PATH[2] = NULL;
    else
    {
      for(int i = 1; i < argc; ++i)
      {
        // PATH[i - 1] <- argv[i] //
        PATH[i + 1] = (char *) malloc(strlen(argv[i]));
        strcpy(PATH[i + 1], argv[i]);
      }
      PATH[argc + 1] = NULL;
    }

    return 1;  
  }

  return 0;
} 

void execute_cmd(short cmd_count, char* argv[16][64], 
      char* PATH[], FILE* OUTPUT[], char *buff_begin)
{
  int status = 0;
  pid_t pid, wpid;
  for(short i = 0; i < cmd_count; ++i)
  {
    // First -- we fork process
    if((pid = fork()) == 0)
    {
        // Executing process on new thread
        // Redirecting output if needed
        if(OUTPUT[i] != stdout)
        {
          int file = fileno(OUTPUT[i]);
          dup2(file, STDOUT_FILENO);    // standart out
          dup2(file, STDERR_FILENO);    // standart error
        } 

        // Find first path with existing command and execute it
        char dest[256];
        dest[0] = '\0';
        for(int j = 0; PATH[j] != NULL; ++j)
        {
          // Building path
          dest[0] = '\0';
          strcat(dest, PATH[j]);
          strcat(dest, "/");
          strcat(dest, argv[i][0]);
          //

          // Executing command on first found path
          if(!access(dest, X_OK))
            execv(dest, argv[i]);
          //
        }
        print_error();

        free_allocated(PATH, buff_begin);
        exit(0);
    } 
    else if(pid > 0)
    {
       continue;
    }
    else
    {
      print_error();
    }
  }
  
  // Wait for all shild processes to end
  while((wpid = wait(&status)) > 0);
  //
}


int main(int argc, char *argv[]) {
  // Input handling //
  char *input = NULL;
  size_t len = 0;
  size_t input_sz = 0;
  //
  
  // Parsing stuff
  short cmd_count = 1;
  char *parse_token[16];
  parse_token[0] = NULL;
  int argsc[16];
  char *argsv[16][64];
  char *arg_buff = NULL;
  //

  // System parameters
  char *PATH[32];
  PATH[0] = "";
  PATH[1] = "/bin";
  PATH[2] = NULL;

  FILE* OUTPUT[16];
  //

  if(argc == INTERACTIVE_MODE)   // Interactive mode -> 1
  {
    while(1)
    {
      printf("wish> ");
      input_sz = getline(&input, &len, stdin) - 1;

      // Empty line handling //
      if(input[0] == '\n')
        continue;
      input[input_sz] = '\0';

      // Removing tabulations //
      for(int i = 0; i < input_sz; ++i)
        if(input[i] == '\t')
          input[i] = ' ';

      // Find for first symbol (not whitespace)
      parse_token[0] = NULL;
      for(int i = 0; i < input_sz; ++i)
      {
        if(input[i] != ' ')
        {
          parse_token[0] = &input[i];
          break;
        }
      }

      if(!parse_token[0])
      {
        free(input);
        input = NULL;
        continue;
      }
      //
      
      // Count and setup parallel commands
      argsc[0] = 0;
      do
      {
        parse_token[cmd_count] = strchr(parse_token[cmd_count - 1], '&');
        if(parse_token[cmd_count] != NULL)
        {
          argsc[cmd_count] = 0;
          parse_token[cmd_count][0] = '\0';
          parse_token[cmd_count]++;   // Cutting off the token
          cmd_count++;
        }
        else
          break;
      } while (1);
      
      
      int bad_parse = 0;
      for(int i = 0; i < cmd_count; ++i)
      {
        // Checking for redirection //
        OUTPUT[i] = check_for_redirection(input);
        if(OUTPUT[i] == NULL)    // Error while processing filename
        { 
          OUTPUT[i] = stdout;
          bad_parse = 1;
          break;
        }

        // Parsing input // 
        do
        {
          arg_buff = strsep(&parse_token[i], " ");
          if(arg_buff == NULL || strcmp(arg_buff, ""))
            argsv[i][argsc[i]++] = arg_buff;
        } while (argsc[i] == 0 || argsv[i][argsc[i] - 1] != NULL);

        // Empty args == no commands
        if(argsv[i][0] == NULL)
        {
          bad_parse = 1;
          break;
        }
        argsc[i]--;  // NULL is not a parameter
      }
      
      if(bad_parse)
      {
        print_error();
        free(input);
        input = NULL;
        continue;
      }
      
      // Executing commant and free input //
      if(!check_and_exec_buildin(argsc[0], argsv[0], PATH, NULL, input))
      {
        execute_cmd(cmd_count, argsv, PATH, OUTPUT, input);
      }
      
      // Clearing data
      for(int i = 0; i < cmd_count; ++i)
      {
        if(OUTPUT[i] != stdout)
        {
            fclose(OUTPUT[i]);
            OUTPUT[i] = stdout;
        }
      }
      cmd_count = 1;
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
      input_sz = getline(&input, &len, file) - 1;
      // Empty line handling //
      if(input_sz == -1 || input[0] == '\n')
        continue;

      // Removing tabulations //
      for(int i = 0; i < input_sz; ++i)
        if(input[i] == '\t' || input[i] == '\n')
          input[i] = ' ';

      // Find for first symbol (not whitespace)
      parse_token[0] = NULL;
      for(int i = 0; i < input_sz; ++i)
      {
        if(input[i] != ' ')
        {
          parse_token[0] = &input[i];
          break;
        }
      }

      if(!parse_token[0])
      {
        free(input);
        input = NULL;
        continue;
      }
      //

      // Count and setup parallel commands
      argsc[0] = 0;
      do
      {
        parse_token[cmd_count] = strchr(parse_token[cmd_count - 1], '&');
        if(parse_token[cmd_count] != NULL)
        {
          argsc[cmd_count] = 0;
          parse_token[cmd_count][0] = '\0';
          parse_token[cmd_count]++;   // Cutting off the token
          cmd_count++;
        }
        else
          break;
      } while (1);
      
      
      int bad_parse = 0;
      for(int i = 0; i < cmd_count; ++i)
      {
        // Checking for redirection //
        OUTPUT[i] = check_for_redirection(input);
        if(OUTPUT[i] == NULL)    // Error while processing filename
        { 
          OUTPUT[i] = stdout;
          bad_parse = 1;
          break;
        }

        // Parsing input // 
        do
        {
          arg_buff = strsep(&parse_token[i], " ");
          if(arg_buff == NULL || strcmp(arg_buff, ""))
          argsv[i][argsc[i]++] = arg_buff;
        } while (argsc[i] == 0 || argsv[i][argsc[i] - 1] != NULL);

        // Empty args == no commands
        if(argsv[i][0] == NULL)
        {
          bad_parse = 1;
          break;
        }

        argsc[i]--;  // NULL is not a parameter
      }

      if(bad_parse)
      {
        print_error();
        free(input);
        input = NULL;
        continue;
      }

      // Executing commant and free input //
      if(!check_and_exec_buildin(argsc[0], argsv[0], PATH, file, input))
      {
        execute_cmd(cmd_count, argsv, PATH, OUTPUT, input);
      }

      // Clearing data
      for(int i = 0; i < cmd_count; ++i)
      {
        if(OUTPUT[i] != stdout)
        {
            fclose(OUTPUT[i]);
            OUTPUT[i] = stdout;
        }
      }
      cmd_count = 1;
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