#ifndef   _COMMON_H_
#define	  _COMMON_H_
#include <stdio.h>
#include <sys/shm.h>
#define LENOFMSG 100
extern sem_t *sendsem;
extern sem_t *recsem;
struct message{
    char msg[LENOFMSG];
};
typedef struct message message;
void opensem();
void detachsem();
void deletesem();
int createS_M();
void detachS_M(void *shmp);//分离内存
void deleteS_M(int shmid);
#endif