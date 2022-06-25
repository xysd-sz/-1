/*
 *  linux/init/main.c
 *
 *  (C) 1991  Linus Torvalds
 */

#define __LIBRARY__
#include <unistd.h>
#include <time.h>
#include <signal.h>



/*
 * we need this inline - forking from kernel space will result
 * in NO COPY ON WRITE (!!!), until an execve is executed. This
 * is no problem, but for the stack. This is handled by not letting
 * main() use the stack at all after fork(). Thus, no function
 * calls - which means inline code for fork too, as otherwise we
 * would use the stack upon exit from 'fork()'.
 *
 * Actually only pause and fork are needed inline, so that there
 * won't be any messing with the stack from main(), but we define
 * some others too.
 */
 
__always_inline _syscall0(int,fork)
__always_inline _syscall0(int,pause)
__always_inline _syscall1(int,setup,void *,BIOS)
__always_inline _syscall0(int,sync)
/*__always_inline _syscall3(int,execve2,const char *, path,char **, argv,char **, envp)
__always_inline _syscall3(unsigned int,getdents,unsigned int, fd,struct linux_dirent *,dirp,unsigned int ,count)
__always_inline _syscall1(unsigned int,sleep,unsigned int, seconds)
__always_inline _syscall2(unsigned int,getcwd,char *,buf,size_t ,size)*/

#include <linux/tty.h>
#include <linux/sched.h>
#include <linux/head.h>
#include <asm/system.h>
#include <asm/io.h>

#include <stddef.h>
#include <stdarg.h>
#include <fcntl.h>
#include <sys/types.h>
#include <linux/fs.h>
#include <asm/segment.h>

static char printbuf[1024];

extern int vsprintf();
extern void init(void);
extern void blk_dev_init(void);
extern void chr_dev_init(void);
extern void hd_init(void);
extern void floppy_init(void);
extern void mem_init(long start, long end);
extern long rd_init(long mem_start, int length);
extern long kernel_mktime(struct tm * tm);
extern long startup_time;
/*
 * This is set up by the setup-routine at boot-time
 */
#define EXT_MEM_K (*(unsigned short *)0x90002)
#define DRIVE_INFO (*(struct drive_info *)0x90080)
#define ORIG_ROOT_DEV (*(unsigned short *)0x901FC)

/*
 * Yeah, yeah, it's ugly, but I cannot find how to do this correctly
 * and this seems to work. I anybody has more info on the real-time
 * clock I'd be interested. Most of this was trial and error, and some
 * bios-listing reading. Urghh.
 */

#define CMOS_READ(addr) ({ \
outb_p(0x80|addr,0x70); \
inb_p(0x71); \
})

#define BCD_TO_BIN(val) ((val)=((val)&15) + ((val)>>4)*10)

static void time_init(void)
{
	struct tm time;

	do {
		time.tm_sec = CMOS_READ(0);
		time.tm_min = CMOS_READ(2);
		time.tm_hour = CMOS_READ(4);
		time.tm_mday = CMOS_READ(7);
		time.tm_mon = CMOS_READ(8);
		time.tm_year = CMOS_READ(9);
	} while (time.tm_sec != CMOS_READ(0));
	BCD_TO_BIN(time.tm_sec);
	BCD_TO_BIN(time.tm_min);
	BCD_TO_BIN(time.tm_hour);
	BCD_TO_BIN(time.tm_mday);
	BCD_TO_BIN(time.tm_mon);
	BCD_TO_BIN(time.tm_year);
	time.tm_mon--;
	startup_time = kernel_mktime(&time);
}

static long memory_end = 0;
static long buffer_memory_end = 0;
static long main_memory_start = 0;

struct drive_info { char dummy[32]; } drive_info;


void main(void)		/* This really IS void, no error here. */
{			/* The startup routine assumes (well, ...) this */
/*
 * Interrupts are still disabled. Do necessary setups, then
 * enable them
 */
 	ROOT_DEV = ORIG_ROOT_DEV;
	__asm__ volatile ("cld");   /* by wyj */ 	
 	drive_info = DRIVE_INFO;
	memory_end = (1<<20) + (EXT_MEM_K<<10);
	memory_end &= 0xfffff000;
	if (memory_end > 16*1024*1024)
		memory_end = 16*1024*1024;
	if (memory_end > 12*1024*1024) 
		buffer_memory_end = 4*1024*1024;
	else if (memory_end > 6*1024*1024)
		buffer_memory_end = 2*1024*1024;
	else
		buffer_memory_end = 1*1024*1024;
	main_memory_start = buffer_memory_end;
#ifdef RAMDISK
	main_memory_start += rd_init(main_memory_start, RAMDISK*1024);
#endif
	mem_init(main_memory_start,memory_end);
	trap_init();
	blk_dev_init();
	chr_dev_init();
	tty_init();
	time_init();
	sched_init();
	buffer_init(buffer_memory_end);
	hd_init();
	floppy_init();
	sti();
	move_to_user_mode();
	if (!fork()) {		/* we count on this going ok */
		init();
	}
/*
 *   NOTE!!   For any other task 'pause()' would mean we have to get a
 * signal to awaken, but task0 is the sole exception (see 'schedule()')
 * as task 0 gets activated at every idle moment (when no other tasks
 * can run). For task0 'pause()' just means we go check if some other
 * task can run, and if not we return here.
 */
	for(;;) pause();
}

