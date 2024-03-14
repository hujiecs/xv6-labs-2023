#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

void prime(int read_fd);

int
main(int argc, char **argv)
{
    int pipe_fd[2];
    int pid;

    if (pipe(pipe_fd) == -1)
    {
        fprintf(2, "open pipe failed");
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
        close(pipe_fd[0]); // close parent read
        for (int i = 2; i <= 35; i++)
        {
            // write 2,3,4,5...35 to child
            write(pipe_fd[1], &i, sizeof(i));
        }
        close(pipe_fd[1]);
    } 
    else { // child
        close(pipe_fd[1]); // close child write
        prime(pipe_fd[0]);
        close(pipe_fd[0]);
    }

    wait(0); // Wait for any child process to terminate
    exit(0);
}

void prime(int read_fd)
{
    int prime_value;
    int p[2];
    if (read(read_fd, &prime_value, sizeof(int)) == 0) // EOF
    {
        exit(0);
    }

    fprintf(1, "prime %d\n", prime_value);

    if (pipe(p) == -1)
    {
        fprintf(2, "open pipe failed");
        exit(1);
    }

    int pid = fork();
    if (pid == -1)
    {
        fprintf(2, "fork failed");
        exit(1);
    }

    if (pid > 0) // parent
    {
        close(p[0]);
        int value;
        while (read(read_fd, &value, sizeof(int)) > 0)
        {
            // first filter % 2 == 0
            // next filter % 3 == 0
            // then filter % 5 == 0
            // ...
            if (value % prime_value != 0)
            {
                write(p[1], &value, sizeof(int));
            }
        }
        close(p[1]); 
    } else { // child
        close(p[1]);
        prime(p[0]);
        close(p[0]);      
    }

    wait(0); // Wait for any child process to terminate
}
