#ifndef __M_HTTPSERVER_H__
#define __M_HTTPSERVER_H__

#include "ConText.hpp"
#define TIMEOUT 10

using Handler = std::function<void(const Request &, Response *)>;
using Handlers = std::vector<std::pair<std::regex, Handler>>;
class HttpServer
{
private:
    /* 各方法的映射处理函数 */
    Handlers _get_route;
    Handlers _post_route;
    Handlers _put_route;
    Handlers _delete_route;
    std::string _basedir; // 静态资源根目录
    Server _server;       // 高性能TCP服务器

private:
    // 错误处理方法
    void ErrorHandler(const Request &req, Response *rsp)
    {
        // 组织一个错误展示页面
        std::string body;
        body += "<html>";
        body += "<head>";
        body += "<meta http-equiv='Content-Type' content='text/html;charset=utf-8'>";
        body += "</head>";
        body += "<body>";
        body += "<h1>";
        body += std::to_string(rsp->_statu);
        body += " ";
        body += Util::StatuDesc(rsp->_statu);
        body += "</h1>";
        body += "</body>";
        body += "</html>";
        // 将页面数据当作响应正文，放入rsp中
        rsp->SetContent(body, "text/html");
    }

    // 将Response中的要素按照http协议格式进行组织发送
    void WriteReponse(const ConnectionPtr &conn, const Request &req, Response *rsp)
    {
        // 完善头部字段
        // 添加长/短连接标志
        if (req.Close())
            rsp->SetHeader("Connection", "close");
        else
            rsp->SetHeader("Connection", "keep-alive");

        // 添加正文长度
        if (!rsp->_body.empty() && !rsp->HasHeader("Content-Length"))
            rsp->SetHeader("Content-Length", std::to_string(rsp->_body.size()));

        // 添加正文类型
        if (!rsp->_body.empty() && !rsp->HasHeader("Content-Type"))
            rsp->SetHeader("Content-Type", "application/octet-stream");

        // 如需添加重定向
        if (rsp->_redirect_flag)
            rsp->SetHeader("Location", rsp->_redirect_url);

        // 将rsp的要素按照http格式组织
        std::stringstream rsp_str;
        rsp_str << req._version << " " << std::to_string(rsp->_statu) << " " << Util::StatuDesc(rsp->_statu) << "\r\n";
        for (auto &head : rsp->_headers)
            rsp_str << head.first << ": " << head.second << "\r\n";
        rsp_str << "\r\n";
        rsp_str << rsp->_body;

        // 发送数据
        DBG_LOG("%s", rsp_str.str().c_str());
        conn->Send(rsp_str.str().c_str(), rsp_str.str().size());
    }

    // 判断是否为静态资源
    bool IsFileHandler(const Request &req)
    {
        // 必须设置了静态资源根目录
        if (_basedir.empty())
            return false;
        // 请求方法必须是GET / HEAD请求方法
        if (req._method != "GET" && req._method != "HEAD")
            return false;
        // 请求的资源路径必须是一个合法路径
        if (Util::ValidPath(req._path) == false)
            return false;
        // 请求的资源必须存在,且是一个普通文件
        // 目录：/, /image/这种情况给后边默认追加一个 index.html
        // 前缀的相对根目录也就是将请求路径转换为实际存在的路径  /image/a.png  ->   ./wwwroot/image/a.png
        // 定义一个临时对象为了避免直接修改请求的资源路径
        std::string req_path = _basedir + req._path;
        if (req._path.back() == '/')
            req_path += "index.html";
        // 必须是普通文件
        if (!Util::IsReg(req_path))
            return false;

        return true;
    }

    // 静态资源请求处理方法
    void FileHandler(const Request &req, Response *rsp)
    {
        std::string req_path = _basedir + req._path;
        // 默认入口
        if (req._path.back() == '/')
            req_path += "index.html";
        // 读取请求资源路径
        if (!Util::ReadFile(req_path, &rsp->_body))
            return;
        std::string mime = Util::GetMime(req_path);
        // 设置响应头部
        rsp->SetHeader("Content-Type", mime);
    }

