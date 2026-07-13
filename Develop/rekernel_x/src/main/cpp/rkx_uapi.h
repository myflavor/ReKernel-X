#ifndef RKX_UAPI_H
#define RKX_UAPI_H

#include <cstdint>


#define RKX_GENL_FAMILY_NAME       "rekernel_x2"
#define RKX_GENL_VERSION           1
#define RKX_GENL_MCGRP_NAME        "events"

#define RKX_RPC_NAME_LEN           140

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

#endif // RKX_UAPI_H