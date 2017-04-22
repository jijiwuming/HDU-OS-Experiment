#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/sched.h>
#include <linux/pid.h>
#include <linux/moduleparam.h>
MODULE_LICENSE("GPL");
static int pid;
module_param(pid,int,0644);
static int sec_init(void){
    struct task_struct* p=pid_task(find_vpid(pid),PIDTYPE_PID);
    //find_task_by_vpid(pid) 2.6.31后不能使用,采用pid_task(find_vpid(pid),PIDTYPE_PID)替代
    //real_parent可能已经终止，此时parent作为父进程,但其实不是真的父进程
    struct list_head *sibp=NULL;
    struct list_head *childp=NULL;
    struct task_struct *parentp=p->parent, *temp=NULL,*temp1=NULL;
    printk(KERN_ALERT"parent processes:\n");    
    printk(KERN_ALERT"\tprocess name\tPID number\tprocess state\tnicevalue of process\n");
    printk(KERN_ALERT"%20s%10d%10ld%10d",parentp->comm,parentp->pid,parentp->state,parentp->prio);
    printk(KERN_ALERT"sibling processes:\n");    
    printk(KERN_ALERT"\tprocess name\tPID number\tprocess state\tnicevalue of process\n");
    list_for_each(sibp,&p->sibling){
        temp=list_entry(sibp,struct task_struct,sibling);
        printk(KERN_ALERT"%20s%10d%10ld%10d",temp->comm,temp->pid,temp->state,temp->prio);
        //p->static_prio静态优先级
        //p->rt_priority实时优先级，0为普通进程，1-99为实时进程，99最高
        //p->normal_prio归一化优先级，根据前两个和调度策略计算得到
    }
    printk(KERN_ALERT"children processes:\n");    
    printk(KERN_ALERT"\tprocess name\tPID number\tprocess state\tnicevalue of process\n");
    list_for_each(childp,&p->children){
        temp1=list_entry(childp,struct task_struct,sibling);//映射过来是子进程的sibling成员地址
        printk(KERN_ALERT"%20s%10d%10ld%10d",temp1->comm,temp1->pid,temp1->state,temp1->prio);
    }
    return 0;
}
static void sec_exit(void){
    printk(KERN_ALERT"exit modules\n");
}
module_init(sec_init);
module_exit(sec_exit);