#include <bpf/bpf_endian.h>
#include <bpf/bpf_helpers.h>
#include <linux/bpf.h>
#include <linux/if_ether.h>
#include <linux/if_packet.h>
#include <linux/in.h>
#include <linux/ip.h>
#include <linux/pkt_cls.h>
#include <linux/tcp.h>
#include <stdbool.h>
#include <stdint.h>

#define FIELD_ID_AMF_UE_NGAP_ID 10
#define FIELD_ID_5G_TMSI 26

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

enum ngap_msg_type {
	NGAP_INITIAL_UE_REQUEST,
	NGAP_SERVICE_REQUEST
};

static __always_inline __u32 parse_ethhdr(struct hdr_cursor *nh, void *data_end) {
	struct ethhdr *eth = nh->pos;
	int hdrsize = sizeof(*eth);

	if (nh->pos + hdrsize > data_end)
		return -1;

	nh->pos += hdrsize;
	return eth->h_proto;
}

static __always_inline __u32 parse_iphdr(struct hdr_cursor *nh, void *data_end) {
	struct iphdr *iph = nh->pos;
	int hdrsize;

	if (iph + 1 > data_end)
		return -1;

	hdrsize = iph->ihl * 4;
	if (hdrsize < sizeof(*iph))
		return -1;

	if (nh->pos + hdrsize > data_end)
		return -1;

	nh->pos += hdrsize;
	return iph->protocol;
}

static __always_inline __u32 parse_tcp_hdr(struct hdr_cursor *nh, void *data_end, bool flag) {
	struct tcphdr *tcph = nh->pos;
	int hdrsize = sizeof(*tcph);
	if (tcph + 1 > data_end)
		return -1;
	nh->pos += hdrsize;
	if (flag) {
		char fmt[] = "TC: ingress: Dest Port Number: %d, %d, %d";
		bpf_trace_printk(fmt, sizeof(fmt), bpf_htons(tcph->dest));
	} else {
		tcph->source = bpf_htons(8000);
	}
	return 0;
}

SEC("ingress")
int tc_ingress(struct __sk_buff *skb) {
	struct hdr_cursor nh;
	void *data_end = (void *)(long)skb->data_end;
	nh.pos = (void *)(long)skb->data;

	__u32 nh_type = parse_ethhdr(&nh, data_end);

	if (bpf_ntohs(nh_type) != ETH_P_IP) {
		return TC_ACT_OK;
	}
	__u32 ip_type = parse_iphdr(&nh, data_end);
	if (ip_type == IPPROTO_TCP) {
		parse_tcp_hdr(&nh, data_end, true);
	}
	return TC_ACT_OK;
}

char __license[] SEC("license") = "GPL";
