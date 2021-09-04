#include <bpf/bpf.h>
#include <bpf/libbpf.h>
#include <err.h>
#include <errno.h>
#include <linux/if_link.h>
#include <net/if.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/sysinfo.h>
/* Exit return codes */
#define EXIT_OK 0
#define EXIT_FAIL 1
#define EXIT_FAIL_OPTION 2
#define EXIT_FAIL_XDP 3
#define EXIT_FAIL_BPF 4
#define EXIT_FAIL_MEM 5

#define AMF_CPUS 4
#define NUM_MAP 3

static int ifindex = -1;
static char *ifname;
static char ifname_buf[IFNAMSIZ];
static int prog_id;
static int xdp_flags = XDP_FLAGS_UPDATE_IF_NOEXIST | XDP_FLAGS_DRV_MODE;
static int n_cpus;
static int map_fds[NUM_MAP];

enum map_type {
	CPU_MAP,
	CPUS_COUNT,
	CPUS_ITERATOR,
};

static const char *const map_type_strings[] = {
	[CPU_MAP] = "cpu_map",
	[CPUS_COUNT] = "cpus_count",
	[CPUS_ITERATOR] = "cpus_iterator",
};

static int init_map_fds(struct bpf_object *obj) {
	enum map_type type;

	for (type = 0; type < NUM_MAP; type++) {
		map_fds[type] =
			bpf_object__find_map_fd_by_name(obj,
											map_type_strings[type]);

		if (map_fds[type] < 0)
			return -ENOENT;
	}
	return 0;
}

static int create_cpu_entry(int cpu, struct bpf_cpumap_val *value) {
	int curr_cpus_count = 0;
	int key = 0;
	int ret;

	/* Add a CPU entry to cpumap, as this allocate a cpu entry in
	 * the kernel for the cpu.
	 */
	ret = bpf_map_update_elem(map_fds[CPU_MAP], &cpu, value, 0);
	if (ret) {
		fprintf(stderr, "Create CPU entry failed (err:%d)\n", ret);
		exit(EXIT_FAIL_BPF);
	}

	ret = bpf_map_lookup_elem(map_fds[CPUS_COUNT], &key, &curr_cpus_count);
	if (ret) {
		fprintf(stderr, "Failed reading curr cpus_count\n");
		exit(EXIT_FAIL_BPF);
	}
	curr_cpus_count++;
	ret = bpf_map_update_elem(map_fds[CPUS_COUNT], &key,
							  &curr_cpus_count, 0);
	if (ret) {
		fprintf(stderr, "Failed write curr cpus_count\n");
		exit(EXIT_FAIL_BPF);
	}
	/* map_fd[7] = cpus_iterator */
	printf("CPU:%u as idx:%u qsize:%d prog_fd: %d (cpus_count:%u)\n", cpu, cpu,
		   value->qsize, value->bpf_prog.fd, curr_cpus_count);
	return 0;
}

static int load_cpumap_prog(char *file_name, char *prog_name) {
	struct bpf_prog_load_attr prog_load_attr = {
		.file = file_name,
		.prog_type = BPF_PROG_TYPE_XDP,
		.expected_attach_type = BPF_XDP_CPUMAP,
	};
	struct bpf_program *prog;
	struct bpf_object *obj;
	int fd;

	if (bpf_prog_load_xattr(&prog_load_attr, &obj, &fd))
		return -1;

	if (fd < 0) {
		fprintf(stderr, "ERR: bpf_prog_load_xattr: %s\n",
				strerror(errno));
		return fd;
	}

	prog = bpf_object__find_program_by_title(obj, prog_name);
	if (!prog) {
		fprintf(stderr, "bpf_object__find_program_by_title failed\n");
		return EXIT_FAIL;
	}

	return bpf_program__fd(prog);
}

int xdp_link_detach(int ifindex, __u32 xdp_flags, __u32 expected_prog_id) {
	__u32 curr_prog_id;
	int err;

	err = bpf_get_link_xdp_id(ifindex, &curr_prog_id, xdp_flags);
	if (err) {
		fprintf(stderr, "ERR: get link xdp id failed (err=%d): %s\n",
				-err, strerror(-err));
		return EXIT_FAIL_XDP;
	}

	if (!curr_prog_id) {
		printf("INFO: %s() no curr XDP prog on ifindex:%d\n",
			   __func__, ifindex);
		return EXIT_OK;
	}

	if (expected_prog_id && curr_prog_id != expected_prog_id) {
		fprintf(stderr,
				"ERR: %s() "
				"expected prog ID(%d) no match(%d), not removing\n",
				__func__, expected_prog_id, curr_prog_id);
		return EXIT_FAIL;
	}

	if ((err = bpf_set_link_xdp_fd(ifindex, -1, xdp_flags)) < 0) {
		fprintf(stderr, "ERR: %s() link set xdp failed (err=%d): %s\n",
				__func__, err, strerror(-err));
		return EXIT_FAIL_XDP;
	}
	printf("INFO: %s() removed XDP prog ID:%d on ifindex:%d\n",
		   __func__, curr_prog_id, ifindex);

	return EXIT_OK;
}

