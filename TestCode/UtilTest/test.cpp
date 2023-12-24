#include "../../Code/Http/Util.hpp"

bool ReadFile(const std::string &filename, std::string *buf)
{
    std::ifstream ifs(filename, std::ios::binary);
    if (!ifs.is_open())
    {
        return false;
    }

    ifs.seekg(0, ifs.end);
    size_t fsize = ifs.tellg();
    ifs.seekg(0, ifs.beg);

    buf->resize(fsize);
    ifs.read(&(*buf)[0], fsize);
    if (ifs.good() == false)
    {
        ifs.close();
        return false;
    }
    ifs.close();
    return true;
}

bool WriteFile(const std::string &filename, const std::string &buf)
{
    // 丢弃原有内容
    std::ofstream ofs(filename, std::ios::binary | std::ios::trunc);
    if (!ofs.is_open())
    {
        return false;
    }
    ofs.write(buf.c_str(), buf.size());
    if (!ofs.good())
    {
        ofs.close();
        return false;
    }
    ofs.close();
    return true;
}

std::string UrlEncode(const std::string url, bool convert_space_to_plus)
{
    // 避免URL中资源路径与查询字符串中的特殊字符与请求中的特殊字符产生歧义
    // 编码格式：将特殊字符的ascii值转换为两个16进制字符 前缀为%
    // 不编码的特殊字符：. - _ ~ 字母和数字
    // RFC3986标准规定编码格式为 %HH
    // W3C标准规定查询字符串中的空格需要转换为+，解码则为+转空格
    std::string res;
    for (auto &c : url)
    {
        if (c == '.' || c == '-' || c == '_' || c == '~' || isalnum(c))
        {
            res += c;
            continue;
        }
        if (c == ' ' && convert_space_to_plus)
        {
            res += '+';
            continue;
        }
        // 剩下的字符都是需要编码成为 %HH 格式
        char tmp[4] = {0};
        snprintf(tmp, 4, "%%%02X", c);
        res += tmp;
    }
    return res;
}
static char HEXTOI(char c)
    {
        if (c >= '0' && c <= '9')
            return c - '0';
        else if (c >= 'a' && c <= 'z')
            return c - 'a' + 10;
        else if (c >= 'A' && c <= 'Z')
            return c - 'A' + 10;
        return -1;
    }
std::string UrlDecode(const std::string url, bool convert_plus_to_space)
{
    // 遇到%将后面的两个字符转换为数字，第一个数字左移4位再加上第二个数字
    std::string res;
    for (int i = 0; i < url.size(); i++)
    {
        if (url[i] == '+' && convert_plus_to_space == true)
        {
            res += ' ';
            continue;
        }
        if (url[i] == '%' && (i + 2) < url.size())
        {
            char v1 = HEXTOI(url[i + 1]);
            char v2 = HEXTOI(url[i + 2]);
            char v = v1 * 16 + v2;
            res += v;
            i += 2;
            continue;
        }
        res += url[i];
    }
    return res;
}
static size_t Split(const std::string &src, const std::string &sep, std::vector<std::string> *arry)
    {
        // 将src字符串按照sep字符进行分割得到的各个字串放到arry中 最终返回字串的数量
        size_t idx = 0;
        while (idx < src.size())
        {
            size_t pos = src.find(sep, idx);
            if (pos == std::string::npos)
            {
                if (pos == src.size())
                    break;
                arry->push_back(src.substr(idx));
                return arry->size();
            }
            if (pos == idx)
            {
                idx = pos + sep.size();
                continue;
            }
            arry->push_back(src.substr(idx, pos - idx));
            idx = pos + sep.size();
        }
        return arry->size();
    }

static bool ValidPath(const std::string &path)
    {
        // 按照 / 进行路径分割根据有多少子目录计算目录深度有多少层 深度不能小于0
        std::vector<std::string> subdir;
        Split(path, "/", &subdir);
        int level = 0;
        for (auto &dir : subdir)
        {
            if (dir == "..")
            {
                level--; // 任意一层走出相对根目录，就认为有问题
                if (level < 0)
                    return false;
                continue;
            }
            level++;
        }
        return true;
    }

int main()
{
    std::string buf = "/login/../";
    std::cout << ValidPath(buf) << std::endl;
    buf = "../../";
    std::cout << ValidPath(buf) << std::endl;


    return 0;
}