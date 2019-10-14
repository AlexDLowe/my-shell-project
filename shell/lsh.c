#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <readline/readline.h>
#include <readline/history.h>
#include "parse.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>


/*
 * Function declarations
 */
void INThandler(int);
void PrintCommand(int, Command *);
void PrintPgm(Pgm *);
void stripwhite(char *);
int builtInCd(char **cmds);
int builtInExit(char **cmds);
int executionDecider(char *line, char **cmds, char **pipedCmds, Command cmd);
int execCmd(char **cmds, Command cmd);
int execBgCmd(char **cmds, Command cmd);
int execCmdsPiped(char **cmds, char **pipedCmds, Command cmd);
int execCmdsPipedBg(char **cmds, char **pipedCmds, Command cmd);

/* When non-zero, this global means the user is done using this program. */
int done = 0;


/* Name:        getCmds
 * Description: Gets the commands into a processable format.
 */
char **getCmds(Pgm *p){
  if (p == NULL) {
  } else {
    char **pl = p->pgmlist;
  }
}

/* Name:        getPipedCmds
 * Description: Gets the piped commands into a processable format.
 */
char **getPipedCmds(Pgm *p){

  if (p->next == NULL) {
  } else {
    char **pipedP = p->next->pgmlist;
    return pipedP;
  }
}

/* Name:        main
 * Description: Gets the ball rolling...
 */
int main(void) {

  Command cmd;
  int n, forSignals;
  char **cmds;
  char **pipedCmds;

  signal(SIGINT, INThandler);

  while (!done) {

    char *line;
    line = readline("> ");

    if (!line) {
      /* Encountered EOF at top level */
      done = 1;
    } else {
      /*
       * Remove leading and trailing whitespace from the line
       * Then, if there is anything left, add it to the history list
       * and execute it.
       */
      stripwhite(line);

      if(*line) {
        add_history(line);
        /* execute it */
        n = parse(line, &cmd);
        PrintCommand(n, &cmd);
        cmds = getCmds(cmd.pgm);
        pipedCmds = getPipedCmds(cmd.pgm);
        executionDecider(line, cmds, pipedCmds, cmd);
      }
    }
    
    if(line) {
      free(line);
    }
  }
  return 0;
}

/* Name:        INThandler
 * Description: Upon receiving the SIGINT signal from Ctrl+C we're brought here for it to be handled.
 */
void  INThandler(int sig)
{
  signal(sig, SIG_IGN);
  kill(SIGINT, 0);
  printf("\n");
}

/* Name:        PrintCommand
 * Description: Prints a Command structure as returned by parse on stdout.
 */
void PrintCommand (int n, Command *cmd) {

  printf("Parse returned %d:\n", n);
  printf("   stdin : %s\n", cmd->rstdin  ? cmd->rstdin  : "<none>" );
  printf("   stdout: %s\n", cmd->rstdout ? cmd->rstdout : "<none>" );
  printf("   bg    : %s\n", cmd->bakground ? "yes" : "no");
  PrintPgm(cmd->pgm);
}

/* Name:        PrintPgm
 * Description: Prints a list of Pgm:s
 */
void PrintPgm (Pgm *p) {

  if (p == NULL) {
    return;
  } else {
    char **pl = p->pgmlist;

    /* The list is in reversed order so print
     * it reversed to get right
     */

    PrintPgm(p->next);
    printf("    [");

    while (*pl) {
      printf("%s ", *pl++);
    }
    printf("]\n");
  }
}

/* Name:        stripwhite
 * Description: Strip whitespace from the start and end of STRING.
 */
void stripwhite (char *string) {

  register int i = 0;

  while (isspace( string[i] )) {
    i++;
  }
  
  if (i) {
    strcpy (string, string + i);
  }

  i = strlen( string ) - 1;

  while (i> 0 && isspace (string[i])) {
    i--;
  }

  string [++i] = '\0';
}

/************************************/
/************************************/
/*****CODE SECTION FOR BUILT-INS*****/
/************************************/
/************************************/

/* Name:        builtIns_str[]
 * Description: Array containing the builtin functions
 */
char *builtIns_str[] = {
  "cd",
  "exit"
};

/* Name:        builtIns_func[]
 * Description: Array containing the functions of my builtins
 */
int (*builtIns_func[]) (char **) = {
  &builtInCd,
  &builtInExit
};

/* Name:        amountOfBuiltIns
 * Description: This returns the quantity of builtins for later use
 */
int amountOfBuiltIns() {
  return sizeof(builtIns_str) / sizeof(char *);
}

