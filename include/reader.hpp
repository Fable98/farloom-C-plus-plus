#pragma once

#include <vector>
#include <cstdint>
#include <cstddef>
#include <sys/types.h>
#include <unistd.h>
#include <sys/socket.h>
#include <cerrno>
#include <algorithm>
#include <span>
#include <string_view>

namespace farloom::io {

class Reader {
public:
    virtual ~Reader() = default;
    virtual ssize_t read(uint8_t* buf, size_t len) = 0;
};

class MemoryReader : public Reader {
public:
    explicit MemoryReader(std::span<const uint8_t> data) : data_(data), pos_(0) {}
    explicit MemoryReader(std::string_view sv)
        : data_(reinterpret_cast<const uint8_t*>(sv.data()), sv.size()), pos_(0) {}

    ssize_t read(uint8_t* buf, size_t len) override {
        if (pos_ >= data_.size()) return 0;
        size_t n = std::min(len, data_.size() - pos_);
        std::copy_n(data_.data() + pos_, n, buf);
        pos_ += n;
        return static_cast<ssize_t>(n);
    }

    size_t remaining() const noexcept { return data_.size() - pos_; }

private:
    std::span<const uint8_t> data_;
    size_t pos_;
};

class SocketReader : public Reader {
public:
    explicit SocketReader(int fd) : fd_(fd) {}

    ssize_t read(uint8_t* buf, size_t len) override {
        ssize_t n = ::recv(fd_, buf, len, 0);
        return n;
    }

private:
    int fd_;
};

class BufferedReader {
public:
    explicit BufferedReader(Reader& reader, size_t capacity = 8192)
        : reader_(reader), buf_(capacity), pos_(0), cap_(0) {}

    ssize_t read_until(uint8_t delim, std::vector<uint8_t>& out, size_t max_bytes, int& err_out) {
        err_out = 0;
        size_t count = 0;
        while (count < max_bytes) {
            if (pos_ >= cap_) {
                ssize_t n = reader_.read(buf_.data(), buf_.size());
                if (n < 0) {
                    err_out = errno;
                    return -1;
                }
                if (n == 0) {
                    return static_cast<ssize_t>(count);
                }
                pos_ = 0;
                cap_ = static_cast<size_t>(n);
            }
            uint8_t b = buf_[pos_++];
            out.push_back(b);
            count++;
            if (b == delim) {
                return static_cast<ssize_t>(count);
            }
        }
        return static_cast<ssize_t>(count);
    }

    ssize_t read_exact(uint8_t* out, size_t len, int& err_out) {
        err_out = 0;
        size_t total = 0;
        while (total < len) {
            if (pos_ < cap_) {
                size_t avail = std::min(len - total, cap_ - pos_);
                std::copy_n(buf_.data() + pos_, avail, out + total);
                pos_ += avail;
                total += avail;
            } else {
                ssize_t n = reader_.read(buf_.data(), buf_.size());
                if (n < 0) {
                    err_out = errno;
                    return -1;
                }
                if (n == 0) {
                    return static_cast<ssize_t>(total);
                }
                pos_ = 0;
                cap_ = static_cast<size_t>(n);
            }
        }
        return static_cast<ssize_t>(total);
    }

private:
    Reader& reader_;
    std::vector<uint8_t> buf_;
    size_t pos_;
    size_t cap_;
};

} // namespace farloom::io
