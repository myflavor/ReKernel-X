/*
 * Copyright (c) 2026 myflavor <admin@myflv.cn>. All rights reserved.
 * Based on Re-Kernel project by nep_timeline@outlook.com.
 * File: rkx_btsock.c — Bluetooth socket learning, readable-event & cleanup kprobes.
 *
 * How it works:
 *   - The Bluetooth stack process (AID_BLUETOOTH, uid 1002) creates a
 *     socketpair() and hands one end to the app through binder.  Both ends
 *     carry the creator's uid, so the inode uid identifies the *creator*
 *     (1002), not the app.
 *   - Learning probe (sock_recvmsg): the app is the one which calls
 *     read()/recvmsg() on the app end -- "who is reading" is free from the
 *     syscall context (current).  If the caller is a monitored uid and the
 *     socket was created by the Bluetooth stack, remember {sk -> uid}.
 *     The watch channel enters read() right after connect(), so the
 *     mapping is learned long before the app is frozen.
 *   - Event probe (sock_def_readable): every write() into an AF_UNIX
 *     socketpair invokes the receiver's sk_data_ready() (default
 *     sock_def_readable) with the *receiver* sock -- even while the app is
 *     frozen, because the Bluetooth stack process is the writer.
 *   - Cleanup probe (sock_release): drop the entry when the socket dies.
 */

#include "rkx_log.h"
#include "rkx.h"
#include <linux/printk.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/version.h>
#include <linux/sched.h>
#include <linux/uidgid.h>
#include <linux/spinlock.h>
#include <linux/hashtable.h>
#include <linux/slab.h>
#include <linux/rcupdate.h>
#include <linux/kprobes.h>
#include <linux/net.h>
#include <linux/socket.h>
#include <net/sock.h>

#define RKX_BT_UID_HASH_BITS 6
#define RKX_BT_SOCK_HASH_BITS 6

struct rkx_bt_uid_info {
	uid_t uid;
	struct hlist_node hnode;
	struct rcu_head rcu;
};

struct rkx_bt_sock_info {
	struct sock *sk;
	uid_t uid;
	struct hlist_node hnode;
	struct rcu_head rcu;
};

/* monitored uids (apps whose Bluetooth sockets we track) */
static DEFINE_HASHTABLE(rkx_bt_uid_map, RKX_BT_UID_HASH_BITS);
static DEFINE_MUTEX(rkx_bt_uid_mutex);
static atomic_t rkx_bt_uid_count = ATOMIC_INIT(0);

/* learned sockets: {sk -> uid} */
static DEFINE_HASHTABLE(rkx_bt_sock_map, RKX_BT_SOCK_HASH_BITS);
static DEFINE_SPINLOCK(rkx_bt_sock_lock);

static inline bool rkx_bt_monitor_active(void)
{
	return atomic_read(&rkx_bt_uid_count) > 0;
}

bool rkx_bt_uid_monitored_rcu(uid_t uid)
{
	struct rkx_bt_uid_info *entry;
	bool found = false;

	hash_for_each_possible_rcu(rkx_bt_uid_map, entry, hnode, uid) {
		if (entry->uid == uid) {
			found = true;
			break;
		}
	}
	return found;
}

/* add a uid to the monitor map (no-op if already present). Caller must NOT hold the mutex. */
void rkx_add_bt_uid(uid_t uid)
{
	struct rkx_bt_uid_info *entry;
	bool found = false;

	mutex_lock(&rkx_bt_uid_mutex);
	hash_for_each_possible(rkx_bt_uid_map, entry, hnode, uid) {
		if (entry->uid == uid) {
			found = true;
			break;
		}
	}
	if (!found) {
		entry = kmalloc(sizeof(*entry), GFP_KERNEL);
		if (entry) {
			entry->uid = uid;
			hash_add_rcu(rkx_bt_uid_map, &entry->hnode, uid);
			atomic_inc(&rkx_bt_uid_count);
		}
	}
	mutex_unlock(&rkx_bt_uid_mutex);
}

