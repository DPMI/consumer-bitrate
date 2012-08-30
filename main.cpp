#include "caputils/caputils.h"
#include "caputils/stream.h"
#include "caputils/filter.h"
#include "caputils/utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <time.h>
#include <getopt.h>
#include <errno.h>
#include <signal.h>

#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <netinet/ip_icmp.h>
#include <sys/time.h>
#include <netdb.h>
#include <cstdlib>
#include <map>
#include <vector>
#include <string.h>
#include <cstring>
#include <string>
#include <iostream>
#include <qd/qd_real.h>
#include <math.h>
#include <iomanip>
//#define debug
#define STPBRIDGES 0x0026
#define CDPVTP 0x016E

#define VERSION "1.1"

using namespace std;

static int keep_running = 1;
static int show_zero = 0;
static int relative_time = 0;
static unsigned int max_packets = 0;
static const char* iface = NULL;
static const struct timeval timeout = {1,0};
static const char* program_name = NULL;

static qd_real remaining_samplinginterval;
static qd_real ref_time;
static qd_real start_time; // has to initialise when the first packet is read
static qd_real end_time; //has to initialise till the next interval
static qd_real tSample;
static double bits; // make bits as qd
static int viz_hack = 0;
static double sampleFrequency = 1.0; //default 1hz

static void handle_sigint(int signum){
	if ( keep_running == 0 ){
		fprintf(stderr, "\rGot SIGINT again, terminating.\n");
		abort();
	}
	fprintf(stderr, "\rAborting capture.\n");
	keep_running = 0;
}
static double my_round (double value){
	static const double bias = 0.0005;
	return (floor(value + bias));
}

static double roundtwo (double value){
	static const double bias = 0.0005;
	return (floor (value + bias));
}

static void default_formatter(double t, double bitrate){
	cout << setiosflags(ios::fixed) << setprecision(15) << t << "\t" << bitrate << endl;
}

static void csv_formatter(double t, double bitrate){
	fprintf(stdout, "%f;%f\n", t, bitrate);
	fflush(stdout);
}

typedef void(*formatter_func)(double t, double bitrate);
static formatter_func formatter = default_formatter;

static void printbitrate() {
	//calculate bitrate
	const double bitrate = roundtwo(bits /to_double(tSample));
	double t = to_double(relative_time ? (start_time - ref_time) : start_time);

	if ( viz_hack ){
		t = to_double(start_time*sampleFrequency);
	}

	//print bitrate greater than zero
	if ( show_zero || bits > 0 ){
		formatter(t, bitrate);
	}

	// reset start_time ; end_time; remaining_sampling interval
	start_time = end_time;
	end_time = start_time + tSample;
	remaining_samplinginterval = tSample;
	bits = 0;
}

enum {
	FMT_CSV = 500,
	FMT_DEF
};

static const char* short_options = "p:i:q:m:l:zxtTh";
static struct option long_options[]= {
	{"packets",          required_argument, 0, 'p'},
	{"iface",            required_argument, 0, 'i'},
	{"level",            required_argument, 0, 'q'},
	{"sampleFrequency",  required_argument, 0, 'm'},
	{"linkCapacity",     required_argument, 0, 'l'},
	{"show-zero",        no_argument,       0, 'z'},
	{"no-show-zero",     no_argument,       0, 'x'},
	{"format-csv",       no_argument,       0, FMT_CSV},
	{"format-default",   no_argument,       0, FMT_DEF},
	{"relative-time",    no_argument,       0, 't'},
	{"absolute-time",    no_argument,       0, 'T'},
	{"viz-hack",         no_argument,       &viz_hack, 1},
	{"help",             no_argument,       0, 'h'},
	{0, 0, 0, 0} /* sentinel */
};

static void show_usage(void){
	printf("%s-" VERSION " (libcap_utils-%s)\n", program_name, caputils_version(NULL));
	printf("(C) 2004 Patrik Arlos <patrik.arlos@bth.se>\n");
	printf("(C) 2012 David Sveningsson <david.sveningsson@bth.se>\n");
	printf("(C) 2012 Vamsi krishna Konakalla <vkk@bth.se>\n\n");
	printf("Usage: %s [OPTIONS] STREAM\n", program_name);
	printf("  -i, --iface                 For ethernet-based streams, this is the interface to listen\n"
	       "                              on. For other streams it is ignored.\n"
	       "  -m, --sampleFrequency       Sampling frequency in Hz. Valid prefixes are 'k', 'm' and 'g'.\n"
	       "  -q, --level 		            Level to calculate bitrate {physical (default), link, network, transport and application}\n"
	       "                              At level N , payload of particular layer is only considered, use filters to select particular streams.\n"
	       "                              To calculate the bitrate at physical , use physical layer, Consider for Network layer use [-q network]\n"
	       "                              It shall contain transport protocol header + payload\n"
	       "                                - link: all bits captured at physical level, i.e link + network + transport + application\n"
	       "                                - network: payload field at link layer , network + transport + application\n"
	       "                                - transport: payload at network  layer, transport + application\n"
	       "                                - application: The payload field at transport leve , ie.application\n"
	       "                              Default is link\n"
	       "  -l, --linkCapacity          Link capacity in bits per second default 100 Mbps, (eg.input 100e6) \n"
	       "  -p, --packets=N             Stop after N packets.\n"
	       "  -z, --show-zero             Show bitrate when zero.\n"
	       "  -x, --no-show-zero          Don't show bitrate when zero [default]\n"
	       "      --format-csv            Use CSV output format.\n"
	       "      --format-default        Use default output format.\n"
	       "      --viz-hack\n"
	       "  -t, --relative-time         Show timestamps relative to the first packet.\n"
	       "  -T, --absolute-time         Show timestamps with absolute values (default).\n"
	       "  -h, --help                  This text.\n\n");
	filter_from_argv_usage();
}

