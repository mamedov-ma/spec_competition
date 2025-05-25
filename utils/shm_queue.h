#pragma once
#include <atomic>
#include <cstdint>
#include <fcntl.h>
#include <sys/mman.h>
#include <string>
#include <cstring>
#include <filesystem>
#include <immintrin.h>

struct spsc_header_t {
    alignas(64) std::atomic<uint32_t> producer_offset;
    alignas(64) std::atomic<uint32_t> consumer_offset;
};

inline constexpr bool is_hugepage_backed(const std::string &path) noexcept
{
    return path.find("/dev/hugepages/") == 0;
}

inline constexpr size_t align8(size_t x) noexcept
{
    return (x + 7) & ~size_t(7);
}

class SPSCQueue {
public:
    SPSCQueue(const std::string &header_path, const std::string &buffer_path, size_t buffer_size, bool is_consumer)
        : buffer_size_(buffer_size), is_consumer_(is_consumer)
    {
        open_header(header_path);
        open_buffer(buffer_path);
        setup_double_mapping(buffer_path);
    }

    ~SPSCQueue()
    {
        if (double_mapped_) {
            munmap(double_mapped_, 2 * buffer_size_);
        }
        if (buffer_fd_ >= 0)
            close(buffer_fd_);
        if (header_)
            munmap(header_, sizeof(spsc_header_t));
        if (header_fd_ >= 0)
            close(header_fd_);
    }

    inline uint32_t *get_ptr_to_write() noexcept
    {
        uint32_t prod = header_->producer_offset.load(std::memory_order_relaxed);
        return (uint32_t *)(double_mapped_ + (prod % buffer_size_));
    }

    inline uint32_t *write(size_t size) noexcept
    {
        size_t aligned_size = align8(size);
        uint32_t prod = header_->producer_offset.load(std::memory_order_relaxed);
        header_->producer_offset.store(prod + aligned_size, std::memory_order_release);
        return (uint32_t *)(double_mapped_ + ((prod + aligned_size) % buffer_size_));
    }

    inline void read(char *&dst) noexcept
    {
        for (;;) {
            // _mm_prefetch(reinterpret_cast<const char *>(header_), _MM_HINT_T0);  // prefetch shared offsets
            const uint32_t prod = header_->producer_offset.load(std::memory_order_acquire);
            const uint32_t cons = header_->consumer_offset.load(std::memory_order_relaxed);
            available_ = prod - cons;
            if (available_ > 0) [[likely]] {
                dst = (char *)double_mapped_ + align8(cons % buffer_size_);
                return;
            }
        }
    }

    inline void notify_read()
    {
        const uint32_t cons = header_->consumer_offset.load(std::memory_order_relaxed);
        header_->consumer_offset.store(cons + available_, std::memory_order_release);
    }

private:
    int header_fd_ = -1;
    int buffer_fd_ = -1;
    spsc_header_t *header_ = nullptr;
    void *double_mapped_ = nullptr;

    size_t buffer_size_;
    bool is_consumer_;
    size_t available_;

    void open_header(const std::string &path)
    {
        header_fd_ = open(path.c_str(), O_RDWR);
        if (header_fd_ < 0)
            std::cerr << "Failed to open header";

        header_ = static_cast<spsc_header_t *>(
            mmap(nullptr, sizeof(spsc_header_t), PROT_READ | PROT_WRITE, MAP_SHARED, header_fd_, 0));
        if (header_ == MAP_FAILED)
            std::cerr << "mmap header failed";
    }

    void open_buffer(const std::string &path)
    {
        buffer_fd_ = open(path.c_str(), O_RDWR);
        if (buffer_fd_ < 0)
            std::cerr << "Failed to open buffer";
    }

    void setup_double_mapping(const std::string &buffer_path)
    {
        int prot = PROT_READ | PROT_WRITE;
        int flags = MAP_SHARED;
        int flags_anonymous = MAP_PRIVATE;
        if (is_hugepage_backed(buffer_path)) {
            flags |= MAP_HUGETLB;
            flags_anonymous |= MAP_HUGETLB;
        }

        void *mapping = mmap(nullptr, 2 * buffer_size_, PROT_NONE, flags_anonymous | MAP_ANONYMOUS, -1, 0);
        if (mapping == MAP_FAILED)
            std::cerr << "anonymous mmap failed";

        void *first = mmap(mapping, buffer_size_, prot, flags | MAP_FIXED, buffer_fd_, 0);
        if (first != mapping)
            std::cerr << "MAP_FIXED first failed";

        void *second = mmap(reinterpret_cast<char *>(mapping) + buffer_size_, buffer_size_, prot, flags | MAP_FIXED,
                            buffer_fd_, 0);
        if (second != reinterpret_cast<char *>(mapping) + buffer_size_)
            std::cerr << "MAP_FIXED second failed";

        double_mapped_ = mapping;
    }
};