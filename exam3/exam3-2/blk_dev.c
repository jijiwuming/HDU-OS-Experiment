#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/fcntl.h>//文件控制选项头文件
#include <linux/blkpg.h>
#include <asm/uaccess.h>//与处理器相关的入口
#include <linux/types.h>
#include <linux/vmalloc.h>
#include <linux/genhd.h>
#include <linux/blkdev.h>
#include <linux/hdreg.h>//定义了一些对硬盘控制器进行编程的一些命令常量符号。
//设备名
#define DEV_NAME "my_blk"

#define DISKNAME "jijiwuming"
//主设备号
static int major=0;
//扇区大小
#define SECTOR_SIZE 512
//总大小 4M
#define TOTAL_SIZE (4*1024*1024)
//定义自旋锁
spinlock_t myram_lock;
//在Linux内核中，使用gendisk(通用磁盘)结构体来表示1个独立的磁盘设备（或分区）。
//内存块设备
static struct gendisk *myram_gendisk;
//请求队列
static struct request_queue *my_queue;
//内存分配指针
static unsigned char *myramblock_buf;
//请求处理函数
static void do_my_blk_request(struct request_queue *q){
    struct request *request;
    while ((request = blk_fetch_request(q)) != NULL) {
        struct bio* bio;//用于遍历
        //若为丢弃请求
		if (request->cmd_type != REQ_TYPE_FS) {
			printk(KERN_NOTICE "%s: skip non-fs request\n",DEV_NAME);
            //更新已完成处理的请求
			__blk_end_request_all(request, -EIO);
			continue;
		}
        //遍历request中的bio结构
        __rq_for_each_bio(bio, request) {
            // bio 结构体表示正在执行的 I/O 操作相关的信息。
            // bio_io_vec ——bio_vec链表表示当前 I/O 操作涉及到的内存页
            // bi_vcnt bi_io_vec链表中bi_vec的个数
            // bi_idx 当前的 bi_vec片段，通过 bi_vcnt（总数）和 bi_idx（当前数）
            // 就可以跟踪当前 I/O 操作的进度
			struct bvec_iter iter;// 遍历节点
			struct bio_vec bio_vec;// bio_vec 结构体表示 I/O 操作使用的片段
			sector_t sector = bio->bi_iter.bi_sector;//当前操作起始扇区
            //获取起始的偏移量
            unsigned long pos = sector << 9;
            //遍历bio中的段
			bio_for_each_segment(bio_vec, bio, iter) {
				//映射page为虚拟地址
				char* buffer = __bio_kmap_atomic(bio, iter);
				//当前偏移量
				unsigned long offset = pos;
				//需要的数据量大小
				unsigned long size = bio_cur_bytes(bio);
                //判断请求是否超出范围
				if ((offset + size) > TOTAL_SIZE) {
					printk(KERN_WARNING "%s: out of range access: (%ld, %ld) \n",
						DEV_NAME,offset, size);
					return;
				}
                //bio_data_dir用来获得bio结构描述的大小和传输方向
				if (bio_data_dir(bio) == WRITE) {
					memcpy(myramblock_buf + offset, buffer, size);
				} else {
					memcpy(buffer, myramblock_buf + offset, size);
				}
                //更新偏移量
				pos += size;
				//释放映射
				__bio_kunmap_atomic(buffer);
			}

		}
        //更新已完成处理的请求
        __blk_end_request_all(request, 0);
    }
}
//打开
static int my_blk_open(struct block_device *device, fmode_t mode){
    printk(KERN_INFO "%s: device open.\n",DEV_NAME);
    return 0;
}
//关闭
static void my_blk_release (struct gendisk *gendisk, fmode_t mode){
    printk(KERN_INFO "%s: device released.\n",DEV_NAME);
    return;
}
//block_device_operations 结构体是对块设备操作的集合
static struct block_device_operations b_fops =
{
    .owner = THIS_MODULE,
    .open = my_blk_open,
    .release = my_blk_release
};
MODULE_LICENSE("GPL");

//初始化模块
static int ramvhd_init(void)
{
    //申请内存
    myramblock_buf = vmalloc(TOTAL_SIZE);
    if(!myramblock_buf){
        printk(KERN_WARNING "%s: vmalloc failed!\n",DEV_NAME);
        return -1;
    }
    //设置主设备号
    major = register_blkdev(major,DEV_NAME);
    //major 参数是块设备要使用的主设备号，name为设备名，它会在/proc/devices中被显示。 
    //如果major为0，内核会自动分配一个新的主设备号register_blkdev()函数的返回值就是这个主设备号。
    //如果返回1个负值，表明发生了一个错误。
    if(major<0){
        printk(KERN_WARNING "%s: register failed!\n",DEV_NAME);
        goto freemem;
    }
    //分配gendisk结构体
    myram_gendisk = alloc_disk(1);
    if (!myram_gendisk){
        printk(KERN_WARNING "%s: alloc disk failed!\n",DEV_NAME);
        goto unreg;
    }
    //申请请求队列
    spin_lock_init(&myram_lock);
    my_queue = blk_init_queue(do_my_blk_request, &myram_lock);
    if(!my_queue){
        printk(KERN_WARNING "%s: init queue failed!\n",DEV_NAME);
        goto releasedisk;
    }
    //初始化设备结构体
    myram_gendisk->major = major;
    myram_gendisk->first_minor = 0;
    myram_gendisk->fops = &b_fops;
    strcpy(myram_gendisk->disk_name, DISKNAME);
    myram_gendisk->queue = my_queue;
    //设置容量
    /* note: http://osdir.com/ml/linux.enbd.general/2002-10/msg00177.html */
    set_capacity(myram_gendisk,0);
    //添加磁盘
    add_disk(myram_gendisk);
    //设置容量
    set_capacity(myram_gendisk,TOTAL_SIZE/512);
    printk(KERN_INFO "%s: driver initialized!\n",DEV_NAME);
    return 0;
releasedisk:
    put_disk(myram_gendisk);
unreg:
    unregister_blkdev(major, DEV_NAME);
freemem:
    vfree(myramblock_buf);
    return -1;
}
//退出模块
static void ramvhd_exit(void)
{
    del_gendisk(myram_gendisk);//删除gendisk结构体    
    put_disk(myram_gendisk);//减少gendisk结构体的引用计数
    //注销设备号
    unregister_blkdev(major, DEV_NAME);
    blk_cleanup_queue(my_queue);//清除请求队列
    //删除内存虚拟盘块
    vfree(myramblock_buf);
    printk(KERN_INFO "%s: driver exited!\n",DEV_NAME);
    return;
}
module_init(ramvhd_init);
module_exit(ramvhd_exit);