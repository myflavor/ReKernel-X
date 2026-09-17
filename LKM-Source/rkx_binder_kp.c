/*
 * Copyright (c) 2026 myflavor <admin@myflv.cn>. All rights reserved.
 * Based on Re-Kernel project by nep_timeline@outlook.com.
 * File: rkx_binder_kp.c — Kprobe hooks for Binder transaction filtering.
 */

#include "rkx_log.h"
#include "rkx.h"
#include "rkx_binder_alloc.h"
#include <linux/printk.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/kprobes.h>
#include <linux/string.h>
#include <linux/version.h>
#include <linux/workqueue.h>
#include "../android/binder_internal.h"

static unsigned long (*k_kallsyms_lookup_name)(const char* name);
static void (*k_binder_transaction_buffer_release)(struct binder_proc* proc, struct binder_thread* thread, struct binder_buffer* buffer, binder_size_t off_end_offset, bool is_failure);
static void (*k_binder_alloc_free_buf)(struct binder_alloc* alloc, struct binder_buffer* buffer);
static int (*k_binder_alloc_copy_from_buffer)(struct binder_alloc* alloc, void* dest, struct binder_buffer* buffer, binder_size_t buffer_offset, size_t bytes);
static struct binder_stats(*k_binder_stats);
static void (*k_binder_proc_dec_tmpref)(struct binder_proc* proc);
static void (*k_binder_free_proc)(struct binder_proc* proc);

static struct workqueue_struct *rkx_free_wq;

struct rkx_free_txn_work {
	struct work_struct work;
	struct binder_proc *proc;
	struct binder_buffer *buffer;
	struct binder_transaction *t;
};

static inline void rk_binder_inner_proc_lock(struct binder_proc* proc)
__acquires(&proc->inner_lock)
{
	spin_lock(&proc->inner_lock);
}

static inline void rk_binder_inner_proc_unlock(struct binder_proc* proc)
__releases(&proc->inner_lock)
{
	spin_unlock(&proc->inner_lock);
}

static inline void rk_binder_node_lock(struct binder_node* node)
__acquires(&node->lock)
{
	spin_lock(&node->lock);
}

static inline void rk_binder_node_unlock(struct binder_node* node)
__releases(&node->lock)
{
	spin_unlock(&node->lock);
}

static bool binder_buffer_data_equal(struct binder_proc* proc,
	struct binder_buffer* b1, struct binder_buffer* b2)
{
	size_t pos, chunk, total;
	u8 c1[64];
	u8 c2[64];

	if (!proc || !b1 || !b2 || !k_binder_alloc_copy_from_buffer)
		return false;
	if (b1->data_size != b2->data_size)
		return false;

	total = b1->data_size;
	pos = 0;
	while (pos < total) {
		chunk = total - pos;
		if (chunk > sizeof(c1))
			chunk = sizeof(c1);
		if (k_binder_alloc_copy_from_buffer(&proc->alloc, c1, b1, pos, chunk))
			return false;
		if (k_binder_alloc_copy_from_buffer(&proc->alloc, c2, b2, pos, chunk))
			return false;
		if (memcmp(c1, c2, chunk))
			return false;
		pos += chunk;
	}
	return true;
}

static bool rkx_parse_interface_token(struct binder_proc* proc,
	struct binder_buffer* buffer, char* rpc_name, size_t rpc_name_size)
{
	u8 hdr[INTERFACETOKEN_BUFF_SIZE];
	size_t copy_size;
	size_t i = 0;
	size_t j;
	char* p;

	if (!proc || !buffer || !rpc_name || rpc_name_size == 0 ||
	    !k_binder_alloc_copy_from_buffer)
		return false;

	rpc_name[0] = '\0';
	if (buffer->data_size <= PARCEL_OFFSET)
		return false;

	copy_size = buffer->data_size;
	if (copy_size > sizeof(hdr))
		copy_size = sizeof(hdr);

	if (k_binder_alloc_copy_from_buffer(&proc->alloc, hdr, buffer, 0, copy_size))
		return false;

	p = (char*)hdr + PARCEL_OFFSET;
	j = PARCEL_OFFSET + 1;
	while (i + 1 < rpc_name_size && j < copy_size && *p != '\0') {
		rpc_name[i++] = *p;
		j += 2;
		p += 2;
	}
	rpc_name[i] = '\0';
	return i > 0;
}

