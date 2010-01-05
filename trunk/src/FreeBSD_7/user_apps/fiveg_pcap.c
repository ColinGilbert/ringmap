#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <errno.h>
#include <pcap.h>
#include <pthread.h>
#include <time.h>
#include <signal.h>

#include <machine/bus.h>

#include <pcap-int.h>
#include "../em/fiveg_da.h"

#ifdef FILE_COPY
#define DUMP_FILE "/home/alexandre/alexandre-da/src/70/userspace/dump.file" 
pcap_dumper_t *dumper;
#endif

#define FILE_NAME "distance.out"
#define FIRST_TIME_STAMP_NUMBER	8

/*   F U N C T I N O S   */
void got_packet(u_char *args, const struct pcap_pkthdr *header, const u_char *packet);
int capture_pkts (long pkt_limit, int snaplen, const char*);
void exitFunc(void);
void sig_ctrlc(int);
void sig_alarm(int);

/*  G L O B A L   V A R S  */
long packet_limit 	= 0;
long time_limit		= 0;
long verbose 		= 0;
int err_ 			= 0;
unsigned long long global_pkt_counter = 0;
unsigned long long buffer_fill = 0;

struct timeval first_timestamp ;
struct timeval last_timestamp ;


#ifdef TRAFFIC_STAT
int dist_arr[500000]; 
unsigned int tmp_ind=0, old_st=0;
FILE *stream;
#endif

pcap_t *handle;

#ifdef MEM_COPY
	char packet_buffer[2048];
#endif 

/******************************************
 *				M A I N 				  *
 ******************************************/
int
main(int argc, char **argv)
{
	char *iface;
	int snaplen;
	
	if (((argc > 1) && (argc < 6)) || (argc > 6) || (argc == 1)){
		printf("Usage: %s pkt_limit time_limit verbose iface snaplen\n", argv[0]);
		exit(1);
	}

	/* Check if all parameters are correct */
	packet_limit = strtol(argv[1], NULL, 10);
	if ((packet_limit == 0) && (errno == EINVAL)){
		printf("Wrong first parameter \n");
		exit(1);
	}
	time_limit = strtol(argv[2], NULL, 10);
	if ((time_limit == 0) && (errno == EINVAL)){
		printf("Wrong second parameter \n");
		exit(1);
	}
	verbose = strtol(argv[3], NULL, 10);
	if ((verbose == 0) && (errno == EINVAL)){
		printf("Wrong third parameter \n");
		exit(1);
	}

	iface = argv[4];
	
	snaplen = strtol(argv[5], NULL, 10);
	if ((verbose == 0) && (errno == EINVAL)){
		printf("Wrong fithf parameter \n");
		exit(1);
	}

	/* Register Exit function */
	if (atexit(exitFunc)){
		printf("Error: Can't register exit function!\n");
		exit(1);
	}

	/* Register signals */
	if (signal(SIGINT, sig_ctrlc) == SIG_ERR){
		printf("Error: Can't register signal\n");
		err_ = -1;
		exit(1);
	}
	if (signal(SIGALRM, sig_alarm) == SIG_ERR){
		printf("Error: Can't register signal\n");
		err_ = -1;
		exit(1);
	}

	if (packet_limit < 0)
		packet_limit = -1;

#ifdef TRAFFIC_STAT
	stream = fopen( FILE_NAME, "w" );
#endif

	/* Capture */
	err_ = capture_pkts(packet_limit, snaplen, iface);

	/* Call our exitFunc() to end the job */
	exit(0);
}
/******************************************
 *			E N D   O F   M A I N         *
 ******************************************/



/******************************************
 *			F U N C T I O N S		      *
 ******************************************/
int 
capture_pkts(long pkt_limit, int snaplen, const char* iface)
{
	char errbuf[PCAP_ERRBUF_SIZE];

	handle = pcap_open_live(iface, snaplen, 1, 1, errbuf);
	if (handle == NULL){
		fprintf(stderr, "pcap handle = NULL!: %s\n", errbuf);
		return (-1);	
	}

#ifdef FILE_COPY
	dumper = pcap_dump_open(handle, DUMP_FILE);
	if (dumper == NULL){
		printf("Error: Can't open dump file \n");
		return (-1);
	}
#endif

	/* Stop capturing after 'time_limit' secs */
	if (time_limit > 0)
		alarm(time_limit);

	pcap_loop(handle, pkt_limit, got_packet, NULL);

	return(0);
}

/*
 * Callback function. Will called for each captured packet
 */
