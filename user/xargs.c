#include "kernel/types.h"
#include "user/user.h"

int MAXSIZE = 100;

int getstdin(char *buffer)
{
  char *p = buffer;
  while (read(0, p, sizeof(char)) != 0)
  {
    if (*p == '\n')
    {
      *p = 0;
      break;
    }
    p++;
  }

  return p - buffer;
}

int
main(int argc, char **argv)
{
  char buffer[MAXSIZE];
  char *cmd = argv[1];
  char *exec_args[MAXSIZE];

  memcpy(exec_args, argv + 1, (argc - 1) * sizeof(char *));
  while(getstdin(buffer) > 0)
  {
    exec_args[argc - 1] = buffer;
    int pid = fork();
    if (pid < 0) {
      fprintf(2, "fork failed");
      exit(1);
    }

    if (pid > 0)
    {
      wait(0);
    } else {
      exec(cmd, exec_args);
    }
  }
  exit(0);
}