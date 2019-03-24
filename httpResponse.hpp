#ifndef _HTTP_REPONSE_HPP_
#define _HTTP_REPONSE_HPP_

#include "utils.hpp"
#include "threadpool.hpp"
#include "httpRequest.hpp"
#include <stdlib.h>
#include <signal.h>

class HttpResponse
{
  //文件请求（完成文件下载/列表功能）接口
  //CGI请求接口
  private:
    int _cli_sock;
    //ETag: "inode-fsize-mtime"\r\n每个文件的inode都是唯一的,文件大小，最后一次修改的时间
    std::string _etag;//是否被修改
    std::string _mtime;//最后一次修改时间
    std::string _date; //系统响应时间
    std::string _filesize;//文件大小
    std::string _mime;//文件类型
    //std::string _cont_len;//客户端请求正文大小
  public:
    HttpResponse(int sock):_cli_sock(sock){}
    bool InitResponse(RequestInfo req_info)//初始化一些请求的响应信息 
    {
      //req_info.st.st_size;
      //req_info.st.st_ino;
      //req_info.st.st_mtime;

      //Last-Modified:
      Utils::TimeToGMT(req_info._st.st_mtime,_mtime);

      //ETag:
      Utils::MakeETag( req_info._st.st_ino, req_info._st.st_size, req_info._st.st_mtime, _etag);

      //Date:
      time_t t = time(NULL);
      Utils::TimeToGMT(t, _date);

      //fszie;
      Utils::DigitToStr(req_info._st.st_size,_filesize);
      
      //mime
      Utils::GetMime(req_info._method,_mime);
      return true;
    }

    //发送头部
    bool SendData(std::string buf)
    {

      if(send(_cli_sock, buf.c_str(), buf.length(), 0) < 0)
      {
        return false;
      }
      return true;
    }

    //发送正文
    bool SendCData(const std::string& buf)
    {
      //std::cout << "In SendCData" << std::endl;
      //发送hello word
      //0x05\r\n                :发送的数据大小
      //hello word\r\n
      //最后一个分块
      //0\r\n\r\n
      if(buf.empty())
      {
        //return SendData("0\r\n"); //最后一个分块
        return SendData("0\r\n\r\n");
      }

      std::stringstream ss;
      ss<<std::hex<<buf.length()<< "\r\n";//std::hex表示十六进制输出


      SendData(ss.str());
      ss.clear();

      SendData(buf);
      SendData("\r\n");

      return true;
    }

    bool IsPartDownload(RequestInfo& info)
    {
      auto it = info._hdr_list.find("If-Range");
      std::cout<<"HttpRespons 91 : etag:" <<_etag<<"\n_date: "<<_date<<std::endl;
      if(it == info._hdr_list.end())
      {
        return false;//不是断点续传
      }
      if(it->second == _mtime || it->second == _etag)//最后一次修改时间，是否被修改
        ;
      else 
        return false;
      it = info._hdr_list.find("Range");
      if(it == info._hdr_list.end())
      {
        return false;
      }
      else
      {
        std::string range = it->second;
        std::cout << "range:" << range << std::endl;
        info._part = Utils::Split(range,", ", info._part_list);
        return true;
      }
      return true;
    }