void
got_packet(u_char *args, const struct pcap_pkthdr *header, const u_char *packet)
{
	unsigned long long  new_st;
	unsigned long long  user_to_kerndist;


	if (global_pkt_counter == 0) {
		system ("date >> cpusagetemp");
		system ("echo '---------------'  >> cpusagetemp");
	}
	if (global_pkt_counter == FIRST_TIME_STAMP_NUMBER) {
		first_timestamp = header->ts;
	}
	last_timestamp = header->ts;

#ifdef TRAFFIC_STAT
	if ( (global_pkt_counter%EACH_PKT) == 0 ) {
		new_st = header->ts.tv_sec * 1000000 + header->ts.tv_usec;
		dist_arr[global_pkt_counter/EACH_PKT] = new_st - old_st;
		old_st = new_st;
	}
#endif

#ifdef MEM_COPY
	memcpy(packet_buffer, packet, header->caplen);
#endif

#ifdef FILE_COPY
	pcap_dump((u_char*)dumper, header, packet);
#endif

	global_pkt_counter++;

	if (verbose > 2){
		printf("[%s] packet addr = 0x%X \n", __func__, packet);

#ifdef RING_MAP
		printf("----------------------------- \n");
		print_ring_pointers(handle);
		printf("----------------------------- \n");
#endif

		FIVEG_PRINT_SOME_BYTES_FROM_PKT(packet);
		printf("pkt number = %lld \n", global_pkt_counter);
		printf("\n \n");
	}
}

/*
 * Signal function for catching Ctrl-C
 */
void 
sig_ctrlc(int signo)
{
	if (signo == SIGINT) {
		printf("Stop Capturing. Exit! \n");
		exit(0);
	}
}

void 
sig_alarm(int signo)
{
	if (signo == SIGALRM){
		printf("Stop capturing after %d seconds... \n", time_limit);
		exit(0);
	}
}

/* Exit Point */
void
exitFunc()
{
	unsigned long long cap_time = 
		((last_timestamp.tv_sec * 1000000 + last_timestamp.tv_usec)  - 
		(first_timestamp.tv_sec	* 1000000 + first_timestamp.tv_usec)) / 1000000;
	unsigned long long pps = 0;


	if ( cap_time == 0 )
		cap_time = 1;

	/* Packets per second */
	pps = global_pkt_counter / cap_time;

#ifdef TRAFFIC_STAT
	int i;
#endif

	system ("date >> cpusagetemp");
	system ("echo '---------------'  >> cpusagetemp");

#ifdef BPF_MAP
	struct pcap_stat ps;
#endif


	printf("\n\nRESULTS:\n \n");
	printf("++++++++++++++++++++++++++++++++++++++++++++++++++ \n");
	printf("\n");
	printf("PROCESS STATISTICS: \n");
	printf("------------------  \n");
	printf("Captured: %llu pkts\n", global_pkt_counter);
	printf("Capturing time: %llu (seconds)\n", cap_time);
	printf("Packets per Second: %llu \n", pps);
#ifndef BPF_MAP
	// printf("Buffer average:  %llu\n", handle->buffer_fill_average);
	printf("Buffer 0 - 127:  %llu\n",  handle->buffer_0);
	printf("Buffer 128 - 255:  %llu\n",  handle->buffer_128);
	printf("Buffer 256 - 511:  %llu\n",  handle->buffer_256);
	printf("Buffer 512 - 1023:  %llu\n", handle->buffer_512);
	printf("Buffer 1024 - 2047:  %llu\n", handle->buffer_1024);
#endif
	printf("------------------  \n");

	/* Print statistics from NIC registers */
#ifdef RING_MAP
	if (err_ != -1){
		err_ = 0;
		sleep(1);

		print_ring_stats(handle);
		printf("\n");
		print_hw_stats(verbose - 1, handle);
	}
#endif

#ifdef BPF_MAP
	if (handle == NULL)
		goto out;

	pcap_stats(handle, &ps);

	/*
	 *"ps_recv" counts packets handed to the filter, not packets
	 * that passed the filter.  This includes packets later dropped
	 * because we ran out of buffer space.
	 */
	printf("packets handed to the filter: %d\n", ps.ps_recv);

	/* 
	 * "ps_drop" counts packets dropped inside the BPF device
	 * because we ran out of buffer space.  It doesn't count
	 * packets dropped by the interface driver.  It counts
	 * only packets that passed the filter.
	 */
	printf("Missed Packets (dropped inside the BPF): %d\n", ps.ps_drop);
#endif

	/* End capturing */
#ifdef FILE_COPY
	if (dumper != NULL)
		pcap_dump_close(dumper);
#endif

out:
	if (handle != NULL)
		pcap_close(handle);

	printf("\n++++++++++++++++++++++++++++++++++++++++++++++++++ \n\n");

#ifdef TRAFFIC_STAT
	for (i = 0 ; i < global_pkt_counter ; i += EACH_PKT ) {
		fprintf (stream, "%d - %d\n", i,  dist_arr[i/EACH_PKT]);
	}
#endif
}
