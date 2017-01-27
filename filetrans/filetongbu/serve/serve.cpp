#include <map>
#include <iostream>
#include<stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>
#include <vector>
#include "sem.h"
#include <fstream>
#include "msg.h"
#include "file.h"
#include "list.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include "redis.h"
#include <fcntl.h>
#include <sys/sendfile.h>
using namespace std;
#define NFDS 3
#define BLOCK 5
#define SPath "/home/youyou/cvte/filetongbu/serve/"
#define FSPath "/home/youyou/cvte/filetongbu/serve/"

static pthread_mutex_t  mutex = PTHREAD_MUTEX_INITIALIZER; //静态初始化
static pthread_cond_t  cond = PTHREAD_COND_INITIALIZER;
typedef struct Md5Path
{
	char md5[40];
	char path[100];
}Md5Path;
vector<Md5Path> md5vec;
void create_block(int fd);///创建链接块的线程池
void recv_one_block(int fd);
map<int,char*> OnlineUserMap;//在线表
map<int,char*> DislineUserMap;

MyRedis myredis((char *)"127.0.0.1",6379);/////////////////////////////////redis

bool doUserLogin(int fd)
{
	LOGIN Lidpsd;
	int length = recv(fd,&(Lidpsd),sizeof(Lidpsd),0);
	cout<<"id:"<<Lidpsd.userid<<endl;
	cout<<"password:"<<Lidpsd.password<<endl;
	
	char log[30]={0};
	sprintf(log,"hget IdPasw %d",Lidpsd.userid);
	
	char *pass=myredis.Redis_hgetall_Hash(log);
	cout<<"pass"<<pass<<endl;
	if(strcmp(pass,Lidpsd.password)==0)
	{
		cout<<"equal"<<endl;
		char l[10]={0};
		strcpy(l,"LOGIN");
		OnlineUserMap[Lidpsd.userid]=Lidpsd.password;
		send(fd,l,sizeof(l),0);
		
	}
	else
	{
		char l[10]={0};
		strcpy(l,"NEGATION");
		send(fd,l,sizeof(l),0);
		cout<<"pass"<<pass<<endl;	
	}
}
static int id=3;
bool Log_Reg(int fd);
bool doUserRegist(int fd)
{
	REGISTER regist;
	int length = recv(fd,&(regist),sizeof(regist),0);
	cout<<"username:"<<regist.username<<endl;
	cout<<"password:"<<regist.password<<endl;
	
	char reg[50]={0};
	cout<<"id:"<<id<<endl;
	sprintf(reg,"hset IdPasw %d %s",id,regist.password);
	myredis.Redis_add_Hash(reg);	
	send(fd,&id,sizeof(id),0);
	id++;
	Log_Reg(fd);
	
	DislineUserMap[id]=regist.password;
}
void Exit(int fd)
{
	int id;
	recv(fd,&id,sizeof(id),0);
	OnlineUserMap.erase(id);
	send(fd,"ok",4,0);
	exit(0);
}
bool Log_Reg(int fd)
{
	cout<<fd<<"fd"<<endl;
	while(1)
	{	
		int i;
		printf("***************\n");
		
		recv(fd,&i,sizeof(i),0);	
		printf("***************\n");	
		cout<<i<<endl;
		if(i-1==0)
		{
			bool res=doUserLogin(fd);
			return res;
		}
		else if(i-2==0)
		{
			bool res=doUserRegist(fd);
			return res;
		}
		else if(i-3==0)
		{
			Exit(fd);
		}
	}
							
}
BigFile bgfile;
void send_one_block(int fds,char *blockname);
void BigFileSend(int fds,char *fpath)
{
	vector<char *>fileblock;
	fileblock=bgfile.Split_FileTon(fpath,50);
	if(!fileblock.empty())
	{
		cout<<"block is not empty!"<<endl;
	}
	int count=fileblock.size();
	send(fds,&count,sizeof(count),0);
	cout<<"block count:"<<count<<endl;
	while(!fileblock.empty())
	{
		cout<<"---------send_one_block------"<<endl;
		pthread_mutex_lock(&mutex);//加锁
		char *blockname=fileblock.back();
		fileblock.pop_back();
		pthread_mutex_unlock(&mutex);//解锁
		cout<<"blockname:"<<blockname<<endl;//block文件分块的名字
		send_one_block(fds,blockname);
		char buff[4]={0};
		recv(fds,&buff,sizeof(buff),0);
		cout<<buff<<endl;//文件块发送成功的返回ok信息///////////////////////////////?
	}
	
}
void send_one_block(int fds,char *blockname)
{
	char bockpath[100]={0};
	strcpy(bockpath,SPath);
	strcat(bockpath,"Tblock/");
	strcat(bockpath,blockname);
	cout<<"bockpath:"<<bockpath<<endl;
		
	unsigned long blocfilesize=bgfile.get_file_size(blockname);

	int fd = open(bockpath,O_RDONLY);
	if( fd == -1 )
        {
	   cout<<"open fail"<<endl;
        }
	struct stat stat_file;//stat是一个结构体
	fstat(fd,&stat_file);
	cout<<"stat_file.st_size"<<stat_file.st_size<<endl;
	cout<<"++++++++++++++++++=="<<endl;
	char sendlength[512]={0};	
	sprintf(sendlength,"[T04@0@%ld]",stat_file.st_size);
	strcat(sendlength,"*");
	strcat(sendlength,blockname);
	cout<<"sendlength:"<<sendlength<<endl;
	send(fds,sendlength,sizeof(sendlength),0);///////////////////////////////////////////////
	
	memset(sendlength,0,128);
	recv(fds,sendlength,127,0);
    	cout<<"sendlength:"<<sendlength<<endl;
	
 	
		cout<<"------------"<<endl;
		while( 1 )
   	 	{
			int n = 0;
			if ((n = sendfile(fds,fd,NULL,256))>0)
			{
					continue;
			}
			else
			{
				break;
			}
		}
	
	close(fd);
}
void filecopy(char *src,int fd)
{
	int pipefd[2];
	pipe(pipefd);
	char buffs[30]={0};
	cout<<"src:"<<src<<endl;
	int fp=open(src,O_WRONLY|O_CREAT,0600);
	if(fp==-1)
	{
		perror("open faile");
	}
	char recvbuff[256]={0};
	long  length = 0;
	int l=recv(fd,recvbuff,sizeof(recvbuff),0);//接收来自服务器中收到的文件大小
	cout<<"recvbuff:"<<recvbuff<<endl;//调试信息
	cout<<"-------------2"<<endl;
	if(l == 0)
	{
		cout<<"recv error"<<endl;
	}

	char *p = recvbuff + 5;
	if (strncmp(p,"0",1) == 0 )
	{
		cout<<"-------------2"<<endl;
		 p[strlen(p)-1] = 0;
		 length = atol(p+2);
		 cout<<"length:"<<length<<endl; 
	}
	send(fd,"OK",2,0);
	long count = 0;
	 while( 1 )
	 {
		int n = splice(fd,NULL,pipefd[1],NULL,256,SPLICE_F_MORE);
		if ( n <= 0 )
		{
			printf("down file error\n");
			break;
		}
		count += n;
	
		//printf("count=%d,n=%d\n",count,n);
		int num = splice(pipefd[0],NULL,fp,NULL,n,SPLICE_F_MORE);
		//printf("num=%d\n",num);
		if ( num < n )
		{
			splice(pipefd[0],NULL,fp,NULL,(n-num),0);
		}
		if ( count == length )
		{
		 	printf("\ndown file finish\n");
			break;
		}
	}	
close(fp);
close(pipefd[0]);
close(pipefd[1]);	
}
Node *cliheads;
void* work_thread(void *arg)
{
	Init_list(&cliheads);
	Node *head=(Node *)arg;
	while(1)
	{
		sem_p();//在线程中sem_p()操作
		int fd=Getc_list(head);//获取该链接描述附
		Add_listC(cliheads,fd);
		int index=Getcount_list(cliheads);	
		cout<<"~~~~~~~~~~~~~~~~~~~~~~~index:~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~"<<index<<endl;
		if(fd==-1)
		{
			continue;
		}
		printf("pthread beging...\n");
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	
		while(1)
		{
			bool res=Log_Reg(fd);
			if(res)
			{
				break;
			}
		}
		while(1)
		{
			
			char *myargv[10]={0};
			char charbuff[128]={0};
			if(recv(fd,charbuff,127,0)==0)
			{
				break;
			}

			printf("%s\n",charbuff);
			
			if(strcmp(charbuff,"check md5")==0)	
			{	
				vector<char*> tmpvec;	
			
				int index=Getcount_list(cliheads);
				cout<<"index:"<<index<<endl;
				vector<Md5Path>::iterator it=md5vec.begin();
				char buff[1024]={0};
				for(;it!=md5vec.end();++it)
				{
					cout<<(*it).md5<<endl;
					cout<<(*it).path<<endl;
					
					strcat(buff,(*it).md5);
					strcat(buff,"#");
					char *s=NULL;char *ptr=NULL;
					cout<<"pah:"<<(*it).path<<endl;
					char gpath[40]={0};
					strcpy(gpath,(*it).path);
					if((ptr=strtok(gpath,"/"))!=NULL)
					{
						s=ptr;
					}
					while((ptr=strtok(NULL,"/"))!=NULL)//strtok默认其内就有一个指针，保存当前截取的位置
					{
						s=ptr;
					}
					strcat(buff,s);
					strcat(buff,"*");
					cout<<"~~~~~~~~~~~~~buff~~~~~~~~~:"<<buff<<endl;
				}
				for(int i=0;i<index;++i)
				{
					int fds=Getstr_fd(cliheads,i);
					send(fds,buff,sizeof(buff),0);//server将md5#file.c传给client
					cout<<"buff"<<buff<<endl;
					cout<<"~~~~~fds~~~~~"<<fds<<endl;
					char rec[128]={0};
					while(1)
					{
						recv(fds,rec,sizeof(rec),0);
						if(strncmp(rec,"equal",5)==0 || strlen(rec)>33 )
						{
							cout<<"++++++++++++++++++++++++++++++++++++"<<endl;
							break;
						}	
					}
					cout<<"*******rec********"<<rec<<endl;//rec应该是md5#file.c
					if(strncmp(rec,"equal",5)==0)
					{
						cout<<"equal"<<endl;
					}				
					if(strlen(rec)>33)
					{
						cout<<"need get!"<<endl;
						char *s=NULL;char *ptr=NULL;
						cout<<"rec:"<<rec<<endl;
						char *myargv[20]={0};
						char *pptr=NULL;char *wptr=NULL;
						if((pptr=strtok(rec,"#"))!=NULL)
						{
					   		myargv[0]=pptr;
						}
						int i=1;
						while((wptr=strtok(NULL,"#"))!=NULL)
					        {
						    myargv[i++]=wptr;
					        }
						char tmp[40]={0};
						myargv[i]='\0';
						for(int i=0;myargv[i]!='\0';i++)
						{
							char *path;char *ptrpath;
					
							if((path=strtok(myargv[i],"#"))!=NULL)
							{
								ptrpath=path;	
							}
							while((path=strtok(NULL,"#"))!=NULL)
							{
								ptrpath=path;
								strcpy(tmp,SPath);
								strcat(tmp,ptrpath);
							}
							tmpvec.push_back(tmp);
						}
						cout<<"myargv[0]:"<<myargv[0]<<endl;
						cout<<"myargv[1]:"<<myargv[1]<<endl;
						
						while(!tmpvec.empty())
						{
							char *sbuff=tmpvec.front();
							tmpvec.pop_back();
							
							char recvbuff[128] = {0};
							printf("get something\n");
							int fp=open(myargv[1],O_RDONLY);//以只读的方式打开list.c
							cout<<"myargv[1]:"<<myargv[1]<<endl;
							if(fp<0)
							{
								cout<<"read error"<<endl;
							}
							cout<<"-----------------1"<<endl;
							struct stat stat_file;//stat是一个结构体
							fstat(fp,&stat_file);
							
							cout<<"size:"<<stat_file.st_size<<endl;
							sprintf(recvbuff,"[T04@0@%ld]",stat_file.st_size);
							
							int i=send(fds,recvbuff,strlen(recvbuff)+1,0);//将这个文件（sem.c）的大小发给client							
							cout<<"+++++++++++++"<<i<<endl;
							cout<<"recvbuff:"<<recvbuff<<endl;
							char buff[4]={0};
							int n=recv(fds,buff,sizeof(buff),0);//ok
							if(stat_file.st_size<1024*1024)
							{	
								
								cout<<"n"<<n<<endl;
								cout<<"buff"<<buff<<endl;
								if(n!=0)
							  	{
									while( 1 )
				   	 				{
										int n = 0;
					       				 	if ((n = sendfile(fds,fp,NULL,256))>0)
										{
												cout<<"n:"<<n<<endl;
												continue;
										}
										else
										{
											break;
										}
				    					}
								}
							}
							else
							{
								BigFileSend(fds,myargv[1]);
							}
						}
						
					}
				}
			}
			else
			{
				cout<<"strlen:"<<strlen(charbuff)<<endl;
				printf("%s\n",charbuff);
				char *ptr = NULL;
				char *pstr = NULL;
				ptr=strtok_r(charbuff,"#",&pstr);
				//char *strtok_r(char *str, const char *delim, char **saveptr);
				if(ptr==NULL)
				{
					continue;
				}
				//printf("%s\n",ptr);
			
				myargv[0]=ptr;//将命令放到myargv[0]中 ls    get    put

				int i=1;

				while((ptr=strtok_r(NULL,"#",&pstr))!=NULL)
				{
					myargv[i++]=ptr;//将一个个的命令以及参数都放在myargv数组中
				}
				cout<<"i:"<<i<<endl;
				printf("myargv[0]=%s\n",myargv[0]);//myargv[0]=get

				printf("myargv[1]=%s\n",myargv[1]);//myargv[1]=sem.c

	//如果所要执行的命令是get（下载）:	它对应的是客户端的void getfile(Node *head,int sockfd)		
				if(strncmp(myargv[0],"get",3)==0)//myargv[0]=get  myargv[1]=sem.c 
				{
					printf("get something\n");
					int fp=open(myargv[1],O_RDONLY);//以只读的方式打开list.c
					if(fp<0)
					{
						cout<<"read error"<<endl;
					}
					struct stat stat_file;//stat是一个结构体
					fstat(fp,&stat_file);
					int length=(int)stat_file.st_size;//获取文件的大小，这个中的stat.st_size是偏移量，把它转成int

					printf("%d\n",length);
				
					send(fd,&length,sizeof(int),0);//将这个文件（sem.c）的大小发给client
					//ssize_t send(int sockfd, const void *buf, size_t len, int flags);
				
					while(sendfile(fd,fp,NULL,521)>0)//sendfile函数在两个文件描述附直接传递数据（完全在内核中操作），从而s避免了内核缓冲区和用户缓冲区之间的数据拷贝，从fp拷贝到fd上，只要fp上有数据，就会一直拷贝。
					//ssize_t sendfile(int out_fd, int in_fd, off_t *offset, size_t count);
					{;}
				
				}
				else if(strncmp(myargv[0],"dirgo",5)==0)
				{
					printf("dir something\n");
					if((mkdir(myargv[1],0777))==-1)
					{
						printf("mkdir si error\n");
					}
					send(fd,"ok",4,0);
					chdir(myargv[1]);
					char mypath[128]={0};
					strcpy(mypath,SPath);
					strcat(mypath,myargv[1]);
					while(1)
					{
						char strtik[30]={0};
						recv(fd,strtik,sizeof(strtik),0);
						cout<<"strtik:"<<strtik<<endl;
						if (strncmp(strtik,"0",1) == 0 )
	    					{
							char *p = strtik + 1;
							char buffs[30]={0};
							strcpy(buffs,p);
							if(mkdir(buffs,0777)==-1)
							{
								printf("mkdir buffs error\n");
							}
							send(fd,"ok",4,0);
							strcat(mypath,"/");
							strcat(mypath,buffs);
							cout<<"mypath:"<<mypath<<endl;
							chdir(mypath);
						}
						else if(strncmp(strtik,"1",1) == 0 )
						{
							char *p = strtik + 1;
							char buffs[30]={0};
							strcpy(buffs,p);
							strcat(mypath,"/");
							strcat(mypath,buffs);
							cout<<"mypath:"<<mypath<<endl;
							send(fd,"ok",4,0);
							filecopy(mypath,fd);
							//mypath:/home/youyou/cvte/filetongbu/serve/a/b/c/d/mybash.c
							////////////////////////////////////////////////
							char src[30]={0};
							char pathh[128]={0};
							strcpy(pathh,SPath);
							strcat(pathh,buffs);
							open(pathh,O_WRONLY|O_CREAT,0600);
							cout<<"pathh:"<<pathh<<endl;
							sprintf(src,"cp %s %s",mypath,pathh);
							cout<<"src:"<<src<<endl;
							system(src);


							char tmpmd5[100]={0};
							strcpy(tmpmd5,mypath);
							
							char md5file[100]={0};
							strcpy(md5file,SPath);
							strcat(md5file,"serve_md5");
							cout<<"tmpmd5:"<<tmpmd5<<endl;
							char str[100]={0};
							sprintf(str,"md5sum %s > %s",tmpmd5,md5file);
							cout<<"str:"<<str<<endl;
							system(str);
							char md5[40]={0};
						 	ifstream infile(md5file,ios::in);	
							infile.getline(md5,33);
							infile.close();
							cout<<"-------md5-------"<<md5<<endl;
							Md5Path mp;
							strcpy(mp.md5,md5);
							strcpy(mp.path,tmpmd5);
							md5vec.push_back(mp);
							cout<<"+++++++++++mp.md5:"<<mp.md5<<endl;
		       					cout<<"+++++++++++mp.path:"<<mp.path<<endl;
							//////////////////////////////////////////////////
							
		
							break;
						}
					}
					/////////////////////////////////////////////////////////////////////////
					
					
				}
				//如果所要执行的命令是put（上传）:	它对应的是客户端的void putfile(Node*head,int sockfd)
				else if(strncmp(myargv[0],"put",3)==0)//put a.c
				{
					printf("put something begin...\n");
					int pipefd[2];
					pipe(pipefd);//创建管道
					char pathbuff[1024]={0};
					strcpy(pathbuff,FSPath);
					strcat(pathbuff,myargv[1]);
	
										
					
					int res=open(pathbuff,O_WRONLY|O_CREAT,0600);

					char recvbuff[256]={0};
					long  length = 0;
					int l=recv(fd,recvbuff,255,0);
					cout<<"recvbuff:"<<recvbuff<<endl;//调试信息
					if(l == 0)
					{
						cout<<"recv error"<<endl;
					}
					send(fd,"OK",2,0);
					char bigsmall[10]={0};
					recv(fd,bigsmall,sizeof(bigsmall),0);
					

					cout<<"++++++bigsmall+++++:"<<bigsmall<<endl;
					if(strcmp(bigsmall,"small")==0)
					{
						cout<<"-----small_file-----"<<endl;
						char *p = recvbuff + 5;
		    				if (strncmp(p,"0",1) == 0 )
		    				{
		       					 p[strlen(p)-1] = 0;
		       					 length = atol(p+2);
							 cout<<"length:"<<length<<endl; 
						}
						long count = 0;
	   					 while( 1 )
	   					 {
							int n = splice(fd,NULL,pipefd[1],NULL,256,SPLICE_F_MORE);
							if ( n <= 0 )
							{
		    						printf("down file error\n");
		    						break;
							}
							count += n;
						
							printf("count=%d,n=%d\n",count,n);
							int num = splice(pipefd[0],NULL,res,NULL,n,SPLICE_F_MORE);
							printf("num=%d\n",num);
							if ( num < n )
							{
			    					splice(pipefd[0],NULL,res,NULL,(n-num),0);
							}
							if ( count == length )
							{
			   				 	printf("\ndown file finish\n");
			    					break;
							}
    						}	
					}
					else if(strcmp(bigsmall,"big")==0)
					{
						cout<<"-----big_file-----"<<endl;
						int count=0;
						recv(fd,&count,sizeof(count),0);
						cout<<"+++count:+++"<<count<<endl;
						for(int i=0;i<count;++i)
						{
							recv_one_block(fd);
							//send(fd,"ok",3,0);////////////////////////////////////////////////////
						}
						char catfile[128]={0};
						for(int i=0;i<count;++i)
						{
							sprintf(catfile,"cat block/00%d>>%s",i,pathbuff);
							cout<<"++++++catfile++++++:"<<catfile<<endl;
							system(catfile);
						}
					}
					
					char tmpmd5[100]={0};
					strcpy(tmpmd5,FSPath);
					strcat(tmpmd5,myargv[1]);
					char md5file[100]={0};
					strcpy(md5file,SPath);
					strcat(md5file,"serve_md5");
					cout<<"tmpmd5:"<<tmpmd5<<endl;
					char str[100]={0};
					sprintf(str,"md5sum %s > %s",tmpmd5,md5file);
					cout<<"str:"<<str<<endl;
					system(str);
					char md5[40]={0};
				 	ifstream infile(md5file,ios::in);	
					infile.getline(md5,33);
					infile.close();
					cout<<"-------md5-------"<<md5<<endl;
					Md5Path mp;
					strcpy(mp.md5,md5);
					strcpy(mp.path,tmpmd5);
					md5vec.push_back(mp);
					cout<<"+++++++++++mp.md5:"<<mp.md5<<endl;
       					cout<<"+++++++++++mp.path:"<<mp.path<<endl;

					close(res);
		    			close(pipefd[0]);
		    			close(pipefd[1]);		
				}
				else
				{
					int pipefd[2];
					pipe(pipefd);//创建管道

					pid_t pid=fork();
			
					if(pid==0)
					{
						dup2(pipefd[1],1);//将标准输出重定向到管道的写端
						dup2(pipefd[1],2);//将标准错误输出重定向到管道的写端
						execvp(myargv[0],myargv);//跳转到myargv[0]即ls操作
						perror("execvp error");
						exit(0);
					}
					else
					{
						close(pipefd[1]);//关闭管道的写端

						char readbuff[521]={0};		
						if(read(pipefd[0],readbuff+2,521)==0)//将管道的读端读到readbuff+2中，
						{
							strncpy(readbuff,"1#",2);//若是没有读到信息，就是类似mkdir的操作，给前面拷贝1#
						}
						else
						{
							strncpy(readbuff,"0#",2);//若读到信息，就是类似ls的操作，给前面拷贝0#
						}
					
						send(fd,readbuff,strlen(readbuff),0);//将这些信息都返回给客户端0#list.c sem.c...
					}
				}
			}
		}
	}
}
void create_thread(Node *head)//创建客户端链接的线程池
{
	pthread_t id[NFDS];
	int i=0;	
	for(;i<NFDS;++i)
	{
		pthread_create(&id[i],NULL,work_thread,(void *)head);//创建了三个线程
	}
}