    bool ProcessPartDownload(RequestInfo& info, int i)
    {
      std::cout<< "HttpResponse 116 :In ProcessPartDownload" <<std::endl;
      std::string range = info._part_list[i];
      if(i == 0)
      {
        //第一个发送的块，要将bytes=去掉
        range.erase(range.begin(), range.begin() + 6);
      }
      std::cout << "HttpResponse 124 : delete range: " << range << std::endl;

      size_t pos = range.find("-");
      int64_t start = 0;
      int64_t end = 0;
      //-500,最后500个字节
      if(pos == 0)
      {
        end = Utils::StrToDig(_filesize) -1;
        start = end - Utils::StrToDig(range.substr(pos+1));
      }
      //500-,从此开始到文件最后
      else if(pos == (range.size()-1))
      {
        end = Utils::StrToDig(_filesize) -1;
        range.erase(pos,1);//将-删除
        start = Utils::StrToDig(range); 
      }
      //200-500，者一段的数据进行传输
      else
      {
        start = Utils::StrToDig(range.substr(0,pos));
        end = Utils::StrToDig(range.substr(pos+1));
      }

      //构造响应头
      std::string rsp_header;
      rsp_header = info._version + "206 PARTIAL CONTENT\r\n";
      rsp_header += "Content-Type: " + _mime + "\r\n";
      
      std::string len;
      Utils::DigitToStr(end-start+1,len);
      
      rsp_header += "Connection: close\r\n";
      rsp_header += "Content-Range: bytes " + Utils::DigitToStr(start)+ "-" + Utils::DigitToStr(end) + "/" + _filesize + "\r\n";
      
      rsp_header += "Content-Length: " + len + "\r\n";
      rsp_header += "Accept_Ranges: bytes\r\n";
      if(info._part_list.size() > 1)
      {
        rsp_header += "Content-Type: multipart/byteranges\r\n";
      }
      rsp_header += "Etag: " + _etag + "\r\n";
      rsp_header += "Last-Modified: " + _mtime + "\r\n";
      rsp_header += "Date: " + _date + "\r\n\r\n";
      SendData(rsp_header);

      std::cout<<"HttpResponse.cpp 170 : part:rsp_header\n" << rsp_header << std::endl;
      std::cout<<"HttpResponse.cpp 171 : phys:" << info._path_phys.c_str() << std::endl;

      int file_fd = open(info._path_phys.c_str(),O_RDONLY);
      if(file_fd < 0)
      {
        std::cout << "HttpRespons.cpp 176 : error open!" << std::endl;
        info._err_code = "400";
        ErrHandler(info);
        return false;
      }
      lseek(file_fd, start,SEEK_SET);
      int64_t title_len = end - start + 1;//需要发送多少数据
      int64_t rlen;//读取出来多少数据
      int64_t flen = 0;//已经发送了多少数据
      char tmp[MAX_BUFF];
      while((rlen = read(file_fd, tmp, MAX_BUFF)) > 0)
      {
        if(flen + rlen > title_len)
        {
          send(_cli_sock, tmp, title_len - flen, 0);//如果刚刚读取的数据加上你已经发送的数据大于你需要发送的数据
          break;
        }
        else
        {
          flen += rlen;
          send(_cli_sock, tmp, rlen, 0);
        }
      }
      close(file_fd);
      return true;

    }
    bool ProcessFile(RequestInfo &info)//文件下载功能
    {

      //下载时断网，来网了继续下载，此时就是断点续传
      std::string rsp_header;
      //printf("In 文件下载功能\n");
      rsp_header = info._version + " 200 OK\r\n";
      rsp_header += "Content-Type: " + _mime + "\r\n";
      //rsp_header += "application/octet-steam";
      //rsp_header += "\r\n";
      rsp_header += "Connection: close\r\n";
      rsp_header += "Content-Length: " + _filesize + "\r\n";
      rsp_header += "ETag: " + _etag + "\r\n";
      rsp_header += "Last-Modified: " + _mtime + "\r\n";
      rsp_header += "Date: " + _date + "\r\n\r\n";
      SendData(rsp_header);

      int fd = open(info._path_phys.c_str(), O_RDONLY);
      if(fd < 0)
      {
        info._err_code = "400";
        ErrHandler(info);
        return false;
      }
      int rlen = 0;
      char tmp[MAX_BUFF];
      while((rlen = read(fd, tmp, MAX_BUFF)) > 0)
      {
        send(_cli_sock, tmp, rlen, 0);
        //tmp[rlen] = '\0';
        //SendData(tmp);
        //使用这种方式发送，数据会被转化为string类型
        //如果文本中有\0，每次发送的数据没有发送完毕
        //对端关闭连接，发送数据send就会受到SIGPIPE信号，默认终止进程
      }
      close(fd);
      return true;
    }

    
    bool ProcessList(RequestInfo &info)//文件列表功能
    {
      //组织头部
      //首行
      //Content_Type：text/html\r\n
      //ETag: \r\n
      //Date: \r\n
      //Transfer-Encoding: chunked\r\n 块传输
      //Connection: close\r\n\r\n

      //正文：
      //每一个目录的文件都要组织一个html标签信息
      std::string rsp_header;

      rsp_header = info._version + " 200 OK\r\n";
    //  rsp_header += "Content-Type:text/html\r\n";
      rsp_header += "Connection: close\r\n";
      if(info._version == "HTTP/1.1")
      {
        rsp_header += "Transfer-Encoding: chunked\r\n";//分块传输，每发送一块数据之前都会告诉对方这个数据多长
      }
      rsp_header += "ETag: " + _etag + "\r\n";
      rsp_header += "Last-Modified: " + _mtime + "\r\n";
      rsp_header += "Date: " + _date + "\r\n\r\n";
      SendData(rsp_header);

      std::string rsp_body;
      rsp_body = "<html><head>";

      rsp_body += "<title>WG";//网页标题
      rsp_body += "</title>";
      
      rsp_body += "<meta charset='UTF-8'>";//meta：对于一个html页面中的元信息
      rsp_body += "</head><body>";

      rsp_body += "<h1>Welcome to my server";
      rsp_body += "</h1>";
      
      //form表单为了出现上传按钮，请求的资源是action，请求方法是POST
      rsp_body += "<form action='/upload' method='POST' enctype='multipart/form-data'>";
      //action:给出upload程序的路径，method：方法，POST请求或者GET请求且QUERY!=empty，凡是要提交数据的，都不在服务器里处理，而是启动一个子进程处理
      rsp_body += "<input type='file' name='FileUpLoad' />";//上传一个文件
      rsp_body += "<input type='file' name='FileUpLoad' />";//上传两个文件
      rsp_body += "<input type='submit' value='上传' />";
      rsp_body += "</form>";
      rsp_body += "<hr /><ol>";// <hr />是横线 <ol>排序
      SendCData(rsp_body);

      struct dirent** p_dirent = NULL;
      //scandir函数,的哥参数，第二个参数：三级指针，第三个参数：filter过滤掉“.”给1，不过滤给0，第四个参数来进行排序
      //获取目录下的每一个文件，组织html信息，chunke传输
      //readdir函数
      // file_html = "<li>";
      int num = scandir(info._path_phys.c_str(), &p_dirent, NULL, alphasort);
      for(int i = 0; i< num; i++)
      {
        std::string file_path;
        std::string file_html; 
        file_path += info._path_phys + p_dirent[i]->d_name; 
        struct stat st;
        //获取文件信息
        if(stat(file_path.c_str(), &st) < 0)
        {
          continue;
        }

        //std::cout<<"HttpResponse193: lllllkkkkklllllll"<<std::endl;
        std::string mtime;
        std::string mime;
        std::string fsize;
        Utils::TimeToGMT(st.st_mtime, mtime);
        Utils::GetMime(p_dirent[i]->d_name, mime);
        Utils::DigitToStrFsize(st.st_size / 1024, fsize);
        file_html += "<li><strong><a href='"+ info._path_info;//href+路径，点击就会连接，进入到一个文件或者目录之后，给这个文件或者目录的网页地址前面加上路径
        file_html += p_dirent[i]->d_name;
        file_html += "'>";
        file_html += p_dirent[i]->d_name;
        file_html += "</a></strong>";
        file_html += "<br /><small>";
        file_html += "modified: " + mtime + "<br />";
        file_html += mime + " - " + fsize + " kbytes";
        file_html += "<br /><br /></small></li>";

        SendCData(file_html);
      }
      rsp_body = "</ol><hr /></body></html>";
      SendCData(rsp_body);
      //进行分块发送的时候告诉客户端已经方法送完毕了，不再让客户端进行洗个等待正文的过程了
      SendCData("");
      return true;
    }		
    bool ProcessCGI(RequestInfo &info)//cgi请求处理
    {
    

      std::cout<<"HttpResponse 221:cgi请求处理\nPHYS PATH"<< info._path_phys <<std::endl;
      //使用外部程序完成CGI处理---文件上传
      //将http头信息和正文全部交给子进程处理
      //使用环境变量传递头信息
      //创建管道传递正文顺序
      //使用管道接收cgi程序的处理结果
      //流程：创建管道，创建子进程，设置子进程环境变量，程序替换
      int in[2];//向子进程传递正文数据
      int out[2];//从子进程读取处理结果
      if(pipe(in) || pipe(out))
      {
        info._err_code = "500";
        ErrHandler(info);
        return false;
      }
      int pid = fork();
      if(pid < 0)
      {
        info._err_code = "500";
        ErrHandler(info);
        return false;
      }
      else if(pid == 0)
      {
        //setenv函数，设置环境变量，第三个参数给非0，覆盖
        setenv("METHOD", info._method.c_str(), 1);
        setenv("VERSION", info._version.c_str(), 1);
        setenv("PATH_INFO", info._path_info.c_str(),1);//上传不需要实际物理路径
        setenv("QUERY_STRING", info._query_string.c_str(), 1);
        for(auto it = info._hdr_list.begin(); it != info._hdr_list.end(); it++)
        {
          setenv(it->first.c_str(), it->second.c_str(), 1);
        }
        close(in[1]);
        close(out[0]);
        dup2(in[0],0);//子进程将从标准输入读取正文数据
        dup2(out[1],1);//子进程直接打印处理结果传递给父进程
        execl(info._path_phys.c_str(), info._path_phys.c_str(), NULL);
        //第一个参数：要执行的文件的路径，
        //第二个参数：表示如何让执行这个二进制程序
        exit(0);
      }
      //父进程接下来就是对管道进行操作
      //1.通过in管道传递正文数据给子进程
      //2.通过out管道读取子进程的处理结果直到返回0
      //3.将数据处理结果组织http数据，响应给客户端
      close(in[0]);
      close(out[1]);
      //1
      auto it = info._hdr_list.find("Content-Length");
      if(it!=info._hdr_list.end())
      {
        char buf[MAX_BUFF] = {0};
        int64_t content_len = Utils::StrToDig(it->second);
        LOG("content-len:[%ld]\n",content_len);

        int tlen = 0;//当前读取的长度
        while(tlen<content_len)
        {
          int len = MAX_BUFF > (content_len - tlen) ? (content_len -tlen) : MAX_BUFF;//剩余接收的大小没有buf那么长，就不接buf那么长了,防止粘包
          int rlen = recv(_cli_sock, buf, len, 0);
          if(rlen <= 0)
          {
            std::cout<<"HttpResponse:284 响应错误给客户端"<<std::endl;
            return false;
          }
          if(write(in[1], buf, rlen) < 0)
          {
            return false;
          }
          tlen += rlen;
        }

      }
      //每一个目录的文件都要组织一个html标签信息
      //2通过out管道读取子进程的处理结果
      //3将处理结果组织http资源，响应给客户端
      std::string rsp_header;

      rsp_header = info._version + " 200 OK\r\n";
      rsp_header += "Content-Type: text/html\r\n";
      rsp_header += "Connection: close\r\n";
      //if(info._version == "HTTP/1.1")
      //{
      //  rsp_header += "Transfer-Encoding: chunked\r\n";//分块传输，每发送一块数据之前都会告诉对方这个数据多长
      //}
      rsp_header += "ETag: " + _etag + "\r\n";
      rsp_header += "Last-Modified: " + _mtime + "\r\n";
      rsp_header += "Data: " + _date + "\r\n\r\n";
      SendData(rsp_header);
      while(1)
      {
        char buf[MAX_BUFF] = {0};
        int rlen = read(out[0], buf, MAX_BUFF);
        if(rlen == 0)
        {
          break;
        }
        //std::cout << "buf->"<<buf << std::endl;
        send(_cli_sock, buf, rlen, 0);
      }
      close(in[1]);
      close(out[0]);

      return true;
    }

