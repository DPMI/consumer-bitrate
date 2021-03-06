#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <caputils/caputils.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <csignal>
#include <iostream>
#include <iomanip>
#include <getopt.h>

#include "extract.hpp"
#include "http.hpp"

static int show_zero = 0;
static int viz_hack = 0;
static const char* iface = NULL;
static char* influx_mpid = nullptr;
const char* program_name = NULL;
static const char* influx_url = "http://localhost:8086/write?db=testdb";
static const char* influx_user = "miffo";
static const char* influx_pwd = "konko";
static const char* influx_tag = NULL;

enum {
	HTTP_CREATED = 204,
};

static void handle_sigint(int signum){
	if ( !keep_running ){
		fprintf(stderr, "\rGot SIGINT again, terminating.\n");
		abort();
	}
	fprintf(stderr, "\rAborting capture.\n");
	keep_running = false;
}

class Output {
public:
	virtual void write_header(double sampleFrequency, double tSample){};
	virtual void write_trailer(){};
	virtual void write_sample(double t, double bitrate) = 0;
};

class DefaultOutput: public Output {
public:
	virtual void write_header(double sampleFrequency, double tSample){
		fprintf(stdout, "sampleFrequency: %.2fHz\n", sampleFrequency);
		fprintf(stdout, "tSample:         %fs\n", tSample);
		fprintf(stdout, "\n");
		fprintf(stdout, "Time                      \t   Bitrate (bps)\n");
	}

	virtual void write_sample(double t, double bitrate){
		fprintf(stdout, "%.15f\t%.15f\n", t, bitrate);
	}
};

class CSVOutput: public Output {
public:
	CSVOutput(char delimiter, bool show_header)
		: delimiter(delimiter)
		, show_header(show_header){

	}

	virtual void write_header(double sampleFrequency, double tSample){
		if ( show_header ){
			fprintf(stdout, "\"Time (tSample: %f)\"%c\"Bitrate (bps)\"\n", tSample, delimiter);
		}
	}

	virtual void write_sample(double t, double bitrate){
		fprintf(stdout, "%.15f%c%.15f\n", t, delimiter, bitrate);
	}

private:
	char delimiter;
	bool show_header;
};

class InfluxOutput: public Output {
public:
	InfluxOutput(const char* url, const char* user, const char* pass)
		: http(url, user, pass) {

	}

	virtual void write_sample(double t, double bitrate){
		static char str[1500];

                if (influx_tag) {
                  sprintf(str, "bitrate,mpid=%s,%s value=%g %llu",
                          influx_mpid, influx_tag,bitrate, (long long int)(t*1e9));
                } else {
                  sprintf(str, "bitrate,mpid=%s value=%g %llu",
                          influx_mpid, bitrate, (long long int)(t*1e9));
                }
		fprintf(stderr,"Sending:%s ",  str);
		//		const char* statusstr=http.POSTstr(str);
		const int status = http.POST(str);
		if ( status != HTTP_CREATED ){
		  	fprintf(stderr, "influx(1) returned HTTP %d\n", status);
		} else {
		  fprintf(stdout," OK \n");
		}
	}

protected:
	HTTPOutput http;
};

static double my_round (double value){
	static const double bias = 0.0005;
	return (floor(value + bias));
}

class BitrateCalculator: public Extractor {
public:
	BitrateCalculator()
		: Extractor()
		, bits(0.0){

		set_formatter(FORMAT_DEFAULT);
	}

	void set_mpid(const mampid_t mpid){
		if ( influx_mpid ) return; /* @todo only first mpid will be used */
		influx_mpid = strdup(mampid_get(mpid));
	}

	void set_formatter(enum Formatter format){
		switch (format){
		case FORMAT_DEFAULT: output = new DefaultOutput; break;
		case FORMAT_CSV:     output = new CSVOutput(';', false); break;
		case FORMAT_TSV:     output = new CSVOutput('\t', false); break;
		case FORMAT_MATLAB:  output = new CSVOutput('\t', true); break;
		case FORMAT_INFLUX:  output = new InfluxOutput(influx_url, influx_user, influx_pwd); break;
		}
	}

	using Extractor::set_formatter;

	virtual void reset(){
		bits = 0.0;
		Extractor::reset();
	}

protected:
	virtual void write_header(int index){
		output->write_header(sampleFrequency, to_double(tSample));
	}

	virtual void write_trailer(int index){
		output->write_trailer();
	}

	virtual void write_sample(double t){
		const double bitrate = my_round(bits / to_double(tSample));

		if ( viz_hack ){
			t *= sampleFrequency;
		}

		if ( show_zero || bitrate > 0 ){
			output->write_sample(t, bitrate);
		}

		bits = 0.0;
	}

	virtual void accumulate(qd_real fraction, unsigned long packet_bits, const cap_head* cp, int counter){
		bits += my_round(to_double(fraction) * packet_bits);
	}

private:
	Output* output;
	double bits;
};