void recv_one_block(int fd)
{
	cout<<"begin recv one block"<<endl;
	char recvallbuff[256]={0};
	
	int l=recv(fd,recvallbuff,255,0);//接收来自服务器中收到的文件大小+文件名
	if(l == 0)
	{
		cout<<"recv error"<<endl;
	}
	
	cout<<"recvbuff:"<<recvallbuff<<endl;//调试信息
	char *ptr=strtok(recvallbuff,"*");//cp main.c test.c
	cout<<"+++++++++++++++"<<ptr<<endl;
	char chunkname[20]={0};
	char recvbuff[30]={0};
	strcpy(recvbuff,ptr);
	while((ptr=strtok(NULL,"*"))!=NULL)
	{
		strcpy(chunkname,ptr);
	}
	
	
	char buf[10]={0};
	
	char path[100] = {0};
	strcpy(path,SPath);
	strcat(path,"block/");
	strcat(path,chunkname);
	cout<<"open path:"<<path<<endl;
	int filerec = open(path,O_WRONLY|O_CREAT,0600);         //在本地创建文件
	if( filerec == -1 )
	{
			cout<<"create chunk faile"<<endl;
			exit(0);
	}
     
	char *p = recvbuff + 5;
	long  length = 0;
	if (strncmp(p,"0",1) == 0 )
	{
		cout<<"-------------2"<<endl;
		 p[strlen(p)-1] = 0;
		 length = atol(p+2);
		cout<<"length:"<<length<<endl; 
	}
	int pipefd[2];                                              //创建管道
	pipe(pipefd);
	send(fd,"OK",2,0);
	long count = 0;
	 while( 1 )
	 {
		cout<<"11111111111111111"<<endl;
		int n = splice(fd,NULL,pipefd[1],NULL,256,SPLICE_F_MORE);
		cout<<"222222222222222"<<endl;
		if ( n <= 0 )
		{
			printf("down file error\n");
			break;
		}
		count += n;
	
		printf("count=%d,n=%d\n",count,n);
		int num = splice(pipefd[0],NULL,filerec,NULL,n,SPLICE_F_MORE);
		printf("num=%d\n",num);
		if ( num < n )
		{
			splice(pipefd[0],NULL,filerec,NULL,(n-num),0);
		}
		if ( count == length )
		{
		 	printf("\ndown file finish\n");
			send(fd,"come",4,0);
			break;
		}
	}	
	close(filerec);
	close(pipefd[0]);
	close(pipefd[1]);	
}

int main()
{
	int sockfd=socket(AF_INET,SOCK_STREAM,0);
	assert(sockfd!=-1);

	struct sockaddr_in saddr,caddr;
	memset(&saddr,0,sizeof(saddr));
	saddr.sin_family=AF_INET;
	saddr.sin_port=htons(6500);
	saddr.sin_addr.s_addr=inet_addr("127.0.0.1");

	int res=bind(sockfd,(struct sockaddr*)&saddr,sizeof(saddr));
	assert(res!=-1);

	listen(sockfd,5);

	Node *head=NULL;
	Init_list(&head);//定义一个链表

	sem_init();//初始化信号量
	create_thread(head);//创建线程池
	while(1)
	{
		socklen_t len=sizeof(caddr);
		int c=accept(sockfd,(struct sockaddr*)&caddr,&len);//获取链接服务器的那个客户端的描述附
		if(c<0)
		{
			perror("client close");
			continue;
		}
		
		printf("begin\n");
		

		Add_listC(head,c);//将此描述附添加到链表里
		sem_v();//v操作
	}
}