static void int_exit(int sig) {
	unsigned int curr_prog_id = 0;

	if (ifindex > -1) {
		if (bpf_get_link_xdp_id(ifindex, &curr_prog_id, xdp_flags)) {
			printf("bpf_get_link_xdp_id failed\n");
			exit(EXIT_FAILURE);
		}
		if (prog_id == curr_prog_id) {
			fprintf(stderr,
					"Interrupted: Removing XDP program on ifindex:%d device:%s\n",
					ifindex, ifname);
			bpf_set_link_xdp_fd(ifindex, -1, xdp_flags);
		} else if (!curr_prog_id) {
			printf("couldn't find a prog id on a given iface\n");
		} else {
			printf("program on interface changed, not removing\n");
		}
	}
	xdp_link_detach(ifindex, xdp_flags, 0);
	exit(EXIT_OK);
}

// static void list_avail_progs(struct bpf_object *obj) {
// 	struct bpf_program *pos;

// 	printf("BPF object (%s) listing avail --progsec names\n",
// 		   bpf_object__name(obj));

// bpf_object__for_each_program(pos, obj) {
// 	if (bpf_program__is_xdp(pos))
// 		printf(" * %s\n", bpf_program__title(pos, false));
// }
// }

int main(int argc, char const *argv[]) {
	int err = 0, prog_fd;
	n_cpus = get_nprocs_conf();

	char *prog_name = "xdp_parse_and_redirect";
	char *mprog_filename = "xdp_prog_kern.o";
	char *prog_cpumap_val = "xdp_redirect_dummy";

	struct bpf_prog_load_attr prog_load_attr = {
		.prog_type = BPF_PROG_TYPE_XDP,
		.file = mprog_filename,
	};

	struct bpf_prog_info info = {};
	struct bpf_program *prog;
	struct bpf_object *obj;

	unsigned int info_len = sizeof(info);

	if (bpf_prog_load_xattr(&prog_load_attr, &obj, &prog_fd))
		return err;

	if (prog_fd < 0) {
		fprintf(stderr, "ERR: bpf_prog_load_xattr: %s\n",
				strerror(errno));
		return err;
	}
	if (init_map_fds(obj) < 0) {
		fprintf(stderr, "bpf_object__find_map_fd_by_name failed\n");
		return err;
	}
	// list_avail_progs(obj);

	signal(SIGINT, int_exit);
	signal(SIGTERM, int_exit);

	ifname = (char *)&ifname_buf;
	strncpy(ifname, "enp7s0f1", IFNAMSIZ);
	ifindex = if_nametoindex(ifname);
	if (ifindex == 0) {
		fprintf(stderr,
				"ERR: --dev name unknown err(%d):%s\n",
				errno, strerror(errno));
		exit(EXIT_FAILURE);
	}
	int qsize = 128;
	struct bpf_cpumap_val value;
	value.bpf_prog.fd = 0;
	value.bpf_prog.fd = load_cpumap_prog(mprog_filename, prog_cpumap_val);
	if (value.bpf_prog.fd < 0) {
		err = value.bpf_prog.fd;
		fprintf(stderr, "ERR: attaching %s to a remote CPU failed", prog_cpumap_val);
		goto out;
	}
	value.qsize = qsize;

	for (int i = 0; i < AMF_CPUS; i++)
		create_cpu_entry(i, &value);

	prog = bpf_object__find_program_by_title(obj, prog_name);
	if (!prog) {
		fprintf(stderr, "bpf_object__find_program_by_title failed\n");
		goto out;
	}

	prog_fd = bpf_program__fd(prog);
	if (prog_fd < 0) {
		fprintf(stderr, "bpf_program__fd failed\n");
		goto out;
	}
	xdp_link_detach(ifindex, xdp_flags, 0);

	if (bpf_set_link_xdp_fd(ifindex, prog_fd, xdp_flags) < 0) {
		fprintf(stderr, "link set xdp fd failed\n");
		// list_avail_progs(obj);
		err = EXIT_FAIL_XDP;
		goto out;
	}

	err = bpf_obj_get_info_by_fd(prog_fd, &info, &info_len);
	if (err) {
		printf("can't get prog info - %s\n", strerror(errno));
		goto out;
	}
	prog_id = info.id;
out:
	return err;
}