static int printf(const char *fmt, ...)
{
	va_list args;
	int i;

	va_start(args, fmt);
	write(1,printbuf,i=vsprintf(printbuf, fmt, args));
	va_end(args);
	return i;
}

static char * argv_rc[] = { "/bin/sh", NULL };
static char * envp_rc[] = { "HOME=/", NULL, NULL };

static char * argv[] = { "-/bin/sh",NULL };
static char * envp[] = { "HOME=/usr/root", NULL, NULL };

void init(void)
{
	int pid,i;

	setup((void *) &drive_info);
	(void) open("/dev/tty0",O_RDWR,0);
	(void) dup(0);
	(void) dup(0);
	printf("%d buffers = %d bytes buffer space\n\r",NR_BUFFERS,
		NR_BUFFERS*BLOCK_SIZE);
	printf("Free mem: %d bytes\n\r",memory_end-main_memory_start);
	if (!(pid=fork())) {
		close(0);
		if (open("/etc/rc",O_RDONLY,0))
			_exit(1);
		execve("/bin/sh",argv_rc,envp_rc);
		_exit(2);
	}
	if (pid>0)
		while (pid != wait(&i))
			/* nothing */;
	while (1) {
		if ((pid=fork())<0) {
			printf("Fork failed in init\r\n");
			continue;
		}
		if (!pid) {
			close(0);close(1);close(2);
			setsid();
			(void) open("/dev/tty0",O_RDWR,0);
			(void) dup(0);
			(void) dup(0);
			_exit(execve("/bin/sh",argv,envp));
		}
		while (1)
			if (pid == wait(&i))
				break;
		printf("\n\rchild %d died with code %04x\n\r",pid,i);
		sync();
	}
	_exit(0);	/* NOTE! _exit, not exit() */
}

/* start four system calls sys_execve2, sys_getdents, sys_sleep, sys_getcwd */

/*
int sys_execve2(const char *file,char **argv,char **envp)
{
	unsigned long Eip[5];
	Eip[0]=0;
	Eip[1]=0x000f;
	Eip[2]=0;
	Eip[3]=0;
	long Tmp=0;
	do_execve(&Eip,Tmp,file,argv,envp);
	return 1;
}
*/

struct linux_dirent{
	long d_ino;
	off_t d_off;
	unsigned short d_reclen;
	char d_name[];
};

int sys_getdents(unsigned int fd,struct linux_dirent *dirp,unsigned int count)
{
	/*	by zyj	    */
	struct m_inode *zyj_m_inode;
	struct buffer_head *zyj_buffer_head;
	struct dir_entry *zyj_dir_entry;
	struct linux_dirent zyj_linux_dir;
	int i, j, res;
	i = 0;
	res = 0;
	zyj_m_inode = current->filp[fd]->f_inode;
	zyj_buffer_head = bread(zyj_m_inode->i_dev, zyj_m_inode->i_zone[0]);
	zyj_dir_entry = (struct dir_entry *)zyj_buffer_head->b_data;
	while (zyj_dir_entry[i].inode>0)
	{
		if (res + sizeof(struct linux_dirent) > count)
		    break;
		zyj_linux_dir.d_ino = zyj_dir_entry[i].inode;
		zyj_linux_dir.d_off = 0;
		zyj_linux_dir.d_reclen = sizeof(struct linux_dirent);
		for (j = 0; j < 14; j++)
		{
		    zyj_linux_dir.d_name[j] = zyj_dir_entry[i].name[j];
		}
		for(j = 0;j <sizeof(struct linux_dirent); j++){
		    put_fs_byte(((char *)(&zyj_linux_dir))[j],(char *)dirp + res);
		    res++;
		}
		i++;
	}
	return res;
}

int sys_sleep(unsigned int seconds)
{
	sys_signal(SIGALRM, SIG_IGN);
	sys_alarm(seconds);
	sys_pause();
	return 0;
}

