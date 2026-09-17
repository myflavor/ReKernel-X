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

	rkx_init_net_uid();
	rkx_init_free_async();

	if (rkx_register_genl() != LINE_SUCCESS)
	{
		rkx_log_err("%s: Failed to register genl family!\n", __func__);
		goto err;
	}

	rkx_log_info("start hooking!\n");

	if (rkx_register_binder() != LINE_SUCCESS)
	{
		rkx_log_err("%s: Failed to hook binder!\n", __func__);
		goto err;
	}

	if (rkx_register_signal() != LINE_SUCCESS)
	{
		rkx_log_err("%s: Failed to hook signal!\n", __func__);
		goto err;
	}

	if (rkx_register_netfilter() != LINE_SUCCESS)
	{
		rkx_log_err("%s: Failed to hook netfilter!\n", __func__);
		goto err;
	}

	rkx_register_binder_kp();

	rkx_log_info("hooked!\n");
	return LINE_SUCCESS;

err:
	rkx_unregister_binder_kp();
	rkx_unregister_netfilter();
	rkx_unregister_signal();
	rkx_unregister_binder();
	tracepoint_synchronize_unregister();
	rkx_unregister_genl();
	rkx_destroy_free_async();
	rkx_destroy_net_uid();
	return LINE_ERROR;
}

static void __exit exit_rekernel(void)
{
	rkx_log_info("closing...\n");
	rkx_unregister_binder_kp();
	rkx_unregister_netfilter();
	rkx_unregister_signal();
	rkx_unregister_binder();
	tracepoint_synchronize_unregister();
	rkx_unregister_genl();
	rkx_destroy_free_async();
	rkx_destroy_net_uid();
}

module_init(start_rekernel);
module_exit(exit_rekernel);

MODULE_LICENSE("GPL");