static int payLoadExtraction(int level, const cap_head* caphead) {
	// payload size at physical (ether+network+transport+app)
	if ( level==0 ) {
		return caphead->len;
	};

	// payload size at link  (network+transport+app)
	if ( level==1 ) {
		return caphead->len - sizeof(struct ethhdr);
	};

	const struct ethhdr *ether = caphead->ethhdr;
	const struct ip* ip_hdr = NULL;
	struct tcphdr* tcp = NULL;
	struct udphdr* udp = NULL;
	size_t vlan_offset = 0;

	switch(ntohs(ether->h_proto)) {
	case ETHERTYPE_IP:/* Packet contains an IP, PASS TWO! */
		ip_hdr = (struct ip*)(caphead->payload + sizeof(cap_header) + sizeof(struct ethhdr));
	ipv4:

		// payload size at network  (transport+app)
		if ( level==2 ) {
			return ntohs(ip_hdr->ip_len)-4*ip_hdr->ip_hl;
		};

		switch(ip_hdr->ip_p) { /* Test what transport protocol is present */
		case IPPROTO_TCP: /* TCP */
			tcp = (struct tcphdr*)(caphead->payload + sizeof(cap_header) + sizeof(struct ethhdr) + vlan_offset + 4*ip_hdr->ip_hl);
			if(level==3) return ntohs(ip_hdr->ip_len)-4*tcp->doff-4*ip_hdr->ip_hl;  // payload size at transport  (app)
			break;
		case IPPROTO_UDP: /* UDP */
			udp = (struct udphdr*)(caphead->payload + sizeof(cap_header) + sizeof(struct ethhdr) + vlan_offset + 4*ip_hdr->ip_hl);
			if(level==3) return ntohs(udp->len)-8;                     // payload size at transport  (app)
			break;
		default:
			fprintf(stderr, "Unknown IP transport protocol: %d\n", ip_hdr->ip_p);
			return 0; /* there is no way to know the actual payload size here */
		}
		break;

	case ETHERTYPE_VLAN:
		ip_hdr = (struct ip*)(caphead->payload + sizeof(cap_header) + sizeof(struct ether_vlan_header));
		vlan_offset = 4;
		goto ipv4;

	case ETHERTYPE_IPV6:
		fprintf(stderr, "IPv6 not handled, ignored\n");
		return 0;

	case ETHERTYPE_ARP:
		fprintf(stderr, "ARP not handled, ignored\n");
		return 0;

	case STPBRIDGES:
		fprintf(stderr, "STP not handled, ignored\n");
		return 0;

	case CDPVTP:
		fprintf(stderr, "CDPVTP not handled, ignored\n");
		return 0;

	default:      /* Packet contains unknown link . */
		fprintf(stderr, "Unknown ETHERTYPE 0x%0x \n", ntohs(ether->h_proto));
		return 0; /* there is no way to know the actual payload size here, a zero will ignore it in the calculation */
	}

	fprintf(stderr, "packet wasn't handled by payLoadExtraction, ignored\n");
	return 0;
}

static int prefix_to_multiplier(char prefix){
	prefix = tolower(prefix);
	switch ( prefix ){
	case 0: return 1;
	case 'k': return 1e3;
	case 'm': return 1e6;
	case 'g': return 1e9;
	default: return -1;
	}
}

/**
 * Get prefix from number represented by a string and removes the prefix by
 * setting it to NULL.
 * If no prefix was found it returns 0.
 * E.g. "100k" -> 'k'.
 */
static char pop_prefix(char* string){
	if ( *string == 0 ) return 0;

	const size_t offset = strlen(string) - 1;
	if ( ! isalpha(string[offset]) ){
		return 0;
	}

	const char prefix = string[offset];
	string[offset] = 0;
	return prefix;
}

static void set_sample_frequency(char* string){
	const char prefix = pop_prefix(string);
	int multiplier = prefix_to_multiplier(prefix);

	if ( multiplier == -1 ){
		fprintf(stderr, "unknown prefix '%c' for --sampleFrequency, ignored.\n", prefix);
		multiplier = 1;
	}

	sampleFrequency = atof(string) * multiplier;
	tSample = 1.0 / sampleFrequency;
}

