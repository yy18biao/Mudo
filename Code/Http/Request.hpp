#ifndef __M_REQUEST_H__
#define __M_REQUEST_H__

#include "Util.hpp"
#include <regex>

class Request
{
public:
    std::string _method;                                   // 请求方法
    std::string _path;                                     // 资源路径
    std::string _version;                                  // 协议版本
    std::string _body;                                     // 请求正文
    std::unordered_map<std::string, std::string> _headers; // 头部字段
    std::unordered_map<std::string, std::string> _params;  // 查询字符串
    std::smatch _matches;                                  // 资源路径的正则提取数据

public:
    Request() : _version("HTTP/1.1") {}

    // 重置请求
    void ReSet()
    {
        _method.clear();
        _path.clear();
        _version = "HTTP/1.1";
        _body.clear();
        std::smatch match;
        _matches.swap(match);
        _headers.clear();
        _params.clear();
    }

    // 设置头部字段
    void SetHeader(std::string &key, std::string &val)
    {
        _headers.insert(std::make_pair(key, val));
    }

    // 判断是否有某个头部
    bool HasHeader(const std::string &key) const
    {
        auto it = _headers.find(key);
        if (it == _headers.end())
            return false;
        return true;
    }

    // 获取头部字段的值
    std::string GetHeader(const std::string &key) const
    {
        auto it = _headers.find(key);
        if (it == _headers.end())
            return "";
        return it->second;
    }

    // 插入查询字符串
    void SetParam(const std::string &key, const std::string &val)
    {
        _params.insert(std::make_pair(key, val));
    }

    // 判断是否有某个指定的查询字符串
    bool HasParam(const std::string &key) const
    {
        auto it = _params.find(key);
        if (it == _params.end())
            return false;
        return true;
    }

    // 获取指定的查询字符串
    std::string GetParam(const std::string &key) const
    {
        auto it = _params.find(key);
        if (it == _params.end())
            return "";
        return it->second;
    }

    // 获取正文长度
    size_t ContentLength() const
    {
        bool ret = HasHeader("Content-Length");
        if (ret == false)
            return 0;
        std::string clen = GetHeader("Content-Length");
        return std::stol(clen);
    }

    // 判断是否是短链接
    bool Close() const
    {
        if (HasHeader("Connection") && GetHeader("Connection") == "keep-alive")
            return false;
        return true;
    }
};

#endif