/*
 * Copyright (c) 2026 myflavor <admin@myflv.cn>. All rights reserved.
 * Based on Re-Kernel project by nep_timeline@outlook.com.
 * File: rkx_genl.c — Generic Netlink transport.
 */

#include "rkx_log.h"
#include "rkx.h"
#include <linux/printk.h>
#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/version.h>
#include <linux/netlink.h>
#include <net/sock.h>
#include <net/genetlink.h>

static bool rkx_genl_registered = false;

bool rkx_netlink_ready(void)
{
    return rkx_genl_registered;
}

static int rkx_genl_monitor_net(struct sk_buff *skb, struct genl_info *info)
{
    uid_t muid;

    if (!info->attrs[RKX_A_UID])
    {
        return -EINVAL;
    }

    muid = (uid_t)nla_get_u32(info->attrs[RKX_A_UID]);
    rkx_log_debug("addMonitorUid uid=%d\n", muid);
    net_uid_add(muid);
    return 0;
}

static int rkx_genl_del_monitor_net(struct sk_buff *skb, struct genl_info *info)
{
    uid_t muid;

    if (!info->attrs[RKX_A_UID])
    {
        return -EINVAL;
    }

    muid = (uid_t)nla_get_u32(info->attrs[RKX_A_UID]);
    rkx_log_debug("delMonitorNet uid=%d\n", muid);
    net_uid_del(muid);
    return 0;
}

static const struct nla_policy rkx_genl_policy[RKX_A_MAX + 1] = {
    [RKX_A_EVENT] = {.type = NLA_NESTED},
    [RKX_A_UID] = {.type = NLA_U32},
};

static const struct genl_ops rkx_genl_ops[] = {
    {
        .cmd = RKX_C_ADD_MONITOR_NET,
        .doit = rkx_genl_monitor_net,
    },
    {
        .cmd = RKX_C_DEL_MONITOR_NET,
        .doit = rkx_genl_del_monitor_net,
    },
};

static const struct genl_multicast_group rkx_genl_mcgrps[] = {
    {.name = RKX_GENL_MCGRP_NAME},
};

static struct genl_family rkx_genl_family = {
    .name = RKX_GENL_FAMILY_NAME,
    .version = RKX_GENL_VERSION,
    .maxattr = RKX_A_MAX,
    .policy = rkx_genl_policy,
    .module = THIS_MODULE,
    .ops = rkx_genl_ops,
    .n_ops = ARRAY_SIZE(rkx_genl_ops),
    .mcgrps = rkx_genl_mcgrps,
    .n_mcgrps = ARRAY_SIZE(rkx_genl_mcgrps),
};

int sendMessage(struct rkx_event *event)
{
    struct sk_buff *skb;
    void *msg_head;
    struct nlattr *evt, *payload;
    int rc;

    skb = genlmsg_new(nla_total_size(256), GFP_ATOMIC);
    if (!skb)
    {
        rkx_log_err("genlmsg alloc failure!\n");
        return LINE_ERROR;
    }

    msg_head = genlmsg_put(skb, 0, 0, &rkx_genl_family, 0, RKX_C_EVENT);
    if (!msg_head)
    {
        rkx_log_err("genlmsg_put failure!\n");
        nlmsg_free(skb);
        return LINE_ERROR;
    }

    evt = nla_nest_start(skb, RKX_A_EVENT);
    if (!evt)
    {
        goto nla_fail;
    }

    switch (event->type)
    {
    case RKX_EVT_BINDER:
    {
        struct rkx_binder_event *b = &event->u.binder;

        payload = nla_nest_start(skb, RKX_A_BINDER);
        if (!payload)
            goto nla_fail;
        if (nla_put_s32(skb, RKX_A_BINDER_TYPE, b->binder_type) ||
            nla_put_s32(skb, RKX_A_BINDER_ONEWAY, b->oneway) ||
            nla_put_s32(skb, RKX_A_BINDER_FROM_PID, b->from_pid) ||
            nla_put_s32(skb, RKX_A_BINDER_FROM_UID, b->from_uid) ||
            nla_put_s32(skb, RKX_A_BINDER_TARGET_PID, b->target_pid) ||
            nla_put_s32(skb, RKX_A_BINDER_TARGET_UID, b->target_uid) ||
            nla_put_s32(skb, RKX_A_BINDER_CODE, b->code) ||
            nla_put_string(skb, RKX_A_BINDER_RPC_NAME, b->rpc_name))
            goto nla_fail;
        nla_nest_end(skb, payload);
        break;
    }
    case RKX_EVT_SIGNAL:
    {
        struct rkx_signal_event *s = &event->u.signal;

        payload = nla_nest_start(skb, RKX_A_SIGNAL);
        if (!payload)
            goto nla_fail;
        if (nla_put_s32(skb, RKX_A_SIGNAL_SIGNAL, s->signal) ||
            nla_put_s32(skb, RKX_A_SIGNAL_KILLER_PID, s->killer_pid) ||
            nla_put_s32(skb, RKX_A_SIGNAL_KILLER_UID, s->killer_uid) ||
            nla_put_s32(skb, RKX_A_SIGNAL_DST_PID, s->dst_pid) ||
            nla_put_s32(skb, RKX_A_SIGNAL_DST_UID, s->dst_uid))
            goto nla_fail;
        nla_nest_end(skb, payload);
        break;
    }
    case RKX_EVT_NETWORK:
    {
        struct rkx_network_event *n = &event->u.network;

        payload = nla_nest_start(skb, RKX_A_NETWORK);
        if (!payload)
            goto nla_fail;
        if (nla_put_s32(skb, RKX_A_NETWORK_PROTO, n->proto) ||
            nla_put_s32(skb, RKX_A_NETWORK_TARGET_UID, n->target_uid) ||
            nla_put_s32(skb, RKX_A_NETWORK_DATA_LEN, n->data_len))
            goto nla_fail;
        nla_nest_end(skb, payload);
        break;
    }
    default:
        goto nla_fail;
    }

    nla_nest_end(skb, evt);
    genlmsg_end(skb, msg_head);

    rc = genlmsg_multicast(&rkx_genl_family, skb, 0, 0, GFP_ATOMIC);
    if (rc && rc != -ESRCH)
    {
        rkx_log_err("genlmsg_multicast failed, rc=%d\n", rc);
        return LINE_ERROR;
    }

    return LINE_SUCCESS;

nla_fail:
    genlmsg_cancel(skb, msg_head);
    nlmsg_free(skb);
    rkx_log_err("sendMessage: nla_put failed\n");
    return LINE_ERROR;
}

int register_genl(void)
{
    rkx_log_info("Trying to register Generic Netlink family......\n");

    if (genl_register_family(&rkx_genl_family) != 0)
    {
        rkx_log_err("Failed to register genl family!\n");
        return LINE_ERROR;
    }
    rkx_genl_registered = true;

    rkx_log_info("Registered genl family! ID: %d\n", rkx_genl_family.id);
    return LINE_SUCCESS;
}

void unregister_genl(void)
{
    if (rkx_genl_registered)
    {
        genl_unregister_family(&rkx_genl_family);
        rkx_genl_registered = false;
    }
}
