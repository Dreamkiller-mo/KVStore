#include <kvstore/common/status.h>
#include <kvstore/storage/file.h>
#include <fcntl.h>     // open, O_RDONLY 等
#include <unistd.h>    // close, read, write
#include <sys/stat.h>  // mode_t, S_IRUSR 等权限位
#include <string>

using namespace kvstore::storage;

File::File(File&& other) noexcept :fd_(other.fd_)
{
    other.fd_ = -1;
}

File& File::operator=(File&& other) noexcept
{
    if(this != &other)
    {
        // release self resource
        Close();

        // steal other resource
        fd_ = other.fd_;
        other.fd_ = -1;
    }

    return *this;
}

File::~File()
{
    Close();
}

kvstore::Status File::Open(std::string_view path)
{
    auto fd = ::open(std::string(path).c_str(), O_CREAT | O_APPEND | O_RDWR, 0644);
    if (fd == -1) 
    {
        return kvstore::Status{StatusCode::IOError, "File::Open fd Invalid"};
    }

    // obtain file descriptor
    fd_ = fd;

    return kvstore::Status{StatusCode::Ok};
}

kvstore::Status File::Append(std::span<const std::uint8_t> data)
{
    if (fd_ < 0)
    {
        return Status{StatusCode::IOError};
    }

    auto remaining = data.size();
    // the starting point of the data that will be writed
    auto current = data.data();

    while (remaining != 0)
    {
        auto bytes_write = ::write(fd_, current, remaining);

        if (bytes_write < 0) { return Status{StatusCode::IOError, "::write error" }; }
        if(bytes_write == 0) { return Status{StatusCode::IOError, "::write return 0" }; }

        remaining -= bytes_write;
        current += bytes_write;
    }

    return Status{StatusCode::Ok};
}

kvstore::Status File::Sync()
{
    if (fd_ < 0) { return Status{StatusCode::IOError, "File::Sync fd < 0"}; }

    if (::fsync(fd_) < 0) { return Status{StatusCode::IOError, "File::Sync fsync failed"}; }

    return Status{StatusCode::Ok};
}

void File::Close() noexcept
{
    if (fd_ >= 0)
    {
        ::close(fd_);
        fd_ = -1;
    }
}

bool File::IsOpen() const noexcept
{
    return fd_ > 0;
}
