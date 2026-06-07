#include "memory.hpp"
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <sstream>
#include <sys/uio.h>
#include <unistd.h>

Memory::Memory(int pid) : pid_(pid) {}

Memory::Memory(Memory&& o) noexcept
    : pid_(o.pid_), backend_(std::move(o.backend_)), mem_fd_(o.mem_fd_) {
    o.mem_fd_ = -1;
}

Memory& Memory::operator=(Memory&& o) noexcept {
    if (this != &o) {
        if (mem_fd_ >= 0) close(mem_fd_);
        pid_ = o.pid_;
        backend_ = std::move(o.backend_);
        mem_fd_ = o.mem_fd_;
        o.mem_fd_ = -1;
    }
    return *this;
}

Memory::~Memory() {
    if (mem_fd_ >= 0) close(mem_fd_);
}

int Memory::open_proc_mem() {
    if (mem_fd_ >= 0) return mem_fd_;
    std::string path = "/proc/" + std::to_string(pid_) + "/mem";
    mem_fd_ = open(path.c_str(), O_RDWR);
    if (mem_fd_ < 0) {
        mem_fd_ = open(path.c_str(), O_RDONLY);
        if (mem_fd_ < 0)
            throw MemError("open " + path + ": " + std::strerror(errno));
    }
    return mem_fd_;
}

std::vector<std::uint8_t> Memory::read_process_vm(std::uint64_t addr, std::size_t size) {
    std::vector<std::uint8_t> buf(size);
    iovec local{buf.data(), size};
    iovec remote{reinterpret_cast<void*>(addr), size};
    ssize_t n = process_vm_readv(pid_, &local, 1, &remote, 1, 0);
    if (n != static_cast<ssize_t>(size)) {
        std::ostringstream oss;
        oss << "read " << size << " @ 0x" << std::hex << addr << ": got " << std::dec << n
            << " (" << std::strerror(errno) << ")";
        throw MemError(oss.str());
    }
    return buf;
}

void Memory::write_process_vm(std::uint64_t addr, const void* data, std::size_t size) {
    iovec local{const_cast<void*>(data), size};
    iovec remote{reinterpret_cast<void*>(addr), size};
    ssize_t n = process_vm_writev(pid_, &local, 1, &remote, 1, 0);
    if (n != static_cast<ssize_t>(size)) {
        std::ostringstream oss;
        oss << "write " << size << " @ 0x" << std::hex << addr << ": got " << std::dec << n
            << " (" << std::strerror(errno) << ")";
        throw MemError(oss.str());
    }
}

std::vector<std::uint8_t> Memory::read_proc_mem(std::uint64_t addr, std::size_t size) {
    int fd = open_proc_mem();
    std::vector<std::uint8_t> buf(size);
    ssize_t n = pread(fd, buf.data(), size, static_cast<off_t>(addr));
    if (n != static_cast<ssize_t>(size))
        throw MemError("short read from /proc/pid/mem: " + std::string(std::strerror(errno)));
    return buf;
}

void Memory::write_proc_mem(std::uint64_t addr, const void* data, std::size_t size) {
    int fd = open_proc_mem();
    ssize_t n = pwrite(fd, data, size, static_cast<off_t>(addr));
    if (n != static_cast<ssize_t>(size))
        throw MemError("short write to /proc/pid/mem: " + std::string(std::strerror(errno)));
}

std::vector<std::uint8_t> Memory::read_bytes(std::uint64_t addr, std::size_t size) {
    std::lock_guard lock(io_mutex_);
    if (backend_ == "process_vm") {
        try {
            return read_process_vm(addr, size);
        } catch (const MemError&) {
            backend_ = "proc_mem";
        }
    }
    return read_proc_mem(addr, size);
}

void Memory::write_bytes(std::uint64_t addr, const void* data, std::size_t size) {
    std::lock_guard lock(io_mutex_);
    if (backend_ == "process_vm") {
        try {
            write_process_vm(addr, data, size);
            return;
        } catch (const MemError&) {
            backend_ = "proc_mem";
        }
    }
    write_proc_mem(addr, data, size);
}

std::uint64_t Memory::read_u64(std::uint64_t addr) {
    auto b = read_bytes(addr, 8);
    std::uint64_t v;
    std::memcpy(&v, b.data(), 8);
    return v;
}

std::uint32_t Memory::read_u32(std::uint64_t addr) {
    auto b = read_bytes(addr, 4);
    std::uint32_t v;
    std::memcpy(&v, b.data(), 4);
    return v;
}

float Memory::read_f32(std::uint64_t addr) {
    auto b = read_bytes(addr, 4);
    float v;
    std::memcpy(&v, b.data(), 4);
    return v;
}

void Memory::write_f32(std::uint64_t addr, float value) {
    write_bytes(addr, &value, sizeof(value));
}

Vec3 Memory::read_vec3(std::uint64_t addr) {
    auto b = read_bytes(addr, 12);
    Vec3 v;
    std::memcpy(&v.x, b.data(), 4);
    std::memcpy(&v.y, b.data() + 4, 4);
    std::memcpy(&v.z, b.data() + 8, 4);
    return v;
}
