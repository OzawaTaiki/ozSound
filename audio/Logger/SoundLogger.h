#pragma once

#include <functional>
#include <string>
#include <format>

namespace ozSound
{

using LogCallback = std::function<void(const std::string&)>;
using LogWCallback = std::function<void(const std::wstring&)>;

//============
// string

inline LogCallback& GetLogCallback()
{
    static LogCallback s_callback;
    return s_callback;
}

inline void SetLogCallback(LogCallback callback)
{
    GetLogCallback() = std::move(callback);
}

inline void Log(const std::string& msg)
{
    if (auto cb = GetLogCallback())
    {
        cb(msg);
    }
}

//============
// wstring

inline LogWCallback& GetLogWCallback()
{
    static LogWCallback s_callback;
    return s_callback;
}

inline void SetLogCallback(LogWCallback callback)
{
    GetLogWCallback() = std::move(callback);
}

inline void Log(const std::wstring& msg)
{
    if (auto cb = GetLogWCallback())
    {
        cb(msg);
    }
}

} // namespace ozSound
