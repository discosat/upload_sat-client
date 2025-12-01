/**
 * Copied and edited from: https://github.com/spaceinventor/libcsp/blob/60e4804ea8451e6202ce2c5c5abc0342ad3b55a4/examples/csp_client.c
 */

#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <getopt.h>
#include <time.h>
#include <pthread.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <csp/csp_debug.h>
#include <csp/csp.h>
#include <csp/drivers/usart.h>
#include <csp/drivers/can_socketcan.h>
#include <csp/interfaces/csp_if_zmqhub.h>
#include <slash/optparse.h>
#include <slash/dflopt.h>
#include <param/param_server.h>
#include <vmem/vmem_file.h>

#include "vmem_storage.h"
#include "vmem_dtp_server.h"
#include "dtp/dtp.h"
#include "dtp/dtp_log.h"
#include "dtp/dtp_session.h"
#include "protobuf/uploadmetadata.pb-c.h"
#include "session/segments_utils.h"
#include "include/session/session_hooks.h"
#include "client_logs.h"
#include "param/client_logs_param.h"

/* Server port, the port the server listens on for incoming connections from the client. */
#define SERVERPORT 10

dtp_opt_session_hooks_cfg default_session_hooks;
extern dtp_opt_session_hooks_cfg apm_session_hooks;

/* This function must be provided in arch specific way */
int router_start(void);

// file to be sent
const char *file_src = NULL;

void *router_task(void *param)
{
	while (1)
	{
		csp_route_work();
	}
	return NULL;
}

// IMPLEMENTATION of router_start
int router_start(void)
{
	pthread_t router_thread;
	if (pthread_create(&router_thread, NULL, router_task, NULL) != 0)
	{
		csp_print("Failed to start router thread\n");
		return -1;
	}
	csp_print("Router thread started\n");
	return 0;
}

// Struct to pass arguments to the DTP client thread
typedef struct
{
	uint32_t server_addr;
	char *file_src_name[256];
	char *file_dst_name[256];

	int color;
	int resume;
	uint32_t server;
	unsigned int throughput;
	unsigned int timeout;
	unsigned int payload_id;
	unsigned int mtu;
} dtp_thread_args_t;

static void *dtp_client_worker(void *param)
{
	dtp_thread_args_t *opts = (dtp_thread_args_t *)param;
	dtp_t *session;
	int *slash_res = NULL;

	int status = 0;

	status = set_dest_addr(opts->file_dst_name);

	if(status == 0)
	{
		set_log_param(UPLOAD_SUCCESS);
	}

	csp_print("Starting DTP client for payload %u from server %u\n", opts->payload_id, opts->server);

	printf("\t%s - [DEBUG] Following values from 'opts' to be sent:%s\n", "\x1B[33m", "\x1B[0m");
	printf("\t\t%s * server: %u %s\n", "\x1B[33m", opts->server, "\x1B[0m");
	printf("\t\t%s * throughput: %u %s\n", "\x1B[33m", opts->throughput, "\x1B[0m");
	printf("\t\t%s * payload_id: %u %s\n", "\x1B[33m", opts->payload_id, "\x1B[0m");
	printf("\t\t%s * mtu: %u %s\n", "\x1B[33m", opts->mtu, "\x1B[0m");
	printf("\t\t%s * resume: %u %s\n", "\x1B[33m", opts->resume, "\x1B[0m");
	printf("\t\t%s * session: %p %s\n", "\x1B[33m", session, "\x1B[0m");

	// Run the DTP client. This will block until the transfer is complete or fails.
	dtp_result result = dtp_client_main(opts->server, opts->throughput, opts->timeout, opts->payload_id, opts->mtu, opts->resume, &session);

	if (DTP_ERR == result)
	{
		switch (dtp_errno(NULL))
		{
		case DTP_EINVAL:
			*slash_res = SLASH_EINVAL;
		default:
			printf("%s\n", dtp_strerror(dtp_errno(NULL)));
			*slash_res = SLASH_SUCCESS;
		}
	}
	else
	{
		dtp_serialize_session(session, NULL);
		dtp_release_session(session);
	}

	printf("\t%s - [DEBUG] dtp_client_worker finished!%s\n", "\x1B[33m", "\x1B[0m");

	// Free the thread arguments
	free(opts);

	pthread_exit(NULL);
}

/* Commandline options */
static uint16_t server_address = 0;
static uint16_t client_address = 0;

/* Test mode, check that server & client can exchange packets */
static bool test_mode = false;
static unsigned int run_duration_in_sec = 3;

enum DeviceType
{
	DEVICE_UNKNOWN,
	DEVICE_CAN,
	DEVICE_KISS,
	DEVICE_ZMQ,
};

#define __maybe_unused __attribute__((__unused__))

