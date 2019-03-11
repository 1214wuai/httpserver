#ifndef __M_UTILLS_H__
#define __M_UTILLS_H__

//#include<limits.h>
#include <iostream>
#include <string.h>
#include <string>
#include <sstream>
#include <vector>
#include <unordered_map>
#include <unistd.h>
#include <errno.h>
#include <dirent.h>
#include <stdlib.h>
#include <stdio.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <time.h>
#include <fcntl.h>
#include <signal.h>

#define MAX_BUFF 4096
#define WWWROOT "www"
#define MAX_PATH 256 
#define LOG(...) do{\
        fprintf(stdout, __VA_ARGS__);\
    }while(0)

#define MAX_HTTPHDR 4096


std::unordered_map<std::string, std::string> g_mime_type = {
  {"txt",  "application/octet-steam"},
  {"html", "text/html"},
  {"htm",  "text/html"},
  {"jpg",  "image/jpeg"},
  {"gif",  "image/gif"},
  {"zip",  "application/zip"},
  {"mp3",  "audio/mpeg"},
  {"mpeg", "video/mpeg"},
  {"unknow", "application/octet-steam"},
};

std::unordered_map<std::string, std::string> g_err_desc = {
	{"200", "OK"},
	{"400", "Bad Request"},
	{"403", "Forbiden"},
	{"404", "Not Found"},
  {"405", "Method Not Allowed"},
  {"413", "Requet Entity Too Large"},
  {"500", "Internal Server Error"},
};

class Utils{                                                                                     
  public:                                                                                        
    static int Split(std::string& src, const std::string &seg, std::vector<std::string> &list)   
    { 
      //分割成了多少数据
      int num = 0;
      size_t  idx = 0;
      size_t pos = 0;                                                                             
      while(idx < src.length())                                                                  
      { 
        pos = src.find(seg, idx);   
        if(pos == std::string::npos)                                                            
          break;//找到末尾了
        list.push_back(src.substr(idx,pos-idx));  
        num++;                  
        idx = pos+seg.length();                                                                 
      }
      //最后一条
      if(idx < src.length())                                                                     
      {                                                                                          
        list.push_back(src.substr(idx, pos - idx));                                       
        num++;                                                                                 
      }                                                                                          
      return num;                                                                              
    }

    
    //static const std::string GetErrDesc(const std::string &code)
    static const std::string GetErrDesc(std::string &code)
    {
      auto it = g_err_desc.find(code);
      if(it == g_err_desc.end())
      {
        return "UNknow Error";
      }
      return it->second;
    }

    static void TimeToGMT(time_t t, std::string &gmt)
    {
      struct tm *mt = gmtime(&t);//函数将一个时间戳转换成一个结构体
      char tmp[128]={0};
      int len;
      len = strftime(tmp, 127, "%a, %d %b %Y %H:%M:%S GMT", mt);//strftime将一个时间转为某个格式
      gmt.assign(tmp,len);
    }

    static void DigitToStr(int64_t num, std::string &str)
    {
      std::stringstream ss;
      ss<<num;
      str=ss.str();
    }

    static std::string DigitToStr(int64_t num)
    {
      std::stringstream ss;
      ss << num;
      return ss.str();
    }

    static void DigitToStrFsize(double num, std::string &str)
    {
      std::stringstream ss;
      ss<<num;
      str=ss.str();
    }

    static int64_t StrToDig(const std::string &str)
    {
      int64_t num;
      std::stringstream ss;
      ss<<str;
      ss>>num;
      return num;
    }

    static void MakeETag(int64_t ino, int64_t size, int64_t mtime, std::string &etag)
    {
      std::stringstream ss;
      ss<<"\"";
      ss<<std::hex<<ino;
      ss<<"-";
      ss<<std::hex<<size;
      ss<<"-";
      ss<<std::hex<<mtime;
      ss<<"\"";
      etag = ss.str();
    }

    static void GetMime(const std::string &file, std::string &mime)
    {
      size_t pos;
      pos = file.find_last_of(".");
      if(pos == std::string::npos)
      {
        mime = g_mime_type["unknow"];
        return;
      }
      std::string suffix = file.substr(pos+1);
      auto it = g_mime_type.find(suffix);
      if(it == g_mime_type.end())
      {

        mime = g_mime_type["unknow"];
        return;
      }
      else
      {
        mime = it->second;
      }

    }
};

#endif // __M_UTILLS_H__
