#define __LIBRARY__
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

_syscall3(int,execve2,const char *, file,char **, argv,char **, envp)
_syscall3(unsigned int,getdents,unsigned int, fd,struct linux_dirent *,dirp,unsigned int ,count)
_syscall1(unsigned int,sleep,unsigned int, seconds)
_syscall2(unsigned long,getcwd,char *,buf,size_t ,size)

int main(void) {
	time_t time1, time2;
	int ret;
	time(&time1);
	assert(time1 >= 0);
	ret = sleep(1);
	assert(ret == 0);
	time(&time2);
	assert(time2 >= 0);

	if(time2 - time1 >= 1){	
		printf("time passwd: %d\n", time2-time1);
		printf("sleep success.\n");
	}else{
		printf("sleep error.\n");
	}
	
	return 0;
}

