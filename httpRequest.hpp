#ifndef _HTTP_REQUEST_HPP_
#define _HTTP_REQUEST_HPP_

#include "utils.hpp"
#include "threadpool.hpp"

//包含HttpRequest解析出来的请求信息
class RequestInfo
{
public:
  std::string _method;//请求方法
  std::string _version;//协议版本
  std::string _path_info;//资源路径
  std::string _path_phys;//资源实际路径  
  std::string _query_string;//查询字符串 
  std::unordered_map<std::string, std::string> _hdr_list;//头部当中的键值对  
  struct stat _st; //获取文件信息 

  size_t _part;
  std::vector<std::string> _part_list;
public:
  std::string _err_code;
public:
  void SetError(const std::string& code)
  {
    _err_code = code;
  }

  bool RequestIsCGI()
  {
    if((_method == "GET" && !_query_string.empty()) || (_method == "POST"))
    {
      std::cout<<"HttpRequest30:请求是CGI\n"<<std::endl;
      return true;
    }
    return false;
  }
//qqqq

};

//http数据的接收接口
//http数据的解析接口
//对外提供能够获取处理结构的接口
class HttpRequest
{
  private:
    int _cli_sock;
    std::string _http_header;
    RequestInfo _req_info;

  public:
    HttpRequest(int sock)
      : _cli_sock(sock)
    {}

    //接收http请求头
    bool RecvHttpHeader(RequestInfo& info)
    {
      //定义一个设置http头部最大值
      char tmp[MAX_HTTPHDR];
      while (1)
      {
        //预先读取，不从缓存区中把数据拿出来
        int ret = recv(_cli_sock, tmp, MAX_HTTPHDR, MSG_PEEK);
        //读取出错，或者对端关闭连接
        if (ret <= 0)
        {
          //EINTR表示这次操作被信号打断，EAGAIN表示当前缓存区没有数据
          if (errno == EINTR || errno == EAGAIN)
          {
            continue;
          }
          std::cout<<"HttpRequest 71:读取文件出错"<<std::endl;
          //std::cout<<tmp<<std::endl;
          info.SetError("500");
          return false;
        }
        //ptr为NULL表示tmp里面没有\r\n\r\n
        char* ptr = strstr(tmp, "\r\n\r\n");
        //当读了MAX_HTTPHDR这么多的字节，但是还是没有把头部读完，说明头部过长了
        if ((ptr == NULL) && (ret == MAX_HTTPHDR))
        {
          info.SetError("413");
          return false;
        }
        //当读的字节小于这么多，并且没有空行出现，说明数据还没有从发送端发送完毕，所以接收缓存区，需要等待一下再次读取数据
        else if ((ptr == NULL) && (ret < MAX_HTTPHDR))
        {
          usleep(1000);
          continue;
        }

        int hdr_len = ptr - tmp;//请求头的总长度
        _http_header.assign(tmp, hdr_len);
        //tmp数组里放着头部，把头部的hdr_len个字节拷贝到_http_header中

        recv(_cli_sock, tmp, hdr_len + 4, 0);
        //LOG("header:%s\n", tmp);
        std::cout<<"HttpRequest 97 接收到的报头"<<std::endl;
        LOG("header:\n%s\n", _http_header.c_str());
        break;
      }

      return true;
    }

    //判断请求报头首行的路径是否合法
    bool PathIsLegal(std::string &path, RequestInfo &info)
    {
      //GET / HTTP/1.1
      //file = www/ 
      std::string file = WWWROOT + path;
      char tmp[MAX_PATH] = {0};
      
      //使用realpath函数进行了虚拟路径转化到物理路径的时候，就自动把最后后面的一个/去掉
      realpath(file.c_str(), tmp);//realpath函数将相对路径转换成绝对路径
      
      info._path_phys = tmp;//物理路径
      //如果路径不在WWW下面
      if(info._path_phys.find(WWWROOT) == std::string::npos)
      {
        info._err_code = "403";
        return false;
      }

      //stat函数，通过路径获取文件信息
      //stat函数不需要物理路径获取文件的信息，只需要相对路径就好了
      if(stat(info._path_phys.c_str(), &(info._st)) < 0)////////////////////////////
      {
        info._err_code = "404";
        return false;
      }

      return true;
    }

    //解析首行

    // bool ParseFirstLine(std::string &line, RequestInfo &info)
    bool ParseFirstLine(std::string &line, RequestInfo &info)
    {
      std::vector<std::string> line_list;
      if(Utils::Split(line, " ", line_list) != 3)
      {
        info._err_code = "400";
        return false;
      }
      std::string url;
      info._method = line_list[0];
      url = line_list[1];
      info._version = line_list[2];
      if(info._method != "GET" && info._method != "POST" && info._method != "HEAD")
      {
        info._err_code = "405";
        return false;
      }
      if(info._version != "HTTP/0.9" && info._version != "HTTP/1.0" && info._version != "HTTP/1.1")
      {
        info._err_code = "400";
        return false;
      }

      //解析URL
      size_t pos;
      pos=url.find("?");//没有？，说明没有参数
      if(pos == std::string::npos)
      {
        info._path_info = url;
      }else{
        info._path_info = url.substr(0,pos);
        info._query_string = url.substr(pos + 1);
        //realpath函数，将相对路径转换成绝对路径，发生错误就是段错误
      }
      PathIsLegal(info._path_info, info);
      return true;
    }

    //解析http请求头
    bool ParseHttpHeader(RequestInfo &info)
    {
      //http请求头解析
      //请求方法 URL 协议版本\r\n
      //key:val\r\nkey:val
      //解析首行，其余都放到哈希表里
      std::vector<std::string> hdr_list;
      Utils:: Split(_http_header, "\r\n",hdr_list);//分割
      if(ParseFirstLine(hdr_list[0], info) == false)//ParseFirstLine(hdr_list[0], info)
      {
        return false;
      }


      //hdr_list.erase(hdr_list.begin());//首行已经解析完毕，可以删除了??????????????????????
      for (size_t i = 1; i < hdr_list.size(); i++)
      {
        size_t pos = hdr_list[i].find(": ");
        info._hdr_list[hdr_list[i].substr(0,pos)] = hdr_list[i].substr(pos+2);
      }

      std::cout<<"接受到的头部信息\n"<<std::endl;
      for(auto it = info._hdr_list.begin(); it != info._hdr_list.end(); it++)
      {
        std::cout<<"["<<it->first << "] = [" <<it->second << "]"<<std::endl;
      }
      std::cout<<"\n\n\n";

      return true;
    }


    //向外提供解析结果
    RequestInfo& GetRequestInfo(); 
};

#endif //_HTTP_REQUEST_HPP_

