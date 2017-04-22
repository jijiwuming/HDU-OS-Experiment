#include <unistd.h>
#include <sys/syscall.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#define __NR_myniceset 333
int myniceset(pid_t pid,int flag,...){
	va_list arg_ptr;
	int nicevalue=0;
	va_start(arg_ptr, flag);
	nicevalue=va_arg(arg_ptr, int);
	va_end(arg_ptr);
	return syscall(__NR_myniceset,pid,flag,nicevalue);
}
int main(){
	pid_t pid;
	int flag,nicevalue=0,res=0;
	printf("请依次输入pid和flag，若flag为1则需要输入要设置的nice值：\n");
	scanf("%d",&pid);
	scanf("%d",&flag);
	if(flag==1){
		scanf("%d",&nicevalue);
		res=myniceset(pid,flag,nicevalue);
	}else{
		res=myniceset(pid,flag);	
	}
	printf("\n%d\n",res);
	return 0;
}
