#ifndef __DIBIO_OS_H
#define __DIBIO_OS_H

#ifdef __linux__
    #define DIB_OS_LINUX true
#else
    #define DIB_OS_LINUX false
#endif

#ifdef __unix__
    #define DIB_OS_UNIX true
#else
    #define DIB_OS_UNIX false
#endif

#if defined(_WIN32) || defined(WIN32) && !defined(_WIN64) && !defined(WIN64)
    #define DIB_OS_WIN_32 true
#else
    #define DIB_OS_WIN_32 false
#endif

#if defined(_WIN64) || defined(WIN64)
    #define DIB_OS_WIN_64 true
#else
    #define DIB_OS_WIN_64 false
#endif

#define DIB_OS_WIN (DIB_OS_WIN_32 | DIB_OS_WIN_64)

namespace dib::io
{
    constexpr static bool os_linux = DIB_OS_LINUX;
    constexpr static bool os_unix = DIB_OS_UNIX;
    constexpr static bool os_win_32 = DIB_OS_WIN_32;
    constexpr static bool os_win_64 = DIB_OS_WIN_64;
    constexpr static bool os_win = DIB_OS_WIN;
}

#endif