/* remove a uid from the monitor map and drop every socket learned for it. */
void rkx_del_bt_uid(uid_t uid)
{
	struct rkx_bt_uid_info *entry;
	struct rkx_bt_sock_info *sinfo;
	struct hlist_node *tmp;
	HLIST_HEAD(doomed);
	bool found = false;
	int bkt;

	mutex_lock(&rkx_bt_uid_mutex);
	hash_for_each_possible(rkx_bt_uid_map, entry, hnode, uid) {
		if (entry->uid == uid) {
			hash_del_rcu(&entry->hnode);
			kfree_rcu(entry, rcu);
			atomic_dec(&rkx_bt_uid_count);
			found = true;
			break;
		}
	}
	mutex_unlock(&rkx_bt_uid_mutex);

	if (!found)
		return;

	spin_lock_bh(&rkx_bt_sock_lock);
	hash_for_each_safe(rkx_bt_sock_map, bkt, tmp, sinfo, hnode) {
		if (sinfo->uid != uid)
			continue;
		hash_del_rcu(&sinfo->hnode);
		hlist_add_head(&sinfo->hnode, &doomed);
	}
	spin_unlock_bh(&rkx_bt_sock_lock);

	hlist_for_each_entry_safe(sinfo, tmp, &doomed, hnode) {
		hlist_del(&sinfo->hnode);
		sock_put(sinfo->sk);
		kfree_rcu(sinfo, rcu);
	}
}

static struct rkx_bt_sock_info *rkx_bt_sock_lookup_rcu(struct sock *sk)
{
	struct rkx_bt_sock_info *entry;

	hash_for_each_possible_rcu(rkx_bt_sock_map, entry, hnode, (unsigned long)sk) {
		if (entry->sk == sk)
			return entry;
	}
	return NULL;
}

/* remember {sk -> uid}; no-op if the socket is already known. */
static void rkx_bt_sock_learn(struct sock *sk, uid_t uid)
{
	struct rkx_bt_sock_info *entry;
	struct rkx_bt_sock_info *known = NULL;

	entry = kmalloc(sizeof(*entry), GFP_ATOMIC);
	if (!entry)
		return;

	spin_lock_bh(&rkx_bt_sock_lock);
	hash_for_each_possible(rkx_bt_sock_map, known, hnode, (unsigned long)sk) {
		if (known->sk == sk) {
			spin_unlock_bh(&rkx_bt_sock_lock);
			kfree(entry);
			return;
		}
	}
	entry->sk = sk;
	entry->uid = uid;
	sock_hold(sk);
	hash_add_rcu(rkx_bt_sock_map, &entry->hnode, (unsigned long)sk);
	spin_unlock_bh(&rkx_bt_sock_lock);

	rkx_log_debug("bt learn sk=%px uid=%u\n", sk, uid);
}

/* drop the entry (sock_put only releases our own reference, the socket still owns one). */
static void rkx_bt_sock_forget(struct sock *sk)
{
	struct rkx_bt_sock_info *entry;
	struct rkx_bt_sock_info *match = NULL;

	spin_lock_bh(&rkx_bt_sock_lock);
	hash_for_each_possible(rkx_bt_sock_map, entry, hnode, (unsigned long)sk) {
		if (entry->sk == sk) {
			hash_del_rcu(&entry->hnode);
			match = entry;
			break;
		}
	}
	spin_unlock_bh(&rkx_bt_sock_lock);

	if (match) {
		sock_put(sk);
		kfree_rcu(match, rcu);
	}
}

/*
 * The Bluetooth discriminator: apps' own sockets carry the app uid, while
 * both ends of the stack-created socketpair carry AID_BLUETOOTH (1002).
 */
static bool rkx_bt_sock_is_bluetooth(struct sock *sk)
{
	struct socket *sock = READ_ONCE(sk->sk_socket);

	if (unlikely(!sock))
		return false;

	return SOCK_INODE(sock)->i_uid.val == RKX_AID_BLUETOOTH;
}

/* Learn: the monitored app reads a socket handed to it by the Bluetooth stack. */
static int __nocfi rkx_sock_recvmsg_pre(struct kprobe *p, struct pt_regs *regs)
{
	struct socket *sock = (struct socket *)regs->regs[0];
	struct sock *sk;
	uid_t uid;
	bool monitored;

	if (unlikely(!sock))
		return 0;

	if (!rkx_bt_monitor_active())
		return 0;

	uid = task_uid(current).val;
	rcu_read_lock();
	monitored = rkx_bt_uid_monitored_rcu(uid);
	rcu_read_unlock();
	if (!monitored)
		return 0;

	sk = READ_ONCE(sock->sk);
	if (unlikely(!sk || sk->sk_family != AF_UNIX))
		return 0;

	if (!rkx_bt_sock_is_bluetooth(sk))
		return 0;

	rkx_bt_sock_learn(sk, uid);
	return 0;
}

