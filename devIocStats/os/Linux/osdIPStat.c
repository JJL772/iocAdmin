/*
 *  Author: Jeremy Lorelli (SLAC)
 *
 *  Modification History
 *
 */

#include <devIocStats.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>

#include <errlog.h>

#define PROC_NETSTAT "/proc/net/netstat"

/**************************************************************************
 * A note on the convoluted (and ugly) implementation below:
 *  fscanf would probably work fine for reading /proc/net/netstat,
 *  but the line of stats can be ~500b long with many dozens of
 *  stats, even just for TCP. What happens if the format changes?
 *  My very delicate fscanf would break, what's what!
 *  Plus, header values for the columns are provided in there, presumably
 *  you're supposed to be using those to match number with stat.
 *  So that's what's being done here, in a very ugly way.
 **************************************************************************/

struct netStatCtx
{
	char* mem;
	size_t len;
};

static struct netStatCtx netStatCtx = {0};

struct linuxNetStat
{
	const char* name;
	int offset;
};

enum
{
	/* IP stats */
	L_IP_IN_RECV,
	L_IP_OUT_TRANS,
	L_IP_IN_UNK_PROTO,
	L_IP_REASM_TIMEO,

	/* Loop helpers */
	L_IP_FIRST_PROP = L_IP_IN_RECV,
	L_IP_LAST_PROP = L_IP_REASM_TIMEO,

	/* UDP stats */
	L_UDP_IN_DG,
	L_UDP_IN_ERRS,
	L_UDP_OUT_DG,

	/* Loop helpers */
	L_UDP_FIRST_PROP = L_UDP_IN_DG,
	L_UDP_LAST_PROP = L_UDP_OUT_DG,

	/* TCP stats */
	L_TCP_RETRANS_SEGS,
	L_TCP_IN_SEGS,
	L_TCP_OUT_SEGS,
	L_TCP_TIMEO,
	L_TCP_IN_ERRS,

	/* Loop helpers */
	L_TCP_FIRST_PROP = L_TCP_RETRANS_SEGS,
	L_TCP_LAST_PROP = L_TCP_IN_ERRS,

	L_PROP_COUNT,
};

static struct linuxNetStat linuxNetStats[L_PROP_COUNT] =
{
	/* Generic IP info */
	[L_IP_IN_RECV]			={"IpInReceives",		-1},
	[L_IP_OUT_TRANS]		={"IpOutTransmits",		-1},
	[L_IP_IN_UNK_PROTO] 	={"IpInUnknownProtos",	-1},
	[L_IP_REASM_TIMEO]		={"IpReasmTimeout",		-1},

	/* UDP info */
	[L_UDP_IN_DG]			= {"UdpInDatagrams",	-1},
	[L_UDP_IN_ERRS]			= {"UdpInErrors",		-1},
	[L_UDP_OUT_DG]			= {"UdpOutDatagrams",	-1},

	/* TCP info */
	[L_TCP_RETRANS_SEGS] 	= {"TcpRetransSegs",	-1},
	[L_TCP_IN_SEGS] 		= {"TcpInSegs",			-1},
	[L_TCP_OUT_SEGS] 		= {"TcpOutSegs",		-1},
	[L_TCP_TIMEO ] 			= {"TcpExtTCPTimeouts",	-1},
	[L_TCP_IN_ERRS]			= {"TcpInErrs",			-1}
};

int devIocStatsInitIPStat()
{
	int fd;
	if ((fd = open("/proc/net/netstat", O_RDONLY)) < 0) {
		errlogPrintf("%s: Unable to open %s: %s\nNetworking stats will be disabled.\n", __FUNCTION__, PROC_NETSTAT, strerror(errno));
		return -1;
	}

	/* Estimate the size we will need to read */
	struct stat st;
	if (stat("/proc/net/netstat", &st) < 0) {
		errlogPrintf("%s: stat failed: %s\n", __FUNCTION__, strerror(errno));
		return -1;
	}

	/* Alloc buffer of sufficient size to grab the data */
	const size_t bufSize = st.st_size + 512;
	char* buf = malloc(bufSize);
	
	ssize_t numRead;
	if ((numRead = read(fd, buf, bufSize-1)) < 0) {
		errlogPrintf("%s: Unable to read %s: %s\n", __FUNCTION__, PROC_NETSTAT, strerror(errno));
		close(fd);
		fd = -1;
		return -1;
	}

	/* Ensure buffer is terminated */
	buf[bufSize-1] = 0;

	/* This big giant mess is to map the property names to values, for ease of parsing later */
	int parsedTcp = 0, parsedUdp = 0, parsedIp = 0;
	for (char* p = strtok(buf, "\n"); p; p = strtok(NULL, "\n")) {
		/* TCP line; determine offsets */
		if (!parsedTcp && !strncmp(p, "TcpExt: ", sizeof("TcpExt: "))) {
			parsedTcp = 1;
			p += sizeof("TcpExt: ");

			/* Now we get to the nested loops! Map up index position with property */
			int index = 0;
			for (char* s = strtok(p, " "); s; ++index, s = strtok(NULL, " ")) {
				for (int i = L_TCP_FIRST_PROP; i <= L_TCP_LAST_PROP; ++i) {
					if (!strcmp(linuxNetStats[i].name, p) && linuxNetStats[i].offset >= 0)
						linuxNetStats[i].offset = index;
				}
			}
		}
		else if (!parsedIp && !strncmp(p, "IpExt: ", sizeof("IpExt: "))) {
			parsedIp = 1;
			p += sizeof("IpExt: ");

			/* Yet more nested loops, this time for IP stats */
			int index = 0;
			for (char* s = strtok(p, " "); s; ++index, s = strtok(NULL, " ")) {
				for (int i = L_IP_FIRST_PROP; i <= L_IP_LAST_PROP; ++i) {
					if (!strcmp(linuxNetStats[i].name, p) && linuxNetStats[i].offset >= 0)
						linuxNetStats[i].offset = index;
				}
			}
		}

		if (parsedIp && parsedTcp && parsedUdp)
			break;
	}

	close(fd);

	return 0;
}

