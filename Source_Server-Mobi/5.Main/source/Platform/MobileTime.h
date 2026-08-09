#pragma once

#include <cstdint>

#if defined(__ANDROID__) || defined(MU_IOS)

void MU_MobileTimeInit();
uint32_t MU_MobileGetTicks();
uint64_t MU_MobilePerfNow();
uint64_t MU_MobilePerfFrequency();
double MU_MobilePerfToSeconds(uint64_t ticks);
double MU_MobilePerfToMilliseconds(uint64_t ticks);
void MU_MobileSleep(uint32_t ms);

#else

#include <chrono>
#include <thread>

inline void MU_MobileTimeInit()
{
}

inline uint32_t MU_MobileGetTicks()
{
    const auto now = std::chrono::steady_clock::now().time_since_epoch();
    const auto ticksMs = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
    return static_cast<uint32_t>(static_cast<uint64_t>(ticksMs) & 0xFFFFFFFFULL);
}

inline uint64_t MU_MobilePerfNow()
{
    const auto now = std::chrono::steady_clock::now().time_since_epoch();
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(now).count());
}

inline uint64_t MU_MobilePerfFrequency()
{
    return 1000000000ULL;
}

inline double MU_MobilePerfToSeconds(uint64_t ticks)
{
    return static_cast<double>(ticks) / static_cast<double>(MU_MobilePerfFrequency());
}

inline double MU_MobilePerfToMilliseconds(uint64_t ticks)
{
    return MU_MobilePerfToSeconds(ticks) * 1000.0;
}

inline void MU_MobileSleep(uint32_t ms)
{
    if (ms == 0)
    {
        std::this_thread::yield();
        return;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

#endif