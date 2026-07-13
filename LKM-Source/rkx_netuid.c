/*
 * Copyright (c) 2026 myflavor <admin@myflv.cn>. All rights reserved.
 * Based on Re-Kernel project by nep_timeline@outlook.com.
 * File: rkx_netuid.c — Network monitor UID hashmap management.
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

/* hashmap for net monitor uids */
#define RKX_NET_UID_HASH_BITS 6
static DEFINE_HASHTABLE(rkx_net_uid_map, RKX_NET_UID_HASH_BITS);
struct uid_info {
	uid_t uid;
	struct hlist_node hnode;
	struct rcu_head rcu;
};

static DEFINE_MUTEX(rkx_net_uid_mutex);

bool net_uid_monitored_rcu(uid_t uid)
{
	struct uid_info *entry;
	bool found = false;

	hash_for_each_possible_rcu(rkx_net_uid_map, entry, hnode, uid) {
		if (entry->uid == uid) {
			found = true;
			break;
		}
	}
	return found;
}

/* add a uid to the monitor map (no-op if already present). Caller must NOT hold the mutex. */
void net_uid_add(uid_t uid)
{
	struct uid_info *entry;
	bool found = false;

	mutex_lock(&rkx_net_uid_mutex);
	hash_for_each_possible(rkx_net_uid_map, entry, hnode, uid) {
		if (entry->uid == uid) {
			found = true;
			break;
		}
	}
	if (!found) {
		entry = kmalloc(sizeof(*entry), GFP_KERNEL);
		if (entry) {
			entry->uid = uid;
			hash_add_rcu(rkx_net_uid_map, &entry->hnode, uid);
		}
	}
	mutex_unlock(&rkx_net_uid_mutex);
}

/* remove a uid from the monitor map. Caller must NOT hold the mutex. */
void net_uid_del(uid_t uid)
{
	struct uid_info *entry;

	mutex_lock(&rkx_net_uid_mutex);
	hash_for_each_possible(rkx_net_uid_map, entry, hnode, uid) {
		if (entry->uid == uid) {
			hash_del_rcu(&entry->hnode);
			kfree_rcu(entry, rcu);
			break;
		}
	}
	mutex_unlock(&rkx_net_uid_mutex);
}

void net_uid_destroy(void)
{
	struct uid_info *entry;
	struct hlist_node *tmp;
	int bkt;

	mutex_lock(&rkx_net_uid_mutex);
	hash_for_each_safe(rkx_net_uid_map, bkt, tmp, entry, hnode) {
		hash_del_rcu(&entry->hnode);
		kfree_rcu(entry, rcu);
	}
	mutex_unlock(&rkx_net_uid_mutex);
}

void net_uid_init(void)
{
	hash_init(rkx_net_uid_map);
}
