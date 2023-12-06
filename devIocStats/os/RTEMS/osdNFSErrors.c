
#include <devIocStats.h>
#include <epicsStdio.h>

#include "librtemsNfs.h"

int devIocStatsInitNFSStat() { return 0; }

#ifdef HAVE_NFS_STATS
const char* nfs_stats_marker = "NFS stats enabled";
#endif

int devIocStatsGetNFSStat(nfsStatInfo* stats, int resolve_mounts)
{
	int i;
	//memset(stats->mounts, 0, sizeof(stats->mounts));
#ifdef HAVE_NFS_STATS
	struct nfs_stats st[MAX_NFS_STATS];
	stats->numMounts = nfsGetStats(st, MAX_NFS_STATS, resolve_mounts);
	for (i = 0; i < stats->numMounts; ++i) {
		nfsStat* stat = &stats->mounts[i];
		stat->gid = st[i].gid;
		stat->uid = st[i].uid;
		stat->liveNodes = st[i].live_nodes;
		stat->port = st[i].port;
		stat->rpcErrors = st[i].errors;
		stat->rpcRequests = st[i].requests;
		stat->rpcRetries = st[i].retries;
		stat->rpcTimeouts = st[i].timeouts;
		stat->retryPeriodMS = st[i].retry_period * 1000.f;
		strncpy(stat->ip, st[i].ip, sizeof(stat->ip)-1);
		strncpy(stat->mount, st[i].mount, sizeof(stat->mount)-1);
	}
#else
#warning NFS stat tracking will be disabled for this target
#endif
	return 0;
}
