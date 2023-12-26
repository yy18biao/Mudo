#include "HttpServer.hpp"

#define WWWROOT "./wwwroot/"
std::string RequestStr(const Request &req)
{
    std::stringstream ss;
    ss << req._method << " " << req._path << " " << req._version << "\r\n";
    for (auto &it : req._params)
        ss << it.first << ": " << it.second << "\r\n";
    for (auto &it : req._headers)
        ss << it.first << ": " << it.second << "\r\n";
    ss << "\r\n";
    ss << req._body;
    return ss.str();
}
void Hello(const Request &req, Response *rsp)
{
    rsp->SetContent(RequestStr(req), "text/plain");
    // sleep(15);
}
void Login(const Request &req, Response *rsp)
{
    rsp->SetContent(RequestStr(req), "text/plain");
}
void PutFile(const Request &req, Response *rsp)
{
    std::string pathname = WWWROOT + req._path;
    Util::WriteFile(pathname, req._body);
}
void DelFile(const Request &req, Response *rsp)
{
    rsp->SetContent(RequestStr(req), "text/plain");
}

int main()
{
    HttpServer server(8000);
    server.SetThreadCount(3);
    server.SetBaseDir(WWWROOT);
    server.Get("/hello", Hello);
    server.Post("/login", Login);
    server.Put("/1234.txt", PutFile);
    server.Delete("/1234.txt", DelFile);
    server.Listen();
    return 0;
}