/* Name:        builtInCd
 * Description: This is the functions used for changing directories (cd)
 */
int builtInCd(char **cmds) {

    if (cmds[1] == NULL) {
    fprintf(stderr, "lsh: expected argument to \"cd\"\n");
  } else {
    if (chdir(cmds[1]) != 0) {
      perror("lsh");
    }
  }
  return 1;
}

/* Name:        builtInExit
 * Description: This is the builtin for exiting the program
 */
int builtInExit(char **cmds){
  done = 1;
  exit(0);
  return 0;
}

/*************************************/
/*************************************/
/*****CODE SECTION FOR EXECUTIONS*****/
/*************************************/
/*************************************/

/* Name:        executionDecider
 * Description: This decides firstly, if the cmds are builtins, or
 *              programs, then, it decides whether it's a bg
 *              program or not.
 */
int executionDecider (char *line, char **cmds, char **pipedCmds, Command cmd){

  int i;
  /*if (cmds != NULL){
    printf("cmds = %s\n", *cmds);
  }
  if (pipedCmds != NULL){
    printf("pipedCmds = %s\n", *pipedCmds);
  }*/
  if (cmds[0] == NULL) {
    // An empty command was entered.
    return 1;
  }

  for (i = 0; i < amountOfBuiltIns(); i++) {
    if (strcmp(cmds[0], builtIns_str[i]) == 0) {
      return (*builtIns_func[i])(cmds);
    }
  }
  if ((strchr(line,'&') != NULL) && (strchr(line, '|') != NULL)) {
    return execCmdsPipedBg(cmds, pipedCmds, cmd);
  } else if (strchr(line,'&') != NULL) {
    return execBgCmd(cmds, cmd);
  } else if (strchr(line, '|') != NULL) {
    return execCmdsPiped(cmds, pipedCmds, cmd);
  } else if (pipedCmds == NULL) {
    return execCmd(cmds, cmd);
  }
}

/* Name:        execCmd
 * Description: Uses execvp to execute the users commands.
 */
int execCmd(char **cmds, Command cmd) {

  pid_t pid;

    pid = fork();
    if (pid <0) {
      fprintf(stderr, "Fork Failure");
      return 1;

    } else if (pid == 0) {
      if (cmd.rstdin != NULL) {
        int fd0 = open(cmd.rstdin, O_RDONLY);
        dup2(fd0, STDIN_FILENO);
        close(fd0);
      }
      if (cmd.rstdout != NULL) {
        int fd1 = open(cmd.rstdout , O_CREAT | O_WRONLY | O_TRUNC, 0644) ;
        dup2(fd1, STDOUT_FILENO);
        close(fd1);
      }
      if (execvp(cmds[0], cmds) < 0) { 
        printf("\nCould not execute command, try again.\n"); 
      } 
      exit(0);

    } else {
      wait(NULL);
    }
}

/* Name:        execBgCmd
 * Description: Uses execvp to execute the users commands.
 *              But this time, in the background.
 */
int execBgCmd(char **cmds, Command cmd) {

  pid_t pid1, pid2;
  int status;

  if (pid1 = fork()) {
    waitpid(pid1, &status, 1);
  } else if (!pid1) {
    if (pid2 = fork()) {
      exit(0);
    } else if (!pid2) {
      if (cmd.rstdin != NULL) {
        int fd0 = open(cmd.rstdin, O_RDONLY);
        dup2(fd0, STDIN_FILENO);
        close(fd0);
      }
      if (cmd.rstdout != NULL) {
        int fd1 = open(cmd.rstdout , O_CREAT | O_WRONLY | O_TRUNC, 0644) ;
        dup2(fd1, STDOUT_FILENO);
        close(fd1);
      }
      if (execvp(cmds[0], cmds) < 0) { 
        printf("\nCould not execute command, try again.\n"); 
      }
    } else {
      fprintf(stderr, "Fork Failure");
    }
  } else {
    fprintf(stderr, "Fork Failure");
  }
  wait(NULL);
}

/* Name:        execCmdsPiped
 * Description: Executes piped commands
 */
