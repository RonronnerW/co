#include "co.h"
#include <stdio.h>

#define PRODUCER "producer.txt"
#define CONSUMER "consumer.txt"

using namespace std;

// 生产者 
// 从producer.txt文件中读取数据，写入队列 
void producer(void * arg)
{
	Schedule* s = (Schedule*)arg;
	char data[DATA_SIZE];
	while(Schedule::flag) {
		// 异步读 
		s->read(PRODUCER,data);
		// 阻塞等待读完再写入队列 
		while(!Schedule::readed);
		// 将异步读完标记清零方便下次读 
		Schedule::readed = 0;
		printf("read---%s\n",data);
		// 写入队列 
		s->push(data);
		printf("push---%s\n",data);
		
		// 主动让出 
		s->co_yield();
	} 
	
}
// 消费者 
// 从队列中获取数据，写入consumer.txt文件 
void consumer(void *arg)
{
	Schedule* s = (Schedule*)arg;
	char data[DATA_SIZE];
	while(1) {
		// 从队列中获取数据 
		strncpy(data, s->pop(), DATA_SIZE);
		printf("pop---%s\n",data);
		// 异步写操作 
	    s->write(CONSUMER,data);
	    // 阻塞等待异步写完成后将标记清零 
	    while(!Schedule::writed);
		Schedule::writed = 0;
	   	printf("write---%s\n",data);
	   	// 主动让出 
	    s->co_yield();
	}
	
}

void schedule_test()
{
	// 协程调度器 
    Schedule s;
 	// 生产者协程 
    int id1 = s.co_create(producer, &s);
    // 消费者协程 
    int id2 = s.co_create(consumer, &s);
    
    // 对未执行完的协程反复唤醒执行 
    while(!s.co_finished()){
        s.co_resume(id1);
        s.co_resume(id2);
    }
 
}
int main()
{
    schedule_test();
 
    return 0;
}
