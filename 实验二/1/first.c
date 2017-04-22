#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
MODULE_LICENSE("GPL");
static int fir_init(void){
    struct task_struct* p=NULL;
    printk(KERN_ALERT"\t\t程序名\tPID号\t\t进程状态\t进程优先级\n");
    for_each_process(p){
        if(p->mm==NULL)printk(KERN_ALERT"%20s%10d%10ld%10d",p->comm,p->pid,p->state,p->prio);
        //p->static_prio静态优先级
        //p->rt_priority实时优先级，0为普通进程，1-99为实时进程，99最高
        //p->normal_prio归一化优先级，根据前两个和调度策略计算得到
    }
    return 0;
}
static void fir_exit(void){
    printk(KERN_ALERT"exit modules\n");
}
module_init(fir_init);
module_exit(fir_exit);