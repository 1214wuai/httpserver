#include "utils.hpp"
#include "threadpool.hpp"
#include "httpResponse.hpp"

#include "httpRequest.hpp"
#include <stdlib.h>
#include <signal.h>

#define MAX_LISTEN 5
#define MAX_THREAD 5
#define _IP_ 0


//建立一个tcp服务器端程序，接收新连接
//为新连接组织一个线程池任务，添加到新线程池中
class HttpServer
{
private:
  int _serv_sock;
  //线程池对象，可以直接new一个线程池，从而来设置线程池的线程数
  ThreadPool* _tp;

private:
  //http任务的处理函数
  static bool HttpHandler(int sock)
  {
    RequestInfo info;
    HttpRequest req(sock);//接收到的请求
    HttpResponse rsp(sock);//响应
    
    //接收http头部
    if (req.RecvHttpHeader(info) == false)
    {
      std::cout<<"httpserver34接收http头部出错"<<std::endl;
      goto out;
    }
    //解析http头部
    if (req.ParseHttpHeader(info) == false)
    {
      std::cout<<"httpserver40解析http头部出错"<<std::endl;
      goto out;
    }
    ////判断请求是否是CGI请求
    if (info.RequestIsCGI())
    {
      //若当前请求类型是CGI请求，则执行CGI响应
      std::cout<<"HttpServer 47：当前请求是CGI"<<std::endl;
      rsp.CGIHandler(info);
    }
    else 
    {
      std::cout<<"httpserver 52：不是CGI请求"<<std::endl;
      //若当前请求类型不是CGI请 求，则执行文件列表或文件下载响应
      rsp.FileHandler(info);
    }
    
    close(sock);
    return true;
out:
    std::cout<<"HttpServer 60:要处理错误了"<<std::endl;
    rsp.ErrHandler(info);
    close(sock);
    return false;
  }

public:
  HttpServer()
    : _serv_sock(-1)
    , _tp(NULL)
  {}

  //tcp服务器socket的初始化，以及线程的初始化
  bool HttpServerInit(std::string ip, std::string port)
  {
    _serv_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    
    if (_serv_sock < 0)
    {
      LOG("sock error:%s\n", strerror(errno));
      return false;
    }

    //地址复用
    int opt = 1;
    setsockopt(_serv_sock, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));

    sockaddr_in lst_addr;
    lst_addr.sin_family = AF_INET;
    lst_addr.sin_port = htons(atoi(port.c_str()));
#if _IP_
    lst_addr.sin_addr.s_addr = inet_addr(ip.c_str());
#else
    //绑定这个局域网的所有地址
    lst_addr.sin_addr.s_addr = htonl(INADDR_ANY);
#endif //_IP_
    socklen_t len = sizeof(sockaddr_in);
    
    //绑定
    if ((bind(_serv_sock, (sockaddr*)&lst_addr, len) < 0))
    {
      LOG("bind error :%s\n", strerror(errno));
      close(_serv_sock);
      return false;
    }

    if (listen(_serv_sock, MAX_LISTEN) < 0)
    {
      LOG("listen errno :%s\n", strerror(errno));
      close(_serv_sock);
      return false;
    }


    //创建线程池
    _tp = new ThreadPool(MAX_THREAD);
    if (_tp == NULL)
    {
      LOG("thread pool malloc error!!\n");
      return false;
    }

    if (_tp->ThreadPoolInit() == false)
    {
      LOG("thread pool init error!!\n");
      return false;
    }
    return true;
  }

  //开始获取客户端新连接--创建任务--任务入队
  bool Start()
  {
      while (1)
      {
        sockaddr_in cli_addr;
        socklen_t len = sizeof(sockaddr_in);
        int new_sock = accept(_serv_sock, (sockaddr*)&cli_addr, &len);
        if (new_sock < 0)
        {
          LOG("accept error :%s\n", strerror(errno));
          return false;
        }
        std::cout << "new connect!" << std::endl;
        HttpTask ht;
        ht.SetHttpTask(new_sock, HttpHandler);
        _tp->PushTask(ht);
      }

      return true;
  }
};


void Usage(const std::string proc)
{
#if _IP_
  std::cout << "Usage: " << proc << "ip port" << std::endl;
#else
  std::cout << "Usage: " << proc << " port" << std::endl;
#endif
}

int main(int argc, char* argv[])
{
#if _IP_
  if (argc != 3)
  {
    Usage(argv[0]);
    exit(1);
  }
  signal(SIGPIPE, SIG_IGN);//重要，连接
  HttpServer server;
  if(server.HttpServerInit(argv[1], argv[2]) == false)
  {
    return -1;
  }
#else

  if (argc != 2)
  {
    Usage(argv[0]);
    exit(1);
  }
  signal(SIGPIPE, SIG_IGN);//重要，连接
  HttpServer server;
  if(server.HttpServerInit(argv[0], argv[1]) == false)
  {
    return -1;
  }
#endif


 if(server.Start() == false)
 {
   return -1;
 }
  return 0;
}
