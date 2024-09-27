
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <net/if.h>
#include <ctype.h>
#include <linux/tcp.h>
#include <linux/udp.h>
#include <linux/ip.h>
#include <linux/ipv6.h>
#include <arpa/inet.h>


#include "redirect.skel.h"
#include "redirect.h"
#include "types.h"
#include "log.h"
#include "default_setting.h"
#define LO_IFINDEX 1

static volatile sig_atomic_t exiting = 0;
int print_level = PRINT_DEBUG;


static void sig_int(int signo)
{
	exiting = 1;
}

static void sig_handler(int sig)
{
	exiting = true;
}


static int libbpf_print_fn(enum libbpf_print_level level, const char *format, va_list args)
{
	return vfprintf(stderr, format, args);
}

static void help()
{
    char msg[] = "Forwarder, transmit any packets to any where as you want.\n" \
    "./forwarder -p [port] -d\n" \
    "  -p port, service port\n" \
	"  -t tap, tap net device name\n" \
    "  -d , enable printing debug message \n";
    printf("%s", msg);
}

static int tapindex = 0;
static int listen_port = DEFAULT_LISTEN_PORT;

static int parse_argv(int argc, char **argv)
{
	int c;
	int ret = 0;

	opterr = 0;

	while ((c = getopt (argc, argv, "dp:t:")) != -1) {
		switch (c){
			case 'd':
				print_level = PRINT_DEBUG;
			break;
			case 'p':
				listen_port = atoi(optarg);
				if( (listen_port<1) || (listen_port>=65535))  {
					ret = 1;
					printf("Wrong port: -%c %s", c, optarg);
				}
			break;
			case 't':
				tapindex = if_nametoindex(optarg);
				if( tapindex == 0) {
					perror("Fail to get index of tap");
					ret = 1;
				}
			break;
			default:
			ret = 1;
		}
	}
	if( ret ) {
		help();
		return 1;
	}
	return 0;
}

static int handle_event(void *ctx, void *data, size_t data_sz)
{
	const struct event *e = data;
	printf("handle event\n");
	print_debug("%x:%d->%x:%d(remote_seq %u local_seq %u)", 
        ntohl(e->conn.key.remote_ip), ntohs(e->conn.key.remote_port), 
		ntohl(e->conn.key.local_ip), ntohs(e->conn.key.local_port), 
		ntohl(e->conn.remote_seq), ntohl(e->conn.local_seq) );
    
	return 0;
}

int main(int argc, char **argv)
{

	bool hook_created = false;
	struct redirect_bpf *skel;
	struct ring_buffer *rb = NULL;
	int err;

	if( parse_argv(argc, argv)) {
		return -1;
	}
	if( tapindex == 0 ) {
		//todo
		tapindex = if_nametoindex("wlp2s0");
		if( tapindex == 0) {
			perror("Fail to get index of default tap: " DEFATULT_TAP_NAME);
			return -1;
		}
	}
	libbpf_set_print(libbpf_print_fn);


	DECLARE_LIBBPF_OPTS(bpf_tc_hook, tc_hook, .ifindex = tapindex,
			    .attach_point = BPF_TC_INGRESS);
	DECLARE_LIBBPF_OPTS(bpf_tc_opts, tc_opts, .handle = 1, .priority = 1);
	signal(SIGINT, sig_handler);
	signal(SIGTERM, sig_handler);
	skel = redirect_bpf__open_and_load();
	if (!skel) {
		fprintf(stderr, "Failed to open BPF skeleton\n");
		return 1;
	}

	/* The hook (i.e. qdisc) may already exists because:
	 *   1. it is created by other processes or users
	 *   2. or since we are attaching to the TC ingress ONLY,
	 *      bpf_tc_hook_destroy does NOT really remove the qdisc,
	 *      there may be an egress filter on the qdisc
	 */
	err = bpf_tc_hook_create(&tc_hook);
	if (!err){
		hook_created = true;
	}
	else{
		if(err != -EEXIST ){
			fprintf(stderr, "Failed to create TC hook: %d\n", err);
			goto cleanup;
		}
	}

	tc_opts.prog_fd = bpf_program__fd(skel->progs.tc_ingress);
	err = bpf_tc_attach(&tc_hook, &tc_opts);
	if (err) {
		fprintf(stderr, "Failed to attach TC: %d\n", err);
		goto cleanup;
	}

	if (signal(SIGINT, sig_int) == SIG_ERR) {
		err = errno;
		fprintf(stderr, "Can't set signal handler: %s\n", strerror(errno));
		goto cleanup;
	}

	rb = ring_buffer__new(bpf_map__fd(skel->maps.log_ringbuff), handle_event, NULL, NULL);
	if (!rb) {
		err = -1;
		fprintf(stderr, "Failed to create ring buffer\n");
		goto cleanup;
	}
	while (!exiting) {
		err = ring_buffer__poll(rb, 100 /* timeout, ms */);
		if (err == -EINTR) {
			err = 0;
			break;
		}
		if (err < 0) {
			printf("Error polling perf buffer: %d\n", err);
			break;
		}
	}

	tc_opts.flags = tc_opts.prog_fd = tc_opts.prog_id = 0;
	err = bpf_tc_detach(&tc_hook, &tc_opts);
	if (err) {
		fprintf(stderr, "Failed to detach TC: %d\n", err);
		goto cleanup;
	}

cleanup:
	ring_buffer__free(rb);
	if (hook_created)
		bpf_tc_hook_destroy(&tc_hook);
	redirect_bpf__destroy(skel);
	return -err;
}
