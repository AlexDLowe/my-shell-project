#include <pwd.h>
#include "parse.h"
#include <fcntl.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <setjmp.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <readline/history.h>
#include <readline/readline.h>

/*************************************/
/*************************************/
/****CODE FOR FUNCTION DECLARATIONS***/
/*************************************/
/*************************************/

void PrintCommand(int, Command *);
void PrintPgm(Pgm *);
void stripwhite(char *);
void myBash(Command *cmd);
void clearZombies(int signalNum);
void handleBreakSignal(int sigNum);
char* getUser();
void pwd();
char* cD(char** pgms);
void execCmd(Command* cmd);
int stdinRedirection(Command* cmd);
int stdoutRedirection(Command* cmd);
int stdinStdoutRedirection(Command* cmd);
void execPipedCmds(char ***cmd, Command *commandStruct, int fdInput);
int pipeRearrangement(Command *cmd);
void executionDecider(Command *cmd);
int listSize(Pgm *p);

/* When non-zero, this global means the user is done using this program.*/
int done = 0;

/*************************************/
/*************************************/
/***CODE FOR CTRL-C PROCESS KILLING***/
/*************************************/
/*************************************/

pid_t nPid;
int bgFlag = 0;
sig_atomic_t childExitStatus;
sigjmp_buf jmpBuf;

/*Removes children and outputs the exit status to childExitStatus.*/
void clearZombies(int signalNum) {

  int status;
  wait(&status);
  childExitStatus = status;
}
/*As the function name implies, this handles the break signals.*/
void handleBreakSignal(int sigNum) {

  if (nPid == 0 && (!bgFlag)) {
    write(1, "\n", 1);
    siglongjmp(jmpBuf, 20001);
  } else {
    killpg(nPid, SIGTERM);
    write(1, "\n", 1);
    siglongjmp(jmpBuf, 20001);
  }
}

/*************************************/
/*************************************/
/**************MAIN CODE**************/
/*************************************/
/*************************************/

int main(void) {

  Command cmd;
  int n;

  /*Makes sure to remove any unwanted zombies.*/
  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = &clearZombies;
  sigaction(SIGCHLD, &sa, NULL);

  /*Makes sure Ctrl-C has the desired effect.*/
  signal(SIGINT, handleBreakSignal);

  /*Return point used by the handleBreakSignal function.*/
  sigsetjmp(jmpBuf, 1);

  while (!done) {   /**/
    char *line;
    char cwd[1024];
    pwd();
    line = readline(": > ");

    if (!line) {
      /*Encountered EOF at top level.*/
      done = 1;
    } else {
      /*
       * Remove leading and trailing whitespace from the line
       * Then, if there is anything left, add it to the history list
       * and execute it.
       */
      stripwhite(line);
      if (*line) {
        add_history(line);
        n = parse(line, &cmd);
        PrintCommand(n, &cmd);
        bgFlag =cmd.bakground;

        /*Main portion of the built-in functions:
        exit, pwd, and cd.*/
        if (!strcmp(*cmd.pgm->pgmlist, "exit")) {
          return 0;
        } else if (!strcmp(*cmd.pgm->pgmlist, "pwd")) {
          pwd();
        } else if (!strcmp(*cmd.pgm->pgmlist, "cd")) {
          cD(cmd.pgm->pgmlist);
        }
        else {
          if (listSize(cmd.pgm) > 1) {
            pipeRearrangement(&cmd);
          } else {
            executionDecider(&cmd);
          }
        }
      }
    }

    if (line) {
      free(line);
    }
  }

  return 0;
}

/*************************************/
/*************************************/
/******CODE FOR PROGRAM EXECUTION*****/
/*************************************/
/*************************************/

/*As the name suggests, this is responsible for execute the commands.*/
void execCmd(Command* cmd) {

  if (execvp(*cmd->pgm->pgmlist, cmd->pgm->pgmlist) < 0) {
    printf("\nCould not execute command, please try again.\n");
    exit(1);
  }
}

/*Redirects input for the commands.*/
int stdinRedirection(Command* cmd) {

  /*Input redirection.*/
  int input = open(cmd->rstdin, O_RDONLY);
  if (input == -1) {
    perror("\nRedirection Failure, please try again.\n");
    return 255;
  }

  /*Creates backup fd of stdin for emergency use.*/
  int saveInput = dup(fileno(stdin));
  if (dup2(input, fileno(stdin)) == -1) {
    perror("\nCan't redirect.\n");
    return 255;
  }

  execCmd(cmd);

  /*Flushes output.*/
  fflush(stdout);
  /*Closes files.*/
  close(input);
  dup2(saveInput, fileno(stdin));
  close(saveInput);
  return 0;
}

