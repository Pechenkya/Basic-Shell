#include "wish.h"
#include <stdlib.h> // exit
#include <sys/types.h>
#include <sys/wait.h> // waitpid

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
    else if(red_begin == input)
    {
      return NULL; // Bad command parse -> no output
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

int check_and_exec_builtin(int argc, char* argv[], 
      char* PATH[], FILE* fl, char *buff_begin)
{
  if(argc == 0)
    return 0;

  // Check if 'exit' command -- then exit //
  if(!strcmp(argv[0], "exit"))
  {
    if(argc > 1)
      fprintf(stderr, "Error (exit): exit must have no parameters\n");
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
      fprintf(stderr, "Error (cd): cd must have exacly one parameter\n");
    else if(chdir(argv[1]))
      fprintf(stderr, "Error (cd): no such folder found (%s)\n", argv[1]);
    
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
        if(argv[i][0] == NULL)
        {
          free_allocated(PATH, buff_begin);
          exit(0);
        }

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
        fprintf(stderr, "Error: no such command found (%s) in PATH\n", argv[i][0]);

        free_allocated(PATH, buff_begin);
        exit(0);
    } 
    else if(pid > 0)
    {
      continue;
    }
    else
    {
      fprintf(stderr, "Error: problem staring new thred\n");
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
  short cmd_count;
  char *parse_token[16];
  parse_token[0] = NULL;
  int argsc[16];
  char *argsv[16][64];
  char *arg_buff = NULL;
  //

  // System parameters
  char *PATH[32];
  PATH[0] = "",    // Current foulder path
  PATH[1] = "/bin"; // Default execution path
  PATH[2] = NULL;

  FILE* OUTPUT[16];
  FILE* INPUT;
  //

  // Setting up mode //
  if(argc == INTERACTIVE_MODE) // Interactive mode -> 1
    INPUT = stdin;
  else if(argc != BATCH_MODE || !(INPUT = fopen(argv[1], "r"))) // Opening if Batch mode -> 2
  {
    fprintf(stderr, "Error: can't open batch file");  // Too much parameters or problem on opening file
    exit(1);
  }
  //

  // Infinite loop, while file ends (stdin never ends) or exit found
  while(!feof(INPUT))
  {
    if(argc == INTERACTIVE_MODE)
      printf("wish> ");
    
    input_sz = getline(&input, &len, INPUT);
    // Empty line handling //
    if(input_sz <= 0)
      continue;
    
    // Removing tabulations and endline //
    if(input[input_sz - 1] == '\n')
      input[--input_sz] = '\0';

    for(int i = 0; i < input_sz; ++i)
      if(input[i] == '\t')
        input[i] = ' ';

    // Find for first symbol (not whitespace), if not found -- skip input
    parse_token[0] = NULL;
    for(int i = 0; i < input_sz && !parse_token[0]; ++i)
      if(input[i] != ' ')
        parse_token[0] = &input[i];

    if(!parse_token[0])
    {
      free(input);
      input = NULL;
      continue;
    }

    // Count and setup parallel commands
    cmd_count = 1;
    while (1)
    {
      parse_token[cmd_count] = strchr(parse_token[cmd_count - 1], '&');
      if(parse_token[cmd_count] != NULL)
      {
        argsc[cmd_count] = 0;
        parse_token[cmd_count][0] = '\0';
        parse_token[cmd_count]++;         // Cutting off the token
        cmd_count++;                      // Increasing commands count
      }
      else
        break;  // Got NULL -> no position -> end loop
    }
    
    // Parse all of found parallel commands //
    int bad_parse = 0;
    for(int i = 0; i < cmd_count; ++i)
    {
      // First no-spase char //
      while(parse_token[i][0] == ' ')
        parse_token[i]++;
        
      // Checking for redirection //
      OUTPUT[i] = check_for_redirection(parse_token[i]);
      if(OUTPUT[i] == NULL)    // Error while processing filename or command
      {
        bad_parse = 1;
        break;
      }

      // Parsing input //
      argsc[i] = 0;
      do
      {
        arg_buff = strsep(&parse_token[i], " ");
        if(arg_buff == NULL || arg_buff[0] != '\0')               // Skip all empty strings
          argsv[i][argsc[i]++] = arg_buff;
      } while (argsc[i] == 0 || argsv[i][argsc[i] - 1] != NULL);  // Find end of parsing token (Segm fault prevented)
      argsc[i]--;  // NULL is not a parameter
    }

    // Executing commant and free input // (if parsed succesfully)
    if(!bad_parse)
    {
      if(!check_and_exec_builtin(argsc[0], argsv[0], PATH, INPUT, input)) // Built-In commands
      {
        execute_cmd(cmd_count, argsv, PATH, OUTPUT, input);               // Other commands
      }
    }
    else
      fprintf(stderr, "Error: problem while parsing input\n");    // Print bad parse error

    // Clearing data
    for(int i = 0; i < cmd_count; ++i)
      if(OUTPUT[i] && OUTPUT[i] != stdout)
        fclose(OUTPUT[i]);
    
    free(input);
    input = NULL;
  }

  clear_path(PATH);
  if(INPUT != stdin)
    fclose(INPUT);

  return 0;
}