/* Event: data got queued on a learned socket -- report it. */
static int __nocfi rkx_sock_readable_pre(struct kprobe *p, struct pt_regs *regs)
{
	struct sock *sk = (struct sock *)regs->regs[0];
	struct rkx_bt_sock_info *entry;
	struct rkx_event event;
	bool hit = false;

	if (unlikely(!sk))
		return 0;

	if (!rkx_bt_monitor_active())
		return 0;

	rcu_read_lock();
	entry = rkx_bt_sock_lookup_rcu(sk);
	if (entry) {
		event.type = RKX_EVT_BLUETOOTH;
		event.u.bluetooth.target_uid = entry->uid;
		event.u.bluetooth.data_len = (s32)atomic_read(&sk->sk_rmem_alloc);
		hit = true;
	}
	rcu_read_unlock();

	if (!hit)
		return 0;

	rkx_log_debug("bt data uid=%u len=%d\n", event.u.bluetooth.target_uid,
		      event.u.bluetooth.data_len);

	if (rkx_netlink_ready()) {
		rkx_send_message(&event);
	}
	return 0;
}

/* Cleanup: the socket is going away. */
static int __nocfi rkx_sock_release_pre(struct kprobe *p, struct pt_regs *regs)
{
	struct socket *sock = (struct socket *)regs->regs[0];
	struct sock *sk;

	if (unlikely(!sock))
		return 0;

	if (!rkx_bt_monitor_active())
		return 0;

	sk = READ_ONCE(sock->sk);
	if (unlikely(!sk))
		return 0;

	rkx_bt_sock_forget(sk);
	return 0;
}

static struct kprobe kp_sock_recvmsg = {
	.symbol_name = "sock_recvmsg",
	.pre_handler = rkx_sock_recvmsg_pre,
};

static struct kprobe kp_sock_def_readable = {
	.symbol_name = "sock_def_readable",
	.pre_handler = rkx_sock_readable_pre,
};

static struct kprobe kp_sock_release = {
	.symbol_name = "sock_release",
	.pre_handler = rkx_sock_release_pre,
};

static bool rkx_bt_registered;

void rkx_register_btsock(void)
{
	int rc;

	rc = register_kprobe(&kp_sock_recvmsg);
	if (rc) {
		rkx_log_err("register sock_recvmsg kprobe failed, rc=%d (bt disabled)\n", rc);
		return;
	}

	rc = register_kprobe(&kp_sock_def_readable);
	if (rc) {
		rkx_log_err("register sock_def_readable kprobe failed, rc=%d (bt disabled)\n", rc);
		goto err_readable;
	}

	rc = register_kprobe(&kp_sock_release);
	if (rc) {
		rkx_log_err("register sock_release kprobe failed, rc=%d (bt disabled)\n", rc);
		goto err_release;
	}

	rkx_bt_registered = true;
	rkx_log_info("bt socket kprobes registered\n");
	return;

err_release:
	unregister_kprobe(&kp_sock_def_readable);
err_readable:
	unregister_kprobe(&kp_sock_recvmsg);
}

void rkx_unregister_btsock(void)
{
	if (!rkx_bt_registered)
		return;

	unregister_kprobe(&kp_sock_release);
	unregister_kprobe(&kp_sock_def_readable);
	unregister_kprobe(&kp_sock_recvmsg);
	rkx_bt_registered = false;
}

void rkx_init_btsock(void)
{
	hash_init(rkx_bt_sock_map);
	hash_init(rkx_bt_uid_map);
}

/* Only called on module exit: genl (and thus DEL_MONITOR_BT) is already gone. */
void rkx_destroy_btsock(void)
{
	struct rkx_bt_sock_info *sinfo;
	struct rkx_bt_uid_info *uinfo;
	struct hlist_node *tmp;
	int bkt;

	hash_for_each_safe(rkx_bt_sock_map, bkt, tmp, sinfo, hnode) {
		hash_del(&sinfo->hnode);
		sock_put(sinfo->sk);
		kfree_rcu(sinfo, rcu);
	}

	mutex_lock(&rkx_bt_uid_mutex);
	hash_for_each_safe(rkx_bt_uid_map, bkt, tmp, uinfo, hnode) {
		hash_del_rcu(&uinfo->hnode);
		kfree_rcu(uinfo, rcu);
	}
	mutex_unlock(&rkx_bt_uid_mutex);
	atomic_set(&rkx_bt_uid_count, 0);
}
