#ifndef RKX_LOG_H
#define RKX_LOG_H

/* Include this header FIRST in each .c — pr_fmt must be defined before printk.h. */
#define pr_fmt(fmt) "[ReKernel-X] " fmt
#include <linux/printk.h>

#define rkx_log_info(fmt, ...)  pr_info(fmt, ##__VA_ARGS__)
#define rkx_log_err(fmt, ...)   pr_err(fmt, ##__VA_ARGS__)
#define rkx_log_warn(fmt, ...)  pr_warn(fmt, ##__VA_ARGS__)

#ifdef RKX_DEBUG
#define rkx_log_debug(fmt, ...)  pr_info(fmt, ##__VA_ARGS__)
#else
#define rkx_log_debug(fmt, ...)  no_printk(fmt, ##__VA_ARGS__)
#endif

#endif /* RKX_LOG_H */