int devIocStatsGetIPStat(ipStatInfo* ipStat)
{
	int fd;
	if ((fd = open("/proc/net/netstat", O_RDONLY)) < 0) {
		errlogPrintf("%s: Failed to open %s\n", __FUNCTION__, PROC_NETSTAT);
		return -1;
	}

	off_t offset;
	if ((offset = lseek(fd, 0, SEEK_END)) == (off_t)-1) {
		errlogPrintf("%s: lseek failed: %s\n", __FUNCTION__, strerror(errno));
		close(fd);
		return -1;
	}

	if (lseek(fd, 0, SEEK_SET) == (off_t)-1) {
		errlogPrintf("%s: lseek failed: %s\n", __FUNCTION__, strerror(errno));
		close(fd);
		return -1;
	}

	/* Some 'smart' buffer resizing to ensure we have enough space */
	if (!netStatCtx.mem || offset > netStatCtx.len) {
		netStatCtx.len = offset + 128;
		if (netStatCtx.mem)
			free(netStatCtx.mem);
		netStatCtx.mem = malloc(netStatCtx.len);
	}

	/* Read it */
	ssize_t numRead = 0;
	if ((numRead = read(fd, netStatCtx.mem, netStatCtx.len-1)) < 0) {
		errlogPrintf("%s: read failed: %s\n", __FUNCTION__, strerror(errno));
		close(fd);
		return -1;
	}
	netStatCtx.mem[numRead] = 0;

	int stats[L_PROP_COUNT] = {0};

	int tcpFound = 0, ipFound = 0;
	for (char* p = strtok(netStatCtx.mem, "\n"); p && tcpFound < 2 && ipFound < 2; p = strtok(NULL, "\n")) {
		if (!strncmp(p, "TcpExt: ", sizeof("TcpExt: "))) {
			if (!tcpFound) {
				++tcpFound;
				continue;
			}

			for (; p; p = strtok(NULL, " ")) {
				for (int i = L_TCP_FIRST_PROP; i <= L_TCP_LAST_PROP; ++i) {
					if (strcmp(linuxNetStats[i].name, p))
						continue;
					stats[linuxNetStats[i].offset] = atoi(p);
				}
			}
			++tcpFound;
		}
		else if (!strncmp(p, "IpExt: ", sizeof("IpExt: "))) {
			if (!ipFound) {
				++ipFound;
				continue;
			}

			for (; p; p = strtok(NULL, " ")) {
				for (int i = L_IP_FIRST_PROP; i <= L_IP_LAST_PROP; ++i) {
					if (strcmp(linuxNetStats[i].name, p))
						continue;
					stats[linuxNetStats[i].offset] = atoi(p);
				}
			}
			++ipFound;
		}
	}

	/* Failed to find the info for some reason, just return error */
	if (tcpFound < 2 || ipFound < 2) {
		close(fd);
		return -1;
	}

	/* Assemble the stats into one big blob */

	ipStat->udpErr = ipStat->udpRecv = ipStat->udpSend = -1; /* Unsupported for now */

	ipStat->tcpRecv = stats[L_TCP_IN_SEGS];
	ipStat->tcpSend = stats[L_TCP_OUT_SEGS];
	ipStat->tcpErr = stats[L_TCP_TIMEO] + stats[L_TCP_IN_ERRS];
	ipStat->tcpRetrans = stats[L_TCP_RETRANS_SEGS];

	ipStat->ipRecv = stats[L_IP_IN_RECV];
	ipStat->ipErr = stats[L_IP_IN_UNK_PROTO] + stats[L_IP_REASM_TIMEO];

	close(fd);

	return 0;
}

