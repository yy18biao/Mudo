#ifndef __M_BUFFER_H__
#define __M_BUFFER_H__

#include <iostream>
#include <vector>
#include <string>
#include <cassert>
#include <cstring>
#include "Log.hpp"

#define BUFFER_DEFAULT_SIZE 1024
class Buffer
{
private:
    std::vector<char> _buffer; // 缓冲区空间
    uint64_t _read_idx;        // 读偏移
    uint64_t _write_idx;       // 写偏移

public:
    Buffer() : _buffer(BUFFER_DEFAULT_SIZE), _read_idx(0), _write_idx(0) {}

public:
    // 获取读写偏移量
    uint64_t Get_Read_idx() { return _read_idx; }
    uint64_t Get_Write_idx() { return _write_idx; }

    // 获取当前写入起始地址
    char *Get_Write_Start_Pos()
    {
        // Buffer空间的起始地址加上写偏移量
        return &*_buffer.begin() + _write_idx;
    }

    // 获取当前读取起始地址
    char *Get_Read_Start_Pos()
    {
        // Buffer空间的起始地址加上读偏移量
        return &*_buffer.begin() + _read_idx;
    }

    // 获取前沿空闲空间大小
    uint64_t Get_Front_IdleSize()
    {
        return _read_idx;
    }

    // 获取后沿空闲空间大小
    uint64_t Get_After_IdleSize()
    {
        // 总体空间大小减去写偏移
        return _buffer.size() - _write_idx;
    }

    // 获取可读空间大小
    uint64_t Get_Read_AbleSize()
    {
        // 写偏移 - 读偏移
        return _write_idx - _read_idx;
    }

    // 将读偏移向后移动
    void Move_Read_Offset(uint64_t len)
    {
        // 向后移动大小必须小于可读数据大小
        assert(len <= Get_Read_AbleSize());
        _read_idx += len;
    }

    // 将写偏移向后移动
    void Move_Write_Offset(uint64_t len)
    {
        // 向后移动的大小必须小于当前后沿所剩空间大小
        assert(len <= Get_After_IdleSize());
        _write_idx += len;
    }

    // 确保可写空间是否足够(整体空间足够则移动数据，否则扩容)
    void Ensure_Write_Space(uint64_t len)
    {
        // 如果后沿空间大小足够直接返回即可
        if (Get_After_IdleSize() >= len)
            return;

        // 如果前沿加后沿空间足够，则将整个数据挪到起始位置
        if (len <= Get_After_IdleSize() + Get_Front_IdleSize())
        {
            uint64_t rsz = Get_Read_AbleSize(); // 获取当前数据大小
            // 把可读数据拷贝到起始位置
            std::copy(Get_Read_Start_Pos(), Get_Read_Start_Pos() + rsz, &*_buffer.begin());
            // 更新读写偏移量
            _read_idx = 0;
            _write_idx = rsz;
        }
        // 总体空间都不够，需要扩容
        else
        {
            // 不移动数据直接在写偏移之后扩容足够空间
            _buffer.resize(_write_idx + len);
        }
    }

    // 写入数据
    void Write_Data(const void *data, uint64_t len)
    {
        if(len == 0) return;
        // 保证有足够空间
        Ensure_Write_Space(len);

        // 拷贝数据进去
        const char *d = (const char *)data;
        std::copy(d, d + len, Get_Write_Start_Pos());
        Move_Write_Offset(len);
        DBG_LOG("写入缓冲区数据成功");
    }

    // 写入String类型数据
    void Write_String(const std::string &data)
    {
        Write_Data(data.c_str(), data.size());
    }

    // 写入BUffer类型数据
    void Write_Buffer(Buffer &data)
    {
        Write_Data(data.Get_Read_Start_Pos(), data.Get_Read_AbleSize());
    }

    // 读取数据
    void Read_Data(void *buff, uint64_t len)
    {
        if(len == 0) return;
        // 获取的数据大小必须小于可读数据大小
        assert(len <= Get_Read_AbleSize());

        std::copy(Get_Read_Start_Pos(), (char *)Get_Read_Start_Pos() + len, (char *)buff);

        Move_Read_Offset(len);
        DBG_LOG("读取缓冲区数据成功");
    }

    // 将读取的数据当作String
    std::string Read_Data_String(uint64_t len)
    {
        assert(len <= Get_Read_AbleSize());
        std::string str;
        str.resize(len);
        Read_Data(&str[0], len);
        return str;
    }

    // 找到换行字符的位置
    char *FindCRLF()
    {
        void *res = memchr(Get_Read_Start_Pos(), '\n', Get_Read_AbleSize());
        return (char *)res;
    }

    // 读取返回一行数据
    std::string GetLine_Data()
    {
        char *pos = FindCRLF();
        if (pos == nullptr)
            return "";
        return Read_Data_String(pos - Get_Read_Start_Pos() + 1);
    }

    // 清空缓冲区
    void Clear_Buff()
    {
        _read_idx = _write_idx = 0;
        DBG_LOG("清空缓冲区成功");
    }
};

#endif