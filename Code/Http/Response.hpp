#ifndef __M_RESPONSE_H__
#define __M_RESPONSE_H__

#include "Request.hpp"

class Response
{
public:
    int _statu;                                            // 状态码
    bool _redirect_flag;                                   // 重定向标志
    std::string _body;                                     // 响应正文
    std::string _redirect_url;                             // 重定向路径
    std::unordered_map<std::string, std::string> _headers; // 头部字段

public:
    HttpResponse() : _redirect_flag(false), _statu(200) {}
    HttpResponse(int statu) : _redirect_flag(false), _statu(statu) {}

    // 重置响应
    void ReSet()
    {
        _statu = 200;
        _redirect_flag = false;
        _body.clear();
        _redirect_url.clear();
        _headers.clear();
    }

    // 插入头部字段
    void SetHeader(const std::string &key, const std::string &val)
    {
        _headers.insert(std::make_pair(key, val));
    }

    // 判断是否存在指定头部字段
    bool HasHeader(const std::string &key)
    {
        auto it = _headers.find(key);
        if (it == _headers.end())
            return false;
        return true;
    }

    // 获取指定头部字段的值
    std::string GetHeader(const std::string &key)
    {
        auto it = _headers.find(key);
        if (it == _headers.end())
            return "";
        return it->second;
    }

    // 设置响应内容
    void SetContent(const std::string &body, const std::string &type = "text/html")
    {
        _body = body;
        SetHeader("Content-Type", type);
    }

    // 设置重定向
    void SetRedirect(const std::string &url, int statu = 302)
    {
        _statu = statu;
        _redirect_flag = true;
        _redirect_url = url;
    }

    // 判断是否是短链接
    bool Close()
    {
        if (HasHeader("Connection") == true && GetHeader("Connection") == "keep-alive")
            return false;
        return true;
    }
};

#endif