#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <semaphore.h>
#include <sys/shm.h>
#include "common.h"
void sender(void *ptr){
    sem_wait(recsem);
    message *shared=ptr;
    printf("Enter you msg:");
    scanf("%s",shared->msg);
    sem_post(sendsem);
    sem_wait(recsem);
    printf("final received:%s\n",shared->msg);
    sem_post(sendsem);
    return;
}
int main(){
    opensem();
    int shmid = createS_M();
    void *shmp_p = shmat(shmid,NULL,0);
    sender(shmp_p);
    detachsem();
    deletesem();
    detachS_M(shmp_p);
    deleteS_M(shmid);
    printf("exit program!\n");
    return 0;
}