/******************************************************************************
*******************************************************************************
**
**  Copyright (C) Sistina Software, Inc.  1997-2003  All rights reserved.
**  Copyright (C) 2004 Red Hat, Inc.  All rights reserved.
**
**  This copyrighted material is made available to anyone wishing to use,
**  modify, copy, or redistribute it subject to the terms and conditions
**  of the GNU General Public License v.2.
**
*******************************************************************************
******************************************************************************/

#include <inttypes.h>
#include "copyright.cf"
#include "cnxman-socket.h"
#include "cman_tool.h"

#define OPTION_STRING		("m:n:v:e:2p:c:r:i:XVh?d")
#define OP_JOIN			1
#define OP_LEAVE		2
#define OP_EXPECTED		3
#define OP_VOTES		4
#define OP_KILL			5


static void print_usage(void)
{
	printf("Usage:\n");
	printf("\n");
	printf("%s <join|leave|kill|expected|votes> [options]\n", prog_name);
	printf("\n");
	printf("Options:\n");
	printf("\n");
	printf("  -m <addr>      * Multicast address to use (combines with -i)\n");
	printf("  -i <ifname>    * Interfaces for above multicast addresses\n");
	printf("  -v <votes>       Number of votes this node has (default 1)\n");
	printf("  -e <votes>       Number of expected votes for the cluster (no default)\n");
	printf("  -c <clustername> Name of the cluster to join\n");
	printf("  -2               This is a two node cluster (-e must be 1)\n");
	printf("  -p <port>        UDP port number for cman communications (default %d)\n", DEFAULT_PORT);
	printf("  -n <nodename>  * The name of this node (defaults to unqualified hostname)\n");
	printf("  -X               Do not use CCS\n");
	printf("  -V               Print program version information, then exit\n");
	printf("  -d               Enable debug output\n");
	printf("  -h               Print this help, then exit\n");
	printf("  remove           Used with leave\n");
	printf("  force            Used with leave\n");
	printf("\n");
	printf("  options with marked * can be specified multiple times for multi-path systems\n");
}

static void leave(commandline_t *comline)
{
	int cluster_sock;
	int result;
	int flags = CLUSTER_LEAVEFLAG_DOWN;

	cluster_sock = socket(AF_CLUSTER, SOCK_DGRAM, CLPROTO_MASTER);
	if (cluster_sock == -1)
		die("can't open cluster socket");

	/* "cman_tool leave remove" adjusts quorum downward */

	if (comline->remove)
		flags |= CLUSTER_LEAVEFLAG_REMOVED;

	/* If the join count is != 1 then there are other things using
	   the cluster and we need to be forced */

	if ((result = ioctl(cluster_sock, SIOCCLUSTER_GET_JOINCOUNT, 0)) != 0) {
		if (result < 0)
			die("error getting join count");

		if (!comline->force) {
	    		die("Can't leave cluster while there are %d active subsystems\n", result);
		}
		flags |= CLUSTER_LEAVEFLAG_FORCE;
	}

	if ((result = ioctl(cluster_sock, SIOCCLUSTER_LEAVE_CLUSTER,
			    flags)))
		die("error leaving cluster");

	close(cluster_sock);
}

static void set_expected(commandline_t *comline)
{
	int cluster_sock;
	int result;

	cluster_sock = socket(AF_CLUSTER, SOCK_DGRAM, CLPROTO_MASTER);
	if (cluster_sock == -1)
		die("Can't open cluster socket");

	if ((result = ioctl(cluster_sock, SIOCCLUSTER_SETEXPECTED_VOTES,
			    comline->expected_votes)))
		die("can't set expected votes");

	close(cluster_sock);
}

static void set_votes(commandline_t *comline)
{
	int cluster_sock;
	int result;

	cluster_sock = socket(AF_CLUSTER, SOCK_DGRAM, CLPROTO_MASTER);
	if (cluster_sock == -1)
		die("can't open cluster socket");

	if ((result = ioctl(cluster_sock, SIOCCLUSTER_SET_VOTES,
			    comline->votes)))
		die("can't set votes");

	close(cluster_sock);
}

static void kill_node(commandline_t *comline)
{
	int cluster_sock;
	int result;
	int nodeid;

	if (!comline->num_nodenames) {
	    die("No node ID specified\n");
	}

	nodeid = atoi(comline->nodenames[0]);

	cluster_sock = socket(AF_CLUSTER, SOCK_DGRAM, CLPROTO_MASTER);
	if (cluster_sock == -1)
		die("can't open cluster socket");

	if ((result = ioctl(cluster_sock, SIOCCLUSTER_KILLNODE, nodeid)))
		die("kill node failed");

	close(cluster_sock);
}

