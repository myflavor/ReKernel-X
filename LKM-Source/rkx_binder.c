/*
 * Copyright (c) 2026 myflavor <admin@myflv.cn>. All rights reserved.
 * Based on Re-Kernel project by nep_timeline@outlook.com.
 * File: rkx_binder.c — Binder trace hooks & freeze detection.
 */

#include "rkx_log.h"
#include "rkx.h"
#include <linux/printk.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/sched.h>
#include <linux/version.h>
#include <linux/uaccess.h>
#include <linux/string.h>
#include <trace/hooks/binder.h>
#include "../android/binder_internal.h"

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6, 6, 0))
static void binder_alloc_new_buf_locked_cb(void *data, size_t size, size_t *free_async_space, int is_async, bool *should_fail)
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0))
static void binder_alloc_new_buf_locked_cb(void *data, size_t size, size_t *free_async_space, int is_async)
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0))
static void binder_alloc_new_buf_locked_cb(void *data, size_t size, struct binder_alloc *alloc, int is_async)
#endif
{
	struct task_struct *p = NULL;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0))
	struct binder_alloc *alloc = NULL;

	alloc = container_of(free_async_space, struct binder_alloc, free_async_space);
	if (alloc == NULL) {
		return;
	}
#endif
	if (is_async
		&& (alloc->free_async_space < 3 * (size + sizeof(struct binder_buffer))
		|| (alloc->free_async_space < WARN_AHEAD_SPACE))) {
		rcu_read_lock();
		p = find_task_by_vpid(alloc->pid);
		rcu_read_unlock();
		if (p != NULL && rkx_is_frozen(p)) {
			rkx_log_debug("Binder Free buffer full! from=%d | target=%d\n", task_uid(current).val, task_uid(p).val);
			if (rkx_netlink_ready()) {
				struct rkx_event event = {
					.type = RKX_EVT_BINDER,
					.u.binder = {
						.binder_type = RKX_BINDER_FREE_BUFFER_FULL,
						.oneway = 1,
						.from_pid = task_tgid_nr(current),
						.from_uid = task_uid(current).val,
						.target_pid = task_tgid_nr(p),
						.target_uid = task_uid(p).val,
						.code = -1,
						.rpc_name = "FREE_BUFFER_FULL",
					},
				};
				rkx_send_message(&event);
			}
		}
	}
}

static struct hlist_head *k_binder_procs = NULL;
static struct mutex *k_binder_procs_lock = NULL;

static bool alloc_buf_hooked;
static bool preset_hooked;
static bool reply_hooked;
static bool trans_hooked;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6, 6, 0))
static void binder_preset_cb(void *data, struct hlist_head *hhead,
	struct mutex *lock, struct binder_proc *proc)
#else
static void binder_preset_cb(void *data, struct hlist_head *hhead,
	struct mutex *lock)
#endif
{
	if (k_binder_procs == NULL)
		k_binder_procs = hhead;

	if (k_binder_procs_lock == NULL)
		k_binder_procs_lock = lock;
}

static void binder_reply_cb(void *data, struct binder_proc *target_proc, struct binder_proc *proc,
	struct binder_thread *thread, struct binder_transaction_data *tr)
{
	if (target_proc
		&& (NULL != target_proc->tsk)
		&& (NULL != proc->tsk)
		&& (task_uid(target_proc->tsk).val <= MAX_SYSTEM_UID)
		&& (proc->pid != target_proc->pid)
		&& rkx_is_frozen(target_proc->tsk)) {
		rkx_log_debug("Sync Binder Reply! from=%d | target=%d\n", task_uid(proc->tsk).val, task_uid(target_proc->tsk).val);
		if (rkx_netlink_ready()) {
			struct rkx_event event = {
				.type = RKX_EVT_BINDER,
				.u.binder = {
					.binder_type = RKX_BINDER_REPLY,
					.from_pid = task_tgid_nr(proc->tsk),
					.from_uid = task_uid(proc->tsk).val,
					.target_pid = task_tgid_nr(target_proc->tsk),
					.target_uid = task_uid(target_proc->tsk).val,
					.code = -1,
					.rpc_name = "SYNC_BINDER_REPLY",
				},
			};
			rkx_send_message(&event);
		}
	}
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0))
static long rk_copy_from_user_nofault(void *dst, const void __user *src, size_t size)
{
	long ret = -EFAULT;
	if (access_ok(src, size)) {
		pagefault_disable();
		ret = __copy_from_user_inatomic(dst, src, size);
		pagefault_enable();
	}
	if (ret)
		return -EFAULT;
	return 0;
}
#endif

static long rk_copy_from_user(void *dst, const void __user *src, size_t size)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0))
	return rk_copy_from_user_nofault(dst, src, size);
#else
	return copy_from_user(dst, src, size);
#endif
}

