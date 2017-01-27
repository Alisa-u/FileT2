#include<pthread.h>
#include<semaphore.h>
#include<iostream>
using namespace std;

class sem
{
public:
	sem()  //创建并初始化信号量
	{
		if(sem_init(&m_sem,0,0) != 0)
		{
			cout<<"sem() error"<<endl;
			exit(1);
		}
	}
	~sem()  //销毁信号量
	{
		sem_destroy(&m_sem);
	}
	bool wait()  //等待信号量
	{
		return sem_wait(&m_sem) == 0;
	}
	bool post()
	{
		return sem_post(&m_sem) == 0;
	}
private:
	sem_t m_sem;
};

class locker
{
public:
	locker() //创建并初始化互斥锁
	{
		if(pthread_mutex_init(&m_mutex,NULL) != NULL)
		{
			cout<<"locker() error"<<endl;
			exit(1);
		}
	}
	~locker()
	{
		pthread_mutex_destroy(&m_mutex);
	}
	bool lock()
	{
		return pthread_mutex_lock(&m_mutex) == 0;
	}
	bool unlock()
	{
		return pthread_mutex_unlock(&m_mutex) == 0;
	}
private:
	pthread_mutex_t m_mutex;
};