/*Redirects output for the commands.*/
int stdoutRedirection(Command* cmd) {

  /*Output redirection.*/
  int output = open(cmd->rstdout, O_RDWR|O_CREAT|O_TRUNC, 0600);
  if (output == -1) {
    perror("\nRedirection Failure, please try again.\n");
    return 255;
  }

  /*Creates backup fd of stout for emergency use.*/
  int saveOutput = dup(fileno(stdout));
  if (dup2(output, fileno(stdout)) == -1) {
    perror("\nCan't redirect.\n");
    return 255;
  }

  execCmd(cmd);

  /*Flushes output.*/
  fflush(stdout);
  /*Closes files.*/
  close(output);
  dup2(saveOutput,fileno(stdout));
  close(saveOutput);
  return 0;
}

/*Redirects input AND output for the commands.*/
int stdinStdoutRedirection(Command* cmd) {

  /*Input redirection.*/
  int input = open(cmd->rstdin, O_RDONLY);
  if (input == -1) {
    perror("\nRedirection Failure, please try again.\n");
    return 255;
  }
  /*Creates backup fd of stdin for emergency use.*/
  int saveInput = dup(fileno(stdin));
  if (dup2(input, fileno(stdin)) == -1) {
    perror("\nCan't redirect.\n");
    return 255;
  }

  /*Output redirection.*/
  int output = open(cmd->rstdout, O_RDWR|O_CREAT|O_TRUNC, 0600);
  if (output == -1) {
    perror("\nRedirection Failure, please try again.\n");
    return 255;
  }
  /*Creates backup fd of stout for emergency use.*/
  int saveOutput = dup(fileno(stdout));
  if (dup2(output, fileno(stdout)) == -1) {
    perror("\nCan't redirect.\n");
    return 255;
  }

  execCmd(cmd);

  /*Flushes output.*/
  fflush(stdout);
  /*Closes files.*/
  close(input);
  close(output);

  dup2(saveInput, fileno(stdin)); close(saveInput);
  dup2(saveOutput, fileno(stdout)); close(saveOutput);

  return 0;
}

/*Responsible for the execution of any commands containing pipes.*/
void execPipedCmds(char ***cmd, Command *commandStruct, int fdInput) {

  int pipefd[2];
  pid_t pid;
  int fdIn = fdInput;

  while (*cmd != NULL) {
    if (pipe(pipefd) == -1) {
      perror("\nPipe failure, please try again.\n");
    }
      if ((pid = fork()) == -1) {
        perror("\nFork failure, please try again.\n");
      } else if (pid == 0) {          /*Child process.*/
        dup2(fdIn, STDIN_FILENO);
        if (*(cmd + 1) != NULL) {
          dup2(pipefd[1], STDOUT_FILENO);
        } else if ((*(cmd + 1) == NULL) && (commandStruct->rstdout != NULL)) {
        pipefd[1] = open(commandStruct->rstdout, O_WRONLY|O_CREAT|O_TRUNC, 0600);
        if (pipefd[1] == -1) {
          perror("\nRedirection Failure, please try again.\n");
        }
        if (dup2(pipefd[1], STDOUT_FILENO) == -1) {
          perror("\nCan't redirect.\n");
          return;
        }
      }
        close(pipefd[0]); //Closes one end of the pipe.
        if (execvp((*cmd)[0], *cmd) == -1) {
          perror("\nCould not execute command, try again.\n");
          exit(EXIT_SUCCESS);
        }
      } else {          /*Parent process.*/
        if ((commandStruct->bakground != NULL) && (*(cmd + 1) == NULL)) {
          close(pipefd[1]); //Closes one end of the pipe.
            fdIn = pipefd[0];
            cmd++; //Next command.
          } else {
          wait(NULL);
          close(pipefd[1]);
            fdIn = pipefd[0]; //Carries input across for the subsequent program.
            cmd++; //Next command.
      }
    }
  }
}