    //真正对外的接口
    bool ErrHandler(RequestInfo &info)//处理错误响应
    {
      std::string rsp_header;
      std::string rsp_body;
      //首行 协议版本 状态码 状态描述
      //头部
      //空行
      //正文
      rsp_header = info._version + " " +info._err_code + " ";
      rsp_header += Utils::GetErrDesc(info._err_code)+ "\r\n";

      time_t t = time(NULL);
      std::string gmt;
      Utils::TimeToGMT(t,gmt);
      rsp_header += "Date: " +gmt + "\r\n";

      std::string cont_len;
      rsp_body = "<html><body><h1>" + info._err_code;
      rsp_body += "<h1></body></html>";
      Utils::DigitToStr(rsp_body.length(), cont_len);
      rsp_body += "Content_Length: " + cont_len + "\r\n\r\n";

      //std::cout<<"\r\n\r\n";
      //std::cout<<rsp_header<<std::endl;
      //std::cout<<rsp_body<<std::endl;
      //std::cout<<"\n\n\n\n";



      send(_cli_sock, rsp_header.c_str(), rsp_header.length(), 0);
      send(_cli_sock, rsp_body.c_str(), rsp_body.length(), 0);
      return true;
    }

    bool FileIsDir(RequestInfo &info)
    {
      std::string path_info = info._path_info;
      std::string path_phys = info._path_phys;
      if(info._st.st_mode & S_IFDIR)
      {
        if(path_info[path_info.length() -1] != '/')
          info._path_info.push_back('/');
        if(path_phys[path_phys.length() -1] != '/')
          info._path_phys.push_back('/');
        return true;
      }

      std::cout <<  info._path_phys << std::endl;
      std::cout <<  info._path_info << std::endl;
      return false;
    }
    
    bool CGIHandler(RequestInfo &info)//处理上传响应
    {
      InitResponse(info);//初始化CGI响应信息
      ProcessCGI(info);
      // FileIsDir(info)
      return true;
    }

    bool FileHandler(RequestInfo &info)//处理文件展示功能
    {
      InitResponse(info);//初始化文件响应信息
      if (FileIsDir(info))
      {
        std::cout<<"HttpResponse.cpp 509: 文件列表展示功能"<<std::endl;
        ProcessList(info);//执行文件列表展示响应
      }
      else
      {
        if(IsPartDownload(info))
        {
        std::cout<< "HttpResponse.cpp 516 : " << info._part << std::endl;
          for(size_t i = 0; i < info._part; i++)//_part表示分几次断点续传
          {
            ProcessPartDownload(info, i);
          }
        }
        else
        {

        std::cout<<"HttpResponse.cpp 525 : 文件下载功能"<<std::endl;
        ProcessFile(info);//执行文件下载响应
        }
      }
      return true;
    }

};

#endif //_HTTP_REPONSE_HPP


