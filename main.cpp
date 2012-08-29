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

#include <sys/time.h>
#include <netdb.h>
#include <cstdlib>
#include <map>
#include <vector>
#include <string.h>
#include <cstring>
#include <string>
#include <iostream>
#include <math.h>
#include <iomanip>

#include "extract.hpp"

#define VERSION "1.1"

static int show_zero = 0;
static int viz_hack = 0;
static const char* iface = NULL;
const char* program_name = NULL;

static void handle_sigint(int signum){
	if ( !keep_running ){
		fprintf(stderr, "\rGot SIGINT again, terminating.\n");
		abort();
	}
	fprintf(stderr, "\rAborting capture.\n");
	keep_running = false;
}

class BitrateCalculator: public Extractor {
public:
	BitrateCalculator()
		: Extractor()
		, formatter(default_formatter){

	}

	enum Formatter {
		FMT_CSV = 500,
		FMT_DEF
	};

	void set_formatter(enum Formatter format){
		switch (format){
		case FMT_CSV: formatter = csv_formatter; break;
		case FMT_DEF: formatter = default_formatter; break;
		}
	}

protected:
	virtual void write_sample(double t, double bitrate){
		if ( viz_hack ){
			t *= get_sampling_frequency();
		}

		if ( show_zero || bitrate > 0 ){
			formatter(t, bitrate);
		}
	}

private:
	typedef void(*formatter_func)(double t, double bitrate);

	static void default_formatter(double t, double bitrate){
		std::cout << setiosflags(std::ios::fixed) << std::setprecision(15) << t << "\t" << bitrate << std::endl;
	}

	static void csv_formatter(double t, double bitrate){
		fprintf(stdout, "%f;%f\n", t, bitrate);
		fflush(stdout);
	}

	formatter_func formatter;
};

static const char* short_options = "p:i:q:m:l:zxh";
static struct option long_options[]= {
	{"packets",          required_argument, 0, 'p'},
	{"iface",            required_argument, 0, 'i'},
	{"level",            required_argument, 0, 'q'},
	{"sampleFrequency",  required_argument, 0, 'm'},
	{"linkCapacity",     required_argument, 0, 'l'},
	{"show-zero",        no_argument,       0, 'z'},
	{"no-show-zero",     no_argument,       0, 'x'},
	{"format-csv",       no_argument,       0, BitrateCalculator::FMT_CSV},
	{"format-default",   no_argument,       0, BitrateCalculator::FMT_DEF},
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
	       "  -h, --help                  This text.\n\n");
	filter_from_argv_usage();
}

int main(int argc, char **argv){
	/* extract program name from path. e.g. /path/to/MArCd -> MArCd */
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

	BitrateCalculator app;

	int op, option_index = -1;
	while ( (op = getopt_long(argc, argv, short_options, long_options, &option_index)) != -1 ){
		switch (op){
		case 0:   /* long opt */
		case '?': /* unknown opt */
			break;

		case BitrateCalculator::FMT_CSV:
		case BitrateCalculator::FMT_DEF:
			app.set_formatter((enum BitrateCalculator::Formatter)op);
			break;

		case 'p':
			app.set_max_packets(atoi(optarg));
			break;

		case 'm' : /* --sampleFrequency */
			app.set_sampling_frequency(optarg);
			break;

		case 'q': /* --level */
			app.set_extraction_level(optarg);
			break;

		case 'l': /* --link */
			app.set_link_capacity(optarg);
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

		case 'h':
			show_usage();
			return 0;

		default:
			fprintf (stderr, "%s: ?? getopt returned character code 0%o ??\n", program_name, op);
		}
	}

	/* handle C-c */
	signal(SIGINT, handle_sigint);

	int ret;

	/* Open stream(s) */
	struct stream* stream;
	if ( (ret=stream_from_getopt(&stream, argv, optind, argc, iface, "-", program_name, 0)) != 0 ) {
		return ret; /* Error already shown */
	}
	stream_print_info(stream, stderr);

	app.reset();
	app.process_stream(stream, &filter);

	/* Release resources */
	stream_close(stream);
	filter_close(&filter);

	return 0;
}