int execCmdsPiped(char **cmds, char **pipedCmds, Command cmd){

  // 0 is read end, 1 is write end 
  int pipefd[2]; 

  pid_t pid1, pid2; 

  if (pipe(pipefd) == -1) {
    fprintf(stderr,"Pipe failed");
    return 1;
  } 
  pid1 = fork(); 
   if (pid1 < 0) { 
    fprintf(stderr, "Fork Failure");
  } 

  if (pid1 == 0) { 
  // Child 1 executing.. 
  // It only needs to write at the write end 
    close(pipefd[0]); 
    dup2(pipefd[1], STDOUT_FILENO); 
    close(pipefd[1]);
    if (cmd.rstdin != NULL) {
      int fd0 = open(cmd.rstdin, O_RDONLY);
      dup2(fd0, STDIN_FILENO);
      close(fd0);
    }
    if (cmd.rstdout != NULL) {
      int fd1 = open(cmd.rstdout , O_CREAT | O_WRONLY | O_TRUNC, 0644) ;
      dup2(fd1, STDOUT_FILENO);
      close(fd1);
    }
    if (execvp(pipedCmds[0], pipedCmds) < 0) { 
      printf("\nCouldn't execute command 1: %s\n", *pipedCmds); 
      exit(0); 
    }
  } else { 
    // Parent executing 
    pid2 = fork(); 

    if (pid2 < 0) { 
      fprintf(stderr, "Fork Failure");
      exit(0);
    }

    // Child 2 executing.. 
    // It only needs to read at the read end 
    if (pid2 == 0) { 
      close(pipefd[1]); 
      dup2(pipefd[0], STDIN_FILENO); 
      close(pipefd[0]);
      if (cmd.rstdin != NULL) {
        int fd0 = open(cmd.rstdin, O_RDONLY);
        dup2(fd0, STDIN_FILENO);
        close(fd0);
      }
      if (cmd.rstdout != NULL) {
        int fd1 = open(cmd.rstdout , O_CREAT | O_WRONLY | O_TRUNC, 0644) ;
        dup2(fd1, STDOUT_FILENO);
        close(fd1);
      }
      if (execvp(cmds[0], cmds) < 0) { 
        //printf("\nCouldn't execute command 2...");
        printf("\nCouldn't execute command 2: %s\n", *cmds);
        exit(0);
      }
    } else {
      // parent executing, waiting for two children
      close(pipefd[0]);
      close(pipefd[1]);
      wait(NULL);
      wait(NULL);
    } 
  }
}

/* Name:        execCmdsPipedBg
 * Description: Executes backgrounded piped commands
 */
int execCmdsPipedBg(char **cmds, char **pipedCmds, Command cmd){

  // 0 is read end, 1 is write end 
  int pipefd[2]; 

  pid_t pid1, pid2; 

  if (pipe(pipefd) == -1) {
    fprintf(stderr,"Pipe failed");
    return 1;
  } 
  pid1 = fork(); 
  if (pid1 < 0) { 
    fprintf(stderr, "Fork Failure");
  } 

  if (pid1 == 0) { 
  // Child 1 executing.. 
  // It only needs to write at the write end 
    close(pipefd[0]); 
    dup2(pipefd[1], STDOUT_FILENO); 
    close(pipefd[1]);

    if (cmd.rstdin != NULL) {
      int fd0 = open(cmd.rstdin, O_RDONLY);
      dup2(fd0, STDIN_FILENO);
      close(fd0);
    }
    if (cmd.rstdout != NULL) {
      int fd1 = open(cmd.rstdout , O_CREAT | O_WRONLY | O_TRUNC, 0644) ;
      dup2(fd1, STDOUT_FILENO);
      close(fd1);
    }
    if (execvp(pipedCmds[0], pipedCmds) < 0) { 
      printf("\nCouldn't execute command 1: %s\n", *pipedCmds); 
      exit(0); 
    }
  } else { 
    // Parent executing 
    pid2 = fork(); 

    if (pid2 < 0) { 
      fprintf(stderr, "Fork Failure");
      exit(0);
    }

    // Child 2 executing.. 
    // It only needs to read at the read end 
    if (pid2 == 0) { 
      close(pipefd[1]); 
      dup2(pipefd[0], STDIN_FILENO); 
      close(pipefd[0]);

      if (cmd.rstdin != NULL) {
        int fd0 = open(cmd.rstdin, O_RDONLY);
        dup2(fd0, STDIN_FILENO);
        close(fd0);
      }
      if (cmd.rstdout != NULL) {
        int fd1 = open(cmd.rstdout , O_CREAT | O_WRONLY | O_TRUNC, 0644) ;
        dup2(fd1, STDOUT_FILENO);
        close(fd1);
      }
      if (execvp(cmds[0], cmds) < 0) { 
        //printf("\nCouldn't execute command 2...");
        printf("\nCouldn't execute command 2: %s\n", *cmds);
        exit(0);
      }
    } else {
      close(pipefd[0]);
      close(pipefd[1]);
      wait(NULL);
    }
    wait(NULL);
  }
}
/*
~
~
~
~
~
~
~
~
~
~
~
~
*/
