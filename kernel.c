#include <linux/if_ether.h>
#include <linux/if_packet.h>
#include <linux/if_vlan.h>
#include <linux/in.h>
#include <linux/ip.h>
#include <linux/ipv6.h>
#include <linux/tcp.h>
#include <linux/udp.h>
#include <uapi/linux/bpf.h>

#define BLOCKLIST_SIZE (1 << 20)

BPF_HASH(blocklist, u64, u64, BLOCKLIST_SIZE);

static int
process_endpoint(uint32_t dst_addr, uint16_t dst_port) {
    u64 key = ((u64)dst_port << 32) | dst_addr;
    u64 *value;

    value = blocklist.lookup(&key);
    if (value == NULL) {
        return XDP_PASS;
    }
    __sync_fetch_and_add(value, 1);
    return XDP_DROP;
}

static int
process_tcp(uint32_t dst_addr, const void *data, const void *data_end) {
    const struct tcphdr *tcp = data;

    if (data + sizeof(*tcp) > data_end) {
        return XDP_PASS;
    }
    return process_endpoint(dst_addr, tcp->dest);
}

static int
process_udp(uint32_t dst_addr, const void *data, const void *data_end) {
    const struct udphdr *udp = data;

    if (data + sizeof(*udp) > data_end) {
        return XDP_PASS;
    }
    return process_endpoint(dst_addr, udp->dest);
}

static int
process_ip(const void *data, const void *data_end) {
    const struct iphdr *ip = data;
    const void *next_header;

    if (data + sizeof(*ip) > data_end) {
        return XDP_PASS;
    }
    next_header = data + ip->ihl * 4;
    switch (ip->protocol) {
    case IPPROTO_TCP:
        return process_tcp(ip->daddr, next_header, data_end);
    case IPPROTO_UDP:
        return process_udp(ip->daddr, next_header, data_end);
    }
    return XDP_PASS;
}

static int
process_ether(const void *data, const void *data_end) {
    const struct ethhdr *ether = data;
    uint16_t type;
    size_t next_header_offset = sizeof(*ether);
    const struct vlan_hdr *vlan;

    if (data + next_header_offset > data_end) {
        return XDP_PASS;
    }
    type = ether->h_proto;

    /*
     * Optional: VLAN and QinQ support.
     *
     * [ET=QinQ] [VLAN][ET=VLAN] [VLAN][EtherType]:
     *     consume [ET=QinQ][VLAN] and continue to the next case.
     * [ET=VLAN] [VLAN][EtherType]:
     *     consume [VLAN][EtherType], read L3 protocol from EtherType.
     */
    switch (type) {
    case htons(ETH_P_8021AD):
        vlan = data + next_header_offset;
        next_header_offset += sizeof(*vlan);
        /* fallthrough */
    case htons(ETH_P_8021Q):
        vlan = data + next_header_offset;
        next_header_offset += sizeof(*vlan);
        if (data + next_header_offset > data_end) {
            return XDP_PASS;
        }
        type = vlan->h_vlan_encapsulated_proto;
        break;
    }

    switch (type) {
    case htons(ETH_P_IP):
        return process_ip(data + next_header_offset, data_end);
    }
    return XDP_PASS;
}

int
xdp_main(struct xdp_md *ctx) {
    const void *data = (const void *)(uintptr_t)ctx->data;
    const void *data_end = (const void *)(uintptr_t)ctx->data_end;

    return process_ether(data, data_end);
}
