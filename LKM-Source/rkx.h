#ifndef RKX_H
#define RKX_H

#include <linux/types.h>
#include <linux/uidgid.h>

struct task_struct;

#ifndef RKX_VERSION
#define RKX_VERSION "dev"
#endif

#define CLEAN_UP_ASYNC_BINDER

#define MIN_USERAPP_UID (10000)
#define MAX_SYSTEM_UID (2000)
#define SYSTEM_APP_UID (1000)
#define RESERVE_ORDER 17
#define WARN_AHEAD_SPACE (1 << RESERVE_ORDER)
#define INTERFACETOKEN_BUFF_SIZE (140)
#define PARCEL_OFFSET (16) /* sync with the writeInterfaceToken */
#define LINE_ERROR (-1)
#define LINE_SUCCESS (0)

#define RKX_GENL_FAMILY_NAME "rekernel_x2"
#define RKX_GENL_VERSION 1
#define RKX_GENL_MCGRP_NAME "events"

// netlink cmd
enum rkx_genl_cmd
{
	RKX_C_UNSPEC,
	RKX_C_EVENT,			  // kernel -> user 
	RKX_C_ADD_MONITOR_NET, // user -> kernel
	RKX_C_DEL_MONITOR_NET, // user -> kernel
	__RKX_C_MAX,
};
#define RKX_C_MAX (__RKX_C_MAX - 1)

// netlink attr 
enum rkx_genl_attr
{
	RKX_A_UNSPEC,

	// event
	RKX_A_EVENT = 1, // nested

	// binder
	RKX_A_BINDER = 10,			 // nested
	RKX_A_BINDER_TYPE = 11,		 // s32
	RKX_A_BINDER_ONEWAY = 12,	 // s32
	RKX_A_BINDER_FROM_PID = 13,	 // s32
	RKX_A_BINDER_FROM_UID = 14,	 // s32
	RKX_A_BINDER_TARGET_PID = 15, // s32
	RKX_A_BINDER_TARGET_UID = 16, // s32
	RKX_A_BINDER_CODE = 17,		 // s32
	RKX_A_BINDER_RPC_NAME = 18,	 // nul_str

	// signal
	RKX_A_SIGNAL = 20,			 // nested
	RKX_A_SIGNAL_SIGNAL = 21,	 // s32
	RKX_A_SIGNAL_KILLER_PID = 22, // s32
	RKX_A_SIGNAL_KILLER_UID = 23, // s32
	RKX_A_SIGNAL_DST_PID = 24,	 // s32
	RKX_A_SIGNAL_DST_UID = 25,	 // s32

	// network
	RKX_A_NETWORK = 30,			  // nested
	RKX_A_NETWORK_PROTO = 31,	  // s32
	RKX_A_NETWORK_TARGET_UID = 32, // s32
	RKX_A_NETWORK_DATA_LEN = 33,	  // s32

	RKX_A_UID = 40, // u32

	__RKX_A_MAX = 49,
};
#define RKX_A_MAX __RKX_A_MAX

// event types
enum rkx_event_type
{
	RKX_EVT_BINDER = 1,
	RKX_EVT_SIGNAL = 2,
	RKX_EVT_NETWORK = 3,
};

// binder type
enum rkx_binder_type
{
	RKX_BINDER_TRANSACTION = 1,
	RKX_BINDER_REPLY = 2,
	RKX_BINDER_FREE_BUFFER_FULL = 3,
};

// network protocol
enum rkx_net_proto
{
	RKX_NET_PROTO_IPV4 = 4,
	RKX_NET_PROTO_IPV6 = 6,
};

struct rkx_binder_event
{
	__u8 binder_type;
	__u8 oneway;
	__s32 from_pid;
	__u32 from_uid;
	__s32 target_pid;
	__u32 target_uid;
	__s32 code;
	char rpc_name[INTERFACETOKEN_BUFF_SIZE];
};

struct rkx_signal_event
{
	__s32 signal;
	__s32 killer_pid;
	__u32 killer_uid;
	__s32 dst_pid;
	__u32 dst_uid;
};

struct rkx_network_event
{
	__u8 proto;
	__u32 target_uid;
	__s32 data_len;
};

struct rkx_event
{
	__u8 type;
	union
	{
		struct rkx_binder_event binder;
		struct rkx_signal_event signal;
		struct rkx_network_event network;
	} u;
};

// genl.c
bool rkx_netlink_ready(void);
int sendMessage(struct rkx_event *event);
int register_genl(void);
void unregister_genl(void);

// net_uid.c
bool net_uid_monitored_rcu(uid_t uid);
void net_uid_add(uid_t uid);
void net_uid_del(uid_t uid);
void net_uid_init(void);
void net_uid_destroy(void);

// frozen.c
bool line_is_frozen(struct task_struct *task);

// binder.c
int register_binder(void);
void unregister_binder(void);

// signal.c
int register_signal(void);
void unregister_signal(void);

// netfilter.c
int register_netfilter(void);
void unregister_netfilter(void);

// binder_kp.c
void register_binder_kp(void);
void unregister_binder_kp(void);

#endif
