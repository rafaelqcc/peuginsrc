#pragma once
#include "common.hpp"
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

class Memory {
public:
    explicit Memory(int pid);
    ~Memory();

    Memory(const Memory&) = delete;
    Memory& operator=(const Memory&) = delete;
    Memory(Memory&& o) noexcept;
    Memory& operator=(Memory&& o) noexcept;

    int pid() const { return pid_; }

    std::vector<std::uint8_t> read_bytes(std::uint64_t addr, std::size_t size);
    void write_bytes(std::uint64_t addr, const void* data, std::size_t size);

    std::uint64_t read_u64(std::uint64_t addr);
    std::uint32_t read_u32(std::uint64_t addr);
    float read_f32(std::uint64_t addr);
    void write_f32(std::uint64_t addr, float value);
    Vec3 read_vec3(std::uint64_t addr);

private:
    int pid_;
    std::string backend_{"process_vm"};
    int mem_fd_{-1};
    mutable std::mutex io_mutex_;

    std::vector<std::uint8_t> read_process_vm(std::uint64_t addr, std::size_t size);
    void write_process_vm(std::uint64_t addr, const void* data, std::size_t size);
    std::vector<std::uint8_t> read_proc_mem(std::uint64_t addr, std::size_t size);
    void write_proc_mem(std::uint64_t addr, const void* data, std::size_t size);
    int open_proc_mem();
};