#define BUF_MAX 1<<12 
/*
	the next two functions copy from find_entry
*/
static struct buffer_head * find_father_dir(struct m_inode ** dir, struct dir_entry ** res_dir)
{
	int entries;
	int block,i;
	struct buffer_head * bh;
	struct dir_entry * de;
	struct super_block * sb; 
	int namelen = 2;
	const char name[] = "..";

#ifdef NO_TRUNCATE
	if (namelen > NAME_LEN)
		return NULL;
#else
	if (namelen > NAME_LEN)
		namelen = NAME_LEN;
#endif
	entries = (*dir)->i_size / (sizeof (struct dir_entry)); 
	*res_dir = NULL;
	if (!namelen)
		return NULL;
	if (namelen==2 && get_fs_byte(name)=='.' && get_fs_byte(name+1)=='.') {
		if ((*dir) == current->root)
			namelen=1;
		else if ((*dir)->i_num == ROOT_INO) {
			sb=get_super((*dir)->i_dev);    
			if (sb->s_imount) {				
				iput(*dir);					
				(*dir)=sb->s_imount;
				(*dir)->i_count++;			
			}
		}
	}
	if (!(block = (*dir)->i_zone[0])) 		
		return NULL;
	if (!(bh = bread((*dir)->i_dev,block))) 
		return NULL;
	i = 0;
	de = (struct dir_entry *) bh->b_data;   
	while (i < entries) {					
		if ((char *)de >= BLOCK_SIZE+bh->b_data) {
			brelse(bh);
			bh = NULL;
			if (!(block = bmap(*dir,i/DIR_ENTRIES_PER_BLOCK)) ||
			    !(bh = bread((*dir)->i_dev,block))) {
				i += DIR_ENTRIES_PER_BLOCK;
				continue;
			}
			de = (struct dir_entry *) bh->b_data;
		}
		if ((de->name[0] == '.' && de->name[1] == '.' && de->name[2] == '\0')) {
			*res_dir = de;
			return bh;
		}
		de++;
		i++;
	}
	brelse(bh);
	return NULL;
}
static struct buffer_head * find_same_inode(struct m_inode ** dir, struct dir_entry ** res_dir, int counter)
{
	int entries;
	int block,i;
	struct buffer_head * bh;
	struct dir_entry * de;
	struct super_block * sb; 
	int namelen = 7;
	const char name[] = "WTF";
#ifdef NO_TRUNCATE
	if (namelen > NAME_LEN)
		return NULL;
#else
	if (namelen > NAME_LEN)
		namelen = NAME_LEN;
#endif
	entries = (*dir)->i_size / (sizeof (struct dir_entry)); 
	*res_dir = NULL;
	if (!namelen)
		return NULL;
	if (namelen==2 && name[0]=='.' && name[1]=='.') 
	{
		if ((*dir) == current->root)
			namelen=1;
		else if ((*dir)->i_num == ROOT_INO) 
		{
			sb=get_super((*dir)->i_dev);    
			if (sb->s_imount) 
			{				
				iput(*dir);					
				(*dir)=sb->s_imount;
				(*dir)->i_count++;			
			}
		}
	}
	if (!(block = (*dir)->i_zone[0])) 		 
		return NULL;
	if (!(bh = bread((*dir)->i_dev,block))) 
		return NULL;
	i = 0;
	de = (struct dir_entry *) bh->b_data;   
	while (i < entries) 
	{					
		if ((char *)de >= BLOCK_SIZE+bh->b_data) 
		{
			brelse(bh);
			bh = NULL;
			if (!(block = bmap(*dir,i/DIR_ENTRIES_PER_BLOCK)) ||
			    !(bh = bread((*dir)->i_dev,block))) 
			{
				i += DIR_ENTRIES_PER_BLOCK;
				continue;
			}
			de = (struct dir_entry *) bh->b_data; 
		}
		if (counter == de->inode) 
		{ 
			*res_dir = de;
			return bh;
		}
		de++;
		i++;
	}
	brelse(bh);
	return NULL;
}

long sys_getcwd(char* buf,size_t size)
{
	char buf_name[BUF_MAX];
	char *nowbuf; 
	struct dir_entry * de;
	struct dir_entry * det;
	struct buffer_head * bh;
	nowbuf = (char *)malloc(BUF_MAX * sizeof(char));
	struct m_inode *now_inode = current->pwd;
	int idev, inid, block;


	int prev_inode_num = now_inode->i_num;
	if (now_inode == current->root)
		strcpy(nowbuf, "/");

	while (now_inode != current->root) {
		bh = find_father_dir(&now_inode, &det);
		idev = now_inode->i_dev;
		inid = det->inode;
		now_inode = iget(idev, inid);
		bh = find_same_inode(&now_inode, &de, prev_inode_num);
		prev_inode_num = det->inode;
		strcpy(buf_name, "/");
		strcat(buf_name, de->name);
		strcat(buf_name, nowbuf);
		strcpy(nowbuf, buf_name);
	}
	int chars = size;
	char *p1 = nowbuf, *p2 = buf;
	++size;
	while (size-- > 0)
		put_fs_byte(*(p1++), p2++);
	return (long)buf;
}

void sys_zyj()
{
}
























