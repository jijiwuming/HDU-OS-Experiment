#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <semaphore.h>
#include <sys/shm.h>
#include "common.h"
void receive(void *ptr){
    sem_wait(sendsem);
    message *shared=ptr;
    printf("received:%s\n",shared->msg);
    strcpy(shared->msg,"over");
    sem_post(recsem);
    sem_wait(sendsem);
    return;
}
int main(){
    opensem();
    int shmid = createS_M();
    void *shmp_p = shmat(shmid,NULL,0);
    receive(shmp_p);
    detachsem();
    detachS_M(shmp_p);
    return 0;
}