    // 功能性请求的分类处理
    void Dispatcher(Request &req, Response *rsp, Handlers &handlers)
    {
        for (auto &handler : handlers)
        {
            const std::regex &re = handler.first;
            const Handler &func = handler.second;
            if (!std::regex_match(req._path, req._matches, re))
                continue;
            // 传入请求信息和空的rsp执行处理函数
            return func(req, rsp);
        }
        rsp->_statu = 404;
    }

    // 执行对应处理函数进行业务处理
    void Route(Request &req, Response *rsp)
    {
        // 对请求进行分辨: 静态资源请求(GET HEAD)/功能性请求
        if (IsFileHandler(req))
            return FileHandler(req, rsp); // 静态资源请求处理完毕

        // 功能性请求处理
        if (req._method == "GET" || req._method == "HEAD")
            return Dispatcher(req, rsp, _get_route);
        else if (req._method == "POST")
            return Dispatcher(req, rsp, _post_route);
        else if (req._method == "PUT")
            return Dispatcher(req, rsp, _put_route);
        else if (req._method == "DELETE")
            return Dispatcher(req, rsp, _delete_route);
        rsp->_statu = 405;
    }

    // 设置上下文
    void OnConnected(const ConnectionPtr &conn)
    {
        conn->SetContext(ConText());
        DBG_LOG("获取新连接 %p", conn.get());
    }

    // 缓冲区数据解析处理
    void OnMessage(const ConnectionPtr &conn, Buffer *buffer)
    {
        while (buffer->Get_Read_AbleSize() > 0)
        {
            // 获取上下文
            ConText *context = std::any_cast<ConText>(conn->GetContext());

            // 通过上下文对缓冲区数据进行解析 得到Request对象
            context->RecvRequest(buffer);
            Request &req = context->GetRequest();
            Response rsp(context->ResponseStatu());

            // 错误响应
            if (context->ResponseStatu() >= 400)
            {
                // 错误响应 关闭连接
                ErrorHandler(req, &rsp);
                // 组织响应发送给客户端
                WriteReponse(conn, req, &rsp);
                // 重置上下文
                context->ReSet();
                // 出错了就把缓冲区数据清空
                buffer->Move_Read_Offset(buffer->Get_Read_AbleSize());
                // 关闭连接
                conn->ShutDown();
                return;
            }

            // 当前请求还没有接收完整则退出 等新数据到来再重新继续处理
            if (context->GetRecvStatu() != RECV_OVER)
                return;

            // 执行对应处理函数进行业务处理
            Route(req, &rsp);

            // 对Response进行组织发送
            WriteReponse(conn, req, &rsp);

            // 重置上下文
            context->ReSet();

            // 根据长短连接判断是否关闭连接或者继续处理
            if (rsp.Close())
                conn->ShutDown();
        }
    }

public:
    HttpServer(int port, int timeout = TIMEOUT) : _server(port)
    {
        // http连接默认开启非活跃连接销毁
        _server.EnableActive(timeout);
        _server.SetConnectedCall(std::bind(&HttpServer::OnConnected, this, std::placeholders::_1));
        _server.SetMessageCall(std::bind(&HttpServer::OnMessage, this, std::placeholders::_1, std::placeholders::_2));
    }

    // 设置静态资源根目录
    void SetBaseDir(const std::string &path)
    {
        // 必须为目录
        assert(Util::IsDir(path));
        _basedir = path;
    }

    /* 设置各请求方法与处理函数的映射关系 不同的请求方法不同的处理方法*/
    void Get(const std::string &pattern, const Handler &handler) { _get_route.push_back(std::make_pair(std::regex(pattern), handler)); }
    void Post(const std::string &pattern, const Handler &handler) { _post_route.push_back(std::make_pair(std::regex(pattern), handler)); }
    void Put(const std::string &pattern, const Handler &handler) { _put_route.push_back(std::make_pair(std::regex(pattern), handler)); }
    void Delete(const std::string &pattern, const Handler &handler) { _delete_route.push_back(std::make_pair(std::regex(pattern), handler)); }
    // 设置线程池中线程数量
    void SetThreadCount(int count) { _server.SetThreadPoolCount(count); }
    // 服务器开始工作
    void Listen() { _server.Start(); }
};

#endif