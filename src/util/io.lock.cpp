#include "dib/io/lock.h"
#include "dib/io/os.h"

#include <fstream>

using namespace dib::io;
namespace fs = std::filesystem;

#if DIB_OS_UNIX
    #include <sys/file.h>
    #include <unistd.h>

lock_handle dib::io::lock(const fs::path &path)
{
    fs::path canon = fs::canonical(path);
    int fd;

    if(!fs::exists(path))
    {
        fd = creat(canon.c_str(), O_RDWR);
        if(errno != 0)
        {
            throw fs::filesystem_error("Could not create lock file", path, std::error_code{errno, std::system_category()});
        }
    }
    else 
    {
        fd = open(canon.c_str(), O_RDWR);
        if(errno != 0)
        {
            throw fs::filesystem_error("Could not open lock file", path, std::error_code{errno, std::system_category()});
        }
    }

    flock(fd, LOCK_EX);
    return {new int(fd)};
}

void dib::io::unlock(lock_handle lock)
{
    int *fd = (int*)lock.ptr;

    flock(*fd, LOCK_UN);
    if(close(*fd) == -1)
    {
        throw fs::filesystem_error("Could not close lock file", std::error_code{errno, std::system_category()});
    }

    delete fd;
}

lock_guard::lock_guard(const fs::path &path)
    : handle(lock(path))
{}

lock_guard::~lock_guard()
{
    unlock(handle);
}
#endif