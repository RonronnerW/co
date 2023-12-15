#ifndef CO_CPP
#define CO_CPP
 
#include "co.h"

int Schedule::readed = 0;
int Schedule::writed = 0;
int Schedule::flag = 1;

// 创建协程并唤醒 
int Schedule::co_create(Fun func, void *arg) {
	// 先找到空闲位置存放协程 
	int id = 0;
	for(id = 0; id < cap; id ++) {
		if (cos[id].state == FREE) {
			break;
		}
	}
	// 扩容 
	if(id == cap) {
		cap *= 2;
		co* new_cos = new co[cap];
		for(int i = 0; i < cap/2; i ++) {
			new_cos[i] = cos[i];
		}
		for(int i = cap/2; i < cap; i ++) {
			new_cos[i].state = FREE;
		}
		cos = new_cos;
	}
	 
	co *t = &cos[id];
			
	getcontext(&(t->ctx)); //获取调用方当前的上下文，并保存到协程的ctx中 
	t->state = RUNNABLE;//变换状态
	t->func = func;//绑定函数
	t->arg = arg;//绑定参数
	
	t->ctx.uc_stack.ss_sp = t->stack;
    t->ctx.uc_stack.ss_size = STACK_SZIE;
    t->ctx.uc_stack.ss_flags = 0;
    t->ctx.uc_link = &main;
	
	running_co = id;

	// 用来设置对应 ucontext 的执行函数和参数 
	// 设置 函数指针 和 堆栈 到对应context保存的sp和pc寄存器中
	makecontext(&(t->ctx),(void (*)(void))(co_func), 1, this);

	swapcontext(&main, &(t->ctx));//保存当前上下文到main中，然后激活t->ctx上下文, 切换协程 
	
	return id;
    
}

// 启动协程 
void Schedule::co_resume(int id) {
	if(id < 0 || id >= cap){//id不合法的情况
        return;
    }
 
    co *t = &cos[id];
    
 	if (t->state == SUSPEND) {
    	running_co = id;
    	t->state = RUNNABLE;
        swapcontext(&main,&(t->ctx)); //保存当前上下文到main中，然后激活t->ctx上下文
    }

}

// 挂起协程 
void Schedule::co_yield() {
	if(running_co != -1 ){
        co *t = &cos[running_co];//找到该协程
        t->state = SUSPEND;//挂起
        //回到主协程 
        running_co = -1;
        swapcontext(&(t->ctx),&main);//保存当前上下文到t->ctx，然后激活main上下文, 切换到主协程 
    }

}

// 检查协程是否全部结束
int Schedule::co_finished() {
	if (running_co != -1) {
		return 0;
	}
	else {
		for(int i = 0; i < cap; i ++){
			if(cos[i].state != FREE){
			    return 0;
			}
    	}
	}
    
    return 1; 
}

// 队列中放数据 
// 使用互斥量和条件变量对队列加锁解锁保证安全 
void Schedule::push(char* x) {
	std::unique_lock<std::mutex> lock(mutex_);
    q.push(Data(x));
    lock.unlock();
    condition_.notify_one(); // 通知等待的协程
}

// 队列中取数据 
char* Schedule::pop() {
	std::unique_lock<std::mutex> lock(mutex_);
	// 队列空 pop 操作会阻塞等待 
    while (q.empty()) {
    	condition_.wait(lock);
    }

    Data value = q.front();
    q.pop();
    char* result = new char[DATA_SIZE];
    strncpy(result, value.data, DATA_SIZE);
    return result;
}

// 异步读 使用信号通知读结束 
void Schedule::read(const char* filename, char* data) {
    struct aiocb cb;
    
    // 设置信号处理函数
    struct sigaction sa;
    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = aio_readed;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGUSR1, &sa, nullptr);

    memset(&cb, 0, sizeof(struct aiocb));
    // 只有第一次读操作执行open操作，while保证open成功 
	while (fdr < 0)
    {
        fdr = open(filename, O_RDONLY);
    }

    cb.aio_fildes = fdr; 
    cb.aio_buf = data;
    cb.aio_nbytes = DATA_SIZE;
    cb.aio_offset = offset; 
    // 偏移量增加，往后读取文件内容 
    offset+=DATA_SIZE;    
    
    // 信号通知相关结构 
    cb.aio_sigevent.sigev_notify = SIGEV_SIGNAL;
    cb.aio_sigevent.sigev_signo = SIGUSR1;
    cb.aio_sigevent.sigev_value.sival_ptr = &cb;
    
    // 异步读取文件 
    int ret = aio_read(&cb);
    if (ret < 0) {
    	puts("aio_read() failed");
    } 
    
    //while(aio_error(&cb) == EINPROGRESS);//阻塞等待读完毕 
    
    //printf("read----%s\n",data);

}

// 异步写 
void Schedule::write(const char* filename, char* data) {
	struct aiocb cb;
	
	// 设置信号处理函数
    struct sigaction sa;
    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = aio_writed;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGUSR2, &sa, nullptr);

    memset(&cb, 0, sizeof(struct aiocb));
    
    while (fdw < 0)
    {
        fdw = open(filename, O_WRONLY | O_APPEND);
    }

    cb.aio_fildes = fdw; 
    cb.aio_buf = data;
    cb.aio_nbytes = DATA_SIZE;
    //cb.aio_offset = 0; 追加写，不用设置偏移量 
    
    // 信号通知结构相关  
    cb.aio_sigevent.sigev_notify = SIGEV_SIGNAL;
    cb.aio_sigevent.sigev_signo = SIGUSR2;
    cb.aio_sigevent.sigev_value.sival_ptr = &cb;
    
    // 异步写文件 
    int ret = aio_write(&cb);
    if (ret < 0)
        puts("aio_write() failed");
    
    //while(aio_error(&cb) == EINPROGRESS);//等待完

    //printf("write----%s\n", data);   
}

void co_func(Schedule* s) {
    int id = s->getRunning_co();
 
    if(id != -1){
        co *t = &(s->getCos()[id]);
 
        t->func(t->arg);
 
        t->state = FREE;
              
        s->setRunning_co(-1);//返回主函数 
    }
}

// 读完成通知处理函数 
void aio_readed(int signo, siginfo_t* info, void* context)
{

    struct aiocb* cb = (struct aiocb*)(info->si_value.sival_ptr);
    // 读完所有文件设置标志 
    if(aio_return(cb)==0) {
    	Schedule::flag = 0;
    	return;
    }
    // 异步读结束标志 
    if (info->si_signo == SIGUSR1) {
        Schedule::readed = 1;
    }
    
}
// 写完成通知处理函数 
void aio_writed(int signo, siginfo_t* info, void* context)
{

	if (info->si_signo == SIGUSR2) {
        Schedule::writed = 1;
    }
}


#endif
