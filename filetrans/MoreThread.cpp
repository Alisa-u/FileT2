#include<stdio.h>
#include<iostream>
#include<sys/epoll.h>
#include<unistd.h>
#include<sys/epoll.h>
#include<string.h>
#include<stdlib.h>
#include<pthread.h>
#include<errno.h>
#include<sys/stat.h>
#include<assert.h>
#include<queue>
#include<arpa/inet.h>
#include<fcntl.h>
#include<sys/socket.h>
#include"lock.h"

using namespace std;

#define MAX_EVENT_NUMBER 1000
#define Num_Thread 2
//创建线程池
pthread_t m_id[Num_Thread]; 
queue<int> Workqueue;  //创建一个任务队列 
void* Thread_Working(void *arg);
void epoll_del(int epollfd,int fd);
void modfd(int epollfd,int fd);

locker QueueLocker; //保护请求队列的互斥锁
sem Asem;  //初始化信号


void CreateThreadPool(int epollfd)
{
   	for(int i = 0;i<Num_Thread;++i)
	{
		if((pthread_create(&m_id[i],NULL,Thread_Working,(void *)epollfd)!=0))  //线程
		{
			cout<<"pthread_create error"<<endl;
			exit(1);
		} 
		
		pthread_detach(m_id[i]);  //将线程设置为脱离线程
		
	}
}

bool Append_Workqueue(int connfd)  
{
	QueueLocker.lock();   //对工作线程进行操作时必须加锁，属于临界资源，所有线程共享
	Workqueue.push(connfd);
	QueueLocker.unlock();
	Asem.post(); //向任务队列中加入节点是进行V操作
	return true;
}

void * Thread_Working(void *arg)
{
		int epollfd = (int)arg;
		while(1)
		{
			Asem.wait();
			QueueLocker.lock();
			if(Workqueue.empty())
			{
				cout<<"##empty##"<<endl;
				QueueLocker.lock();  ////
				continue;
			}
			int c = Workqueue.front();
			Workqueue.pop();
		        QueueLocker.unlock();
			while(1)  //在ET模式下必须循环读取数据，因为只通知一次确保数据的有效的接收
			{
				char buff[1024] = {0};
		    	        int s = recv(c,buff,1024,0);
				if(s == 0)  //客户端关闭链接
				{
					epoll_del(epollfd,c);
					close(c);
					cout<<"one client close"<<endl;
					break;
				}
	 	     		else if(s == -1)  //非阻塞模式下数据读完会返回-1
				{
					if(errno == EAGAIN || errno == EWOULDBLOCK)
					{
						send(c,"ok",2,0);   //然后修改事件内核表
						modfd(epollfd,c);
						break;
					}		
				}
				else
				{
					cout<<"buff:"<<buff<<endl;
				}
			}
	}
}
void setnonblock(int fd)
{
	int old = fcntl(fd,F_GETFL);
	old |= O_NONBLOCK;
	fcntl(fd,F_SETFL,old);
} 

void epoll_add(int epollfd,int fd,bool One_shot)
{
     if(fd == -1)
	 {
		return;
 	 }
     struct epoll_event ev;
     ev.events = EPOLLIN | EPOLLET;
     ev.data.fd = fd;
	 if(One_shot)
	 {
		ev.events |= EPOLLONESHOT;
	 }
     if(epoll_ctl(epollfd,EPOLL_CTL_ADD,fd,&ev) == -1)//将epoll_ctl封装
     {
		perror("epoll_add error");
     }
     setnonblock(fd);   //将描述符设置为非阻塞模式
}

void epoll_del(int epollfd,int fd)
{
	if(epollfd == -1 || fd == -1)
	{
		return;
	}
	if(epoll_ctl(epollfd,EPOLL_CTL_DEL,fd,0) == -1)
	{
		perror("epoll del error");
	}
}

void modfd(int epollfd,int fd)  //修改就绪事件表
{
	struct epoll_event event;
	event.data.fd = fd;
	event.events =EPOLLIN | EPOLLET | EPOLLONESHOT | EPOLLRDHUP;
	epoll_ctl(epollfd,EPOLL_CTL_MOD,fd,&event);
}


int main()
{
	int sockfd = socket(AF_INET,SOCK_STREAM,0);
   	assert(sockfd != -1);
   
   	struct sockaddr_in saddr,caddr;
   	memset(&saddr,0,sizeof(saddr));
   	saddr.sin_family = AF_INET;
   	saddr.sin_port = htons(6500);
   	saddr.sin_addr.s_addr = inet_addr("127.0.0.1");
  
   	int res = bind(sockfd,(struct sockaddr*)&saddr,sizeof(saddr));
   	assert(res != -1);
   
   	listen(sockfd,5);

   	int epollfd = epoll_create(5);//在内核中完成
   	assert(epollfd != -1);
	
	struct epoll_event events[MAX_EVENT_NUMBER];//存就绪事件描述符
   	epoll_add(epollfd,sockfd,false);

	CreateThreadPool(epollfd);

	while(1)
 	{
		int n = epoll_wait(epollfd,events,MAX_EVENT_NUMBER,-1);  //还未设置超时时间
		cout<<"n:"<<n<<endl;
		for(int i = 0;i<n;++i)
		{
			int fd = events[i].data.fd;
			if(fd == sockfd)   //如果是监听套接字
			{
				socklen_t len = sizeof(caddr);
				int c = accept(fd,(struct sockaddr *)&caddr,&len);
				if(c<0)
				{
					cout<<"accept error"<<endl;
					continue;
				}
				epoll_add(epollfd,c,true);
			}
			else if(events[i].events & EPOLLIN)  //说明连接套接字上有可读事件发生，就把它添加到任务队列中
			{
				 cout<<Append_Workqueue(fd)<<endl;
			}
			else if(events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR))
			{
				epoll_del(epollfd,fd);
				close(fd);
			}
			else
			{
			}
		}
	}
	
	close(sockfd);
	return 0;
}

