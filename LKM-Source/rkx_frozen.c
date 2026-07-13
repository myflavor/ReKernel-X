/*
 * Copyright (c) 2026 myflavor <admin@myflv.cn>. All rights reserved.
 * Based on Re-Kernel project by nep_timeline@outlook.com.
 * File: rkx_frozen.c — Kernel-version compatible frozen state predicate.
 */

#include "rkx_log.h"
#include "rkx.h"
#include <linux/printk.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/freezer.h>
#include <linux/cgroup.h>
#include <linux/version.h>
#include <linux/sched.h>

static inline bool rkx_is_frozen_state_compatible(struct task_struct *task)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0))
	return READ_ONCE(task->__state) & TASK_FROZEN;
#else
	return frozen(task);
#endif
}

static inline bool rkx_is_jobctl_frozen_compatible(struct task_struct *task)
{
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(5, 10, 0))
	return cgroup_task_freeze(task);
#else
	return ((task->jobctl & JOBCTL_TRAP_FREEZE) != 0);
#endif
}

bool line_is_frozen(struct task_struct *task)
{
	if (cgroup_task_frozen(task) || rkx_is_jobctl_frozen_compatible(task))
		return true;

	/* if task->group_leader is NULL, unfreeze it to avoid some unknown problems */
	if (NULL == task->group_leader)
		return true;

	return rkx_is_frozen_state_compatible(task->group_leader) || freezing(task->group_leader);
}
