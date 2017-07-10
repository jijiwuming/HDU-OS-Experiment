#include <stdlib.h>
#include <malloc.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

//虚拟磁盘VD
#define VD_SIZE 1048576//虚拟磁盘总大小 1M
#define VD_BLOCK_SIZE 1024//虚拟磁盘块大小 1024字节
#define VD_BLOCK_NUM 1024//虚拟磁盘总数
/**
* File System Structure | 文件系统结构
*
* BLOCK_INDEX_BOOT_BLOCK	：引导块的首盘块号
* BLOCK_INDEX_FAT1			 : FAT1 的首盘块号
* BLOCK_INDEX_FAT2			 : FAT2 的首盘块号
* BOLCK_INDEX_ROOT_DIR 	     : 根目录文件的盘块号
*
* FAT 文件系统结构
*
* |Start| End |	Name	   |	Note		|
* |-----|-----|------------|----------------|
* |  0  |  0  | Boot Block | 引导区			|
* |-----|-----|------------|----------------|
* |  1  |  2  | FAT1	   | 文件分配表 		|
* |-----|-----|------------|----------------|
* |  3  |  4  | FAT2       | 文件分配表-备份 |
* |-----|-----|------------|----------------|
* |  5  |  5  | ROOT DIR   | 根目录区		|
* |-----|-----|------------|----------------|
* |  6  | 1024| DATA Area  | 数据区			|
*
*/
#define BLOCK_INDEX_BOOT_BLOCK 0
#define BLOCK_INDEX_FAT1 1
#define BLOCK_INDEX_FAT2 3
#define BLOCK_INDEX_ROOT_DIR 5
#define BLOCK_INDEX_DATA_AREA 5
//各个区域盘块数
#define BLOCK_NUM_BOOT_BLOCK 1
#define BLOCK_NUM_FAT 2
#define BLOCK_NUM_ROOT_DIR 2
//FAT表特殊标记 65535(END)为结尾盘块，65534(FREE)为空盘块
#define END 65535
#define FREE 65534
//判断用特殊标记
#define FALSE 0
#define TRUE 1
#define NOT_FOUND -1
//文件attr标志
#define WS_REWRITE 0//重写
#define WS_APPEND 1//追加
#define WR 0
#define R 1
#define HIDE 2
#define ROOTDIR 4
#define DIR 8
#define DATA 16
//最大目录长度
#define DIR_NAME_MAX_LEN 80
//打开文件列表最大长度
#define OFT_MAX_LEN 10
//存档的文件名
#define file_system_name "virtual_file_system.txt"

//星期
const char* weekdays[] = { "星期日","星期一","星期二", "星期三", "星期四", "星期五", "星期六" };
//FAT32-fcb 32字节
typedef struct FCB {
	char filename[8];		  //文件名				 8
	char exname[3];			  //扩展名				 3
	unsigned char attr;		  //属性字节				 1		可用于读写保护和区分 0-读写 1-只读 2-隐藏 4-根目录 8-子目录 16-数据文件 32-归档
	unsigned char use;		  //是否使用				 1      0-未使用 1-使用
	unsigned char wday;		  //创建时星期几			 1		0-6
	unsigned short c_time;    //文件创建时间			 2		对于32位的 Windows、Linux 和 OS X，short 为2个字节,64位环境下也为2个字节
	unsigned short c_date;    //文件创建日期			 2
	unsigned short l_date;    //文件最后访问日期		 2
	unsigned short s_block;   //文件起始盘块号		 2
	unsigned short u_time;    //文件最近修改时间		 2
	unsigned short u_date;    //文件最近修改日期		 2
	unsigned char start_l[2]; //文件起始簇号低16位	 2      暂时无用
	unsigned int  len;		  //文件长度				 4
}fcb;
/**
* File Discriptor Table | 文件描述符表
* @note
*
* @wiki
* 文件描述符、文件描述符表、打开文件表、目录项、索引节点之间的联系
* http://www.cnblogs.com/yanenquan/p/4614805.html
*
*/
typedef struct FDT {
	int  filePtr;//文件读写指针,用于记录读取的位置
	char isUpate;//	是否更新过FCB,TRUE-更新过，FALSE-未更新
	int  pdfBlockNo;//父目录文件所在的盘块号
	int  fdEntryNo;//该文件在父目录文件中对应的FCB序号
	char dirName[DIR_NAME_MAX_LEN];// 路径名称
}fdt;
/*
*  Open File Table | 打开文件表
*  @note
*  	打开文件表包含两部分 ： 用户文件描述表 和 内存FCB表
*
*/
typedef struct OFT
{
	fcb fcb;//文件控制块
	fdt fdt;//文件描述符表
	char isUse;//是否被占用 TRUE-正在被使用   FALSE-OFT空闲
}oft;
//FAT表
typedef struct FAT {
	unsigned short next_num;  //下一盘块号			 2 	-1(END)为结尾盘块，-2(FREE)为空盘块
}fat;
/*
* Boot Block | 引导块
*
* @note
* 	引导块占用第0号物理块，不属于文件系统管辖，
* 	如果系统中有多个文件系统，只有根文件系统才
* 	有引导程序放在引导块中，其余文件系统都不
* 	使用引导块
*
*/
typedef struct BootBlock
{
	char sysInfo[80];//系统信息
	unsigned short rootBlockNo;//根目录文件的盘块号
	unsigned char *dataBlockPtr;//指向数据区首盘块的指针

}bootBlock;