static bool rk_binder_can_update_transaction(struct binder_transaction* t1,
	struct binder_transaction* t2, u8 strategy)
{
	if ((t1->flags & t2->flags & TF_ONE_WAY) != TF_ONE_WAY || !t1->to_proc || !t2->to_proc)
		return false;
	if (t1->to_proc->tsk == t2->to_proc->tsk && t1->code == t2->code &&
		t1->flags == t2->flags && t1->buffer->pid == t2->buffer->pid &&
		t1->buffer->target_node->ptr == t2->buffer->target_node->ptr &&
		t1->buffer->target_node->cookie == t2->buffer->target_node->cookie) {
		if (t1->buffer->offsets_size != 0 || t2->buffer->offsets_size != 0)
			return false;
		if (strategy == RKX_FREE_ASYNC_BY_CODE)
			return true;
		if (strategy == RKX_FREE_ASYNC_BY_DATA)
			return binder_buffer_data_equal(t1->to_proc, t1->buffer, t2->buffer);
	}
	return false;
}

static struct binder_transaction* rk_binder_find_outdated_transaction_ilocked(
	struct binder_transaction* t, struct list_head* target_list, u8 strategy)
{
	struct binder_work* w;

	list_for_each_entry(w, target_list, entry) {
		struct binder_transaction* t_queued;

		if (w->type != BINDER_WORK_TRANSACTION)
			continue;
		t_queued = container_of(w, struct binder_transaction, work);
		if (rk_binder_can_update_transaction(t_queued, t, strategy))
			return t_queued;
	}
	return NULL;
}

static inline void __nocfi k_binder_release_entire_buffer(struct binder_proc* proc,
	struct binder_thread* thread, struct binder_buffer* buffer, bool is_failure)
{
	binder_size_t off_end_offset;

	off_end_offset = ALIGN(buffer->data_size, sizeof(void*));
	off_end_offset += buffer->offsets_size;

	k_binder_transaction_buffer_release(proc, thread, buffer,
		off_end_offset, is_failure);
}

static inline void k_binder_stats_deleted(enum binder_stat_types type)
{
	atomic_inc(&k_binder_stats->obj_deleted[type]);
}

static void __nocfi rk_binder_proc_dec_tmpref(struct binder_proc *proc)
{
	if (k_binder_proc_dec_tmpref) {
		k_binder_proc_dec_tmpref(proc);
		return;
	}

	rk_binder_inner_proc_lock(proc);
	proc->tmp_ref--;
	if (proc->is_dead && RB_EMPTY_ROOT(&proc->threads) && !proc->tmp_ref) {
		rk_binder_inner_proc_unlock(proc);
		k_binder_free_proc(proc);
		return;
	}
	rk_binder_inner_proc_unlock(proc);
}

static void __nocfi rkx_free_txn_func(struct work_struct *work)
{
	struct rkx_free_txn_work *w =
		container_of(work, struct rkx_free_txn_work, work);

	k_binder_release_entire_buffer(w->proc, NULL, w->buffer, false);
	k_binder_alloc_free_buf(&w->proc->alloc, w->buffer);
	kfree(w->t);
	k_binder_stats_deleted(BINDER_STAT_TRANSACTION);
	rk_binder_proc_dec_tmpref(w->proc);
	kfree(w);
}

static void __nocfi rkx_queue_free_txn(struct rkx_free_txn_work *w,
	struct binder_proc *proc, struct binder_transaction *t, struct binder_buffer *buffer)
{
	w->proc = proc;
	w->buffer = buffer;
	w->t = t;
	INIT_WORK(&w->work, rkx_free_txn_func);
	queue_work(rkx_free_wq, &w->work);
}