static struct option long_options[] = {
	{"kiss-device", required_argument, 0, 'k'},
#if (CSP_HAVE_LIBSOCKETCAN)
#define OPTION_c "c:"
	{"can-device", required_argument, 0, 'c'},
#else
#define OPTION_c
#endif
#if (CSP_HAVE_LIBZMQ)
#define OPTION_z "z:"
	{"zmq-device", required_argument, 0, 'z'},
#else
#define OPTION_z
#endif
#if (CSP_USE_RTABLE)
#define OPTION_R "R:"
	{"rtable", required_argument, 0, 'R'},
#else
#define OPTION_R
#endif
	{"interface-address", required_argument, 0, 'a'},
	{"connect-to", required_argument, 0, 'C'},
	{"test-mode", no_argument, 0, 't'},
	{"test-mode-with-sec", required_argument, 0, 'T'},
	{"help", no_argument, 0, 'h'},
	{0, 0, 0, 0}};

void print_help()
{
	csp_print("Usage: upload_sat-client [options]\n");
	if (CSP_HAVE_LIBSOCKETCAN)
	{
		csp_print(" -c <can-device>  set CAN device\n");
	}
	if (1)
	{
		csp_print(" -k <kiss-device> set KISS device\n");
	}
	if (CSP_HAVE_LIBZMQ)
	{
		csp_print(" -z <zmq-device>  set ZeroMQ device\n");
	}
	if (CSP_USE_RTABLE)
	{
		csp_print(" -R <rtable>      set routing table\n");
	}
	if (1)
	{
		csp_print(" -a <address>     set interface address\n"
				  " -s <address>     connect to server at address\n"
				  " -t               enable test mode\n"
				  " -T <duration>    enable test mode with running time in seconds\n"
				  " -h               print help\n");
	}
}

csp_iface_t *add_interface(enum DeviceType device_type, const char *device_name)
{
	csp_iface_t *default_iface = NULL;

	if (device_type == DEVICE_KISS)
	{
		csp_usart_conf_t conf = {
			.device = device_name,
			.baudrate = 115200, /* supported on all platforms */
			.databits = 8,
			.stopbits = 1,
			.paritysetting = 0,
		};
		int error = csp_usart_open_and_add_kiss_interface(&conf, CSP_IF_KISS_DEFAULT_NAME, client_address, &default_iface);
		if (error != CSP_ERR_NONE)
		{
			csp_print("failed to add KISS interface [%s], error: %d\n", device_name, error);
			exit(1);
		}
		default_iface->is_default = 1;
	}

	if (CSP_HAVE_LIBSOCKETCAN && (device_type == DEVICE_CAN))
	{
		int error = csp_can_socketcan_open_and_add_interface(device_name, CSP_IF_CAN_DEFAULT_NAME, client_address, 1000000, true, &default_iface);
		if (error != CSP_ERR_NONE)
		{
			csp_print("failed to add CAN interface [%s], error: %d\n", device_name, error);
			exit(1);
		}
		default_iface->is_default = 1;
	}

	if (CSP_HAVE_LIBZMQ && (device_type == DEVICE_ZMQ))
	{
		int error = csp_zmqhub_init(client_address, device_name, 0, &default_iface);
		if (error != CSP_ERR_NONE)
		{
			csp_print("failed to add ZMQ interface [%s], error: %d\n", device_name, error);
			exit(1);
		}
		default_iface->is_default = 1;
	}

	return default_iface;
}

