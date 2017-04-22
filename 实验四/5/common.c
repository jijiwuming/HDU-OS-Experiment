#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <semaphore.h>
#include <sys/shm.h>
#include "common.h"
#define semname1 "send"
#define semname2 "rec"
#define key 2333
sem_t *sendsem;
sem_t *recsem;

void opensem(){
    if((sendsem=sem_open(semname1,O_CREAT,0644,0))==SEM_FAILED){
        printf("create sem failed!\n");
        exit(1);
    }
    if((recsem=sem_open(semname2,O_CREAT,0644,1))==SEM_FAILED){
        printf("create sem failed!\n");
        exit(1);
    }
}
void detachsem(){
    if(sem_close(sendsem)==-1||sem_close(recsem)==-1){
        printf("close sem failed!\n");
        exit(1);
    }
    printf("detach sems!\n");
}
void deletesem(){
    if(sem_unlink(semname1)==-1||sem_unlink(semname2)==-1){
        printf("delete sem failed!\n");
        exit(1);
    }
}
int createS_M(){
    int shmid=shmget(key,sizeof(message),IPC_CREAT|0666);
    if(shmid<0){
        printf("create S_M failed!\n");
        exit(1);
    }
    return shmid;
}
void detachS_M(void *shmp){
    if(shmdt(shmp)==-1){
        printf("detach S_M failed!\n");
        exit(1);
    }
}
void deleteS_M(int shmid){
    if(shmctl(shmid,IPC_RMID,0)==-1){
        printf("delete S_M failed!\n");
        exit(1);
    }
}