/* SPDX-License-Identifier: GPL-2.0 */
#include <bpf/bpf_endian.h>
#include <bpf/bpf_helpers.h>
#include <linux/bpf.h>
#include <linux/if_ether.h>
#include <linux/if_packet.h>
#include <linux/in.h>
#include <linux/ip.h>
// #include <linux/sctp.h>
#include <linux/tcp.h>

// #include <stddef.h>
/* Header cursor to keep track of current parsing position */
struct hdr_cursor {
  void *pos;
};

struct sctphdr {
  __be16 source;
  __be16 dest;
  __be32 vtag;
  __le32 checksum;
};

/* Packet parsing helpers.
 *
 * Each helper parses a packet header, including doing bounds checking, and
 * returns the type of its contents if successful, and -1 otherwise.
 *
 * For Ethernet and IP headers, the content type is the type of the payload
 * (h_proto for Ethernet, nexthdr for IPv6), for ICMP it is the ICMP type field.
 * All return values are in host byte order.
 */
static __always_inline int parse_ethhdr(struct hdr_cursor *nh, void *data_end,
                                        struct ethhdr **ethhdr) {
  struct ethhdr *eth = nh->pos;
  int hdrsize = sizeof(*eth);

  /* Byte-count bounds check; check if current pointer + size of header
   * is after data_end.
   */
  if (nh->pos + hdrsize > data_end)
    return -1;

  nh->pos += hdrsize;
  *ethhdr = eth;
  return eth->h_proto; /* network-byte-order */
}

static __always_inline int parse_iphdr(struct hdr_cursor *nh, void *data_end,
                                       struct iphdr **iphdr) {
  struct iphdr *iph = nh->pos;
  int hdrsize;

  if (iph + 1 > data_end)
    return -1;

  hdrsize = iph->ihl * 4;
  /* Sanity check packet field is valid */
  if (hdrsize < sizeof(*iph))
    return -1;

  /* Variable-length IPv4 header, need to use byte-based arithmetic */
  if (nh->pos + hdrsize > data_end)
    return -1;

  nh->pos += hdrsize;
  *iphdr = iph;

  return iph->protocol;
}

static __always_inline int
parse_tcppacket(struct hdr_cursor *nh, void *data_end, struct tcphdr **tcphdr) {
  struct tcphdr *tcph = nh->pos;
  int hdrsize = sizeof(*tcph);
  if (tcph + 1 > data_end)
    return -1;

  *tcphdr = tcph;
  nh->pos += hdrsize;
  return 0;
}

static __always_inline int parse_sctppacket(struct hdr_cursor *nh,
                                            void *data_end,
                                            struct sctphdr **sctphdr) {
  struct sctphdr *sctph = nh->pos;
  int hdrsize = sizeof(*sctph);

  if (sctph + 1 > data_end)
    return -1;
  *sctphdr = sctph;
  nh->pos += hdrsize;
  return 0;
}

SEC("xdp_packet_parser")
int xdp_parser_func(struct xdp_md *ctx) {
  void *data_end = (void *)(long)ctx->data_end;
  void *data = (void *)(long)ctx->data;
  struct ethhdr *eth;
  struct iphdr *iphdr;
  struct tcphdr *tcph;
  struct sctphdr *sctph;

  /* Default action XDP_PASS, imply everything we couldn't parse, or that
   * we don't want to deal with, we just pass up the stack and let the
   * kernel deal with it.
   */
  /* These keep track of the next header type and iterator pointer */
  struct hdr_cursor nh;
  int nh_type, ip_type;

  /* Start next header cursor position at data start */
  nh.pos = data;

  /* Packet parsing in steps: Get each header one at a time, aborting if
   * parsing fails. Each helper function does sanity checking (is the
   * header type in the packet correct?), and bounds checking.
   */
  nh_type = parse_ethhdr(&nh, data_end, &eth);

  if (bpf_ntohs(nh_type) != ETH_P_IP) {
    goto out;
  }

  ip_type = parse_iphdr(&nh, data_end, &iphdr);
  if (ip_type == IPPROTO_TCP) {
    parse_tcppacket(&nh, data_end, &tcph);
  } else if (ip_type == IPPROTO_SCTP) {
    parse_sctppacket(&nh, data_end, &sctph);
  }
out:
  return XDP_PASS;
}

char _license[] SEC("license") = "GPL";
