/*
Project Rigs of Rods
Code grabbed from Remotery (https://github.com/Celtoys/Remotery), commit 5483f65

DEVELOPER NOTE: Lesson learned!
Hardly any improvement was obtained over just using `Ogre::Timer`
which uses `std::chrono` under the hood
(56 FPS vs. 52 FPS under RelWithDebInfo on my Win10 machine).
~ohlidalp, 2026
*/ 

#include "Prof.h"

/*
Copyright 2014-2022 Celtoys Ltd
Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at
    http://www.apache.org/licenses/LICENSE-2.0
Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

/*--------------------------------------------------------------------------------------------------------------------------------
   Compiler/Platform Detection and Preprocessor Utilities
---------------------------------------------------------------------------------------------------------------------------------*/


// Platform identification
#if defined(_WINDOWS) || defined(_WIN32)
    #define RMT_PLATFORM_WINDOWS
#elif defined(__linux__) || defined(__FreeBSD__) || defined(__OpenBSD__)
    #define RMT_PLATFORM_LINUX
    #define RMT_PLATFORM_POSIX
#elif defined(__APPLE__)
    #define RMT_PLATFORM_MACOS
    #define RMT_PLATFORM_POSIX
#endif

/*--------------------------------------------------------------------------------------------------------------------------------
   Types
--------------------------------------------------------------------------------------------------------------------------------*/

// Unsigned integer types
typedef unsigned char rmtU8;
typedef unsigned short rmtU16;
typedef unsigned int rmtU32;
typedef unsigned long long rmtU64;

/*
------------------------------------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------------------------------------
   @DEPS: External Dependencies
------------------------------------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------------------------------------
*/

#ifdef RMT_PLATFORM_MACOS
    #include <mach/mach_time.h>
    #include <mach/vm_map.h>
    #include <mach/mach.h>
    #include <sys/time.h>
#else
    #if !defined(__FreeBSD__) && !defined(__OpenBSD__)
        #include <malloc.h>
    #endif
#endif

#include <assert.h>
#include <stdio.h>
#include <time.h>
#include <limits.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>

#ifdef RMT_PLATFORM_WINDOWS
    #include <winsock2.h>
    #include <timeapi.h>
    #ifndef __MINGW32__
        #include <intrin.h>
    #endif
    #undef min
    #undef max
    #include <tlhelp32.h>
    #include <winnt.h>
    #include <processthreadsapi.h>
    typedef long NTSTATUS;  // winternl.h

#ifdef _XBOX_ONE
    #ifdef _DURANGO
        #include "xmem.h"
    #endif
#else
    #define RMT_ENABLE_THREAD_SAMPLER
#endif

#endif

#ifdef RMT_PLATFORM_LINUX
    #if defined(__FreeBSD__) || defined(__OpenBSD__)
        #include <pthread_np.h>
    #else
        #include <sys/prctl.h>
    #endif
#endif

#if defined(RMT_PLATFORM_POSIX)
    #include <pthread.h>
    #include <unistd.h>
    #include <string.h>
    #include <arpa/inet.h>
    #include <sys/select.h>
    #include <sys/socket.h>
    #include <sys/mman.h>
    #include <netinet/in.h>
    #include <fcntl.h>
    #include <errno.h>
    #include <dlfcn.h>
#endif

#ifdef __MINGW32__
    #include <pthread.h>
#endif

/*
------------------------------------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------------------------------------
   @TIMERS: Platform-specific timers
------------------------------------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------------------------------------
*/


//
// Micro-second accuracy high performance counter
//
#ifndef RMT_PLATFORM_WINDOWS
typedef rmtU64 LARGE_INTEGER;
#endif
typedef struct
{
    LARGE_INTEGER counter_start;
    double counter_scale;
} usTimer;

static void usTimer_Init(usTimer* timer)
{
#if defined(RMT_PLATFORM_WINDOWS)
    LARGE_INTEGER performance_frequency;

    assert(timer != NULL);

    // Calculate the scale from performance counter to microseconds
    QueryPerformanceFrequency(&performance_frequency);
    timer->counter_scale = 1000000.0 / performance_frequency.QuadPart;

    // Record the offset for each read of the counter
    QueryPerformanceCounter(&timer->counter_start);

#elif defined(RMT_PLATFORM_MACOS)

    mach_timebase_info_data_t nsScale;
    mach_timebase_info(&nsScale);
    const double ns_per_us = 1.0e3;
    timer->counter_scale = (double)(nsScale.numer) / ((double)nsScale.denom * ns_per_us);

    timer->counter_start = mach_absolute_time();

#elif defined(RMT_PLATFORM_LINUX)

    struct timespec tv;
    clock_gettime(CLOCK_REALTIME, &tv);
    timer->counter_start = (rmtU64)(tv.tv_sec * (rmtU64)1000000) + (rmtU64)(tv.tv_nsec * 0.001);

#endif
}

#if defined(RMT_PLATFORM_WINDOWS)
    #define usTimer_FromRawTicks(timer, ticks) (rmtU64)(((ticks) - (timer)->counter_start.QuadPart) * (timer)->counter_scale)
#elif defined(RMT_PLATFORM_MACOS)
    #define usTimer_FromRawTicks(timer, ticks) (rmtU64)(((ticks) - (timer)->counter_start) * (timer)->counter_scale)
#elif defined(RMT_PLATFORM_LINUX)
    #define usTimer_FromRawTicks(timer, ticks) (rmtU64)((ticks) - (timer)->counter_start)
#endif

static rmtU64 usTimer_Get(usTimer* timer)
{
#if defined(RMT_PLATFORM_WINDOWS)
    LARGE_INTEGER performance_count;

    assert(timer != NULL);

    // Read counter and convert to microseconds
    QueryPerformanceCounter(&performance_count);
    return usTimer_FromRawTicks(timer, performance_count.QuadPart);

#elif defined(RMT_PLATFORM_MACOS)

    rmtU64 curr_time = mach_absolute_time();
    return usTimer_FromRawTicks(timer, curr_time);

#elif defined(RMT_PLATFORM_LINUX)

    struct timespec tv;
    clock_gettime(CLOCK_REALTIME, &tv);
    rmtU64 ticks = (rmtU64)(tv.tv_sec * (rmtU64)1000000) + (rmtU64)(tv.tv_nsec * 0.001);
    return usTimer_FromRawTicks(timer, ticks);

#endif
}

static usTimer gTimer;
/*static*/ bool RoR::Prof::prof_globaltimer_initialized;

/*static*/ void RoR::Prof::ProfGlobalTimerInit()
{
    usTimer_Init(&gTimer);
    Prof::prof_globaltimer_initialized = true;
}

RoR::Prof::Timeval_t RoR::Prof::ProfNow()
{
    return usTimer_Get(&gTimer);
}