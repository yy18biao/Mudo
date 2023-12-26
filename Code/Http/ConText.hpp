#ifndef __M_CONTEXT_H__
#define __M_CONTEXT_H__

#include "Response.hpp"

typedef enum
{
    RECV_ERROR,
    RECV_LINE,
    RECV_HEAD,
    RECV_BODY,
    RECV_OVER
} RecvStatu;

#define MAX_LINE 8192
class ConText
{
private:
    int _respStatu;       // 响应状态码
    RecvStatu _recvStatu; // 当前解析阶段状态
    Request _request;     // 解析得到的请求信息

private:
    // 解析请求行
    bool ParseLine(const std::string &line)
    {
        std::smatch matches;
        std::regex e("(GET|HEAD|POST|PUT|DELETE) ([^?]*)(?:\\?(.*))? (HTTP/1\\.[01])(?:\n|\r\n)?", std::regex::icase);
        bool ret = std::regex_match(line, matches, e);
        if (ret == false)
        {
            _recvStatu = RECV_ERROR;
            _respStatu = 400;
            return false;
        }
        // 请求方法的获取
        _request._method = matches[1];
        std::transform(_request._method.begin(), _request._method.end(), _request._method.begin(), ::toupper);

        // 资源路径的获取需要进行URL解码操作，但是不需要+转空格
        _request._path = Util::UrlDecode(matches[2], false);

        // 协议版本的获取
        _request._version = matches[4];

        // 查询字符串的获取与处理
        std::vector<std::string> query_string_arry;
        std::string query_string = matches[3];
        // 查询字符串的格式 key=val&key=val..... 先以 & 符号进行分割得到各个字串
        Util::Split(query_string, "&", &query_string_arry);
        // 针对各个字串以 = 符号进行分割得到key和val 得到之后也需要进行URL解码
        for (auto &str : query_string_arry)
        {
            size_t pos = str.find("=");
            if (pos == std::string::npos)
            {
                _recvStatu = RECV_ERROR;
                _respStatu = 400;
                return false;
            }
            std::string key = Util::UrlDecode(str.substr(0, pos), true);
            std::string val = Util::UrlDecode(str.substr(pos + 1), true);
            _request.SetParam(key, val);
        }
        return true;
    }

    // 获取请求行
    bool RecvLine(Buffer *buf)
    {
        // 获取一行数据 需要带有换行符避免获取的数据并不是一行
        std::string line = buf->GetLine_Data();
        if (line.size() == 0)
        {
            // 缓冲区中的数据不足一行则需要判断缓冲区的可读数据长度，如果很长了都不足一行是有问题的
            if (buf->Get_Read_AbleSize() > MAX_LINE)
            {
                _recvStatu = RECV_ERROR;
                _respStatu = 414;
                return false;
            }
            // 缓冲区中数据不足一行，但是不多等新数据的到来
            return true;
        }

        if (line.size() > MAX_LINE)
        {
            _recvStatu = RECV_ERROR;
            _respStatu = 414;
            return false;
        }

        // 解析请求行
        if (!ParseLine(line))
            return false;
        // 首行处理完毕，进入头部获取阶段
        _recvStatu = RECV_HEAD;
        return true;
    }

    // 解析请求头部
    bool ParseHead(std::string &line)
    {
        // 末尾是换行/回车则去掉换行字符
        if (line.back() == '\n')
            line.pop_back();
        if (line.back() == '\r')
            line.pop_back();
        size_t pos = line.find(": ");
        if (pos == std::string::npos)
        {
            _recvStatu = RECV_ERROR;
            _respStatu = 400;
            return false;
        }
        std::string key = line.substr(0, pos);
        std::string val = line.substr(pos + 2);
        _request.SetHeader(key, val);
        return true;
    }

    // 获取请求头部
    bool RecvHead(Buffer *buf)
    {
        // 一行一行取出数据，直到遇到空行为止
        if (_recvStatu != RECV_HEAD)
            return false;
        while (1)
        { // 获取一行数据 需要带有换行符避免获取的数据并不是一行
            std::string line = buf->GetLine_Data();
            if (line.size() == 0)
            {
                // 缓冲区中的数据不足一行则需要判断缓冲区的可读数据长度，如果很长了都不足一行是有问题的
                if (buf->Get_Read_AbleSize() > MAX_LINE)
                {
                    _recvStatu = RECV_ERROR;
                    _respStatu = 414;
                    return false;
                }
                // 缓冲区中数据不足一行，但是不多等新数据的到来
                return true;
            }

            if (line.size() > MAX_LINE)
            {
                _recvStatu = RECV_ERROR;
                _respStatu = 414;
                return false;
            }

            if (line == "\n" || line == "\r\n")
                break;

            // 解析请求行
            if (!ParseHead(line))
                return false;
        }
        // 头部处理完毕，进入正文获取阶段
        _recvStatu = RECV_BODY;
        return true;
    }

    // 获取请求正文
    bool RecvBody(Buffer *buf)
    {
        if (_recvStatu != RECV_BODY)
            return false;
        // 获取正文长度
        size_t content_length = _request.ContentLength();
        if (content_length == 0)
        {
            // 没有正文，则请求接收解析完毕
            _recvStatu = RECV_OVER;
            return true;
        }

        // 当前已经接收了多少正文,其实就是往 _request._body 中放了多少数据了
        // 实际还需要接收的正文长度
        size_t real_len = content_length - _request._body.size();
        // 接收正文放到body中，但是也要考虑当前缓冲区中的数据是否是全部的正文
        // 缓冲区中数据包含了当前请求的所有正文 则取出所需的数据
        if (buf->Get_Read_AbleSize() >= real_len)
        {
            _request._body.append(buf->Get_Read_Start_Pos(), real_len);
            buf->Move_Read_Offset(real_len);
            _recvStatu = RECV_OVER;
            return true;
        }

        //  缓冲区中数据无法满足当前正文的需要，数据不足取出数据等待新数据到来
        _request._body.append(buf->Get_Read_Start_Pos(), buf->Get_Read_AbleSize());
        buf->Move_Read_Offset(buf->Get_Read_AbleSize());
        return true;
    }

public:
    ConText() : _respStatu(200), _recvStatu(RECV_LINE) {}
    // 重置上下文
    void ReSet()
    {
        _respStatu = 200;
        _recvStatu = RECV_LINE;
        _request.ReSet();
    }
    // 获取响应状态码
    int ResponseStatu() { return _respStatu; }
    // 获取当前解析阶段状态
    RecvStatu GetRecvStatu() { return _recvStatu; }
    // 获取请求
    Request &GetRequest() { return _request; }

    // 接受并解析请求信息
    void RecvRequest(Buffer *buf)
    {
        // 不同的状态做不同的事情
        // 不需要break
        // 因为处理完请求行后应该立即处理头部 而不是退出等新数据
        switch (_recvStatu)
        {
        case RECV_LINE:
            RecvLine(buf);
        case RECV_HEAD:
            RecvHead(buf);
        case RECV_BODY:
            RecvBody(buf);
        }
    }
};

#endif