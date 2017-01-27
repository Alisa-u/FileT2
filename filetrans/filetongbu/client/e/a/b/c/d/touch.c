#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>    //以下两个头文件可以不包含，但在某些unix 系统上必不可少！！
#include <sys/types.h>

int main(int argc ,char* argv[])  //touch 实现
{
	if(argc == 1)
	{
		perror(" arguement is less");
		return ;
	}
	int touch;
	touch = open(argv[1],O_RDWR | O_CREAT | O_EXCL,S_IWUSR|S_IRUSR|S_IRGRP|S_IWGRP|S_IROTH);
	if(touch == -1)
	{
		perror("touch is fail\n");
	}
	exit(0);
}