/*As function name suggests, this function rearranges the piped
command, so that it's back to front - for the purpose of parsing.
However, this does also check for any necessary redirections.*/
int pipeRearrangement(Command *cmd) {

  Pgm *head =cmd->pgm;
  char ***inputList = (char***) malloc((listSize(cmd->pgm) + 1) * sizeof(char**));
  int len = listSize(cmd->pgm);
  int i = listSize(cmd->pgm);

  /*Turns the list around.*/
  while (i !=0) {
    inputList[i-1] = cmd->pgm->pgmlist;
    cmd->pgm = cmd->pgm->next;
    i--;
  }

  inputList[len] = NULL;

  if (cmd->rstdin != NULL) {
    int input = open(cmd->rstdin, O_RDONLY);
    int saveInput = dup(fileno(stdin));

    if (dup2(input, fileno(stdin)) == -1) {
      perror("\nCan't redirect.\n");
      return 255;
    }
    execPipedCmds(inputList, cmd, input);
    fflush(stdout);

    close(input);
    dup2(saveInput, fileno(stdin));
    close(saveInput);
  } else {
    execPipedCmds(inputList, cmd, 0);
  }

  return 0;
}

void executionDecider(Command *cmd) {
  if ((nPid = fork()) == -1) {
    perror("\nFork failure, please try again.\n");
    exit(1);
  }
  if (nPid == 0) {          /*Child process.*/
    setpgid(nPid, nPid);
    if (cmd->rstdout != NULL) {
      if (cmd->rstdin != NULL) { //Both input and output redirecetion needed.
        stdinStdoutRedirection(cmd);
      } else { //Just output redirecetion needed.
        stdoutRedirection(cmd);
      }
    } else {
      if (cmd->rstdin != NULL) { //Just input redirecetion needed.
        stdinRedirection(cmd);
      } else { //Neither input nor output redirecetion needed.
        execCmd(cmd);
      }
    }
  }

  if (nPid > 0) {          /*Parent process.*/
    if (cmd->bakground) { //Runs in bg, so shell doesn't wait.
      waitpid(nPid,NULL, WNOHANG);
    } else { // Runs in fg, so shell does wait.
      wait(NULL);
    }
  }
}

int listSize(Pgm *p) {

  int i = 0;

	while(p != NULL) {
		i++;
		p = p->next;
	}
	return i;
}

/*************************************/
/*************************************/
/**************GIVEN CODE*************/
/*************************************/
/*************************************/

/*
 * Name: PrintCommand
 *
 * Description: Prints a Command structure as returned by parse on stdout.
 *
 */
void PrintCommand (int n, Command *cmd) {

  printf("Parse returned %d:\n", n);
  printf("   stdin : %s\n", cmd->rstdin  ? cmd->rstdin  : "<none>" );
  printf("   stdout: %s\n", cmd->rstdout ? cmd->rstdout : "<none>" );
  printf("   bg    : %s\n", cmd->bakground ? "yes" : "no");
  PrintPgm(cmd->pgm);
}

/*
 * Name: PrintPgm
 * Description: Prints a list of Pgm:s
 */
void PrintPgm (Pgm *p) {

	if (p == NULL) {
    return;
  } else {
		char **pl = p->pgmlist;
		/* The list is in reversed order so print it reversed to get right*/
		PrintPgm(p->next);
		printf("    [");
		while (*pl) {printf("%s ", *pl++);}
		printf("]\n");
	}
}
/*
 * Name: stripwhite
 * Description: Strip whitespace from the start and end of STRING.
 */
void stripwhite (char *string) {

  register int i = 0;
  while (whitespace( string[i] )) {i++;}
  if (i) {strcpy (string, string + i);}
  i = strlen( string ) - 1;
  while (i> 0 && whitespace (string[i])) {i--;}
  string [++i] = '\0';
}

/*************************************/
/*************************************/
/*********MISCELLANEOUS CODE**********/
/*************************************/
/*************************************/

/*Retrieves the current user's username.*/
char* getUser() {

  struct passwd *passwd;
  passwd = getpwuid(getuid());
  return passwd->pw_name;
}

/*Similar to the Linux terminal command, this prints the working directory.*/
void pwd() {

  char cwd[1024];
  printf("%s@%s", getUser(), getcwd(cwd, sizeof(cwd)));
}

/*Changes the current directory (cd).*/
char* cD(char** pgms) {

  char cwd[1024];
  if (*(pgms+1) != NULL) {
    if (chdir(*(pgms+1)) == -1) {
      perror("\nPlease try again, check for spelling.\n");
    } else {
      printf("Directory now:\n");
      getcwd(cwd, sizeof(cwd));
    }
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
*/