static int __nocfi binder_proc_transaction_pre(struct kprobe* p, struct pt_regs* regs)
{
	struct binder_transaction* t = (struct binder_transaction*)regs->regs[0];
	struct binder_proc* proc = (struct binder_proc*)regs->regs[1];

	struct binder_node* node = t->buffer->target_node;
	struct binder_transaction* t_outdated = NULL;
	struct rkx_free_txn_work* w = NULL;
	char rpc_name[INTERFACETOKEN_BUFF_SIZE] = {0};
	u8 strategy;

	if (!(t->flags & TF_ONE_WAY))
		return 0;

	if (rkx_is_frozen(proc->tsk)) {
		strategy = RKX_FREE_ASYNC_BY_CODE;
		if (rkx_free_async_has_entries() && rkx_parse_interface_token(proc, t->buffer, rpc_name, sizeof(rpc_name)))
			rkx_free_async_lookup_rcu(rpc_name, t->code, &strategy);
		if (strategy == RKX_FREE_ASYNC_SKIP)
			return 0;

		rk_binder_node_lock(node);
		if (!node->has_async_transaction) {
			rk_binder_node_unlock(node);
			return 0;
		}
		rk_binder_inner_proc_lock(proc);
		if (proc->is_dead || proc->is_frozen) {
			rk_binder_inner_proc_unlock(proc);
			rk_binder_node_unlock(node);
			return 0;
		}
		t_outdated = rk_binder_find_outdated_transaction_ilocked(t, &node->async_todo, strategy);
		if (t_outdated) {
			w = kzalloc(sizeof(*w), GFP_ATOMIC);
			if (w) {
				proc->tmp_ref++;
				list_del_init(&t_outdated->work.entry);
				proc->outstanding_txns--;
			} else {
				t_outdated = NULL;
			}
		}
		rk_binder_inner_proc_unlock(proc);
		rk_binder_node_unlock(node);

		if (t_outdated) {
			struct binder_buffer* buffer = t_outdated->buffer;
			rkx_log_debug("free_outdated uid=%u rpc=%s code=%d strategy=%u debug_id=%d data_size=%zu\n",
				task_uid(proc->tsk).val, rpc_name, t->code, strategy, t_outdated->debug_id, buffer->data_size);
			t_outdated->buffer = NULL;
			buffer->transaction = NULL;
			rkx_queue_free_txn(w, proc, t_outdated, buffer);
		}
	}
	return 0;
}

static struct kprobe kp_kallsyms_lookup_name = {
	.symbol_name = "kallsyms_lookup_name"
};
static struct kprobe kp_binder_proc_transaction = {
	.symbol_name = "binder_proc_transaction",
	.pre_handler = binder_proc_transaction_pre
};

static bool kp_registered;

void __nocfi rkx_register_binder_kp(void)
{
	int rc = LINE_SUCCESS;

	rkx_free_wq = alloc_workqueue("rkx_free_async", WQ_UNBOUND | WQ_MEM_RECLAIM, 0);
	if (!rkx_free_wq) {
		rkx_log_err("alloc free-async workqueue failed (free-async disabled)\n");
		goto err;
	}

	rc = register_kprobe(&kp_kallsyms_lookup_name);
	if (rc != LINE_SUCCESS) {
		rkx_log_err("register kallsyms_lookup_name kprobe failed, rc=%d (free-async disabled)\n", rc);
		goto err;
	}
	k_kallsyms_lookup_name = (void *)kp_kallsyms_lookup_name.addr;
	unregister_kprobe(&kp_kallsyms_lookup_name);

	k_binder_transaction_buffer_release = (void*)k_kallsyms_lookup_name("binder_transaction_buffer_release");
	k_binder_alloc_free_buf = (void*)k_kallsyms_lookup_name("binder_alloc_free_buf");
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 0, 0)
	k_binder_alloc_copy_from_buffer = rk_binder_alloc_copy_from_buffer;
#else
	k_binder_alloc_copy_from_buffer = (void *)k_kallsyms_lookup_name("binder_alloc_copy_from_buffer");
#endif
	k_binder_stats = (void*)k_kallsyms_lookup_name("binder_stats");
	k_binder_proc_dec_tmpref = (void*)k_kallsyms_lookup_name("binder_proc_dec_tmpref");
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 15, 0)
	k_binder_free_proc = (void*)k_kallsyms_lookup_name("binder_free_proc");
#endif

	if (k_binder_proc_dec_tmpref == NULL && k_binder_free_proc == NULL) {
		rkx_log_err("resolve tmpref helpers failed (free-async disabled)\n");
		goto err;
	}

	if (k_binder_transaction_buffer_release == NULL || k_binder_alloc_free_buf == NULL ||
	    k_binder_alloc_copy_from_buffer == NULL || k_binder_stats == NULL) {
		rkx_log_err("resolve binder symbols failed (free-async disabled)\n");
		goto err;
	}

	rc = register_kprobe(&kp_binder_proc_transaction);
	if (rc != LINE_SUCCESS) {
		rkx_log_err("register binder_proc_transaction kprobe failed, rc=%d (free-async disabled)\n", rc);
		goto err;
	}
	kp_registered = true;
	return;

err:
	rkx_unregister_binder_kp();
}

void rkx_unregister_binder_kp(void)
{
	if (kp_registered) {
		unregister_kprobe(&kp_binder_proc_transaction);
		kp_registered = false;
	}
	if (rkx_free_wq) {
		flush_workqueue(rkx_free_wq);
		destroy_workqueue(rkx_free_wq);
		rkx_free_wq = NULL;
	}
}
