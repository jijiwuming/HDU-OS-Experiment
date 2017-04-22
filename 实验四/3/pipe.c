#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

int main()
{
    int fileps[2];
    pid_t childpid[3]; //子进程pid
    pipe(fileps);
    childpid[0] = fork();
    if (childpid[0] == 0)
    {
        close(fileps[0]); //关闭读，只写
        char s[] = "this is write by child1.\n";
        printf("child1\n");
        write(fileps[1], s, sizeof(s));
        exit(0); //退出子进程
    }else if(childpid[0]<0){
        printf("child1 fork failed!\n");
    }

    childpid[1] = fork();
    if (childpid[1] == 0)
    {
        close(fileps[0]);
        char s[] = "this is write by child2.\n";
        printf("child2\n");
        sleep(1);
        write(fileps[1], s, sizeof(s));
        exit(0);
    }else if(childpid[1]<0){
        printf("child2 fork failed!\n");
    }

    childpid[2] = fork();
    if (childpid[2] == 0)
    {
        close(fileps[0]);
        char s[] = "this is write by child3.\n";
        printf("child3\n");
        sleep(2);
        write(fileps[1], s, sizeof(s));
        exit(0);
    }else if(childpid[2]<0){
        printf("child3 fork failed!\n");
    }
    close(fileps[1]); //关闭写，只读

    char buf[90];
    read(fileps[0], buf, sizeof(buf));
    printf("%s", buf);
    
    char buf1[90];
    read(fileps[0], buf1, sizeof(buf1));
    printf("%s", buf1);

    char buf2[90];
    read(fileps[0], buf2, sizeof(buf2));
    printf("%s", buf2);
    wait(0);
    return 0;
}