static void decode_arguments(int argc, char *argv[], commandline_t *comline)
{
	int cont = TRUE;
	int optchar, i;

	while (cont) {
		optchar = getopt(argc, argv, OPTION_STRING);

		switch (optchar) {

		case 'i':
			i = comline->num_interfaces;
			if (i >= MAX_INTERFACES)
				die("maximum of %d interfaces allowed",
				    MAX_INTERFACES);
			comline->interfaces[i] = strdup(optarg);
			if (!comline->interfaces[i])
				die("no memory");
			comline->num_interfaces++;
			break;

		case 'm':
		        i = comline->num_multicasts;
			if (i >= MAX_INTERFACES)
			        die("maximum of %d multicast addresses allowed",
				    MAX_INTERFACES);
			if (strlen(optarg) > MAX_MCAST_NAME_LEN)
				die("maximum multicast name length is %d",
				    MAX_MCAST_NAME_LEN);
			comline->multicast_names[i] = strdup(optarg);
			comline->num_multicasts++;
			break;

		case 'n':
		        i = comline->num_nodenames;
			if (i >= MAX_INTERFACES)
			        die("maximum of %d node names allowed",
				    MAX_INTERFACES);
			if (strlen(optarg) > MAX_NODE_NAME_LEN)
				die("maximum node name length is %d",
				    MAX_NODE_NAME_LEN);
			comline->nodenames[i] = strdup(optarg);
			comline->num_nodenames++;
			break;

		case 'r':
			comline->config_version = atoi(optarg);
			break;

		case 'v':
			comline->votes = atoi(optarg);
			break;

		case 'e':
			comline->expected_votes = atoi(optarg);
			break;

		case '2':
			comline->two_node = TRUE;
			break;

		case 'p':
			comline->port = atoi(optarg);
			break;

		case 'c':
			if (strlen(optarg) > MAX_NODE_NAME_LEN)
				die("maximum cluster name length is %d",
				    MAX_CLUSTER_NAME_LEN);
			strcpy(comline->clustername, optarg);
			break;

		case 'X':
			comline->no_ccs = TRUE;
			break;

		case 'V':
			printf("cman_tool %s (built %s %s)\n",
				CMAN_RELEASE_NAME, __DATE__, __TIME__);
			printf("%s\n", REDHAT_COPYRIGHT);
			exit(EXIT_SUCCESS);
			break;

		case 'h':
			print_usage();
			exit(EXIT_SUCCESS);
			break;

		case ':':
		case '?':
			fprintf(stderr, "Please use '-h' for usage.\n");
			exit(EXIT_FAILURE);
			break;

		case 'd':
		        comline->verbose++;
			break;

		case EOF:
			cont = FALSE;
			break;

		default:
			die("unknown option: %c", optchar);
			break;
		};
	}

	while (optind < argc) {
		if (strcmp(argv[optind], "join") == 0) {
			if (comline->operation)
				die("can't specify two operations");
			comline->operation = OP_JOIN;
		} else if (strcmp(argv[optind], "leave") == 0) {
			if (comline->operation)
				die("can't specify two operations");
			comline->operation = OP_LEAVE;
		} else if (strcmp(argv[optind], "expected") == 0) {
			if (comline->operation)
				die("can't specify two operations");
			comline->operation = OP_EXPECTED;
		} else if (strcmp(argv[optind], "votes") == 0) {
			if (comline->operation)
				die("can't specify two operations");
			comline->operation = OP_VOTES;
		} else if (strcmp(argv[optind], "kill") == 0) {
			if (comline->operation)
				die("can't specify two operations");
			comline->operation = OP_KILL;
		} else if (strcmp(argv[optind], "remove") == 0) {
			comline->remove = TRUE;
		} else if (strcmp(argv[optind], "force") == 0) {
			comline->force = TRUE;
		} else
			die("unknown option %s", argv[optind]);

		optind++;
	}

	if (!comline->operation)
		die("no operation specified");
}

int uname_to_nodename(char *name)
{
	struct utsname utsname;
	char *dot;
	int error;

	error = uname(&utsname);
	if (error)
		return error;

	dot = strstr(utsname.nodename, ".");
	if (dot)
		*dot = '\0';

	strcpy(name, utsname.nodename);
	return 0;
}

static void check_arguments(commandline_t *comline)
{
	int error;

	if (!comline->expected_votes)
	        die("expected votes not set");

	if (!comline->clustername[0])
		die("cluster name not set");

	if (!comline->votes)
		comline->votes = DEFAULT_VOTES;

	if (!comline->port)
		comline->port = DEFAULT_PORT;

	if (comline->two_node && comline->expected_votes != 1)
		die("expected_votes value (%d) invalid in two node mode",
		    comline->expected_votes);

	if (!comline->nodenames[0]) {
		comline->nodenames[0] = malloc(255);
		error = uname_to_nodename(comline->nodenames[0]);
		if (error)
			die("cannot get local node name from uname");
		comline->num_nodenames++;
	}

	if (!comline->num_interfaces) {
	        comline->interfaces[0] = strdup("eth0");
		if (!comline->interfaces[0])
			die("no memory");
	}

	if (comline->num_multicasts != comline->num_interfaces) {
	        die("Number of multicast addresses (%d) must match number of "
		    "interfaces (%d)", comline->num_multicasts,
		    comline->num_interfaces);
	}

	if (comline->num_nodenames && comline->num_multicasts &&
	    comline->num_nodenames != comline->num_multicasts) {
	        die("Number of node names (%d) must match number of multicast "
		    "addresses (%d)", comline->num_nodenames,
		    comline->num_multicasts);
	}

	if (comline->port <= 0 || comline->port > 65535)
		die("Port must be a number between 1 and 65535");

	if (strlen(comline->clustername) > MAX_CLUSTER_NAME_LEN) {
	        die("Cluster name must be <= %d characters long",
		    MAX_CLUSTER_NAME_LEN);
	}
}

int main(int argc, char *argv[])
{
	commandline_t comline;

	prog_name = argv[0];

	memset(&comline, 0, sizeof(commandline_t));

	decode_arguments(argc, argv, &comline);

	switch (comline.operation) {
	case OP_JOIN:
		if (!comline.no_ccs)
			get_ccs_join_info(&comline);
		check_arguments(&comline);
		join(&comline);
		break;

	case OP_LEAVE:
		leave(&comline);
		break;

	case OP_EXPECTED:
		set_expected(&comline);
		break;

	case OP_VOTES:
		set_votes(&comline);
		break;

	case OP_KILL:
		kill_node(&comline);
		break;

	/* FIXME: support CLU_SET_NODENAME? */
	}

	exit(EXIT_SUCCESS);
}

char *prog_name;
