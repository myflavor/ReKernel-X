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

static int __init rkx_init(void)
{
	rkx_log_info("starting...\n");
	rkx_log_debug("Debug mode is enabled!\n");
	rkx_log_info("Version %s |  by myflavor, Sakion Team\n", RKX_VERSION);

	rkx_init_net_uid();
	rkx_init_free_async();

	if (rkx_register_genl() != RKX_SUCCESS)
	{
		rkx_log_err("%s: Failed to register genl family!\n", __func__);
		goto err;
	}

	rkx_log_info("start hooking!\n");

	if (rkx_register_binder() != RKX_SUCCESS)
	{
		rkx_log_err("%s: Failed to hook binder!\n", __func__);
		goto err;
	}

	if (rkx_register_signal() != RKX_SUCCESS)
	{
		rkx_log_err("%s: Failed to hook signal!\n", __func__);
		goto err;
	}

	if (rkx_register_netfilter() != RKX_SUCCESS)
	{
		rkx_log_err("%s: Failed to hook netfilter!\n", __func__);
		goto err;
	}

	rkx_register_binder_kp();

	rkx_log_info("hooked!\n");
	return RKX_SUCCESS;

err:
	rkx_unregister_binder_kp();
	rkx_unregister_netfilter();
	rkx_unregister_signal();
	rkx_unregister_binder();
	tracepoint_synchronize_unregister();
	rkx_unregister_genl();
	rkx_destroy_free_async();
	rkx_destroy_net_uid();
	return RKX_ERROR;
}

static void __exit rkx_exit(void)
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

module_init(rkx_init);
module_exit(rkx_exit);

MODULE_LICENSE("GPL");
