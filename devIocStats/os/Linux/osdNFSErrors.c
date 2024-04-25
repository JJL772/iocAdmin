
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

static int nfsStatFd = -1;

static struct {
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

    if ((nfsStatFd = open(PROCFS_FILE, O_RDONLY)) < 0) {
        errlogPrintf("Unable to open %s: %s", PROCFS_FILE, strerror(errno));
        return -1;
    }

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

    int fd;
    if ((fd = open(PROCFS_NFS_SERVERS, O_RDONLY)) < 0)
        return 0; /* No mounts!! */

    char* buf = malloc(st.st_size+1);
    if (read(fd, buf, st.st_size) < 0) {
        errlogPrintf("Unable to read %s: %s\n", PROCFS_NFS_SERVERS, strerror(errno));
        goto error;
    }

    /* First line just describes columns */
    char* p = strpbrk(buf, "\n");

    /* No mounts (probably) */
    if (!p) {
        close(fd);
        return 0;
    }

    /* All subsequent lines are mounts */
    while((p = strpbrk(p, "\n"))) {
        char line[512];
        char* val = NULL;
        ++p;

        /* Read a line */
        int i;
        for (i = 0; i < sizeof(line)-1 && p[i] != '\n'; ++i)
            line[i] = p[i];
        line[i] = 0;

        /* Get NFS version */
        if ((val = strtok(line, " ")) && *val == 'v')
            nfsServers[numMounts].ver = atoi(val+1);
        else
            goto error;

        /* Don't care about server 'name' */
        strtok(NULL, " ");

        /* Read PORT */
        if ((val = strtok(NULL, " ")))
            nfsServers[numMounts].port = atoi(val);
        else
            goto error;

        /* Read number of times used */
        if ((val = strtok(NULL, " ")))
            nfsServers[numMounts].use = atoi(val);
        else
            goto error;

        /* Copy host name */
        if ((val = strtok(NULL, " "))) {
            strncpy(nfsServers[numMounts].hostname, val, HOSTNAME_MAX);
            nfsServers[numMounts].hostname[HOSTNAME_MAX-1] = 0;
        }
        else
            goto error;

        numMounts++;
    }

    close(fd);
    return 0;

error:
    close(fd);
    return -1;
}

/* Read NFS mounts off of procfs */
static int devIocStatsReadNfsMounts(void) {
    struct stat st;
    if (stat(PROCFS_MOUNTS, &st) < 0) {
        errlogPrintf("Could not stat %s: %s\n", PROCFS_MOUNTS, strerror(errno));
        return -1;
    }

    int fd;
    if ((fd = open(PROCFS_MOUNTS, O_RDONLY)) < 0) {
        errlogPrintf("Could not open %s: %s\n", PROCFS_MOUNTS, strerror(errno));
        return -1;
    }

    /* Alloc a buffer just big enough for the entire file */
    char* buf = malloc(st.st_size+1);
    memset(buf, 0, st.st_size);
    if (read(fd, buf, st.st_size) < 0)
        goto error;

    /* Parse line-by-line */
    int mnt = 0;
    for (char* line = strtok(buf, "\n"); line && mnt < MAX_NFS_STATS; line = strtok(NULL, "\n")) {
        char* val = strtok(NULL, " "); /* Don't care about first part */

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
    }

    close(fd);
    return 0;

error:
    close(fd);
    return -1;
}

int devIocStatsGetNFSStat(nfsStatInfo* p, int resolve_mounts) {
    if (nfsStatFd < 0)
        return 0;

    if (lseek(nfsStatFd, 0, SEEK_SET) == (off_t)-1) {
        errlogPrintf("lseek failed: %s\n", strerror(errno));
        return -1;
    }

    /* Read the latest data */
    char buf[512];
    if (read(nfsStatFd, buf, sizeof(buf)-1) < 0) {
        return -1;
    }

    int calls = 0, retrans = 0;

    /* Parse it! */
    char* ptr = strtok(buf, "\n");
    if ((ptr = strtok(NULL, "\n"))) {
        strtok(NULL, " "); /* Seek to second line */

        if ((ptr = strtok(NULL, " ")))
            calls = atoi(ptr);
        if ((ptr = strtok(NULL, " ")))
            retrans = atoi(ptr);
    }

    for (int i = 0; i < numMounts; ++i) {
        nfsStat* mnt = &p->mounts[i];

        /* The following properties are unsupported by the Linux version of this code */
        mnt->gid = 0;
        mnt->liveNodes = 0;
        mnt->uid = 0;
        mnt->retryPeriodMS = 0;
        mnt->rpcTimeouts = 0;
        mnt->rpcErrors = 0;

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

