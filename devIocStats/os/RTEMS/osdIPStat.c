
#include <devIocStats.h>
#include <rtems.h>

#define _KERNEL /* for udpstat, tcpstat and ipstat... */
#include <netinet/in.h>
#include <netinet/tcp_timer.h>
#include <netinet/udp.h>
#include <netinet/ip_var.h>
#include <netinet/udp_var.h>
#include <netinet/tcp_var.h>

int devIocStatsInitIPStat() { return 0; }

int devIocStatsGetIPStat(ipStatInfo* ipStat)
{
	ipStat->udpRecv = udpstat.udps_ipackets;
	ipStat->udpSend = udpstat.udps_opackets;
	ipStat->udpErr = udpstat.udps_badlen + udpstat.udps_badsum + udpstat.udps_noportbcast + udpstat.udps_noport;
	ipStat->tcpSend = tcpstat.tcps_sndpack;
	ipStat->tcpRecv = tcpstat.tcps_rcvpack;
	ipStat->tcpErr = tcpstat.tcps_rcvbadoff + tcpstat.tcps_rcvbadsum + tcpstat.tcps_rcvshort + tcpstat.tcps_drops +
		tcpstat.tcps_pawsdrop;
	ipStat->ipErr = ipstat.ips_badhlen + ipstat.ips_badlen + ipstat.ips_badsum + ipstat.ips_badoptions + ipstat.ips_badvers +
		ipstat.ips_noroute + ipstat.ips_noproto + ipstat.ips_cantforward + ipstat.ips_toolong + ipstat.ips_tooshort + ipstat.ips_toosmall;
	ipStat->ipRecv = ipstat.ips_total;
	return 0;
}

