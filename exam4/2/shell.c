#include <stdio.h>
#include <stdlib.h>
int main(){
    char cmd[20];
    while(gets(cmd)!=NULL){
        if(strcmp(cmd,"exit")==0){
            break;
        }else if(strcmp(cmd,"cmd1")==0){
            system("./cmd1");
        }else if(strcmp(cmd,"cmd2")==0){
            system("./cmd2");
        }else if(strcmp(cmd,"cmd3")==0){
            system("./cmd3");
        }else{
            printf("Command not found\n");
        }
    }
    return 0;
}