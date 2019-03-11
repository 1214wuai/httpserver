#ifndef _THREADPOOL_HPP_
#define _THREADPOOL_HPP_
#include "utils.hpp"
#include <queue>

typedef bool (*Handler) (int sock);
//http请求的处理的任务
//包含一个成员就是socket
//包含一个任务处理函数
class HttpTask
{
private:
  int _cli_sock;
  Handler TaskHandler;

public:
  HttpTask()
    : _cli_sock(-1)
  {}

  HttpTask(int sock, Handler handler)
    : _cli_sock(sock)
    , TaskHandler(handler)
  {}

  //设置任务，也就是对于这个任务类进行初始化
  void SetHttpTask(int sock, Handler handler)
  {
    _cli_sock = sock;
    TaskHandler = handler;
  }

  //执行任务处理函数
  void Run()
  {
    TaskHandler(_cli_sock);
  }

};

//线程池类
//创建指定数量的线程
//创建一个线程安全的任务队列
//提供任务的入队和出队，线程池的销毁/初始化接口
class ThreadPool
{
private:
  //当前线程池中的最大线程数
  int _max_thr;
  //当前线程池中的线程数
  int _cur_thr;
  bool _is_stop;
  std::queue<HttpTask> _task_queue;
  pthread_mutex_t _mutex;
  pthread_cond_t _cond;

private:
  void QueueLock()
  {
    pthread_mutex_lock(&_mutex);
  }

  void QueueUnLock()
  {
    pthread_mutex_unlock(&_mutex);
  }

  bool IsStop()
  {
    return _is_stop;
  }

  void ThreadExit()
  {
    _cur_thr--;
    pthread_exit(NULL);
  }

  void ThreadWait()
  {
    if (IsStop())
    {
      //若线程池销毁，则无需等待，解锁后直接退出
      QueueUnLock();
      ThreadExit();
    }
    pthread_cond_wait(&_cond, &_mutex);
  }

  void ThreadWakeUpOne()
  {
    pthread_cond_signal(&_cond);
  }

  void ThreadWakeUpAll()
  {
    pthread_cond_broadcast(&_cond);
  }

  bool QueueIsEmpty()
  {
    return _task_queue.empty();
  }

private:
  //完成线程获取任务，处理任务
  //static是为了这个函数只能有一个参数，不能有隐含的this指针
  static void* thr_start(void* arg)
  {
   while(1)
   {
    ThreadPool *tp  = (ThreadPool*)arg;
    tp->QueueLock();
    while (tp->QueueIsEmpty())
    {
      tp->ThreadWait();
    }

    HttpTask ht;
    tp->PopTask(ht);
    tp->QueueUnLock();
    ht.Run();
    
  }
   return NULL;
  }

public:
  ThreadPool(int max)
    : _max_thr(max)
    , _cur_thr(0)
    , _is_stop(false)
  {
  }

  ~ThreadPool()
  {
    pthread_mutex_destroy(&_mutex);
    pthread_cond_destroy(&_cond);
  }

  //完成线程创建，互斥锁/条件变量初始化
  bool ThreadPoolInit()
  {
    pthread_t tid;
    for (int i = 0; i < _max_thr; i++)
    {
      int ret = pthread_create(&tid, NULL, thr_start, this);
      if (ret != 0)
      {
        LOG("thread create error\n");
        return false;
      }
      pthread_detach(tid);
      _cur_thr++;
    }
    pthread_mutex_init(&_mutex, NULL);
    pthread_cond_init(&_cond, NULL);

    return true;
  }

  //线程安全的任务入队
  bool PushTask(HttpTask& tt)
  {
    std::cout << "add task!" << std::endl;
    QueueLock();
    _task_queue.push(tt);
    ThreadWakeUpOne();
    QueueUnLock();
    return true;
  }

  //线程安全的任务出队
  bool PopTask(HttpTask& tt)
  {
    //因为任务的出队是在线程接口中调用，但是
    //线程接口中再出队之前就会进行加锁，因此，这里不需要进行加解锁
    tt = _task_queue.front();
    _task_queue.pop();

    return true;
  }

  //销毁线程池
  bool ThreadPoolStop()
  {
    if (!IsStop())
    {
      _is_stop = true;
    }

    while (_cur_thr > 0)
    {
      ThreadWakeUpAll();
    }

    return true;
  }

};


#endif //_THREADPOOL_HPP
