#ifndef RKX_NLA_H
#define RKX_NLA_H

#include <cstring>
#include <cstdint>
#include <linux/netlink.h>
#include <linux/genetlink.h>

/* ==========================================================================
 * Read / iterate — verbatim from Linux 6.12 include/net/netlink.h
 * (SPDX-License-Identifier: GPL-2.0)
 * ========================================================================== */

static inline int nla_type(const struct nlattr *nla) {
    return nla->nla_type & NLA_TYPE_MASK;
}

static inline void *nla_data(const struct nlattr *nla) {
    return (char *) nla + NLA_HDRLEN;
}

static inline uint16_t nla_len(const struct nlattr *nla) {
    return nla->nla_len - NLA_HDRLEN;
}

static inline int nla_ok(const struct nlattr *nla, int remaining) {
    return remaining >= (int) sizeof(*nla) &&
           nla->nla_len >= sizeof(*nla) &&
           nla->nla_len <= remaining;
}

static inline struct nlattr *nla_next(const struct nlattr *nla, int *remaining) {
    unsigned int totlen = NLA_ALIGN(nla->nla_len);
    *remaining -= totlen;
    return (struct nlattr *) ((char *) nla + totlen);
}

#define nla_for_each_attr(pos, head, len, rem) \
    for (pos = (struct nlattr *)(head), rem = (len); \
         nla_ok(pos, rem); \
         pos = nla_next(pos, &(rem)))

#define nla_for_each_nested(pos, nla, rem) \
    nla_for_each_attr(pos, nla_data(nla), nla_len(nla), rem)

static inline uint8_t nla_get_u8(const struct nlattr *nla) {
    return *(uint8_t *) nla_data(nla);
}

static inline uint16_t nla_get_u16(const struct nlattr *nla) {
    return *(uint16_t *) nla_data(nla);
}

static inline uint32_t nla_get_u32(const struct nlattr *nla) {
    return *(uint32_t *) nla_data(nla);
}

static inline int32_t nla_get_s32(const struct nlattr *nla) {
    return *(int32_t *) nla_data(nla);
}

/* ==========================================================================
 * Defensive reader — payload-length-guarded, for sites without a pre-check.
 * ========================================================================== */

template<typename T>
static inline bool rkx_nla_get(struct nlattr *nla, T &out) {
    if (!nla || nla_len(nla) < static_cast<int>(sizeof(T))) {
        return false;
    }
    out = *static_cast<T *>(nla_data(nla));
    return true;
}

template<typename T>
static inline T nla_get_val(struct nlattr *nla, T def_val = -1) {
    if (!nla || nla_len(nla) < static_cast<int>(sizeof(T))) {
        return def_val;
    }
    return *static_cast<T *>(nla_data(nla));
}

/* ==========================================================================
 * Table-driven Parsers (Zero-overhead flattening)
 * ========================================================================== */

static inline int rkx_nla_parse(struct nlattr *tb[], int maxtype, struct nlattr *head, int len) {
    struct nlattr *nla;
    int rem;

    std::memset(tb, 0, sizeof(struct nlattr *) * (maxtype + 1));

    nla_for_each_attr(nla, head, len, rem) {
        int type = nla_type(nla);
        if (type > 0 && type <= maxtype) {
            tb[type] = nla;
        }
    }
    return 0;
}

static inline int rkx_nla_parse_nested(struct nlattr *tb[], int maxtype, struct nlattr *nla) {
    return rkx_nla_parse(tb, maxtype, static_cast<struct nlattr *>(nla_data(nla)), nla_len(nla));
}

/* ==========================================================================
 * NlaBuilder — user-space genl message constructor (no struct sk_buff).
 * ========================================================================== */
class NlaBuilder {
    uint8_t *buf_;
    size_t cap_;
    size_t len_;

    bool put(uint16_t type, const void *data, uint16_t datalen) {
        uint16_t attr_len = NLA_HDRLEN + datalen;
        uint16_t aligned = NLA_ALIGN(attr_len);
        if (len_ + aligned > cap_) {
            return false;
        }
        auto *nla = reinterpret_cast<struct nlattr *>(buf_ + len_);
        nla->nla_len = attr_len;
        nla->nla_type = type;
        if (datalen) {
            memcpy(reinterpret_cast<uint8_t *>(nla) + NLA_HDRLEN, data, datalen);
        }
        if (aligned > attr_len) {
            memset(buf_ + len_ + attr_len, 0, aligned - attr_len);
        }
        len_ += aligned;
        return true;
    }

public:
    NlaBuilder(uint8_t *buf, size_t cap) : buf_(buf), cap_(cap), len_(NLMSG_HDRLEN + GENL_HDRLEN) {
        memset(buf_, 0, NLMSG_HDRLEN + GENL_HDRLEN);
    }

    void putGenlHeader(uint16_t family, uint32_t seq, uint8_t cmd, uint8_t version) {
        auto *nlh = reinterpret_cast<struct nlmsghdr *>(buf_);
        nlh->nlmsg_len = 0;
        nlh->nlmsg_type = family;
        nlh->nlmsg_flags = NLM_F_REQUEST;
        nlh->nlmsg_seq = seq;
        nlh->nlmsg_pid = 0;

        auto *ghdr = reinterpret_cast<struct genlmsghdr *>(buf_ + NLMSG_HDRLEN);
        ghdr->cmd = cmd;
        ghdr->version = version;
        ghdr->reserved = 0;
    }

    bool putU32(uint16_t type, uint32_t v) {
        return put(type, &v, 4);
    }

    bool putS32(uint16_t type, int32_t v) {
        return put(type, &v, 4);
    }

    bool putU8(uint16_t type, uint8_t v) {
        return put(type, &v, 1);
    }

    bool putString(uint16_t type, const char *s) {
        return put(type, s, static_cast<uint16_t>(strlen(s) + 1));
    }

    size_t finish() {
        reinterpret_cast<struct nlmsghdr *>(buf_)->nlmsg_len = static_cast<uint32_t>(len_);
        return len_;
    }
};

#endif /* RKX_NLA_H */