static const char* short_options = "u:U:P:Q:p:i:q:m:l:f:zxtTh";
static struct option long_options[]= {
	{"packets",          required_argument, 0, 'p'},
	{"iface",            required_argument, 0, 'i'},
	{"level",            required_argument, 0, 'q'},
	{"sampleFrequency",  required_argument, 0, 'm'},
	{"linkCapacity",     required_argument, 0, 'l'},
	{"format",           required_argument, 0, 'f'},
	{"show-zero",        no_argument,       0, 'z'},
	{"no-show-zero",     no_argument,       0, 'x'},
	{"relative-time",    no_argument,       0, 't'},
	{"absolute-time",    no_argument,       0, 'T'},
	{"viz-hack",         no_argument,       &viz_hack, 1},
	{"influx-url",       required_argument, 0, 'u'},
	{"influx-user",      required_argument, 0, 'U'},
	{"influx-pwd",       required_argument, 0, 'P'},
	{"influx-tag",       required_argument, 0, 'Q'},
	{"help",             no_argument,       0, 'h'},
	{0, 0, 0, 0} /* sentinel */
};

static void show_usage(void){
	printf("%s-" VERSION " (libcap_utils-%s)\n", program_name, caputils_version(NULL));
	printf("(C) 2004 Patrik Arlos <patrik.arlos@bth.se>\n");
	printf("(C) 2012 David Sveningsson <ext@sidvind.com>\n");
	printf("(C) 2012 Vamsi krishna Konakalla <vkk@bth.se>\n\n");
	printf("Usage: %s [OPTIONS] STREAM\n", program_name);
	printf("  -i, --iface                 For ethernet-based streams, this is the interface\n"
	       "                              to listen on. For other streams it is ignored.\n"
	       "  -m, --sampleFrequency       Sampling frequency in Hz. Prefixes: k, m, g.\n"
	       "  -q, --level                 Level to calculate bitrate on. At level N, only\n"
	       "                              packet size at particular layer is considered.\n"
	       "                                - link: all bits captured at physical level.\n"
	       "                                - network: payload field at link layer.\n"
	       "                                - transport: payload at network layer.\n"
	       "                                - application: payload field at transport level.\n"
	       "                              Default is link-layer.\n"
	       "  -l, --linkCapacity          Link capacity in BPS, default is 100e6 (100 Mbps).\n"
	       "  -p, --packets=N             Stop after N packets.\n"
	       "  -z, --show-zero             Show bitrate when zero.\n"
	       "  -x, --no-show-zero          Don't show bitrate when zero [default]\n"
	       "  -f, --format=FORMAT         Set a specific output format. See below for list\n"
	       "                              of supported formats.\n"
	       "  -t, --relative-time         Show timestamps relative to the first packet.\n"
	       "  -T, --absolute-time         Show timestamps with absolute values (default).\n"
	       "  -h, --help                  This text.\n"
	       "\n"
	       "Influx\n"
	       "  -u, --influx-url            URL used to send data to Infux; \n "
	       "                              cf. http://localhost:8086/write?db=testdb \n"
	       "  -U  --influx-user           Influx Username for authentication.\n"
	       "  -P  --influx-pwd            Influx Password for authentication.\n"
	        "  -Q  --influx-tag           Additional Influx tag. tag=<string>\n"

	       "\n"
		);

	output_format_list();
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
	char* format = nullptr;

	int op, option_index = -1;
	while ( (op = getopt_long(argc, argv, short_options, long_options, &option_index)) != -1 ){
		switch (op){
		case 0:   /* long opt */
		case '?': /* unknown opt */
			break;

		case 'f': /* --format */
			/* format initialization is deferred until all args has been
			 * consumed */
			format = strdup(optarg);
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

		case 't': /* --relative-time */
			app.set_relative_time(true);
			break;

		case 'T': /* --absolute-time */
			app.set_relative_time(false);
			break;

		case 'u': /* --influx-url */
			influx_url = optarg;
			break;

		case 'U': /* --influx-user */
			influx_user = optarg;
			break;

		case 'P': /* --influx-pwd */
			influx_pwd = optarg;
			break;

                case 'Q': /* --influx-tag */
                        influx_tag = optarg;
                        break;


		case 'h':
			show_usage();
			return 0;

		default:
			fprintf (stderr, "%s: ?? getopt returned character code 0%o ??\n", program_name, op);
		}
	}

	/* load output format */
	if ( format ){
		app.set_formatter(format);
		free(format);
	}

	/* handle C-c */
	signal(SIGINT, handle_sigint);

	int ret;

	/* Open stream(s) */
	stream_t stream;
	if ( (ret=stream_from_getopt(&stream, argv, optind, argc, iface, "-", program_name, 0)) != 0 ) {
		return ret; /* Error already shown */
	}
	stream_print_info(stream, stderr);

	app.reset();
	app.process_stream(stream, &filter);

	/* Release resources */
	free(influx_mpid);
	stream_close(stream);
	filter_close(&filter);

	return 0;
}