int main(int argc, char **argv){
	/* extract program name from path. e.g. /path/to/MArCd -> MArCd */
	tSample = 1.0/sampleFrequency;
	double linkCapacity = 100e6;
	int level = 0;
	bits = 0;
	const char* separator = strrchr(argv[0], '/');
	if ( separator ){
		program_name = separator + 1;
	} else {
		program_name = argv[0];
	}

	struct filter filter;
	if ( filter_from_argv(&argc, argv, &filter) != 0 ){
		return 0; /* error already shown */
	}

	int op, option_index = -1;
	while ( (op = getopt_long(argc, argv, short_options, long_options, &option_index)) != -1 ){
		switch (op){
		case 0:   /* long opt */
		case '?': /* unknown opt */
			break;

		case FMT_CSV:
			formatter = csv_formatter;
			break;

		case FMT_DEF:
			formatter = default_formatter;
			break;

		case 'p':
			max_packets = atoi(optarg);
			break;

		case 'm' : /* --sampleFrequency */
			set_sample_frequency(optarg);
			break;

		case 'q': /* --level */
			if (strcmp (optarg, "link") == 0)
				level = 0;
			else if ( strcmp (optarg, "network" ) == 0)
				level = 1;
			else if (strcmp (optarg ,"transport") == 0)
				level = 2;
			else if (strcmp (optarg , "application") == 0)
				level = 3;
			else {
				fprintf(stderr, "unrecognised level arg %s \n", optarg);
				exit(1);
			}
			break;

		case 'l': /* --link */
			linkCapacity = atof (optarg);
			cout << " Link Capacity input = " << linkCapacity << " bps\n";
			break;

		case 'i':
			iface = optarg;
			break;

		case 'z':
			show_zero = 1;
			break;

		case 'x':
			show_zero = 0;
			break;

		case 't': /* --relative-time */
			relative_time = 1;
			break;

		case 'T': /* --absolute-time */
			relative_time = 0;
			break;

		case 'h':
			show_usage();
			return 0;

		default:
			printf ("?? getopt returned character code 0%o ??\n", op);
		}
	}

	int ret;
	// initialise packet count
	/* Open stream(s) */
	struct stream* stream;
	if ( (ret=stream_from_getopt(&stream, argv, optind, argc, iface, "-", program_name, 0)) != 0 ) {
		return ret; /* Error already shown */
	}
	const stream_stat_t* stat = stream_get_stat(stream);
	stream_print_info(stream, stderr);

	/* handle C-c */
	signal(SIGINT, handle_sigint);

	while ( keep_running ) {
		/* A short timeout is used to allow the application to "breathe", i.e
		 * terminate if SIGINT was received. */
		struct timeval tv = timeout;

		static int first_packet = 1;

		/* Read the next packet */
		cap_head* cp;
		ret = stream_read(stream, &cp, &filter, &tv);
		if ( ret == EAGAIN ){
			if ( !first_packet ){
				printbitrate();
			}
			continue; /* timeout */
		} else if ( ret != 0 ){
			break; /* shutdown or error */
		}

		const int payLoadSize = payLoadExtraction(level, cp); //payload size
		const qd_real current_time=(qd_real)(double)cp->ts.tv_sec+(qd_real)(double)(cp->ts.tv_psec/(double)PICODIVIDER); // extract timestamp.

		if ( first_packet ) {
			ref_time = current_time;
			start_time = ref_time;
			end_time = ref_time + tSample;
			remaining_samplinginterval = end_time - start_time;
			first_packet = 0;
		}

		while ( keep_running && (to_double(current_time) - to_double(end_time)) >= 0.0){
			printbitrate();
		}

		// estimate transfer time of the packet
		const qd_real transfertime_packet = (payLoadSize*8)/linkCapacity;
		qd_real remaining_transfertime = transfertime_packet;
		remaining_samplinginterval = end_time - current_time; //added now
		while ( keep_running && remaining_transfertime >= remaining_samplinginterval){
			bits += my_round(((to_double(remaining_samplinginterval))/(to_double(transfertime_packet)))*payLoadSize*8); //28 march
			remaining_transfertime-=remaining_samplinginterval;
			printbitrate(); // print bitrate -- dont forget to reset the remaining sampling interval in print_bitrate; set it to tSample; reset Bits;
		}

		// handle small packets or the remaining fractional packets which are in next interval
		bits+= my_round(((to_double(remaining_transfertime))/(to_double(transfertime_packet)))*payLoadSize*8);
		remaining_samplinginterval = end_time - current_time - transfertime_packet;

		if ( max_packets > 0 && stat->matched >= max_packets) {
			/* Read enough pkts lets break. */
			printf("read enought packages\n");
			break;
		}
	}

	/* if ret == -1 the stream was closed properly (e.g EOF or TCP shutdown)
	 * In addition EINTR should not give any errors because it is implied when the
	 * user presses C-c */
	if ( ret > 0 && ret != EINTR ){
		fprintf(stderr, "stream_read() returned 0x%08X: %s\n", ret, caputils_error_string(ret));
	}

	/* Write stats */
	//fprintf(stderr, "%"PRIu64" packets received.\n", stat->read);
	//fprintf(stderr, "%"PRIu64" packets matched filter.\n", stat->matched);

	/* Release resources */
	stream_close(stream);
	filter_close(&filter);

	return 0;
}
