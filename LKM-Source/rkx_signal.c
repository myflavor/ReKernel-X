/*
 * Copyright (c) 2026 myflavor <admin@myflv.cn>. All rights reserved.
 * Based on Re-Kernel project by nep_timeline@outlook.com.
 * File: rkx_signal.c — Signal trace hooks & frozen task mitigation.
 */

#include "rkx_log.h"
#include "rkx.h"
#include <linux/printk.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/sched.h>
#include <linux/sched/signal.h>
#include <trace/hooks/signal.h>

static bool rkx_signal_hooked;

static void rkx_do_send_sig_info_vh(void *data, int sig, struct task_struct *killer, struct task_struct *dst)
{
	if (!dst || !killer)
		return;

	if (rkx_is_frozen(dst) &&
			(sig == SIGKILL
			|| sig == SIGTERM
			|| sig == SIGABRT
			|| sig == SIGQUIT)) {
		rkx_log_debug("Process Signal! signal=%d\n", sig);
		if (rkx_netlink_ready()) {
			struct rkx_event event = {
				.type = RKX_EVT_SIGNAL,
				.u.signal = {
					.signal = sig,
					.killer_pid = task_tgid_nr(killer),
					.killer_uid = task_uid(killer).val,
					.dst_pid = task_tgid_nr(dst),
					.dst_uid = task_uid(dst).val,
				},
			};
			rkx_send_message(&event);
		}
	}
}

int rkx_register_signal(void)
{
	int rc = RKX_SUCCESS;

	rc = register_trace_android_vh_do_send_sig_info(rkx_do_send_sig_info_vh, NULL);
	if (rc != RKX_SUCCESS) {
		rkx_log_err("register_trace_android_vh_do_send_sig_info failed, rc=%d\n", rc);
		return rc;
	}
	rkx_signal_hooked = true;
	return RKX_SUCCESS;
}

void rkx_unregister_signal(void)
{
	if (rkx_signal_hooked) {
		unregister_trace_android_vh_do_send_sig_info(rkx_do_send_sig_info_vh, NULL);
		rkx_signal_hooked = false;
	}
}
