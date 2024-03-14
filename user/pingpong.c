#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char **argv)
{
    int pipe1[2];
    int pipe2[2];
    int pid;
    char buffer[1];

    if (pipe(pipe1) == -1)
    {
      fprintf(2, "open pipe1 failed");
      exit(1);
    }

    if (pipe(pipe2) == -1)
    {
      fprintf(2, "open pipe2 failed");
      exit(1);
    }

    pid = fork();
    if (pid == -1)
    {
      fprintf(2, "fork failed");
      exit(1);
    }

    if (pid > 0) // parent
    {
      write(pipe1[1], "a", 1);

      read(pipe2[0], buffer, sizeof(buffer));
      fprintf(1, "%d: received ping\n", getpid());
    } 
    else { // child
      read(pipe1[0], buffer, sizeof(buffer));
      fprintf(1, "%d: received pong\n", getpid());

      write(pipe2[1], "a", 1);
    }

  wait(0); // Wait for any child process to terminate
  exit(0);
}