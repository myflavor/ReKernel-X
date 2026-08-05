/*
 * Copyright (c) 2026 myflavor <admin@myflv.cn>. All rights reserved.
 * File: rkx_free_async.c — free async rule management.
 */

#include "rkx_log.h"
#include "rkx.h"
#include <linux/printk.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/list.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/hashtable.h>
#include <linux/slab.h>
#include <linux/rcupdate.h>
#include <linux/string.h>
#include <linux/jhash.h>
#include <linux/atomic.h>

#define RKX_FREE_ASYNC_HASH_BITS 6

struct free_async_entry {
	char rpc_name[INTERFACETOKEN_BUFF_SIZE];
	s32 code;
	u8 strategy;
	struct hlist_node hnode;
	struct rcu_head rcu;
};

static DEFINE_HASHTABLE(rkx_free_async_map, RKX_FREE_ASYNC_HASH_BITS);
static DEFINE_MUTEX(rkx_free_async_mutex);
static atomic_t rkx_free_async_count = ATOMIC_INIT(0);

static inline u32 free_async_hash(const char *rpc_name)
{
	return jhash(rpc_name, strlen(rpc_name), 0);
}

static bool free_async_strategy_valid(u8 strategy)
{
	return strategy == RKX_FREE_ASYNC_SKIP ||
	       strategy == RKX_FREE_ASYNC_BY_CODE ||
	       strategy == RKX_FREE_ASYNC_BY_DATA;
}

bool free_async_has_entries(void)
{
	return atomic_read(&rkx_free_async_count) > 0;
}

bool free_async_lookup_rcu(const char *rpc_name, s32 code, u8 *strategy_out)
{
	struct free_async_entry *entry;
	u32 key;
	u8 exact_strategy = 0;
	u8 wild_strategy = 0;

	if (!rpc_name || !strategy_out || !free_async_has_entries())
		return false;

	key = free_async_hash(rpc_name);

	rcu_read_lock();
	hash_for_each_possible_rcu(rkx_free_async_map, entry, hnode, key) {
		if (strcmp(entry->rpc_name, rpc_name))
			continue;
		if (entry->code == code) {
			exact_strategy = READ_ONCE(entry->strategy);
			break;
		}
		if (entry->code == -1)
			wild_strategy = READ_ONCE(entry->strategy);
	}
	rcu_read_unlock();

	if (exact_strategy) {
		*strategy_out = exact_strategy;
		return true;
	}
	if (wild_strategy) {
		*strategy_out = wild_strategy;
		return true;
	}
	return false;
}

int add_free_async(const char *rpc_name, s32 code, u8 strategy)
{
	struct free_async_entry *entry;
	size_t len;
	u32 key;
	bool found = false;

	if (!rpc_name || !*rpc_name || code < -1 || !free_async_strategy_valid(strategy))
		return -EINVAL;

	len = strnlen(rpc_name, INTERFACETOKEN_BUFF_SIZE);
	if (len == 0 || len >= INTERFACETOKEN_BUFF_SIZE)
		return -EINVAL;

	key = free_async_hash(rpc_name);

	mutex_lock(&rkx_free_async_mutex);
	hash_for_each_possible(rkx_free_async_map, entry, hnode, key) {
		if (entry->code == code && !strcmp(entry->rpc_name, rpc_name)) {
			WRITE_ONCE(entry->strategy, strategy);
			found = true;
			break;
		}
	}
	if (!found) {
		entry = kmalloc(sizeof(*entry), GFP_KERNEL);
		if (!entry) {
			mutex_unlock(&rkx_free_async_mutex);
			return -ENOMEM;
		}
		strscpy(entry->rpc_name, rpc_name, sizeof(entry->rpc_name));
		entry->code = code;
		entry->strategy = strategy;
		hash_add_rcu(rkx_free_async_map, &entry->hnode, key);
		atomic_inc(&rkx_free_async_count);
	}
	mutex_unlock(&rkx_free_async_mutex);

	rkx_log_debug("addFreeAsync rpc=%s code=%d strategy=%u\n",
		rpc_name, code, strategy);
	return 0;
}

int del_free_async(const char *rpc_name, s32 code)
{
	struct free_async_entry *entry;
	u32 key;
	bool found = false;

	if (!rpc_name || !*rpc_name || code < -1)
		return -EINVAL;

	key = free_async_hash(rpc_name);

	mutex_lock(&rkx_free_async_mutex);
	hash_for_each_possible(rkx_free_async_map, entry, hnode, key) {
		if (entry->code == code && !strcmp(entry->rpc_name, rpc_name)) {
			hash_del_rcu(&entry->hnode);
			kfree_rcu(entry, rcu);
			atomic_dec(&rkx_free_async_count);
			found = true;
			break;
		}
	}
	mutex_unlock(&rkx_free_async_mutex);

	if (found)
		rkx_log_debug("delFreeAsync rpc=%s code=%d\n", rpc_name, code);
	return 0;
}

void destroy_free_async(void)
{
	struct free_async_entry *entry;
	struct hlist_node *tmp;
	int bkt;

	mutex_lock(&rkx_free_async_mutex);
	hash_for_each_safe(rkx_free_async_map, bkt, tmp, entry, hnode) {
		hash_del_rcu(&entry->hnode);
		kfree_rcu(entry, rcu);
	}
	atomic_set(&rkx_free_async_count, 0);
	mutex_unlock(&rkx_free_async_mutex);
	rcu_barrier();
}

void init_free_async(void)
{
	hash_init(rkx_free_async_map);
	atomic_set(&rkx_free_async_count, 0);
}
