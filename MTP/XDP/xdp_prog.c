#include <bpf/bpf_endian.h>
#include <bpf/bpf_helpers.h>
#include <linux/bpf.h>
#include <linux/if_ether.h>
#include <linux/if_packet.h>
#include <linux/in.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <sys/sysinfo.h>

#define MAX_CPUS 8
#define AMF_CPUS 4

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

struct bpf_cpumap_val1 {
	__u32 qsize; /* queue size to remote target CPU */
	union {
		int fd;	  /* prog fd on map write */
		__u32 id; /* prog id on map read */
	} bpf_prog;
};

struct {
	__uint(type, BPF_MAP_TYPE_CPUMAP);
	__uint(key_size, sizeof(__u32));
	__uint(value_size, sizeof(struct bpf_cpumap_val1));
	__uint(max_entries, MAX_CPUS);
} cpu_map SEC(".maps");

static __always_inline __u32 parse_ethhdr(struct hdr_cursor *nh, void *data_end,
										  struct ethhdr **ethhdr) {
	struct ethhdr *eth = nh->pos;
	int hdrsize = sizeof(*eth);

	if (nh->pos + hdrsize > data_end)
		return -1;

	nh->pos += hdrsize;
	*ethhdr = eth;
	return eth->h_proto;
}

static __always_inline __u32 parse_iphdr(struct hdr_cursor *nh, void *data_end,
										 struct iphdr **iphdr) {
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
	*iphdr = iph;

	return iph->protocol;
}

static __always_inline __u32 parse_tcp_hdr(struct hdr_cursor *nh, void *data_end,
										   struct tcphdr **tcphdr) {
	struct tcphdr *tcph = nh->pos;
	int hdrsize = sizeof(*tcph);
	if (tcph + 1 > data_end)
		return -1;

	*tcphdr = tcph;
	nh->pos += hdrsize;
	return 0;
}

static __always_inline __u32 parse_sctp_hdr(struct hdr_cursor *nh, void *data_end,
											struct sctphdr **sctphdr) {
	struct sctphdr *sctph = nh->pos;
	int hdrsize = sizeof(*sctph);

	if (sctph + 1 > data_end)
		return -1;
	*sctphdr = sctph;
	nh->pos += hdrsize;
	return 0;
}

static __always_inline __u32 parse_ngap_hdr(struct hdr_cursor *nh,
											void *data_end) {
	__u16 *procedure_code = nh->pos;
	if (procedure_code + 1 > data_end)
		return -1;
	return 0b0000000011111111 & *procedure_code;
}

static __always_inline __u32 parse_ngapid(struct hdr_cursor *nh,
										  void *data_end) {
	__u8 *procedure_code = nh->pos;
	if (procedure_code + 9 > data_end)
		return -1;
	return 0 | *(procedure_code + 8) << 8 | *(procedure_code + 9);
}

static __always_inline __u32 schedule_cpu(struct xdp_md *ctx, __u32 key) {
	__u32 *cpu_selected;
	__u32 cpu_dest;

	cpu_selected = bpf_map_lookup_elem(&cpu_map, &key);
	if (!cpu_selected)
		return XDP_ABORTED;
	cpu_dest = *cpu_selected;

	if (cpu_dest >= MAX_CPUS) {
		return XDP_ABORTED;
	}
	return bpf_redirect_map(&cpu_map, cpu_dest, 0);
}

static __always_inline __u32 get_cpu_ngapid(struct xdp_md *ctx, __u32 ngap_id) {
	__u32 bin = ~0;
	bin = bin / AMF_CPUS;
	int i;
#pragma clang loop unroll(full)
	for (i = 0; i < AMF_CPUS; i++) {
		if (ngap_id < i * bin) {
			return i;
		}
	}
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

	/* These keep track of the next header type and iterator pointer */
	struct hdr_cursor nh;
	__u32 nh_type, ip_type;

	/* Start next header cursor position at data start */
	nh.pos = data;

	nh_type = parse_ethhdr(&nh, data_end, &eth);

	if (bpf_ntohs(nh_type) != ETH_P_IP) {
		return XDP_PASS;
	}

	ip_type = parse_iphdr(&nh, data_end, &iphdr);
	__u32 cpu_rr = 0, cpu = 0, ngap_id = 0;
	if (ip_type == IPPROTO_TCP) {
		parse_tcp_hdr(&nh, data_end, &tcph);
	} else if (ip_type == IPPROTO_SCTP) {
		parse_sctp_hdr(&nh, data_end, &sctph);
		int procedure_code = parse_ngap_hdr(&nh, data_end);
		char fmt[] = "Procedure Code: %d\n";
		bpf_trace_printk(fmt, sizeof(fmt), procedure_code);
		if (procedure_code == 15) {
			// InitialUEMessage
			schedule_cpu(ctx, cpu_rr++);
		} else if (procedure_code == 46) {
			// UpLinkNASTTransport
			ngap_id = parse_ngapid(&nh, data_end);
			char fmt2[] = "NGAPID : %d\n";
			bpf_trace_printk(fmt2, sizeof(fmt2), ngap_id);
			cpu = get_cpu_ngapid(ctx, ngap_id);
			schedule_cpu(ctx, cpu);
		} else if (procedure_code == 14) {
			// InitialContextSetupResponse
			ngap_id = parse_ngapid(&nh, data_end);
			char fmt2[] = "NGAPID : %d\n";
			bpf_trace_printk(fmt2, sizeof(fmt2), ngap_id);
			cpu = get_cpu_ngapid(ctx, ngap_id);
			schedule_cpu(ctx, cpu);
		}
	}
	return XDP_PASS;
}

char _license[] SEC("license") = "GPL";
