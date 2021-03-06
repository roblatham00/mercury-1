/*
 * Copyright (C) 2013-2017 Argonne National Laboratory, Department of Energy,
 *                    UChicago Argonne, LLC and The HDF Group.
 * All rights reserved.
 *
 * The full copyright notice, including terms governing use, modification,
 * and redistribution, is contained in the COPYING file that can be
 * found at the root of the source code distribution tree.
 */

#ifndef MERCURY_TIME_H
#define MERCURY_TIME_H

#include "mercury_util_config.h"
#if defined(_WIN32)
# include <windows.h>
#else
# if defined(HG_UTIL_HAS_TIME_H)
#  include <time.h>
# endif
# if defined(__APPLE__) && !defined(HG_UTIL_HAS_CLOCK_GETTIME)
#  include <sys/time.h>
#  include <mach/mach_time.h>
# endif
#endif

typedef struct hg_time hg_time_t;
struct hg_time
{
    long tv_sec;
    long tv_usec;
};

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Get an elapsed time on the calling processor.
 *
 * \param tv [OUT]              pointer to returned time structure
 *
 * \return Non-negative on success or negative on failure
 */
static HG_UTIL_INLINE int
hg_time_get_current(hg_time_t *tv);

/**
 * Convert hg_time_t to double.
 *
 * \param tv [IN]               time structure
 *
 * \return Converted time in seconds
 */
static HG_UTIL_INLINE double
hg_time_to_double(hg_time_t tv);

/**
 * Convert double to hg_time_t.
 *
 * \param d [IN]                time in seconds
 *
 * \return Converted time structure
 */
static HG_UTIL_INLINE hg_time_t
hg_time_from_double(double d);

/**
 * Compare time values.
 *
 * \param in1 [IN]              time structure
 * \param in2 [IN]              time structure
 *
 * \return 1 if in1 < in2, 0 otherwise
 */
static HG_UTIL_INLINE int
hg_time_less(hg_time_t in1, hg_time_t in2);

/**
 * Add time values.
 *
 * \param in1 [IN]              time structure
 * \param in2 [IN]              time structure
 *
 * \return Summed time structure
 */
static HG_UTIL_INLINE hg_time_t
hg_time_add(hg_time_t in1, hg_time_t in2);

/**
 * Subtract time values.
 *
 * \param in1 [IN]              time structure
 * \param in2 [IN]              time structure
 *
 * \return Subtracted time structure
 */
static HG_UTIL_INLINE hg_time_t
hg_time_subtract(hg_time_t in1, hg_time_t in2);

/**
 * Sleep until the time specified in rqt has elapsed.
 *
 * \param reqt [IN]             time structure
 * \param rmt  [OUT]            pointer to time structure
 *
 * \return Non-negative on success or negative on failure
 */
static HG_UTIL_INLINE int
hg_time_sleep(const hg_time_t rqt, hg_time_t *rmt);

/**
 * Get a string containing current time/date stamp.
 *
 * \return Valid string or NULL on failure
 */
static HG_UTIL_INLINE char *
hg_time_stamp(void);

/*---------------------------------------------------------------------------*/
#ifdef _WIN32
static HG_UTIL_INLINE LARGE_INTEGER
get_FILETIME_offset(void)
{
    SYSTEMTIME s;
    FILETIME f;
    LARGE_INTEGER t;

    s.wYear = 1970;
    s.wMonth = 1;
    s.wDay = 1;
    s.wHour = 0;
    s.wMinute = 0;
    s.wSecond = 0;
    s.wMilliseconds = 0;
    SystemTimeToFileTime(&s, &f);
    t.QuadPart = f.dwHighDateTime;
    t.QuadPart <<= 32;
    t.QuadPart |= f.dwLowDateTime;

    return t;
}

/*---------------------------------------------------------------------------*/
static HG_UTIL_INLINE int
hg_time_get_current_quad(hg_time_t *tv)
{
    LARGE_INTEGER t;
    FILETIME f;
    double t_usec;
    static LARGE_INTEGER offset;
    static double freq_to_usec;
    static int initialized = 0;
    static BOOL use_perf_counter = 0;
    int ret = HG_UTIL_SUCCESS;

    if (!initialized) {
        LARGE_INTEGER perf_freq;
        initialized = 1;
        use_perf_counter = QueryPerformanceFrequency(&perf_freq);
        if (use_perf_counter) {
            QueryPerformanceCounter(&offset);
            freq_to_usec = (double) perf_freq.QuadPart / 1000000.;
        } else {
            offset = get_FILETIME_offset();
            freq_to_usec = 10.;
        }
    }
    if (use_perf_counter) {
        QueryPerformanceCounter(&t);
    } else {
        GetSystemTimeAsFileTime(&f);
        t.QuadPart = f.dwHighDateTime;
        t.QuadPart <<= 32;
        t.QuadPart |= f.dwLowDateTime;
    }

    t.QuadPart -= offset.QuadPart;
    t_usec = (double) t.QuadPart / freq_to_usec;
    t.QuadPart = t_usec;
    tv->tv_sec = t.QuadPart / 1000000;
    tv->tv_usec = t.QuadPart % 1000000;

    return ret;
}

/*---------------------------------------------------------------------------*/
#elif defined(HG_UTIL_HAS_TIME_H) && defined(HG_UTIL_HAS_CLOCK_GETTIME)
static HG_UTIL_INLINE int
hg_time_get_current_clock_gettime(hg_time_t *tv)
{
    struct timespec tp = {0, 0};
    /* NB. CLOCK_MONOTONIC_RAW is not explicitly supported in the vdso */
    clockid_t clock_id = CLOCK_MONOTONIC;

    clock_gettime(clock_id, &tp);
    tv->tv_sec = tp.tv_sec;
    tv->tv_usec = tp.tv_nsec / 1000;

    return HG_UTIL_SUCCESS;
}

