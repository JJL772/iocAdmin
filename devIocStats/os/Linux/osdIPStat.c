
#include <devIocStats.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>

#include <errlog.h>

static int netStatFd = -1;

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
	[L_IP_IN_RECV]			={"IpInReceives", -1},
	[L_IP_OUT_TRANS]		={"IpOutTransmits", -1},
	[L_IP_IN_UNK_PROTO] 	={"IpInUnknownProtos", -1},
	[L_IP_REASM_TIMEO]		={"IpReasmTimeout", -1},

	/* UDP info */
	[L_UDP_IN_DG]			= {"UdpInDatagrams", -1},
	[L_UDP_IN_ERRS]			= {"UdpInErrors", -1},
	[L_UDP_OUT_DG]			= {"UdpOutDatagrams", -1},

	/* TCP info */
	[L_TCP_RETRANS_SEGS] 	= {"TcpRetransSegs", -1},
	[L_TCP_IN_SEGS] 		= {"TcpInSegs", -1},
	[L_TCP_OUT_SEGS] 		= {"TcpOutSegs", -1},
	[L_TCP_TIMEO ] 			= {"TcpExtTCPTimeouts", -1},
	[L_TCP_IN_ERRS]			= {"TcpInErrs", -1}
};

int devIocStatsInitIPStat()
{
	if ((netStatFd = open("/proc/net/netstat", O_RDONLY)) < 0) {
		errlogPrintf("%s: Unable to open /proc/net/netstat: %s\nNetworking stats will be disabled.\n", __FUNCTION__, strerror(errno));
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
	if ((numRead = read(netStatFd, buf, bufSize-1)) < 0) {
		errlogPrintf("%s: Unable to read /proc/net/netstat: %s\n", __FUNCTION__, strerror(errno));
		close(netStatFd);
		netStatFd = -1;
		return -1;
	}

	/* Ensure buffer is terminated */
	buf[bufSize-1] = 0;

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
					if (!strcmp(linuxNetStats[i].name, p))
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
					if (!strcmp(linuxNetStats[i].name, p))
						linuxNetStats[i].offset = index;
				}
			}
		}

		if (parsedIp && parsedTcp && parsedUdp)
			break;
	}

	return 0;
}

int devIocStatsGetIPStat(ipStatInfo* ipStat)
{
	if (netStatFd < 0)
		return 0;

	
	
	return 0;
}

