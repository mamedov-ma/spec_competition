#pragma once
#include <linux/perf_event.h>
#include <sys/syscall.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <cstring>
#include <cstdint>

class PerfCounter {
public:
    explicit PerfCounter(uint32_t type = PERF_TYPE_HARDWARE, uint64_t event = PERF_COUNT_HW_REF_CPU_CYCLES)
    {
        perf_event_attr pea {};
        pea.size = sizeof(pea);
        pea.type = type;
        pea.config = event;
        pea.disabled = 1;
        pea.exclude_kernel = 1;
        pea.exclude_hv = 1;

        fd_ = syscall(SYS_perf_event_open, &pea, 0, -1, -1, 0);
    }
    ~PerfCounter()
    {
        if (fd_ >= 0)
            close(fd_);
    }

    void start()
    {
        ioctl(fd_, PERF_EVENT_IOC_RESET, 0);
        ioctl(fd_, PERF_EVENT_IOC_ENABLE, 0);
    }
    uint64_t stop()
    {
        ioctl(fd_, PERF_EVENT_IOC_DISABLE, 0);
        uint64_t val = 0;
        read(fd_, &val, sizeof(val));
        return val;
    }

private:
    int fd_;
};