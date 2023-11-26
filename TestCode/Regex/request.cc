#include <iostream>
#include <string>
#include <regex>

int main(){
    // HTTP请求格式：GET /biao/login?user=biao&pass=12345 HTTP/1.1\r\n
    std::string str = "GET /biao/login?user=biao&pass=12345 HTTP/1.1\r\n";
    std::smatch matches;

    /* (GET|POST|HEAD|PUT|DELET)表示提取其中任意一个字符串
       ([^?]*) [^?]表示匹配非问号字符，后边的*表示匹配0次或多次
       \\?表示原始问号字符，.*表示任意字符0次或多次
       HTTP/1\\.[01]  表示匹配以HTTP/1.开始，后边有个0或1的字符串
       (?:\n|\r\n)? （?: ...） 表示匹配某个格式字符串，但是不提取，最后的？表示的是匹配前边的表达式0次或1次
    */
    std::regex e("(GET|POST|HEAD|PUT|DELET) ([^?]*)(?:\\?(.*))? (HTTP/1\\.[01])(?:\n|\r\n)?");
    bool ret = std::regex_match(str, matches, e);
    if(ret == false) return -1;
    for(auto& s : matches)
        std::cout << s << std::endl;

    return 0;
}