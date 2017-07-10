#include <linux/fs.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/kernel.h>
#include <asm/uaccess.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <asm-generic/errno-base.h>
#define len 512
MODULE_LICENSE("GPL");
//主设备号
static int dev_major=255;
//自定义的字符设备
typedef struct mymem_dev{
    struct cdev cdev;
    unsigned char mem[len];
}mymem_dev;
//全局设备结构体指针
mymem_dev* my_dev;
//打开设备
static int char_dev_open(struct inode *inode,struct file *file){
    //初始化
    mymem_dev* private;
    private = container_of(inode->i_cdev,mymem_dev,cdev);
    file->private_data = private;
    printk("char_dev device open.\n");
    return 0;
}
//修改读写指针位置
static loff_t char_dev_llseek (struct file *file, loff_t offset, int way){
    loff_t newpos=0;
    switch(way){
        case 0://从起始位置开始
            newpos = offset;
            break;
        case 1://当前位置开始
            newpos = file->f_pos+offset;
            break;
        case 2://结尾开始
            newpos = len+offset;
            break;
        default:
            break;
    }
    if(newpos<0||newpos>len){
        return -EINVAL;
    }
    file->f_pos=newpos;
    return newpos;
}
//读设备
static ssize_t char_dev_read(struct file *file,char __user *buff,size_t count,loff_t *offp){
    unsigned long offset = *offp;
    unsigned long leave = len - offset;
    unsigned long length = count<leave?count:leave;
    mymem_dev* tmp = (mymem_dev*)file->private_data;
    long res = copy_to_user(buff,tmp->mem+offset,length);
    printk("read offset:%ld\n",offset);
    printk("read count:%ld\n",count);
    if(res)return -1;
    (*offp)+=length;
    return length;
}
//写设备
static ssize_t char_dev_write(struct file *file, const char __user *buff, size_t count, loff_t *offp)
{
    unsigned long offset = *offp;
    unsigned long leave = len - offset;
    unsigned long length = count<leave?count:leave;
    mymem_dev* tmp = (mymem_dev*)file->private_data;
    long res = copy_from_user(tmp->mem+offset,buff, length);
    printk("write offset:%ld\n",offset);
    printk("write count:%ld\n",count);
    if(res)return -1;
    (*offp)+=length;
    return length;
}
//I/O控制
static long char_dev_ioctl(struct file *file, unsigned int cmd, unsigned long arg){
    switch(cmd){
        case 0:
            printk("choose 0\n");
            break;
        default:
            printk("default\n");
            break;
    }
    return 0;
}
//关闭设备
static int char_dev_release(struct inode *inode, struct file *file){
    printk("char_dev device resealed.\n");
    return 0;
}
//文件操作结构
struct file_operations fops = {
    .owner = THIS_MODULE,
    .llseek = char_dev_llseek,
    .read = char_dev_read,
    .write = char_dev_write,
    .unlocked_ioctl = char_dev_ioctl,//原ioctl消失的版本是v2.6.35到v2.6.36-rc1间
    //现在去除内核锁保护，最好需要自己加锁
    .open = char_dev_open,
    .release = char_dev_release,
};
//初始化
static int char_dev_init(void){
    dev_t dev;
    //动态分配设备号
    int res = alloc_chrdev_region(&dev, 0, 1, "my_driver");
    if(res<0){
        printk("Assign device number failed!\n");
        return -1;
    }
    //设置主设备号
    dev_major = MAJOR(dev);
    //定义cdev结构及初始化
    my_dev = kmalloc(sizeof(mymem_dev), GFP_KERNEL);
    if(!my_dev){
        printk("memory request failed!\n");
        //释放设备号
        unregister_chrdev_region(dev,1);
        return -1;
    }
    //对申请的内存进行清零
    memset(my_dev,0,sizeof(mymem_dev));
    cdev_init(&(my_dev->cdev),&fops);
    my_dev->cdev.owner = THIS_MODULE;
    //注册cdev
    res = cdev_add(&(my_dev->cdev),dev,1);
    if(res){
        printk("Add device failed，errcode is %d\n",res);
        //释放内存
        kfree(my_dev);
        //释放设备号
        unregister_chrdev_region(dev,1);
        return -1;
    }
    printk("The device has been installed!\n");
    return 0;
}
//释放设备
static void char_dev_exit(void){
    //注销设备
    cdev_del(&(my_dev->cdev));
    //释放内存
    kfree(my_dev);
    {
        //释放设备号
        dev_t dev=MKDEV(dev_major, 0);
        unregister_chrdev_region(dev,1);
    }
    printk("The device has been released!\n");
    return;
}
module_init(char_dev_init);
module_exit(char_dev_exit);