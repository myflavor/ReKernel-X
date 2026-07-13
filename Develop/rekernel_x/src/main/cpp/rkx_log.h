#ifndef RKX_LOG_H
#define RKX_LOG_H

#include <android/log.h>

#define TAG "ReKernel-X"

#define rkx_log_info(fmt, ...) __android_log_print(ANDROID_LOG_INFO,  TAG, fmt, ##__VA_ARGS__)
#define rkx_log_err(fmt, ...)  __android_log_print(ANDROID_LOG_ERROR, TAG, fmt, ##__VA_ARGS__)
#define rkx_log_warn(fmt, ...) __android_log_print(ANDROID_LOG_WARN,  TAG, fmt, ##__VA_ARGS__)

#ifdef RKX_DEBUG
#define rkx_log_debug(fmt, ...) __android_log_print(ANDROID_LOG_DEBUG, TAG, fmt, ##__VA_ARGS__)
#else
#define rkx_log_debug(fmt, ...) ((void)0)
#endif

#endif // RKX_LOG_H