/*************************************************************************\
* Copyright (c) 2009-2010 Helmholtz-Zentrum Berlin
*     fuer Materialien und Energie GmbH.
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/* devIocStats.h -  Device Support Include for IOC statistics - based on */
/* devVXStats.c - Device Support Routines for vxWorks statistics */
/*
 *	Author: Jim Kowalkowski
 *	Date:  2/1/96
 *
 */

#ifndef devIocStats_H
#define devIocStats_H

#include <epicsVersion.h>

#ifndef VERSION_INT
#  define VERSION_INT(V,R,M,P) ( ((V)<<24) | ((R)<<16) | ((M)<<8) | (P))
#  define EPICS_VERSION_INT  VERSION_INT(EPICS_VERSION, EPICS_REVISION, EPICS_MODIFICATION, EPICS_PATCH_LEVEL)
#endif


/* Cluster info pool types */
#define DATA_POOL 0
#define SYS_POOL  1

/* devIocStats value types (different update rates) */
#define MEMORY_TYPE	0
#define LOAD_TYPE	1
#define FD_TYPE		2
#define CA_TYPE		3
#define QUEUE_TYPE	4
#define STATIC_TYPE	5
#define NET_TYPE	6
#define TOTAL_TYPES	7

/* Names of environment variables (may be redefined in OSD include) */
#define STARTUP  "STARTUP"
#define ST_CMD   "ST_CMD"
#define ENGINEER "ENGINEER"
#define LOCATION "LOCATION"

#include "devIocStatsOSD.h"

typedef int clustInfo[CLUSTSIZES][4];

typedef struct {
    double numBytesTotal;
    double numBytesFree;
    double numBytesAlloc;
    double numBlocksFree;
    double numBlocksAlloc;
    double maxBlockSizeFree;
} memInfo;

typedef struct {
    int used;
    int max;
} fdInfo;

typedef struct {
    int ierrors;
    int oerrors;
} ifErrInfo;

typedef struct {
    long noOfCpus;
    double cpuLoad;
    double iocLoad;
} loadInfo;

typedef struct {
	unsigned long ipRecv;	/* How many packets we've recv'ed on the IP layer (TCP + UDP + ICMP + etc.) */
	unsigned long ipErr;	/* How many packets were lost due to error on IP layer */
	unsigned long udpRecv;	/* How many UDP packets we've recv'ed */
	unsigned long udpSend;	/* How many UDP packets we've sent */
	unsigned long udpErr;	/* How many UDP packets were lost due to errors (i.e. bad cksum) */
	unsigned long tcpRecv;	/* How many TCP packets we've recv'ed */
	unsigned long tcpSend;	/* How many TCP packets we've sent */
	unsigned long tcpErr;	/* How many TCP packets were lost and had to be resent */
} ipStatInfo;

/* Stats for individual NFS mount */
typedef struct {
	char mount[128];
	char ip[32];
	unsigned long port;
	unsigned long uid;
	unsigned long gid;
	unsigned long liveNodes;	/* Live NFS nodes */
	unsigned long rpcRequests;	/* How many RPC requests total */
	unsigned long rpcRetries;	/* How many RPC retries total */
	unsigned long rpcErrors;	/* How many RPC errors */
	unsigned long rpcTimeouts;	/* How many RPC timeouts */
	unsigned long retryPeriodMS;	/* Retry period in MS */
} nfsStat;

#define MAX_NFS_STATS 8 /* Max 8 mounts! */
typedef struct {
	int numMounts;
	nfsStat mounts[MAX_NFS_STATS];
} nfsStatInfo;

/* Functions (API) for OSD layer */
/* All funcs return 0 (OK) / -1 (ERROR) */

/* CPU Load */
extern int devIocStatsInitCpuUsage (void);
extern int devIocStatsGetCpuUsage (loadInfo *pval);

/* IOC Load (CPU utilization by this IOC) */
extern int devIocStatsInitCpuUtilization (loadInfo *pval);
extern int devIocStatsGetCpuUtilization (loadInfo *pval);

/* FD Usage */
extern int devIocStatsInitFDUsage (void);
extern int devIocStatsGetFDUsage (fdInfo *pval);

/* Memory Usage */
extern int devIocStatsInitMemUsage (void);
extern int devIocStatsGetMemUsage (memInfo *pval);

/* RAM Workspace Usage */
extern int devIocStatsInitWorkspaceUsage (void);
extern int devIocStatsGetWorkspaceUsage (memInfo *pval);

/* Suspended Tasks */
extern int devIocStatsInitSuspTasks (void);
extern int devIocStatsGetSuspTasks (int *pval);

/* Cluster Info */
extern int devIocStatsInitClusterInfo (void);
extern int devIocStatsGetClusterInfo (int pool, clustInfo *pval);
extern int devIocStatsGetClusterUsage (int pool, int *pval);

/* Network Interface Errors */
extern int devIocStatsInitIFErrors (void);
extern int devIocStatsGetIFErrors (ifErrInfo *pval);

/* Network Stats */
extern int devIocStatsInitIPStat (void);
extern int devIocStatsGetIPStat (ipStatInfo *pval);

/* NFS stats */
extern int devIocStatsInitNFSStat (void);
extern int devIocStatsGetNFSStat (nfsStatInfo *pval, int resolve_mounts);

/* Boot Info */
extern int devIocStatsInitBootInfo (void);
extern int devIocStatsGetBootLine (char **pval);
extern int devIocStatsGetStartupScript (char **pval);
extern int devIocStatsGetStartupScriptDefault (char **pval);

/* System Info */
extern int devIocStatsInitSystemInfo (void);
extern int devIocStatsGetBSPVersion (char **pval);
extern int devIocStatsGetKernelVersion (char **pval);

/* Host Info */
extern int devIocStatsInitHostInfo (void);
extern int devIocStatsGetPwd (char **pval);
extern int devIocStatsGetHostname (char **pval);
extern int devIocStatsGetPID (double *proc_id);
extern int devIocStatsGetPPID (double *proc_id);

#endif /* devIocStats_H */