/*---------------------------------------------------------------------------*/
#elif defined(__APPLE__) && defined(HG_UTIL_HAS_SYSTIME_H)
static HG_UTIL_INLINE int
hg_time_get_current_mach(hg_time_t *tv)
{
    static uint64_t monotonic_timebase_factor = 0;
    uint64_t monotonic_nsec;
    int ret = HG_UTIL_SUCCESS;

    if (monotonic_timebase_factor == 0) {
        mach_timebase_info_data_t timebase_info;

        (void) mach_timebase_info(&timebase_info);
        monotonic_timebase_factor = timebase_info.numer / timebase_info.denom;
    }
    monotonic_nsec = (mach_absolute_time() * monotonic_timebase_factor);
    tv->tv_sec  = (long) (monotonic_nsec / 1000000000);
    tv->tv_usec = (long) ((monotonic_nsec - (uint64_t) tv->tv_sec) / 1000);

    return ret;
}
#endif

/*---------------------------------------------------------------------------*/
static HG_UTIL_INLINE int
hg_time_get_current(hg_time_t *tv)
{
    if (!tv)
        return HG_UTIL_FAIL;

#if defined(_WIN32)
    return hg_time_get_current_quad(tv);
#elif defined(HG_UTIL_HAS_TIME_H) && defined(HG_UTIL_HAS_CLOCK_GETTIME)
    return hg_time_get_current_clock_gettime(tv);
#elif defined(__APPLE__) && defined(HG_UTIL_HAS_SYSTIME_H)
    return hg_time_get_current_mach(tv);
#endif
}

/*---------------------------------------------------------------------------*/
static HG_UTIL_INLINE double
hg_time_to_double(hg_time_t tv)
{
    return (double) tv.tv_sec + (double) (tv.tv_usec) * 0.000001;
}

/*---------------------------------------------------------------------------*/
static HG_UTIL_INLINE hg_time_t
hg_time_from_double(double d)
{
    hg_time_t tv;

    tv.tv_sec = (long) d;
    tv.tv_usec = (long) ((d - (double) (tv.tv_sec)) * 1000000);

    return tv;
}

/*---------------------------------------------------------------------------*/
static HG_UTIL_INLINE int
hg_time_less(hg_time_t in1, hg_time_t in2)
{
    return ((in1.tv_sec < in2.tv_sec) ||
        ((in1.tv_sec == in2.tv_sec) && (in1.tv_usec < in2.tv_usec)));
}

/*---------------------------------------------------------------------------*/
static HG_UTIL_INLINE hg_time_t
hg_time_add(hg_time_t in1, hg_time_t in2)
{
    hg_time_t out;

    out.tv_sec = in1.tv_sec + in2.tv_sec;
    out.tv_usec = in1.tv_usec + in2.tv_usec;
    if(out.tv_usec > 1000000) {
        out.tv_usec -= 1000000;
        out.tv_sec += 1;
    }

    return out;
}

/*---------------------------------------------------------------------------*/
static HG_UTIL_INLINE hg_time_t
hg_time_subtract(hg_time_t in1, hg_time_t in2)
{
    hg_time_t out;

    out.tv_sec = in1.tv_sec - in2.tv_sec;
    out.tv_usec = in1.tv_usec - in2.tv_usec;
    if(out.tv_usec < 0) {
        out.tv_usec += 1000000;
        out.tv_sec -= 1;
    }

    return out;
}

/*---------------------------------------------------------------------------*/
static HG_UTIL_INLINE int
hg_time_sleep(const hg_time_t rqt, hg_time_t *rmt)
{
    int ret = HG_UTIL_SUCCESS;

#ifdef _WIN32
    DWORD dwMilliseconds = (DWORD) (hg_time_to_double(rqt) / 1000);

    Sleep(dwMilliseconds);
#else
    struct timespec rqtp;
    struct timespec rmtp;

    rqtp.tv_sec = rqt.tv_sec;
    rqtp.tv_nsec = rqt.tv_usec * 1000;

    if (nanosleep(&rqtp, &rmtp)) {
        ret = HG_UTIL_FAIL;
        return ret;
    }

    if (rmt) {
        rmt->tv_sec = rmtp.tv_sec;
        rmt->tv_usec = rmtp.tv_nsec / 1000;
    }
#endif

    return ret;
}

/*---------------------------------------------------------------------------*/
#define HG_UTIL_STAMP_MAX 128
static HG_UTIL_INLINE char *
hg_time_stamp(void)
{
    char *ret = NULL;
    static char buf[HG_UTIL_STAMP_MAX];

#if defined(_WIN32)
    /* TODO not implemented */
#elif defined(HG_UTIL_HAS_TIME_H)
    struct tm *local_time;
    time_t t;

    t = time(NULL);
    local_time = localtime(&t);
    if (local_time == NULL) {
        ret = NULL;
        return ret;
    }

    if (strftime(buf, HG_UTIL_STAMP_MAX, "%a, %d %b %Y %T %Z",
        local_time) == 0) {
        ret = NULL;
        return ret;
    }

    ret = buf;
#endif

    return ret;
}

#ifdef __cplusplus
}
#endif

#endif /* MERCURY_TIME_H */