/* main - initialization of CSP and start of client task */
int main(int argc, char *argv[])
{

	const char *device_name = NULL;
	enum DeviceType device_type = DEVICE_UNKNOWN;
	const char *rtable __maybe_unused = NULL;
	csp_iface_t *default_iface;
	int ret = EXIT_SUCCESS;
	int opt;

	vmem_file_init(&vmem_storage);

	while ((opt = getopt_long(argc, argv, OPTION_c OPTION_z OPTION_R "k:a:s:f:tT:h", long_options, NULL)) != -1)
	{
		switch (opt)
		{
		case 'c':
			device_name = optarg;
			device_type = DEVICE_CAN;
			break;
		case 'k':
			device_name = optarg;
			device_type = DEVICE_KISS;
			break;
		case 'z':
			device_name = optarg;
			device_type = DEVICE_ZMQ;
			break;
		case 'f':
			file_src = optarg;
			break;
#if (CSP_USE_RTABLE)
		case 'R':
			rtable = optarg;
			break;
#endif
		case 'a':
			client_address = atoi(optarg);
			break;
		case 's':
			server_address = atoi(optarg);
			break;
		case 't':
			test_mode = true;
			break;
		case 'T':
			test_mode = true;
			run_duration_in_sec = atoi(optarg);
			break;
		case 'h':
			print_help();
			exit(EXIT_SUCCESS);
		case '?':
			// Invalid option or missing argument
			print_help();
			exit(EXIT_FAILURE);
		}
	}

	// Unless one of the interfaces are set, print a message and exit
	if (device_type == DEVICE_UNKNOWN)
	{
		csp_print("One and only one of the interfaces can be set.\n");
		print_help();
		exit(EXIT_FAILURE);
	}

	csp_print("Initialising CSP\n");

	/* Init CSP */
	csp_conf.hostname = HOSTNAME;
	csp_init();

	/* Start router */
	router_start();

	printf("\t%s - [INFO] Initializing all parameters %s\n", "\x1B[36m", "\x1B[0m");

    // Reg. specific parameter
    client_logs_param_init();

    printf("\t%s - [INFO] Initializing VMEM subsystem %s\n", "\x1B[36m", "\x1B[0m");
    vmem_file_init(&vmem_storage);

	/* Add interface(s) */
	default_iface = add_interface(device_type, device_name);

	/* Setup routing table */
	if (CSP_USE_RTABLE)
	{
		if (rtable)
		{
			int error = csp_rtable_load(rtable);
			if (error < 1)
			{
				csp_print("csp_rtable_load(%s) failed, error: %d\n", rtable, error);
				exit(1);
			}
		}
		else if (default_iface)
		{
			csp_rtable_set(0, 0, default_iface, CSP_NO_VIA_ADDRESS);
		}
	}

	csp_print("Connection table\r\n");
	csp_conn_print_table();

	csp_print("Interfaces\r\n");
	csp_iflist_print();

	if (CSP_USE_RTABLE)
	{
		csp_print("Route table\r\n");
		csp_rtable_print();
	}

	/* Start client work */
	csp_print("Client started\n");

	csp_socket_t sock = {0};
	csp_bind(&sock, CSP_ANY);
	csp_listen(&sock, 10);

	/* This loop now runs forever, as intended */
	while (1)
	{
		csp_conn_t *conn;
		if ((conn = csp_accept(&sock, CSP_MAX_TIMEOUT)) == NULL)
		{
			/* Timed out, continue listening */
			continue;
		}

		csp_packet_t *packet;
		while ((packet = csp_read(conn, 100)) != NULL)
		{
			int dport = csp_conn_dport(conn);

			switch (dport)
			{
			case 20:
				param_serve(packet);
				break;
			case SERVERPORT:
				printf("\t%s - [DEBUG] Received DTP trigger request on port %d. %s\n", "\x1B[33m", dport, "\x1B[0m");

				UploadMetadataItem *metadata;
				metadata = upload_metadata_item__unpack(NULL, packet->length, packet->data);

				if (metadata == NULL)
				{
					printf("\t%s - [ERROR] Failed to unpack Protobuf metadata message! %s\n", "\x1B[31m", "\x1B[0m");
					set_log_param(ERR_PROTOBUF_UNPACK_FAILURE);
					csp_buffer_free(packet);
					continue;
				}
				else {
					printf("\t%s - [DEBUG] Received metadata NOT null. %s\n", "\x1B[33m", "\x1B[0m");
				}

				/* A. Allocate memory for the thread arguments */
				dtp_thread_args_t *opts = malloc(sizeof(dtp_thread_args_t));
				if (!opts)
				{
					printf("\t%s - [ERROR] Failed to allocate memory for DTP options! %s\n", "\x1B[31m", "\x1B[0m");
					set_log_param(ERR_DTP_OPT_MEM_ALL);
					upload_metadata_item__free_unpacked(metadata, NULL); // Free the unpacked message
					csp_buffer_free(packet);
					continue;
				}

				opts->server = metadata->dtp_server_address;
				opts->payload_id = metadata->payload_id;

				strncpy(opts->file_src_name, metadata->file_src, sizeof(opts->file_src_name) - 1);
				opts->file_dst_name[sizeof(opts->file_dst_name) - 1] = '\0';

				strncpy(opts->file_dst_name, metadata->file_dest, sizeof(opts->file_dst_name) - 1);
				opts->file_dst_name[sizeof(opts->file_dst_name) - 1] = '\0';

				// You will need to set the other opts fields here too!
				opts->timeout = 10000;
				opts->mtu = 256;
				opts->resume = 0;
				opts->throughput = 1024;

				upload_metadata_item__free_unpacked(metadata, NULL);

				/* This is very important, else the default no-op hooks will be used */
				default_session_hooks = apm_session_hooks;

				/* C. Start the DTP client worker in a new thread */
				pthread_t dtp_thread;
				if (pthread_create(&dtp_thread, NULL, dtp_client_worker, opts) != 0)
				{
					printf("\t%s - [ERROR] Failed to create DTP worker thread! %s\n", "\x1B[31m", "\x1B[0m");
					set_log_param(ERR_DTP_THREAD_CREATION);
					free(opts); // Don't forget to free if thread creation fails
				}
				else
				{
					pthread_detach(dtp_thread); // Allow thread to clean up itself
				}

				csp_buffer_free(packet);
				break;

			default:
				/* For pings and other management traffic, use the service handler */
				printf("\t%s - [DEBUG] Request on service port %d, passing to handler. %s\n", "\x1B[33m", dport, "\x1B[0m");
				csp_service_handler(packet);
				break;
			}
		}

		/* Close the connection when done */
		csp_close(conn);
	}

	return ret;
}