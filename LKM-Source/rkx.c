/*
 * Copyright (c) 2026 myflavor <admin@myflv.cn>. All rights reserved.
 * Based on Re-Kernel project by nep_timeline@outlook.com.
 * File: rkx.c — Module entry (init/exit) & hooks wiring.
 */

#include "rkx_log.h"
#include "rkx.h"
#include <linux/printk.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/tracepoint.h>

static int __init start_rekernel(void)
{
	rkx_log_info("starting...\n");
	rkx_log_debug("Debug mode is enabled!\n");
	rkx_log_info("Version %s |  by myflavor, Sakion Team\n", RKX_VERSION);

	net_uid_init();

	if (register_genl() != LINE_SUCCESS)
	{
		rkx_log_err("%s: Failed to register genl family!\n", __func__);
		goto err;
	}

	rkx_log_info("start hooking!\n");

	if (register_binder() != LINE_SUCCESS)
	{
		rkx_log_err("%s: Failed to hook binder!\n", __func__);
		goto err;
	}

	if (register_signal() != LINE_SUCCESS)
	{
		rkx_log_err("%s: Failed to hook signal!\n", __func__);
		goto err;
	}

	if (register_netfilter() != LINE_SUCCESS)
	{
		rkx_log_err("%s: Failed to hook netfilter!\n", __func__);
		goto err;
	}

	register_binder_kp();

	rkx_log_info("hooked!\n");
	return LINE_SUCCESS;

err:
	unregister_binder_kp();
	unregister_netfilter();
	unregister_signal();
	unregister_binder();
	tracepoint_synchronize_unregister();
	unregister_genl();
	net_uid_destroy();
	return LINE_ERROR;
}

static void __exit exit_rekernel(void)
{
	rkx_log_info("closing...\n");
	unregister_binder_kp();
	unregister_netfilter();
	unregister_signal();
	unregister_binder();
	tracepoint_synchronize_unregister();
	unregister_genl();
	net_uid_destroy();
}

module_init(start_rekernel);
module_exit(exit_rekernel);

MODULE_LICENSE("GPL");
