/*
 *  Author: Jeremy Lorelli (SLAC)
 *
 *  Modification History
 *
 */
#include <devIocStats.h>
#include <errlog.h>

#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#define PROCFS_FILE "/proc/net/rpc/nfs"
#define PROCFS_NFS_SERVERS "/proc/net/nfsfs/servers"
#define PROCFS_MOUNTS "/proc/mounts"

#define HOSTNAME_MAX 255
#define MOUNTPOINT_MAX 255

static struct nfsServer_t {
    int ver;
    int port;
    int use;
    char hostname[HOSTNAME_MAX];
    char mountpoint[MOUNTPOINT_MAX];
} nfsServers[MAX_NFS_STATS];

static int numMounts = 0;

static int devIocStatsReadNfsServers(void);
static int devIocStatsReadNfsMounts(void);

int devIocStatsInitNFSStat(void) {
    struct stat st;
    if (stat(PROCFS_FILE, &st) < 0)
        return 0; /** No NFS on this system; silently eat the error **/

    /* Start filling in nfsServers */
    if (devIocStatsReadNfsServers() < 0) {
        return -1;
    }

    /* Finish filling in nfsServers with the mountpoint  */
    if (devIocStatsReadNfsMounts() < 0) {
        return -1;
    }

    return 0;
}

/* Read NFS server info off of procfs */
static int devIocStatsReadNfsServers(void) {
    /* Grab some basic info from procfs. Do this only once, unlikely that the mounts are going to change during IOC exec */

    struct stat st;
    if (stat(PROCFS_NFS_SERVERS, &st) < 0)
        return 0; /* No mounts!! */

    FILE* fp;
    if ((fp = fopen(PROCFS_NFS_SERVERS, "r")) == NULL)
        return 0;

    /* Skip whole first line */
    char* line = NULL;
    size_t sz;
    getline(&line, &sz, fp);
    free(line);

    /* Loop through all of the mounts now */
    char nv[32];
    char hostname[255];
    int port, use;
    while(fscanf(fp, "%31s %*s %d %d %254s\n", nv, &port, &use, hostname) == 4 && numMounts < MAX_NFS_STATS) {
        struct nfsServer_t* s = &nfsServers[numMounts];
        s->use = use;
        s->port = port;
        s->ver = atoi(nv+1);
        strncpy(s->hostname, hostname, sizeof(s->hostname));
        s->hostname[sizeof(s->hostname)-1] = 0;
        numMounts++;
    }

    fclose(fp);

    return 0;
}

/* Read NFS mounts off of procfs */
static int devIocStatsReadNfsMounts(void) {
    struct stat st;
    if (stat(PROCFS_MOUNTS, &st) < 0) {
        errlogPrintf("%s: Could not stat %s: %s\n", __FUNCTION__, PROCFS_MOUNTS, strerror(errno));
        return -1;
    }

    FILE* fp = fopen(PROCFS_MOUNTS, "r");
    if (!fp) {
        errlogPrintf("%s: Could not open %s: %s\n", __FUNCTION__, PROCFS_MOUNTS, strerror(errno));
        return -1;
    }

    char* line = NULL;
    size_t sz = 0;
    int mnt = 0;
    while((getline(&line, &sz, fp)) >= 0 && mnt < MAX_NFS_STATS) {
        char* val = strtok(line, " "); /* Don't care about first part */

        /* Copy in mountpoint */
        if ((val = strtok(NULL, " "))) {
            /* We don't care about mounts other than nfs or nfs4, so check the type and skip optionally  */
            char* type = NULL;
            if ((type = strtok(NULL, " ")) && strcmp(type, "nfs4") && strcmp(type, "nfs"))
                continue;

            strncpy(nfsServers[mnt].mountpoint, val, MOUNTPOINT_MAX);
            nfsServers[mnt].mountpoint[MOUNTPOINT_MAX-1] = 0;
        }
        else
            continue;

        ++mnt;
        free(line);
    }

    fclose(fp);
    return 0;
}

int devIocStatsGetNFSStat(nfsStatInfo* p, int resolve_mounts) {
    FILE* fp;
    if ((fp = fopen(PROCFS_FILE, "r")) == NULL) {
        errlogPrintf("%s: %s open failed: %s\n", __FUNCTION__, PROCFS_FILE, strerror(errno));
        return -1;
    }

    /* Skip what we don't care about */
    fscanf(fp, "net %*d %*d %*d %*d\n");

    long calls, retrans;
    fscanf(fp, "rpc %li %li %*li", &calls, &retrans);

    for (int i = 0; i < numMounts; ++i) {
        nfsStat* mnt = &p->mounts[i];

        /* The following properties are unsupported by the Linux version of this code */
        mnt->gid = -1;
        mnt->liveNodes = -1;
        mnt->uid = -1;
        mnt->retryPeriodMS = -1;
        mnt->rpcTimeouts = -1;
        mnt->rpcErrors = -1;

        /* Fill in with latest data */
        mnt->rpcRequests = calls;
        mnt->rpcRetries = retrans;

        /* First pass fill-in */
        if (resolve_mounts) {
            strncpy(mnt->ip, nfsServers[i].hostname, sizeof(mnt->ip)-1);
            mnt->ip[sizeof(mnt->ip)-1] = 0;

            strncpy(mnt->mount, nfsServers[i].mountpoint, sizeof(mnt->mount)-1);
            mnt->mount[sizeof(mnt->mount)-1] = 0;
        }
    }

    p->numMounts = numMounts;

    return 0;
}