static void binder_trans_cb(void *data, struct binder_proc *target_proc, struct binder_proc *proc,
	struct binder_thread *thread, struct binder_transaction_data *tr)
{
	if (!(tr->flags & TF_ONE_WAY) /* sync binder */
		&& target_proc
		&& (NULL != target_proc->tsk)
		&& (NULL != proc->tsk)
		&& (task_uid(target_proc->tsk).val > MIN_USERAPP_UID)
		&& (proc->pid != target_proc->pid)
		&& rkx_is_frozen(target_proc->tsk)) {
		rkx_log_debug("Sync Binder Transaction! from=%d | target=%d\n", task_uid(proc->tsk).val, task_uid(target_proc->tsk).val);
		if (rkx_netlink_ready()) {
			struct rkx_event event = {
				.type = RKX_EVT_BINDER,
				.u.binder = {
					.binder_type = RKX_BINDER_TRANSACTION,
					.from_pid = task_tgid_nr(proc->tsk),
					.from_uid = task_uid(proc->tsk).val,
					.target_pid = task_tgid_nr(target_proc->tsk),
					.target_uid = task_uid(target_proc->tsk).val,
					.code = -1,
					.rpc_name = "SYNC_BINDER",
				},
			};
			rkx_send_message(&event);
		}
	}

	if ((tr->flags & TF_ONE_WAY) /* async binder */
		&& target_proc
		&& (NULL != target_proc->tsk)
		&& (NULL != proc->tsk)
		&& (task_uid(target_proc->tsk).val > MIN_USERAPP_UID)
		&& (proc->pid != target_proc->pid)
		&& rkx_is_frozen(target_proc->tsk)) {
		char buf_data[INTERFACETOKEN_BUFF_SIZE];
		char rpc_name[INTERFACETOKEN_BUFF_SIZE] = {0};
		size_t buf_data_size;
		int i = 0, j = 0;

		buf_data_size = tr->data_size > INTERFACETOKEN_BUFF_SIZE ? INTERFACETOKEN_BUFF_SIZE : tr->data_size;
		if (!rk_copy_from_user(buf_data, (char*)tr->data.ptr.buffer, buf_data_size)) {
			if (buf_data_size > PARCEL_OFFSET) {
				char *p = (char *)(buf_data) + PARCEL_OFFSET;
				j = PARCEL_OFFSET + 1;
				while (i < INTERFACETOKEN_BUFF_SIZE && j < buf_data_size && *p != '\0') {
					rpc_name[i++] = *p;
					j += 2;
					p += 2;
				}
				if (i == INTERFACETOKEN_BUFF_SIZE) rpc_name[i-1] = '\0';
			}
			rkx_log_debug("ASync Binder Transaction! from=%d | target=%d\n", task_uid(proc->tsk).val, task_uid(target_proc->tsk).val);
			if (rkx_netlink_ready()) {
				struct rkx_event event = {
					.type = RKX_EVT_BINDER,
					.u.binder = {
						.binder_type = RKX_BINDER_TRANSACTION,
						.oneway = 1,
						.from_pid = task_tgid_nr(proc->tsk),
						.from_uid = task_uid(proc->tsk).val,
						.target_pid = task_tgid_nr(target_proc->tsk),
						.target_uid = task_uid(target_proc->tsk).val,
						.code = tr->code,
					},
				};
				strscpy(event.u.binder.rpc_name, rpc_name, sizeof(event.u.binder.rpc_name));
				rkx_send_message(&event);
			}
		}
	}
}

int rkx_register_binder(void)
{
	int rc = LINE_SUCCESS;

	rc = register_trace_android_vh_binder_alloc_new_buf_locked(binder_alloc_new_buf_locked_cb, NULL);
	if (rc != LINE_SUCCESS) {
		rkx_log_err("register_trace_android_vh_binder_alloc_new_buf_locked failed, rc=%d\n", rc);
		goto err;
	}
	alloc_buf_hooked = true;

	rc = register_trace_android_vh_binder_preset(binder_preset_cb, NULL);
	if (rc != LINE_SUCCESS) {
		rkx_log_err("register_trace_android_vh_binder_preset failed, rc=%d\n", rc);
		goto err;
	}
	preset_hooked = true;

	rc = register_trace_android_vh_binder_reply(binder_reply_cb, NULL);
	if (rc != LINE_SUCCESS) {
		rkx_log_err("register_trace_android_vh_binder_reply failed, rc=%d\n", rc);
		goto err;
	}
	reply_hooked = true;

	rc = register_trace_android_vh_binder_trans(binder_trans_cb, NULL);
	if (rc != LINE_SUCCESS) {
		rkx_log_err("register_trace_android_vh_binder_trans failed, rc=%d\n", rc);
		goto err;
	}
	trans_hooked = true;

	return LINE_SUCCESS;
err:
	rkx_unregister_binder();
	return rc;
}

void rkx_unregister_binder(void)
{
	if (trans_hooked) {
		unregister_trace_android_vh_binder_trans(binder_trans_cb, NULL);
		trans_hooked = false;
	}
	if (reply_hooked) {
		unregister_trace_android_vh_binder_reply(binder_reply_cb, NULL);
		reply_hooked = false;
	}
	if (preset_hooked) {
		unregister_trace_android_vh_binder_preset(binder_preset_cb, NULL);
		preset_hooked = false;
	}
	if (alloc_buf_hooked) {
		unregister_trace_android_vh_binder_alloc_new_buf_locked(binder_alloc_new_buf_locked_cb, NULL);
		alloc_buf_hooked = false;
	}
}
