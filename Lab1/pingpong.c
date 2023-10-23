#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#define BUFSIZE 5
#define PSIZE 2

int
main(int argc, char *argv[])
{
    int fd[PSIZE];
    char buffer[BUFSIZE];
    
    if(pipe(fd) < 0){
    	printf("Error while creating pipe\n");
    	exit(-1);
    }
    
    int pid = fork();
     	
    if (pid == 0) {
        read(fd[0], buffer, BUFSIZE);
        printf("%d: got %s\n", getpid(), buffer);
        close(fd[0]);
        write(fd[1], "pong", BUFSIZE);
        close(fd[1]);

    } else if(pid > 0){
        write(fd[1], "ping", BUFSIZE);
        wait(0);
        close(fd[1]);
        read(fd[0], buffer, BUFSIZE);
        printf("%d: got %s\n", getpid(), buffer);
        close(fd[0]);
    }
    else{
    	printf("Error while fork");
    	exit(-1);
    }
    exit(0);
}
