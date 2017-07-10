#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#define mydevice "/dev/mycdev"

int main()
{
    int fp, ret;
    unsigned char ch[10]="hello";
    ch[5]='\0';
	//打开设备驱动，前提是用mknode把字符设备建立为/dev/mycdev
    fp = open(mydevice, O_RDWR);
    //open的返回值不可能是0，1，2，这三个file descriptor是一直被打开的(除非在某个进程内被暂时关闭)。
    //习惯上，标准输入（standard input）的文件描述符是 0，标准输出（standard output）是 1，标准错误（standard error）是 2。
    //尽管这种习惯并非Unix内核的特性，但是因为一些 shell 和很多应用程序都使用这种习惯，
    //因此，如果内核不遵循这种习惯的话，很多应用程序将不能使用。
    printf("fp数值:%d\n", fp);
    //通过系统调用调用接口
    ret = ioctl(fp,0,0);
    printf("控制0返回 : %d\n", ret);
    //赋值为2时出现未在预期内的问题，根据可见的源码猜测可能为被安全机制过滤
    //security_file_ioctl
    ret = ioctl(fp,1,0);
    printf("控制1返回 : %d\n", ret);
    //写入
    ret = write(fp, ch, strlen(ch));
    printf("写入返回 : %d\n", ret);
    unsigned char ch2[3]="\0";
    //移动读写指针
    ret = lseek(fp,0,0);
    printf("新指针位置 : %d\n", ret);
    //读出
    ret = read(fp, ch2, sizeof(ch2)-1);
    printf("读出字符数 : %d\n", ret);
    printf("读到数据 :%s\n", ch2);
    ret = read(fp, ch2, sizeof(ch2)-1);
    printf("读出字符数 : %d\n", ret);
    printf("读到数据 :%s\n", ch2);
    close(fp);
    return 0;
}
