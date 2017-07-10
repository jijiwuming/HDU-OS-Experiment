#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>

#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/vmalloc.h>
#include <linux/genhd.h>
#include <linux/blkdev.h>
#include <linux/hdreg.h>

MODULE_LICENSE("GPLv2");
// bio 结构体表示正在执行的 I/O 操作相关的信息。
// bio_io_vec ——bio_vec链表表示当前 I/O 操作涉及到的内存页
// bio_vec 结构体表示 I/O 操作使用的片段
// bi_vcnt bi_io_vec链表中bi_vec的个数
// bi_idx 当前的 bi_vec片段，通过 bi_vcnt（总数）和 bi_idx（当前数），就可以跟踪当前 I/O 操作的进度
static int MAJOR_NUMBER = 0;
static int MINOR_NUMBER = 16;
static int SECTOR_SIZE = 512;
static int SECTOR_SHIFT = 9;

struct ramdisk_device {
	unsigned long size;
	spinlock_t lock;
	u8* data;
	struct gendisk* gd;
};

static struct ramdisk_device DEVICE;
static struct request_queue* QUEUE;

static int SECTORS = 2 * 1024 * 1024; /* 1Gb default */
module_param(SECTORS, int, 0);

static void ramdisk_transfer(struct ramdisk_device* dev, unsigned long sector,
                             unsigned long sector_count, char* buffer,
                             int write) {
	unsigned long offset = sector * SECTOR_SIZE;
	unsigned long size = sector_count * SECTOR_SIZE;

	if ((offset + size) > dev->size) {
		printk(KERN_WARNING "ramdisk: out of range access: (%ld, %ld) \n",
		       offset, size);
		return;
	}
	if (write) {
		memcpy(dev->data + offset, buffer, size);
	} else {
		memcpy(buffer, dev->data + offset, size);
	}
}

static void ramdisk_xfer_bio(struct ramdisk_device* dev, struct bio* bio) {
	struct bvec_iter iter;
	struct bio_vec bio_vec;
	sector_t sector = bio->bi_iter.bi_sector;
//遍历bio中的段
	bio_for_each_segment(bio_vec, bio, iter) {
		//映射page为虚拟地址
		char* buffer = __bio_kmap_atomic(bio, iter);
		//当前段页
		unsigned long sector_count = bio_cur_bytes(bio) >> SECTOR_SHIFT;
		ramdisk_transfer(dev, sector, sector_count, buffer,
		                 bio_data_dir(bio) == WRITE);
		sector += sector_count;
		//释放映射
		__bio_kunmap_atomic(buffer);
	}
}

static int ramdisk_xfer_request(struct ramdisk_device* dev,
                                struct request* request) {
	struct bio* bio;
	int sector_count = 0;
	//遍历bio结构体
	__rq_for_each_bio(bio, request) {
		ramdisk_xfer_bio(dev, bio);
		sector_count += bio->bi_iter.bi_size / SECTOR_SIZE;
	}
	return sector_count;
}

static void ramdisk_request(struct request_queue* queue) {
	struct request* request;
	int sectors_xferred;

	while ((request = blk_fetch_request(queue)) != NULL) {
		if (request->cmd_type != REQ_TYPE_FS) {
			printk(KERN_NOTICE "Skip non-fs request\n");
			//更新已完成的数据请求
			__blk_end_request_all(request, -EIO);
			continue;
		}
		sectors_xferred = ramdisk_xfer_request(&DEVICE, request);
		__blk_end_request_all(request, 0);
	}
}
//可以去除
int ramdisk_ioctl(struct block_device* device, fmode_t mode,
                  unsigned int cmd, unsigned long arg) {
	struct hd_geometry geo;

	switch (cmd) {
	case HDIO_GETGEO:
		geo.cylinders = (DEVICE.size & ~0x3f) >> 6;
		geo.heads = 4;
		geo.sectors = 16;
		geo.start = 4;
		if (copy_to_user((void*)arg, &geo, sizeof(geo)) != 0) {
			return -EFAULT;
		}
		return 0;
	default:
		return -ENOTTY;
	}
}

static struct block_device_operations RAMDISK_OPS = {
	.owner = THIS_MODULE,
	.ioctl = ramdisk_ioctl
};

static int __init ramdisk_init(void) {
	DEVICE.size = SECTORS * SECTOR_SIZE;
	//自旋锁初始化
	spin_lock_init(&DEVICE.lock);

	DEVICE.data = vmalloc(DEVICE.size);
	if (!DEVICE.data) {
		printk(KERN_ERR "ramdisk: vmalloc fail.\n");
		return -ENOMEM;
	}

	QUEUE = blk_init_queue(ramdisk_request, &DEVICE.lock);
	if (!QUEUE) {
		printk(KERN_ERR "ramdisk: blk_init_queue failed.\n");
		goto out_deallocate;
	}
	//设置扇区尺寸
	blk_queue_logical_block_size(QUEUE, SECTOR_SIZE);

	MAJOR_NUMBER = register_blkdev(MAJOR_NUMBER, "ramdisk");
	if (MAJOR_NUMBER <= 0) {
		printk(KERN_ERR "ramdisk: register_blkdev failed.\n");
		goto out_deallocate;
	}

	DEVICE.gd = alloc_disk(MINOR_NUMBER);
	if (!DEVICE.gd) {
		printk(KERN_ERR "ramdisk: alloc_disk failed.\n");
		goto out_unregister;
	}

	DEVICE.gd->major = MAJOR_NUMBER;
	DEVICE.gd->first_minor = 0;
	DEVICE.gd->fops = &RAMDISK_OPS;
	DEVICE.gd->queue = QUEUE;
	strcpy(DEVICE.gd->disk_name, "ramdisk");

	/* http://osdir.com/ml/linux.enbd.general/2002-10/msg00177.html */
	set_capacity(DEVICE.gd, 0);
	add_disk(DEVICE.gd);
	set_capacity(DEVICE.gd, SECTORS);

	return 0;

out_unregister:
	unregister_blkdev(MAJOR_NUMBER, "ramdisk");
out_deallocate:
	vfree(DEVICE.data);
	return -ENOMEM;
}

static void __exit ramdisk_exit(void) {
	del_gendisk(DEVICE.gd);
	unregister_blkdev(MAJOR_NUMBER, "ramdisk");
	blk_cleanup_queue(QUEUE);
	vfree(DEVICE.data);
}

module_init(ramdisk_init);
module_exit(ramdisk_exit);
