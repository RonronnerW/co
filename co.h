#ifndef CO_H
#define CO_H

#include <ucontext.h>
#include <cstring>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <aio.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>

#define STACK_SZIE 1024*32	//stack大小
#define MAX_CO_SIZE 16 		//最多有几个协程
#define DATA_SIZE 2			//队列和文件读写操作数据大小 
/*
FREE：创建态 
RUNNABLE：运行态 
SUSPEND：挂起态
*/
enum CoState{FREE, RUNNABLE, SUSPEND};

typedef void (*Fun)(void *arg);

// 通信数据类型 
typedef struct Data
{
	char data[DATA_SIZE];
	Data(const char* str) {
		strncpy(data, str, DATA_SIZE);
	}
}Data;

//协程的定义 
typedef struct Co
{
    ucontext_t ctx; 		//协程上下文 
    Fun func; 				//协程用户执行的函数 
    void *arg; 				//函数参数 
    enum CoState state; 	//协程状态 
    char stack[STACK_SZIE]; //协程的栈 
}co;

//协程调度器
class Schedule
{
	private:
	    ucontext_t main;	//当前运行协程的上下文 
	    int running_co; 	//正在运行协程的id 
	    int cap;			//调度器容量 
	    co *cos; 			//存放协程 
	    std::queue<Data> q;	//协程间通信的队列
	    int fdr;			//读文件操作符 
	    int fdw;			//写文件操作符 
	    int offset;			//异步IO的偏移量 
	    std::mutex mutex_;	//互斥锁 
		std::condition_variable condition_;//条件变量 
	public:
		static int flag;			//文件已读完标志 
	    static int readed;			//文件读完通知信号
		static int writed;			//文件写完通知队列 
 	
 	public: 
		// 协程调度器初始化 
	    Schedule():running_co(-1), cap(MAX_CO_SIZE), fdr(-1), fdw(-1), offset(0) {//一开始设置为-1
	        cos = new co[MAX_CO_SIZE];
	        for (int i = 0; i < MAX_CO_SIZE; i++) {
	            cos[i].state = FREE;
	        }
	    }
	    
	    ~Schedule() {
	        delete[] cos;
	        close(fdr);
	        close(fdw);
	    }
	    int getRunning_co() {
	    	return running_co;
	    }
	    void setRunning_co(int co) {
	    	running_co = co;
	    }
	    co* getCos() {
	    	return cos;
	    }
	    int getQueueSize() {
			return q.size();
		}
	    
	    // 创建协程 
		int  co_create(Fun func, void *arg);
		
		// 启动协程 
		void co_resume(int id);
		
		// 挂起协程 
		void co_yield();
		
		// 检查协程是否全部执行完 
		int co_finished();
		
		// 队列中放入数据 
		void push(char* data);
		
		// 队列中取数据 
		char* pop();
		
		// 异步读 
		void read(const char* filename, char* data);
		
		// 异步写 
		void write(const char* filename, char* data);
		
};
// 协程执行函数 
void co_func(Schedule* s);
// 异步读信号通知处理函数 
void aio_readed(int signo, siginfo_t* info, void* context);
// 异步写信号通知处理函数 
void aio_writed(int signo, siginfo_t* info, void* context);

#endif