/**
*一些全局变量
*/
void *VDPtr;//虚拟磁盘起始指针
oft OftList[OFT_MAX_LEN];//打开文件列表
int currOftIndex = 0;//当前文件索引
/// <summary>
///    返回指定盘块地址OK
/// </summary>
/// <param name="ptr">起始盘块地址</param>
/// <param name="blocknum">偏移盘块数</param>
/// <returns>
///    void
/// </returns>
void* get_block_ptr(int blocknum) {
	return (unsigned char *)VDPtr+VD_BLOCK_SIZE*blocknum;
}
/// <summary>
///    根据文件FCB判断是否为目录文件OK
/// </summary>
/// <param name="fcbPtr">FCB指针</param>
/// <returns>
///    是，返回 <c>TRUE</c>, 否则返回<c>FALSE</c>.
/// </returns>
int judgeDir(fcb* fcbPtr) {
	if ((fcbPtr->attr >> 4)&(~2))return FALSE;
	return TRUE;
}
/// <summary>
///    判断文件名是否匹配OK
/// </summary>
/// <param name="fcbPtr">fcb指针</param>
/// <param name="fname">文件名</param>
/// <param name="ename">扩展名</param>
/// <returns>
///    成功，返回 <c>true</c>, 否则返回<c>FALSE</c>.
/// </returns>
int fileNameMatch(fcb * fcbPtr, char *fname, char *ename) {
	char strf[9], stre[4];
	strncpy(strf, fcbPtr->filename, 8);
	strncpy(stre, fcbPtr->exname, 3);
	strf[8] = '\0';
	stre[3] = '\0';
	if (strcmp(strf, fname) == 0 && strcmp(stre, ename) == 0) {
		return TRUE;
	}
	else {
		return FALSE;
	}
}
/// <summary>
///    寻找当前打开文件的父目录文件的OFT在OftList中的序号
/// </summary>
/// <param name="iOft">当前打开文件OFT在OftList中的序号</param>
/// <returns>
///    成功，返回 <c>父目录在OftList中的序号</c>, 否则返回<c>NOT_FOUND</c>.
/// </returns>
int findParentOft(int iOft) {
	int i;
	for (i = 0; i < OFT_MAX_LEN; i++) {
		if (OftList[i].fcb.s_block == OftList[iOft].fdt.pdfBlockNo) {
			return i;
		}
	}
	return NOT_FOUND;
}
/// <summary>
///    寻找下一个空闲的盘块
/// </summary>
/// <param name="fatPtr">fat指针</param>
/// <returns>
///    成功，返回 <c>找到的盘块号</c>, 否则返回<c>END</c>.
/// </returns>
unsigned short int getNextVhdBlock(fat * fatPtr) {
	int i;
	for (i = BLOCK_INDEX_DATA_AREA; i < VD_BLOCK_NUM; i++) {
		if (fatPtr[i].next_num == FREE) {
			fatPtr[i].next_num == END;
			return i;
		}
	}
	return END;
}
/// <summary>
///    获取文件夹中下一个空闲的fcb
/// </summary>
/// <param name="fcbPtr">fcb指针</param>
/// <returns>
///    成功，返回 <c>找到的fcb索引值</c>, 否则返回<c>NOT_FOUND</c>.
/// </returns>
int getNextFcb(fcb *fcbPtr) {
	int fcbNum = (int)(VD_BLOCK_SIZE / sizeof(fcb));
	for (int i = 2; i < fcbNum; i++) {
		if (fcbPtr[i].use == FALSE) {
			return i;
		}
	}
	return NOT_FOUND;
}
/// <summary>
///    获取下一个空闲的打开列表
/// </summary>
/// <returns>
///    成功，返回 <c>空闲的索引值</c>, 否则返回<c>NOT_FOUND</c>.
/// </returns>
int getNextOft() {
	int i;
	for (i = 0; i < OFT_MAX_LEN; i++) {
		if (OftList[i].isUse == FALSE) {
			OftList[i].isUse = TRUE;
			return i;
		}
	}
	return NOT_FOUND;
}
/// <summary>
///    保存文件系统到磁盘（确保文件存在）OK
/// </summary>
/// <returns>
///    成功，返回 <c>void</c>, 否则返回<c>异常退出</c>.
/// </returns>
void saveVdFile() {
	FILE *filePtr = fopen(file_system_name, "w");
	if (filePtr == NULL) {
		printf("虚拟磁盘文件不存在！/n");
		exit(1);
	}
	fwrite(VDPtr, VD_SIZE, 1, filePtr);
	fclose(filePtr);
}
/// <summary>
///    初始化FCB 的时间OK
/// </summary>
/// <param name="fcb">fcb指针</param>
/// <returns>
///    void
/// </returns>
void initFcbTime(fcb *fcb) {
	time_t seconds = time((time_t*)NULL);
	struct tm *time = localtime(&seconds);
	/*时间 是unsigned short int类型的数据, 占2字节16位
	* 所以完整的表示一个时间的秒数是不够长的,所以,保存秒数的一半
	* 这样小时占5位（0-23）,分钟占6位（0-59）,秒占5位（0-60）
	*/
	unsigned short currentTime = time->tm_hour * 2048 + time->tm_min * 32 + time->tm_sec / 2;	
	/**
	* 年份我们保存的是实际值-1900, time->tm_mon要+1是因为,这个结构设计的时候0代表1月
	* 年份占7位,月份占4位,日期占5位
	*/
	unsigned short currentDate = (time->tm_year) * 512 + (time->tm_mon + 1) * 32 + (time->tm_mday);
	fcb->c_time = currentTime;
	fcb->c_date = currentDate;
	fcb->wday = time->tm_wday;
	fcb->l_date = currentDate;
	fcb->u_time = currentTime;
	fcb->u_date = currentDate;
}
/// <summary>
///    更新文件最后访问日期
/// </summary>
/// <param name="fcb">fcb指针</param>
/// <returns>
///    void
/// </returns>
void viewFcbTime(fcb *fcb) {
	time_t seconds = time((time_t*)NULL);
	struct tm *time = localtime(&seconds);
	unsigned short currentDate = (time->tm_year) * 512 + (time->tm_mon + 1) * 32 + (time->tm_mday);
	fcb->l_date = currentDate;
}
/// <summary>
///    修改文件后更新时间
/// </summary>
/// <param name="fcb">fcb指针</param>
/// <returns>
///    void
/// </returns>
void updateFcbTime(fcb *fcb) {
	time_t seconds = time((time_t*)NULL);
	struct tm *time = localtime(&seconds);
	unsigned short currentTime = time->tm_hour * 2048 + time->tm_min * 32 + time->tm_sec / 2;
	unsigned short currentDate = (time->tm_year) * 512 + (time->tm_mon + 1) * 32 + (time->tm_mday);
	fcb->l_date = currentDate;
	fcb->u_time = currentTime;
	fcb->u_date = currentDate;
}
/// <summary>
///    初始化FCB
/// </summary>
/// <param name="fcbPtr">fcb指针</param>
/// <param name="fname">文件名</param>
/// <param name="ename">拓展名</param>
/// <param name="use">是否已使用</param>
/// <returns>
///    void
/// </returns>
void initFcb(fcb *fcbPtr, char *fname, char *ename, unsigned char use) {
	memset(fcbPtr, 0, sizeof(fcb));
	fcbPtr->len = 0;
	strcpy(fcbPtr->filename, fname);
	strcpy(fcbPtr->exname, ename);
	fcbPtr->attr = 0;
	fcbPtr->use = use;
	initFcbTime(fcbPtr);
}
/// <summary>
///    初始化文件描述符表
/// </summary>
/// <param name="fdtPtr">文件描述符表指针</param>
/// <returns>
///    修改后的fdtPtr
/// </returns>
fdt* initFdt(fdt *fdtPtr) {
	fdtPtr->filePtr = 0;
	fdtPtr->isUpate = FALSE;
	return fdtPtr;
}
/// <summary>
///    初始化打开文件
/// </summary>
/// <param name="oftPtr">打开文件指针</param>
/// <returns>
///    修改后的oftPtr
/// </returns>
oft* initOft(oft* oftPtr) {
	initFcb(&(oftPtr->fcb), "", "", FALSE);
	initFdt(&(oftPtr->fdt));
	oftPtr->isUse = FALSE;
	return oftPtr;
}
/// <summary>
///    初始化文件/文件夹FCB OK
/// </summary>
/// <param name="parentBlock">父目录盘块号</param>
/// <param name="offset">偏移量</param>
/// <param name="blockNo">当前文件分配的盘块号</param>
/// <param name="fname">文件名</param>
/// <param name="ename">拓展名</param>
/// <param name="isdir">是否文件夹</param>
/// <returns>
///    成功，返回 <c>0</c>, 否则返回<c>其他</c>.
/// </returns>
int initFileFCB(int parentBlock, int offset, int blockNo, char* fname, char* ename, unsigned char isdir) {
	fcb *parentBlockPtr = (fcb *)get_block_ptr(parentBlock);
	fcb* fcbPtr = parentBlockPtr + offset;
	// 创建父目录中的fcb
	initFcb(fcbPtr, fname, ename, 1);
	parentBlockPtr->len += sizeof(fcb);
	fcbPtr->s_block = blockNo;
	fcbPtr->len = 0;
	initFcbTime(fcbPtr);
	if (isdir == FALSE) {
		fcbPtr->attr += DATA;
	}
	else
	{
		unsigned char attr = 0;
		if (blockNo == parentBlock) {
			//根目录
			attr = ROOTDIR;
		}
		else {
			attr = DIR;
		}
		fcbPtr->attr += attr;
		fcb *childBlockPtr = (fcb *)get_block_ptr(blockNo);
		// 创建当前文件入口 "."
		initFcb(childBlockPtr, ".", "di", TRUE);
		childBlockPtr->attr = attr;
		childBlockPtr->s_block = blockNo;
		childBlockPtr->len = 0;
		initFcbTime(childBlockPtr);
		// 创建父文件入口 ".."
		childBlockPtr++;
		initFcb(childBlockPtr, "..", "di", TRUE);
		childBlockPtr->attr = attr;
		childBlockPtr->s_block = parentBlock;
		childBlockPtr->len = 0;
		initFcbTime(childBlockPtr);
		//标记其他空间为未使用
		int i;
		for (i = 2; i < (VD_BLOCK_SIZE / sizeof(fcb)); i++) {
			childBlockPtr++;
			childBlockPtr->use = FALSE;
		}
	}
	return 0;
}
/// <summary>
///    写入预处理函数
/// </summary>
/// <param name="oftPtr">要处理的文件的oft指针</param>
/// <param name="wstyle">写入方式</param>
/// <returns>
///    void
/// </returns>
void writePreProcess(oft *oftPtr, char wstyle) {
	//WS_REWRITE 0	WS_APPEND 1
	fcb* parentPtr = (fcb*)get_block_ptr(oftPtr->fdt.pdfBlockNo);
	fcb* currentPtr = parentPtr + oftPtr->fdt.fdEntryNo;
	switch (wstyle) {
	case WS_REWRITE:
		oftPtr->fdt.filePtr = 0;
		oftPtr->fcb.len = 0;
		currentPtr->len = 0;
		break;
	case WS_APPEND:
		/*if (judgeDir(&(oftPtr->fcb)) == FALSE && oftPtr->fcb.len != 0) {*/
			//删除数据文件末尾的‘\0’
			oftPtr->fdt.filePtr = oftPtr->fcb.len;
		/*}*/
		break;
	}
}
/// <summary>
///    从当前打开的数据文件位置写入指定数据
/// </summary>
/// <param name="oftPtr">当前打开文件指针</param>
/// <param name="text">写入内容</param>
/// <param name="len">写入长度</param>
/// <returns>
///    成功，返回 <c>写入长度</c>, 否则返回<c>EXIT_FAILURE</c>.
/// </returns>
int do_write(oft *oftPtr, char *text, int len) {
	int blockNo = oftPtr->fcb.s_block;
	int block_back = blockNo;
	fat *fat1 = (fat *)get_block_ptr(BLOCK_INDEX_FAT1);//获取fat1表位置
	//偏移量
	int offset = oftPtr->fdt.filePtr;
	// 移动fat指针，找到要写入的块
	while (offset > (VD_BLOCK_SIZE-1)) {
		blockNo = fat1[blockNo].next_num;
		if (blockNo == END) {
			printf("读写指针异常\n");
			return EXIT_FAILURE;
		}
		block_back = blockNo;
		offset -= VD_BLOCK_SIZE;
	}
	// 写入数据
	int lenTmp = 0;
	unsigned char *blockPtr = (unsigned char *)get_block_ptr(blockNo);
	while (len > lenTmp) {
		//在修改位置开始写入
		while (len > lenTmp && offset < VD_BLOCK_SIZE) {
			*(blockPtr + offset) = *(text);
			offset++;
			lenTmp++;
			text++;
		}
		// 当前块写满了，写入内容还没完
		if (offset == VD_BLOCK_SIZE && len >= lenTmp) {
			// 进入下一个盘块
			offset = 0;
			block_back = blockNo;
			blockNo = fat1[block_back].next_num;
			// 不存在则开辟一个新的盘块
			if (blockNo == END) {
				blockNo = getNextVhdBlock(fat1);
				if (blockNo == END) {
					printf("没有多余的磁盘块了\n");
					return EXIT_FAILURE;
				}
				blockPtr = (unsigned char *)get_block_ptr(blockNo);
				fat1[block_back].next_num = blockNo;
				fat1[blockNo].next_num = END;
			}
			else {
				blockPtr = (unsigned char *)get_block_ptr(blockNo);
			}
		}
	}
	//若为重写且长度不足则清空之后盘块
	while (fat1[blockNo].next_num != END) {
		unsigned short next = fat1[blockNo].next_num;
		fat1[blockNo].next_num = END;
		blockNo = next;
	}
	// 同步 FAT2
	fat * fat2 = (fat *)get_block_ptr(BLOCK_INDEX_FAT2);
	memcpy(fat2, fat1, VD_BLOCK_SIZE * 2);
	//修改文件FCB
	// 文件读写指针比fcb长度长，修改长度
	oftPtr->fdt.filePtr += len;
	oftPtr->fcb.len += len;
	updateFcbTime(&(oftPtr->fcb));
	//修改磁盘上的FCB
	fcb* parentPtr = (fcb*)get_block_ptr(oftPtr->fdt.pdfBlockNo);
	fcb* currentPtr = parentPtr + oftPtr->fdt.fdEntryNo;
	currentPtr->len += len;
	updateFcbTime(currentPtr);
	return len;
}
/// <summary>
///    获取当前打开目录下的指定文件名的文件
/// </summary>
/// <param name="oftPtr">当前打开目录</param>
/// <param name="fname">文件名</param>
/// <param name="ename">拓展名</param>
/// <param name="isdir">是否为目录文件，是-1，否-0</param>
/// <returns>
///    成功，返回 <c>索引值</c>, 否则返回<c>NOT_FOUND</c>.
/// </returns>
int getIndexOfFcb(oft *oftPtr, char *fname, char *ename, unsigned char isdir) {
	int i;
	int fcbNum = (int)(oftPtr->fcb.len / sizeof(fcb));
	fcb *fcbPtr = (fcb *)get_block_ptr(oftPtr->fcb.s_block);
	for (i = 2; i < fcbNum+2; i++) {
		if (fcbPtr[i].use == FALSE) {
			fcbNum++;
			continue;
		}
		if (judgeDir(&fcbPtr[i]) == isdir && fileNameMatch(&fcbPtr[i], fname, ename)) {
			return i;
		}
	}
	return NOT_FOUND;
}
/// <summary>
///    打开当前目录下的文件
/// </summary>
/// <param name="piOft">父目录文件的oft索引</param>
/// <param name="filename">文件名</param>
/// <param name="isdir">是否为目录文件，是-1，否-0</param>
/// <returns>
///    成功，返回 <c>新建的oft索引</c>, 否则返回<c>-1</c>.
/// </returns>
int initOftByFileName(int piOft, char *filename,unsigned char isdir) {
	char filenameBack[13];
	strncpy(filenameBack, filename, 12);
	//分解文件名
	char *fname = strtok(filename, ".");
	char *ename = strtok(NULL, ".");
	//父目录指针
	oft *pOftPtr = OftList + piOft;
	if (pOftPtr == NULL) {
		printf("父目录不存在\n");
		return -1;
	}
	if (isdir == TRUE)ename = "di";
	// 在文件目录下找到指定的fcb的索引值
	int indexOfFcb = getIndexOfFcb(pOftPtr, fname, ename, isdir);
	if (indexOfFcb == NOT_FOUND) {
		printf("没有这样的文件存在\n");
		return -1;
	}
	fcb* parentPtr = (fcb *)get_block_ptr(pOftPtr->fcb.s_block);
	fcb* fcbPtr = (fcb *)parentPtr + indexOfFcb;
	//申请oft空间
	int indexOfOft = getNextOft();
	if (indexOfOft == NOT_FOUND) {
		printf("打开的文件列表已满\n");
		return -1;
	}
	//写oft
	oft *cOftPtr = OftList + indexOfOft;
	initOft(cOftPtr);
	cOftPtr->isUse = TRUE;
	//写fcb
	memcpy(&(cOftPtr->fcb), fcbPtr, sizeof(fcb));
	//写fdt
	cOftPtr->fdt.fdEntryNo = indexOfFcb;
	cOftPtr->fdt.pdfBlockNo = pOftPtr->fcb.s_block;
	cOftPtr->fdt.isUpate = FALSE;
	if (judgeDir(fcbPtr) == FALSE) {
		strcpy(cOftPtr->fdt.dirName, pOftPtr->fdt.dirName);
		strcat(cOftPtr->fdt.dirName, filenameBack);
	}
	else {
		strcpy(cOftPtr->fdt.dirName, pOftPtr->fdt.dirName);
		strcat(cOftPtr->fdt.dirName, filenameBack);
		strcat(cOftPtr->fdt.dirName, "\\");
	}
	return indexOfOft;
}
/// <summary>
///    创建文件/文件夹OK
/// </summary>
/// <param name="filename">完整文件名</param>
/// <param name="isdir">是否为文件夹</param>
/// <returns>
///    成功，返回 <c>0</c>, 否则返回<c>EXIT_FAILURE</c>.
/// </returns>
int createfile(char *filename, unsigned char isdir) {
	char *fname = strtok(filename, ".");
	char *ename = strtok(NULL, ".");
	oft *oftPtr = OftList + currOftIndex;
	if (judgeDir(&(oftPtr->fcb)) == FALSE) {
		printf("无法在数据文件下执行创建操作\n");
		return EXIT_FAILURE;
	}
	//判断处理
	if (isdir == TRUE&&ename) {
		printf("目录文件不能有文件拓展名\n");
		return EXIT_FAILURE;
	}
	else if (isdir == FALSE && (!ename)) {
		printf("数据文件的拓展名不能为空\n");
		return EXIT_FAILURE;
	}
	else if (isdir == TRUE) {
		ename = "di";
	}
	// 加载文件夹数据到缓冲区
	if (judgeDir(&(oftPtr->fcb)) == FALSE) {
		printf("当前打开的为数据文件，无法进行该操作\n");
		return EXIT_FAILURE;
	}
	oftPtr->fdt.filePtr = 0;
	//重名查找
	int fcbNum = (int)(oftPtr->fcb.len / sizeof(fcb));
	fcb *fcbPtr = (fcb *)get_block_ptr(oftPtr->fcb.s_block);
	for (int i = 0; i < fcbNum&&i<VD_BLOCK_SIZE / sizeof(fcb); i++) {
		if (isdir==TRUE&&fileNameMatch(&fcbPtr[i + 2], fname, "di") &&
			judgeDir(&fcbPtr[i + 2]) == isdir) {
			printf("该目录已存在\n");
			return EXIT_FAILURE;
		}
		if (isdir == FALSE&&fileNameMatch(&fcbPtr[i + 2], fname, ename) &&
			judgeDir(&fcbPtr[i + 2]) == isdir) {
			printf("该数据文件已存在\n");
			return EXIT_FAILURE;
		}
		if (fcbPtr[i + 2].use == FALSE) {
			fcbNum++;
		}
	}
	// 在父目录下寻找空闲的FCB
	int offset=getNextFcb(fcbPtr);
	if (offset == NOT_FOUND) {
		printf("当前目录已满\n");
		return EXIT_FAILURE;
	}
	// 申请存储的盘块
	int blockNo;
	fat *fat1 = (fat *)get_block_ptr(BLOCK_INDEX_FAT1);
	blockNo = getNextVhdBlock(fat1);
	if (blockNo == END) {
		printf("没有空闲存储块\n");
		return EXIT_FAILURE;
	}
	//分配盘块并备份fat表
	fat1[blockNo].next_num = END;
	fat * fat2 = (fat *)get_block_ptr(BLOCK_INDEX_FAT2);
	memcpy(fat2, fat1, VD_BLOCK_SIZE * 2);

	//创建初始文件fcb内容
	initFileFCB(oftPtr->fcb.s_block, offset, blockNo, fname, ename, isdir);

	//更新磁盘上父目录长度和打开的父目录长度
	//父目录磁盘上fcb
	if (oftPtr->fdt.pdfBlockNo != oftPtr->fcb.s_block) {
		fcb* parentPtr = ((fcb*)get_block_ptr(oftPtr->fdt.pdfBlockNo)) + oftPtr->fdt.fdEntryNo;
		parentPtr->len += sizeof(fcb);
	}
	oftPtr->fcb.len += sizeof(fcb);

	return 0;
}
/// <summary>
///    输出文件信息(文件名 类型 大小 创建日期 星期 创建时间 修改日期 修改时间 最后访问日期)
/// </summary>
/// <param name="fcbPtr">文件fcb指针</param>
/// <returns>
///    void
/// </returns>
void printFile(fcb *fcbPtr) {
	printf("%.8s", fcbPtr->filename);
	unsigned int length = fcbPtr->len;
	if (judgeDir(fcbPtr)) {
		printf("\\\t<DIR>");
	}
	else {
		printf(".%.3s\t", fcbPtr->exname);
		if (fcbPtr->attr % 2 == 1) {
			//只读
			printf("D_R");
		}
		else {
			printf("D_WR");
		}
		//if(length!=0)length -= 1;//因为末尾有/0字符
	}
	printf("\t%dB\t%04d/%02d/%02d%s %02d:%02d:%02d\t%04d/%02d/%02d %02d:%02d:%02d\t%04d/%02d/%02d\n",
		length,
		(fcbPtr->c_date >> 9) + 1900,
		(fcbPtr->c_date >> 5) % 16,
		(fcbPtr->c_date) % 32,
		weekdays[fcbPtr->wday],
		(fcbPtr->c_time >> 11),
		(fcbPtr->c_time >> 5) % 64,
		(fcbPtr->c_time) % 32 * 2,
		(fcbPtr->u_date >> 9) + 1900,
		(fcbPtr->u_date >> 5) % 16,
		(fcbPtr->u_date) % 32,
		(fcbPtr->u_time >> 11),
		(fcbPtr->u_time >> 5) % 64,
		(fcbPtr->u_time) % 32 * 2,
		(fcbPtr->l_date >> 9) + 1900,
		(fcbPtr->l_date >> 5) % 16,
		(fcbPtr->l_date) % 32);
}
/// <summary>
///    删除fat上指定的盘块号，使用后请备份到fat2
/// </summary>
/// <param name="blockNo">文件的起始盘块号</param>
/// <returns>
///    void
/// </returns>
void deleteFileAtFat(int blockNo) {
	fat * fat1 = (fat *)get_block_ptr(BLOCK_INDEX_FAT1);
	int current = blockNo;
	int next;
	while (TRUE) {
		next = fat1[current].next_num;
		fat1[current].next_num = FREE;
		if (next != END) {
			current = next;
		}
		else {
			fat1[next].next_num = FREE;
			break;
		}
	}
}
/// <summary>
///    删除fat上指定文件夹及其子文件的盘块号，使用后请备份到fat2
/// </summary>
/// <param name="fcbPtr">文件的fcb指针</param>
/// <returns>
///    void
/// </returns>
void deleteDirAtFat(fcb *fcbPtr) {
	fcb *curPtr = fcbPtr;
	fcb* childPtr = (fcb*)get_block_ptr(fcbPtr->s_block);
	int fnum = fcbPtr->len / sizeof(fcb);
	for(int i=2;i<fnum+2;i++)
	{//文件夹内有内容一并删除
		if (childPtr[i].use == FALSE) {
			fnum++;
			continue;
		}
		if (judgeDir(&childPtr[i])) {
			deleteDirAtFat(&childPtr[i]);
		}
		else
		{
			deleteFileAtFat(childPtr[i].s_block);
		}
	}
	fat * fat1 = (fat *)get_block_ptr(BLOCK_INDEX_FAT1);
	fat1[fcbPtr->s_block].next_num = FREE;
}
/// <summary>
///    在文件夹下找文件
/// </summary>
/// <param name="fcbPtr">文件夹起始盘块fcb指针</param>
/// <param name="len">文件夹fcb长度</param>
/// <param name="fname">文件名</param>
/// <param name="ename">拓展名</param>
/// <param name="isdir">是否为文件夹</param>
/// <returns>
///    成功，返回 <c>寻找的文件fcb所在位置</c>, 否则返回<c>-1</c>.
/// </returns>
int findFileAtDir(fcb *fcbPtr,int len,char* fname,char* ename,unsigned char isdir) {
	int i;
	int fcbNum = (int)(len / sizeof(fcb));
	//查找对应匹配文件fcb的位置
	for (i = 2; i < fcbNum + 2; i++) {
		if (fileNameMatch(&(fcbPtr[i]), fname, ename) && judgeDir(&fcbPtr[i])==isdir) {
			break;
		}
		if (fcbPtr[i].use == FALSE) {
			fcbNum++;
		}
	}
	if (i == fcbNum + 2) {
		if (isdir) {
			printf("没有找到对应文件夹\n");
		}
		else
		{
			printf("没有找到对应数据文件\n");
		}
		return -1;
	}
	return i;
}
/// <summary>
///    删除文件
/// </summary>
/// <param name="filename">文件全名</param>
/// <returns>
///    成功，返回 <c>0</c>, 否则返回<c>EXIT_FAILURE</c>.
/// </returns>
int deleteFile(char *filename, unsigned char isdir) {
	char *fname = strtok(filename, ".");
	char *ename = strtok(NULL, ".");
	//预处理和判断
	if (isdir == TRUE) {
		if (strcmp(filename, ".") == 0 || strcmp(filename, "..") == 0) {
			printf("无法删除已打开的目录，请先关闭目录再试\n");
			return EXIT_FAILURE;
		}
		if (ename) {
			printf("目录文件不能带拓展名\n");
			return EXIT_FAILURE;
		}
		ename = "di";
	}
	if (isdir == FALSE && !ename) {
		printf("请输入文件后缀\n");
		return EXIT_FAILURE;
	}
	//读入当前目录
	oft *oftPtr = OftList + currOftIndex;
	oftPtr->fdt.filePtr = 0;
	char *blockPtr = (char *)get_block_ptr(oftPtr->fcb.s_block);
	if (judgeDir(&(oftPtr->fcb)) == FALSE) {
		printf("当前打开文件为数据文件,无法继续操作\n");
		return EXIT_FAILURE;
	}
	//遍历查找
	int i = findFileAtDir((fcb *)blockPtr, oftPtr->fcb.len, fname, ename, isdir);
	if (i == -1) {
		return EXIT_FAILURE;
	}
	if (isdir)deleteDirAtFat(&((fcb *)blockPtr)[i]);
	else deleteFileAtFat(((fcb *)blockPtr)[i].s_block);


	//更新fat2
	fat * fat1 = (fat *)get_block_ptr(BLOCK_INDEX_FAT1);
	fat * fat2 = (fat *)get_block_ptr(BLOCK_INDEX_FAT2);
	memcpy(fat2, fat1, VD_BLOCK_SIZE * 2);
	//更新fcb
	//清空父目录fcb内容及长度改写
	initFcb(&((fcb *)blockPtr)[i], "", "", FALSE);
	oftPtr->fcb.len -= sizeof(fcb);
	fcb* parentPtr = (fcb *)get_block_ptr(oftPtr->fdt.pdfBlockNo);
	fcb* currentPtr = parentPtr + oftPtr->fdt.fdEntryNo;
	currentPtr->len -= sizeof(fcb);
	return 0;
}
//命令函数
/// <summary>
///    设置读写控制和隐藏
/// </summary>
/// <param name="filename">文件名</param>
/// <param name="i">控制参数</param>
/// <returns>
///    成功，返回 <c>true</c>, 否则返回<c>true</c>.
/// </returns>
int my_ctrl(char *filename,int i) {
	if (i < 0 || i>3) {
		printf("设置参数非法\n");
		return -1;
	}
	//分解文件名
	char *fname = strtok(filename, ".");
	char *ename = strtok(NULL, ".");
	unsigned char isdir = FALSE;
	//预处理
	if (!ename) {
		if (i != 2) {
			printf("只能更改数据文件读写权限\n");
			return -1;
		}
		else {
			ename = "di";
			isdir = TRUE;
		}
	}
	oft* pOftPtr = OftList + currOftIndex;
	// 在文件目录下找到指定的fcb的索引值
	int indexOfFcb = getIndexOfFcb(pOftPtr, fname, ename, isdir);
	if (indexOfFcb == NOT_FOUND) {
		printf("没有这样的文件存在\n");
		return -1;
	}
	fcb* parentPtr = (fcb *)get_block_ptr(pOftPtr->fcb.s_block);
	fcb* fcbPtr = (fcb *)parentPtr + indexOfFcb;
	switch (i)
	{
	case 0:
		if (fcbPtr->attr % 2 == 1)
			fcbPtr->attr -= 1;
		break;
	case 1:
		if (fcbPtr->attr % 2 == 0)
			fcbPtr->attr += 1;
		break;
	case 2:
		if ((fcbPtr->attr /2) % 2 == 0)
			fcbPtr->attr += 2;
		break;
	case 3:
		if ((fcbPtr->attr / 2) % 2 == 1)
			fcbPtr->attr -= 2;
		break;
	default:
		break;
	}
	return 0;
}
/// <summary>
///    格式化存储器
/// </summary>
/// <returns>
///    成功，返回 <c>0</c>, 否则返回<c>非0</c>.
/// </returns>
int my_format() {
	//创建boot区
	bootBlock* bootp = (bootBlock*)get_block_ptr(BLOCK_INDEX_BOOT_BLOCK);
	strcpy(bootp->sysInfo,"This is jijiwuming's Virtual File System!");
	bootp->rootBlockNo = BLOCK_INDEX_ROOT_DIR;
	bootp->dataBlockPtr = (unsigned char *)get_block_ptr(BLOCK_INDEX_DATA_AREA);
	//创建fat1分区
	fat *fat1 = (fat *)get_block_ptr(BLOCK_INDEX_FAT1);
	int i;
	for (i = 0; i < BLOCK_INDEX_DATA_AREA; i++) {
		fat1[i].next_num = END;
	}
	for (i = BLOCK_INDEX_DATA_AREA; i < VD_BLOCK_NUM; i++) {
		fat1[i].next_num = FREE;
	}
	fat1[BLOCK_INDEX_ROOT_DIR].next_num = END;
	//创建fat2分区
	fat * fat2 = (fat *)get_block_ptr(BLOCK_INDEX_FAT2);
	memcpy(fat2, fat1, VD_BLOCK_SIZE * 2);
	//初始化根目录
	initFileFCB( BLOCK_INDEX_ROOT_DIR,0,BLOCK_INDEX_ROOT_DIR,"\\root\\","di",TRUE);
	//保存格式化后产物
	saveVdFile();

	//初始化根目录打开文件表
	for (int i = 0;i<OFT_MAX_LEN; i++) {
		initOft(&OftList[i]);
	}
	fcb * fcbPtr = (fcb *)get_block_ptr(BLOCK_INDEX_ROOT_DIR);
	memcpy(&(OftList->fcb), fcbPtr, sizeof(fcb));
	strcpy(OftList->fdt.dirName, "\\root\\");
	OftList->fdt.pdfBlockNo = BLOCK_INDEX_ROOT_DIR;  //5
	OftList->fdt.fdEntryNo = 0;					     //Root Dir 的父目录文件就是自己 目录项为首个FCB .
	OftList->fdt.isUpate = FALSE;
	OftList->fdt.filePtr = 0;
	OftList->isUse = TRUE;
	currOftIndex = 0;
	return 0;
}
/// <summary>
///    显示当前目录文件
/// </summary>
/// <param name="isall">是否显示所有文件</param>
/// <returns>
///    成功，返回 <c>0</c>, 否则返回<c>EXIT_FAILURE</c>.
/// </returns>
int my_ls(unsigned char isall) {
	oft *oftPtr = OftList + currOftIndex;
	if (judgeDir(&oftPtr->fcb) == FALSE) {
		printf("当前为数据文件，无法ls\n");
		return EXIT_FAILURE;
	}
	oftPtr->fdt.filePtr = 0;
	fcb *fcbPtr = (fcb *)get_block_ptr(oftPtr->fcb.s_block);
	int i,flag=0;
	int fcbNum = (int)(oftPtr->fcb.len / sizeof(fcb));
	//遍历fcb
	for (i = 2; i < (fcbNum+2)&&i<(VD_BLOCK_SIZE / sizeof(fcb)); i++) {
		if (fcbPtr[i].use == 1) {
			if ((fcbPtr[i].attr / 2) % 2 != 1 || isall) {
				if (flag == 0) {
					printf("文件名\t类型\t大小\t创建时间                 \t修改时间           \t最后访问日期\n");
					flag++;
				}
				printFile(&fcbPtr[i]);
			}
		}
		else {
			fcbNum++;
		}
	}
	return 0;
}
/// <summary>
///    删除文件夹//TODO:fix bug缓冲区？
/// </summary>
/// <param name="dirname">文件夹名</param>
/// <returns>
///    成功，返回 <c>0</c>, 否则返回<c>EXIT_FAILURE</c>.
/// </returns>
int my_rmdir(char *dirname) {
	deleteFile(dirname, TRUE);
	return 0;
}
/// <summary>
///    删除文件
/// </summary>
/// <param name="filename">文件名</param>
/// <returns>
///    成功，返回 <c>0</c>, 否则返回<c>EXIT_FAILURE</c>.
/// </returns>
int my_rm(char *filename) {
	deleteFile(filename, FALSE);
	return 0;
}
/// <summary>
///    读取文件
/// </summary>
/// <param name="iOft">打开的文件索引</param>
/// <returns>
///    成功，返回 <c>0</c>, 否则返回<c>EXIT_FAILURE</c>.
/// </returns>
int my_read(int iOft) {
	if (iOft > OFT_MAX_LEN || iOft < 0) {
		printf("不存在这样的打开文件\n");
		return EXIT_FAILURE;
	}
	oft *oftPtr = OftList + iOft;
	//从头读取
	OftList[iOft].fdt.filePtr = 0;
	int blockNo = oftPtr->fcb.s_block; // 起始盘块号
	char *tmpPtr = (char *)get_block_ptr(blockNo);
	int len = oftPtr->fcb.len;
	fat* fat1 = (fat *)get_block_ptr(BLOCK_INDEX_FAT1);
	while (len > 0) {
		if (len >= VD_BLOCK_SIZE) {
			//超过一个磁盘块大小
			if (fat1[blockNo].next_num == END) {
				printf("无法读取内容\n");
				return EXIT_FAILURE;
			}
			//读取内容
			for (int i = 0; i < VD_BLOCK_SIZE; i++, tmpPtr++)putchar(*tmpPtr);
			len -= VD_BLOCK_SIZE;
			blockNo = fat1[blockNo].next_num;
			tmpPtr= (char *)get_block_ptr(blockNo);
		}
		else {
			//剩余不足一个盘块大小
			while (len > 0 && (*tmpPtr)!='\0') {
				putchar(*tmpPtr);
				tmpPtr++;
				len--;
			}
			break;
		}
	}
	return 0;
}
/// <summary>
///    写入文件
/// </summary>
/// <param name="iOft">文件在打开列表中的索引值</param>
/// <returns>
///    成功，返回 <c>0</c>, 否则返回<c>EXIT_FAILURE</c>.
/// </returns>
int my_write(int iOft) {
	if (iOft > OFT_MAX_LEN || iOft < 0) {
		printf("不存在这样的打开文件\n");
		return EXIT_FAILURE;
	}
	oft *oftPtr = OftList + iOft;
	if (oftPtr->fcb.attr %2 == TRUE) {
		printf("只读的文件无法写入\n");
		return EXIT_FAILURE;
	}
	int wstyle;
	printf("请选择打开方式：0——重写，1——追加\n");
	scanf("%d", &wstyle);
	if (wstyle > 1 || wstyle < 0) {
		printf("错误的写入方式\n");
		return EXIT_FAILURE;
	}
	// 预处理写入
	writePreProcess(oftPtr, wstyle);
	printf("请输入内容\n");
	//清空输入流
	int c;
	while ((c = getchar()) != '\n' && c != EOF);
	//读取输入
	char textTmp[VD_BLOCK_SIZE+1] = "\0"; //缓冲区
	int len = 0;
	char tmp[2]="\0";
	while (1) {
		//结束输入标记
		if ((tmp[0] = getchar()) == '#'&&(tmp[1] = getchar()) == '&') {
			strcat(textTmp, "\0");
			do_write(oftPtr, textTmp, strlen(textTmp));
			break;
		}
		//超过一个磁盘块大小动态写入
		if (len >= VD_BLOCK_SIZE) {
			do_write(oftPtr, textTmp, VD_BLOCK_SIZE);
			textTmp[0] = '\0';
			len -= VD_BLOCK_SIZE;
		}
		strcat(textTmp,tmp);
		len += 1;
	}
	//清空输入流
	while ((c = getchar()) != '\n' && c != EOF);
	//设置更新位
	oftPtr->fdt.isUpate = TRUE;
	return 0;
}
/// <summary>
///    打开当前目录下数据文件
/// </summary>
/// <param name="filename">文件全名</param>
/// <returns>
///    成功，返回 <c>0</c>, 否则返回<c>EXIT_FAILURE</c>.
/// </returns>
int my_open(char *filename) {
	char *name = filename;
	char filenameBack[13];
	strncpy(filenameBack, filename,12);
	//分解文件名
	char *fname = strtok(filenameBack, ".");
	char *ename = strtok(NULL, ".");
	if (!ename) {
		printf("只能打开数据文件\n");
		return EXIT_FAILURE;
	}
	int i = initOftByFileName(currOftIndex, filename, FALSE);
	if (i == -1) {
		return EXIT_FAILURE;
	}
	else {
		currOftIndex = i;
		return 0;
	}
}
/// <summary>
///    关闭文件,返回父目录的索引值并重置currOftIndex为父目录的索引值
/// </summary>
/// <param name="iOft">要关闭的文件索引位置</param>
/// <returns>
///    成功，返回 <c>父目录的索引值</c>, 否则返回<c>NOT_FOUND/EXIT_FAILURE</c>.
/// </returns>
int my_close(int iOft) {
	if (iOft > OFT_MAX_LEN || iOft < 0) {
		printf("不存在这样的已打开文件\n");
		return EXIT_FAILURE;//1		定义在stdlib.h中
	}
	else {
		// 获取父目录在打开文件列表中的位置
		int piOft = findParentOft(iOft);
		if (piOft == NOT_FOUND) {
			printf("父目录不存在\n");
			return NOT_FOUND;
		}
		//如果文件有更新内容，则写回
		if (OftList[iOft].fdt.isUpate == TRUE) {
			// 找到父目录
			fcb* parentPtr = (fcb *)get_block_ptr(OftList[piOft].fcb.s_block);
			// 在父目录找到子文件入口
			fcb *fcbPtr = &parentPtr[OftList[iOft].fdt.fdEntryNo];
			//将子文件更改写回父目录
			memcpy(fcbPtr, &((OftList + iOft)->fcb), sizeof(fcb));
		}
		//清除原有子文件的打开内容
		initOft(OftList + iOft);
		currOftIndex = piOft;
		return piOft;
	}
}
/// <summary>
///    进入当前目录下的目录
/// </summary>
/// <param name="dirname">文件名</param>
/// <returns>
///    成功，返回 <c>0</c>, 否则返回<c>EXIT_FAILURE/NOT_FOUND</c>.
/// </returns>
int open_curpath(char *dirname) {
	if (judgeDir(&(OftList[currOftIndex].fcb)) == FALSE) {
		printf("数据文件下无法cd\n");
		return NOT_FOUND;
	}
	oft *oftPtr = OftList + currOftIndex;
	fcb *fcbPtr = (fcb *)get_block_ptr(oftPtr->fcb.s_block);
	int fcbNum = (int)(oftPtr->fcb.len / sizeof(fcb));
	// 遍历目录文件 查看该目录文件下 dirname匹配的目录文件是否存在
	int i;
	for (i = 0; i < fcbNum + 2 && i<(VD_BLOCK_SIZE / sizeof(fcb)); i++) {
		if (fcbPtr[i].use == FALSE) {
			fcbNum++;
			continue;
		}
		if (fileNameMatch(&fcbPtr[i], dirname, (char *)"di") == TRUE &&
			judgeDir(&fcbPtr[i]) == TRUE) {
			break;
		}
	}
	if (i == fcbNum + 2) {
		printf("未找到这样的目录文件\n");
		return EXIT_FAILURE;
	}
	//若为当前目录
	if (strcmp(fcbPtr[i].filename, ".") == 0) {
		// 特殊目录项 "."
		return 0;
	}
	else if (strcmp(fcbPtr[i].filename, "..") == 0) {// 特殊目录项 ".."
		if (currOftIndex == 0) {//如果是根目录文件
			return 0;
		}
		else {
			currOftIndex = my_close(currOftIndex);
		}
	}
	else {
		//普通目录文件
		int iOft = getNextOft();
		if (iOft == NOT_FOUND) {
			printf("没有足够的打开文件空间\n");
			return EXIT_FAILURE;
		}
		oft* oftPtrChild = OftList + iOft;
		// FCB 拷贝更新访问
		viewFcbTime(&fcbPtr[i]);
		memcpy(&(oftPtrChild->fcb), &fcbPtr[i], sizeof(fcb));
		// FDT 初始化
		oftPtrChild->fdt.filePtr = 0;
		oftPtrChild->fdt.isUpate = FALSE;
		strcpy(oftPtrChild->fdt.dirName, oftPtr->fdt.dirName);
		strcat(oftPtrChild->fdt.dirName, dirname);
		strcat(oftPtrChild->fdt.dirName, "\\");
		oftPtrChild->isUse = TRUE;
		oftPtrChild->fdt.pdfBlockNo = oftPtr->fcb.s_block;
		oftPtrChild->fdt.fdEntryNo = i;
		currOftIndex = iOft;
	}
	return 0;
}
/// <summary>
///    切换当前目录
/// </summary>
/// <param name="pathname">路径名</param>
/// <returns>
///    void
/// </returns>
void my_cd(char* pathname) {
	char dirname[DIR_NAME_MAX_LEN];
	strcpy(dirname, OftList[currOftIndex].fdt.dirName);
	int flag = 0;
	char *path = strtok(pathname, "\\");
	while (path != NULL) {
		if (strcmp(path, "root") == 0) {
			while (currOftIndex) {
				my_close(currOftIndex);
			}
			path = ".";
		}
		flag = open_curpath(path);
		if (flag == EXIT_FAILURE||flag==NOT_FOUND)break;
		path = strtok(NULL, "\\");
	}
	if (flag == EXIT_FAILURE)my_cd(dirname);
	return;
}
/// <summary>
///    创建文件夹（修改）OK
/// </summary>
/// <param name="dirname">文件夹名字</param>
/// <returns>
///    成功，返回 <c>0</c>, 否则返回<c>EXIT_FAILURE</c>.
/// </returns>
int my_mkdir(char *dirname) {
	createfile(dirname, TRUE);
	return 0;
}
/// <summary>
///    创建文件ok?
/// </summary>
/// <param name="filename">文件名</param>
/// <returns>
///    成功，返回 <c>0</c>, 否则返回<c>EXIT_FAILURE</c>.
/// </returns>
int my_create(char *filename) {
	createfile(filename, FALSE);
	return 0;
}
/// <summary>
///    初始化系统?
/// </summary>
/// <returns>
///    void
/// </returns>
void initsys() {
	//开辟内存空间作为文件系统
	VDPtr = malloc(VD_SIZE);
	//读取文件，还原系统
	FILE* readptr = fopen(file_system_name, "r");
	if (readptr == NULL) {
		//未读到文件的存在
		printf("打开实体文件失败\n尝试新建文件系统\n");
		if (my_format()!=0) {
			printf("无法创建\n");
			exit(1);
		}
		else {
			return;
		}
	}
	else {
		fread(VDPtr, VD_SIZE, 1, readptr);
		fclose(readptr);
		//初始化根目录打开文件表
		initOft(OftList);
		fcb * fcbPtr = (fcb *)get_block_ptr(BLOCK_INDEX_ROOT_DIR);
		memcpy(&(OftList->fcb), fcbPtr, sizeof(fcb));
		strcpy(OftList->fdt.dirName, "\\root\\");
		OftList->fdt.pdfBlockNo = BLOCK_INDEX_ROOT_DIR;  //5
		OftList->fdt.fdEntryNo = 0;					     //Root Dir 的父目录文件就是自己 目录项为首个FCB .
		OftList->fdt.isUpate = FALSE;
		OftList->fdt.filePtr = 0;
		OftList->isUse = TRUE;
		currOftIndex = 0;
	}
}
/// <summary>
///    退出系统ok
/// </summary>
/// <returns>
///    void
/// </returns>
void my_exit_sys() {
	while (currOftIndex) {
		my_close(currOftIndex);
	}
	saveVdFile();
	free(VDPtr);
}
/// <summary>
///    使用帮助
/// </summary>
/// <returns>
///    void
/// </returns>
void help() {
	printf("\tmy_format           ————格式化磁盘\n");
	printf("\tmy_mkdir  [dirname] ————创建文件夹\n");
	printf("\tmy_rmdir  [dirname] ————删除文件夹\n");
	printf("\tmy_ls     [-l]      ————查看目录内容[全部]\n");
	printf("\tmy_cd     [path]    ————切换当前所在目录\n");
	printf("\tmy_create [filename]————创建文件\n");
	printf("\tmy_rm     [filename]————删除文件\n");
	printf("\tmy_open   [filename]————打开文件\n");
	printf("\tmy_close  [filename]————关闭文件\n");
	printf("\tmy_write            ————写入文件\n");
	printf("\tmy_read             ————读出文件\n");
	printf("\tmy_ctrl   [filename]————控制文件属性\n");
	printf("\tmy_man              ————帮助菜单\n");
	printf("\tmy_clear            ————屏幕清空\n");
	printf("\tmy_exitsys          ————退出系统\n");
}
//命令数组
const char* cmds[] = { "my_format","my_mkdir","my_rmdir","my_ls","my_cd","my_create",
"my_rm","my_open","my_close","my_write","my_read","my_exitsys","my_ctrl","my_man","my_clear" };
/// <summary>
///    获取命令索引ok
/// </summary>
/// <param name="cmdStr">命令字符串</param>
/// <returns>
///    成功，返回 <c>索引值</c>, 否则返回<c>-1</c>.
/// </returns>
int getIndexOfCmd(char *cmdStr) {
	int i;
	if (cmdStr == NULL) {
		return -1;
	}
	if (strcmp(cmdStr, "") == 0) {
		printf("命令不能为空\n");
		return -1;
	}
	for (i = 0; i < 15; i++) {
		if (strcmp(cmds[i], cmdStr) == 0) {
			return i;
		}
	}
	return NOT_FOUND;
}
int flag = 1;
/// <summary>
///    执行命令ok
/// </summary>
/// <param name="cmdLine">命令</param>
/// <returns>
///    成功，返回 <c>true</c>, 否则返回<c>true</c>.
/// </returns>
void excuteCmd(char *cmdLine) {
	char *arg1 = strtok(cmdLine, " ");
	char *arg2 = strtok(NULL, " ");
	int indexOfCmd = getIndexOfCmd(arg1);
	int ctrl = 0;
	switch (indexOfCmd) {
	case 0:
		//format
		my_format();
		break;
	case 1:
		//mkdir
		if (arg2 != NULL) {
			my_mkdir(arg2);
		}
		break;
	case 2:
		if (arg2 != NULL) {
			my_rmdir(arg2);
		}
		break;
	case 3:
		//ls
		if (arg2 != NULL&&strcmp(arg2, "-l") == 0) {
			my_ls(TRUE);
		}
		else
		{
			my_ls(FALSE);
		}
		break;
	case 4:
		//cd
		if (arg2 == NULL)break;
		else {
			my_cd(arg2);	
		}
		break;
	case 5:
		//create
		if (arg2 != NULL) {
			my_create(arg2);
		}
		break;
	case 6:
		// rm
		if (arg2 != NULL) {
			my_rm(arg2);
		}
		break;
	case 7:
		// open
		if (arg2 != NULL) {
			my_open(arg2);
		}
		break;
	case 8:
		// close
		if (judgeDir(&(OftList[currOftIndex].fcb)) == FALSE) {
			my_close(currOftIndex);
		}
		else {
			printf("当前为目录文件，无法被关闭\n");
		}
		break;
	case 9:
		// write
		if (judgeDir(&(OftList[currOftIndex].fcb)) == FALSE) {
			my_write(currOftIndex);
		}
		else {
			printf("当前为目录文件，无法被写入\n");
		}
		break;
	case 10:
		// read
		if (judgeDir(&OftList[currOftIndex].fcb) == FALSE) {
			my_read(currOftIndex);
		}
		else {
			printf("当前为目录文件，无法被读出\n");
		}
		break;
	case 11:
		//exit
		my_exit_sys();
		printf("退出文件系统\n");
		flag = 0;
		break;
	case 12:
		//ctrl
		if (judgeDir(&OftList[currOftIndex].fcb) == FALSE) {
			printf("无法在数据文件下进行操作\n");
			break;
		}
		if (arg2 != NULL) {
			printf("请输入控制参数,0-读写(默认)，1-只读，2-隐藏，3-取消隐藏\n");
			scanf("%d", &ctrl);
			//清空输入流
			int c;
			while ((c = getchar()) != '\n' && c != EOF);
			my_ctrl(arg2 , ctrl);
		}
		break;
	case 13:
		help();
		break;
	case 14:
		system("clear");
		break;
	default:
		printf("没有这样的命令\n");
		break;
	}
}
int main() {
	initsys();
	char cmdLine[80];
	while (flag) {
		printf("%s > ", OftList[currOftIndex].fdt.dirName);
		gets_s(cmdLine);
		//gets(cmdLine);
		excuteCmd(cmdLine);
	}
	return 0;
}