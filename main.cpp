// Functions to remember in the new libcap utilities

// To Open a stream:
// int stream_open (struct stream ** stptr, const stream_addr_t* addr, const char * nic, const int port )

// general usage : first obtain a stream address stream_addr_t input;

//comments
#define pdebug() printf("########## %s %d \n",_FILE_,_LINE_);
//#ifdef __cplusplus
//extern "C"{
//#endif
#include "caputils/caputils.h"
#include "caputils/stream.h"
#include "caputils/filter.h"
//#ifdef __cplusplus
//}
//#endif



#include <stdio.h>
#include <net/if_arp.h>
#include <unistd.h>
#include <getopt.h>
#include <time.h>
#include <iostream>
#include <iomanip>
#include <string.h>
#include <math.h>
#include <qd/qd_real.h>
#include <signal.h>
#include <string>
#include <stdlib.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
using namespace std;

#define APP_VERSION_MAJOR 0
#define APP_VERSION_MINOR 2
#define PICODIVIDER (double)1.0e12
#define STPBRIDGES 0x0026
#define CDPVTP 0x016E

void pktArrival (double tArr, int pktSize, double linkCapacity,qd_real *BITS, qd_real *ST, int bins, double tSample);
double sampleBINS (qd_real *BITS, qd_real * ST, int bins, double tSample, double linkCapacity);
int payLoadExtraction (int level, char* data);
void filter_from_argv_usage(void);
void Sample (int sig);

int dropCount; // number of packets that have been dropped
qd_real timeOffset;
int fractionalPDU;

int main (int argc , char **argv) {
	int noBins, level, payLoadSize; // Bits Per Second calculation variables [BPScv]
	//int noBins - number of bins (histogram bar width) , calculated automatically
	//int level -  at what level should bitrate be calculated, given by user
	// int payLoadSize  - how large is number of bits covered by the desired level, automatically calculated

	double linkCapacity, sampleFrequency, sampleValue;
	int triggerPoint = 0; //When do we do the sampling
	int verbose = 0; //Verbose output
	int option_index, eventCounter,sampleCounter ; //BPScv Consider alter to double
	qd_real tSample, nextSample,lastEvent, pktArrivalTime; // when does the next sample occur, sample interval time
	qd_real *BINS, *ST;
	dropCount =0; // initializing packets dropped
	option_index = 0; //variables to handle calling arguments
	fractionalPDU = 1;
	//double xLow,xHigh,xDiff ; //histogram variables
	//int histFlag;

	sampleFrequency = 1;
	tSample = 1/(double)sampleFrequency;
	linkCapacity=10e6; // initializing as 10 Mbps
	level = 0;

	int sampleLoop = 60;
	double linkC = 100.0 ;
	extern int optind, opterr, optopt ; // STD file reader variables
	register int op;
	op = 0;

	int this_option_optind ;

	static struct option long_options [] = {
		{"pkts",1 , 0, 'p'},
		{"samplefrequency", required_argument, 0 ,'m'},
		{"triggerpoint", required_argument, 0, 'n'},
		{"level", required_argument, 0 ,'q'},
		{"database",       required_argument, 0, 'a'},
		{"table",          required_argument, 0, 'b'},
		{"user",           required_argument, 0, 'c'},
		{"password",       required_argument, 0, 'd'},
		{"host",           required_argument, 0, 'e'},
		{"low",            required_argument, 0, 'f'},
		{"upper",	   required_argument, 0, 'g'},
		{"delta",	   required_argument, 0, 'k'},
		{"link",	   required_argument, 0, 'l'},
		{"sampleinterval", required_argument, 0, 'j'},
		{"help",           required_argument, 0, 'h'},
		{"if",             required_argument, 0, 'i'},
		{"tcp",            required_argument, 0, 't'},
		{"udp",            required_argument, 0, 'u'},
		{"port",           required_argument, 0, 'v'},
		{"verbose",           required_argument, 0, 'w'},
		{0,0,0,0}
	};



	/* Libcap 0.7 variables */
	struct cap_header *caphead;
	char * filename;
	struct filter myFilter; // filter to filter arguments
	stream_t* inStream; // stream to read from
	stream_addr_t src; // address of stream
	int streamType; // stream type defining udp tcp or ethernet
	streamType =0; // default initialization is ethernet
	int l ; //temporary variable
	char *nic = 0; // holder of NIC string
	int portnumber = 0x810; // default port number initialization
	int readStreamStatus; // reading stream status default must return 0
	//int d; // for dealing with packets
	//int* data = &d; //pointer to packets
	//int** dataPtr = &data; // pointer to pointer of packets
	double pktCount = 0; // counting number of packets
	struct timeval tv = {1,0} ; // initilizing timeval to be passed on to

	// end of libcap 0.7 variables

	double pkts;
	struct timeval tid1, tid2; //times used with runtime.
	// timer used with runtime
	struct timeval theTime;

	if ((filter_from_argv(&argc,argv, &myFilter)) != 0) {
		fprintf (stderr, "could not create filter ");
		exit (0);
	}

	pkts = -1;
	pktCount = 0;
	int required = 0;

	if ( argc < 2 ) {
		printf ("use %s -h or --help for help \n" , argv[0]);
		exit (0);
	}

	while ((op = getopt_long (argc,argv,"l:hi:u:t:w:v:m:n:q:",long_options, &option_index))!=EOF) {
		// this_option_optind = optind ? optind :1;
		//option_index = 0;
		//op = getopt_long (argc,argv,"l:h:i:u:t:w:v:m:n:q:",long_options, &option_index);

		//if (op == -1)
		// break ;

		switch (op) {
		case 'm' : /*sample Frequency*/
			sampleFrequency = atof (optarg);
			tSample = 1/(double) sampleFrequency;
			break;
		case 'n': /*Trigger point*/
			triggerPoint = 1;
			break;
		case 'w': /*verbose */
			verbose = atoi (optarg);
			printf ("verbose enabled \n");
			break ;
		case 'q':
			if (strcmp (optarg, "link") == 0)
				level = 0;
			else if ( strcmp (optarg, "network" ) == 0)
				level = 1;
			else if (strcmp (optarg ,"transport") == 0)
				level = 2;
			else if (strcmp (optarg , "application") == 0)
				level = 3;
			else {
				printf ("unrecognised level arg %s \n", optarg);
				exit (0);
			}
			break;
		case 'l': /*link Capacity in Mbps*/
			linkC = atof (optarg);
			cout << " Link Capacity input = " << linkC << " bps\n";
			break;
		case 'p':
			pkts = atoi (optarg);
			break;
		case 'i' :
			l = strlen (optarg);
			nic = (char*) malloc (l +1);
			strcpy (nic, optarg);
			streamType = 1;
			break;
		case 'u':
			streamType = 2;
			break;
		case 't':
			streamType = 3;
			break;
		case 'v' :
			portnumber = atoi (optarg);
			break;
		case 'h':
			printf ("-m or -- samplefrequency	sample frequency in Hz default [1 Hz] \n");
			printf ("-n or -- triggerpoint	 If enabled Sampling will start 1/(2*fs) s prior to the first packet. \n");
			printf ("	  		otherwise it shall start floor( 1/(2*fs)) s  prior to the first packet.	\n");
			printf ("-q or --level 		Level to calculate bitrate {physical (default), link, network, transport and application} \n");
			printf ("			At level N , payload of particular layer is only considered, use filters to select particular streams. \n");
			printf ("			To calculate the bitrate at physical , use physical layer, Consider for Network layer use [-q network] \n");
			printf ("			It shall contain transport protocol header + payload  \n");
			printf ("			link:		all bits captured at physical level, i.e link + network + transport + application \n");
			printf ("			network:	payload field at link layer , network + transport + application \n");
			printf ("			transport	payload at network  layer, transport + application \n");
			printf ("			application:	The payload field at transport leve , ie.application \n");
			printf ("			Default is link \n");
			printf ("-w or --verbose 	Disable verbose output \n");
			printf ("-h or -- help 		This help Text \n");
			printf ("-e or --host  		Host holding database: default localhost\n");
			printf ("-l or --link 		link capacity in Mbps [Default 100 Mbps]");
			printf ("-i or --if <NIC> 	Listen to NIC for Ethernet multicast Address \n");
			printf ("			Identified by <INPUT> (01:00:00:00:00:01) \n");
			printf ("-t or --tcp 		Listen to TCP stream \n");
			printf ("			Identified by <INPUT> (192.168.0.10) \n");
			printf ("-u or --udp 		Listen to UDP multicast address \n");
			printf ("			Listen by <INPUT> Identified by (225.10.11.10) \n");
			printf ("-v or --port 		TCP/UDP port to listen to. Default 0x0810 \n");
			printf ("<INPUT>		if n,t, u hasnt been declared this \n");
			printf ("			is interpretted as filename\n");
			printf ("Usage: \n");
			printf ("%s [filter options] [application options] <INPUT>\n",argv[0]);
			filter_from_argv_usage();
			exit (0);
			break;

		default:
			l = 0;
		}
	}

	l = strlen (argv [argc -1]);
	filename = (char*) malloc (l +1);
	filename = argv [argc -1];
	printf ("filename is %s \n",filename);
	/* debug */

	noBins =  (ceil ((double)(1514*8)/(double) linkCapacity/to_double(tSample)));
	noBins+= 2; // +1 to account for edge values, second +1 to account for n+1 samples.
	BINS = new qd_real [noBins];
	ST =  new qd_real [noBins];
//to_double is a qd function that converts a qd to int type. else we get an error
	if (verbose) {
		cout << "Longest transfer time = "<< (double)1514*8 /(double) linkCapacity<< endl;
		cout << "tT/tStamp = "<< (double) (double) 1514*8 /(double) linkCapacity/tSample<< endl;
		printf ("we need %d bins \n",noBins);
		printf ("Allocating memory buffer \n");
		cout << "sampleFrequency ="<< sampleFrequency<<"Hz" << to_double(tSample)<< endl;
		cout << "LinkCapacity = "<< linkCapacity/1e6<<"Mbps \n"<<endl;
	}

	long ret;
	if ((argc - optind) > 0) {
		ret = stream_addr_aton(&src, filename,STREAM_ADDR_GUESS,0);
	}
	else {
		printf("must specify source \n");
	}

	if ((ret = stream_open (&inStream, &src, nic,0)) != 0) {
		fprintf (stderr, "stream_open () failed with code 0x%08lX: %s",ret, caputils_error_string(ret));
		exit (0);
		return 1;
	}
// Modified 21 feb 2012

	struct file_version version;
	const char* mampid = stream_get_mampid(inStream);
	const char* comment=  stream_get_comment(inStream);
	stream_get_version (inStream, &version);

	if (verbose) {
		//  printf ("comment size : %d, ver = %d.%d, MPid = %s  \n comments is %s \n", inStream->FH.comment_size, inStream->FH.version.major, inStream->FH.version.minor,inStream->FH.mpid,inStream->comment);
		// disabled for security reasons
		printf ("version.major = %d, version.minor = %d \n", version.major, version.minor);
		printf("measurementpoint-id = %s \n", mampid != 0 ? mampid : "(unset)\n");
		printf("comment = %s \n",comment ? comment : "(comment)\n");
	}

	tid1.tv_sec = sampleLoop;
	tid1.tv_usec = 0;
	tid2.tv_sec =sampleLoop;
	tid2.tv_usec= 0;
	// begin packet processing

	//while  ( (ret = stream_read (src,dataPtr,NULL,&tv)) == 0){

	///////////////////// HERE ONWARS LOOK INTOOOOO
	//Begin Packet processing
	// rpStatus=read_post(&inStream,dataPtr,myfilter);
	ret = 0; // Here the pointer to first packet is already retuned , so extract the timing information from the packet
	//*dataPtr=(inStream->buffer+inStream->readPos);
	ret = stream_read (inStream,&caphead,&myFilter,&tv);
	if(ret==0){
		// data=*dataPtr;
		//caphead=(cap_header*)data;
		pktArrivalTime=(qd_real)(double)caphead->ts.tv_sec+(qd_real)(double)caphead->ts.tv_psec/PICODIVIDER;
		timeOffset=floor(pktArrivalTime);
		pktArrivalTime-=timeOffset;

		if(triggerPoint==1) {
			nextSample=pktArrivalTime+tSample/2.0; // Set the first sample to occur 0.5Ts from the arrival of this packet.
		} else {
			nextSample=floor(pktArrivalTime+tSample/2.0);
		}

		/*
		  cout << "First packet arrives at " << setiosflags(ios::fixed) << setprecision(12) << (double)(pktArrivalTime+timeOffset) << endl;
		  printf("                        %lu.%06llu \n",caphead->ts.tv_sec,caphead->ts.tv_psec);
		  printf("Bins=\n");
		*/

		for(op=0;op<noBins;op++){
			ST[op]=nextSample;
			BINS[op]=0.0;
			nextSample-=tSample; // Since these will have taken place.
			//  cout << "[" << op << "] <" << setiosflags(ios::fixed) << setprecision(12) << (double)ST[op] << " = " << BINS[op]  << endl;
		}

		eventCounter=0;
		sampleCounter=0;

		payLoadSize=0;
		ret = stream_read (inStream,&caphead,&myFilter,&tv); // readpost gives a data pointer.
		//data=*dataPtr;
		//caphead=(cap_header*)data;


		while (ret == 0){
			payLoadSize=0;
			if(pktArrivalTime<ST[0]) { /* Packet arrived first */
				//  cout << "pkt["<< pktCount << "]";
				if(pktArrivalTime<ST[1]) { /* Packet should have arrived earlier, dropping (pktArrival CAN handle this partially */
					//	  cout << caphead->nic << ":" << caphead->ts.tv_sec << "." << caphead->ts.tv_psec << "\t";
					//	  cout << "Dropping packet!!!" << endl;
					dropCount++;
				} else {                   /* Normal packet treatment */
					/* Begin by extracting the interesting information. */
					//    printf("%s:%f.%f:LINK(%4d): \t",caphead->nic,caphead->ts.tv_sec,caphead->ts.tv_psec, caphead->len);
					//     cout << caphead->nic << ":" << caphead->ts.tv_sec << "." << caphead->ts.tv_psec << ":LINK(" << caphead->len << ") \t";
					payLoadSize=payLoadExtraction(level,(char*) caphead);
					pktArrival(to_double(pktArrivalTime),payLoadSize,linkCapacity, BINS,ST, noBins,to_double(tSample));
					lastEvent=pktArrivalTime;
				}  // End Normal packet treatment.

				if(pkts>0 && (pktCount+1)>pkts) {
					/* Read enough pkts lets break. */
					break;
				}
				ret = stream_read (inStream,&caphead,&myFilter,&tv);
				if(ret == 0){ // readpost says not zero so return must be 0 vamsi
					//data=*dataPtr;
					//caphead=(cap_header*)data;
					pktArrivalTime=(double)caphead->ts.tv_sec+(double)caphead->ts.tv_psec/PICODIVIDER;
					pktArrivalTime-=timeOffset;
					pktCount++;
					// printf("Packet number %g read, it had payload %d \n",pktCount,payLoadSize);
				}

			} else { /* Sample shoud occur */
				lastEvent=ST[noBins-1];
				sampleValue=sampleBINS(BINS,ST,noBins,to_double(tSample),linkCapacity);
				//	cout << "[" << sampleCounter <<"]  " << setiosflags(ios::fixed) << setprecision(12) << (double)(lastEvent+timeOffset)<< ": " << sampleValue << " bps " << endl;
				cout << setiosflags(ios::fixed) << setprecision(6) << to_double(lastEvent+timeOffset)<< ":" << sampleValue << endl;
				sampleCounter++;
			}
		}//End Packet processing
		//  cout << "Terminated loop. Cleaning up." << endl;

		for(op=0;op<noBins+2;op++){
			lastEvent=ST[noBins-1];
			sampleValue=sampleBINS(BINS,ST,noBins,to_double(tSample),linkCapacity);
			//      cout << "[" << sampleCounter <<"]" << setiosflags(ios::fixed) << setprecision(12) << (double)(lastEvent+timeOffset) << ": " << sampleValue << endl;
			cout << setiosflags(ios::fixed) << setprecision(6) << to_double(lastEvent+timeOffset) << ":" << sampleValue << endl;
			sampleCounter++;

		}
	} else {
		/* Cant start stream.. */
	}

	delete(ST);
	delete(BINS);

	stream_close(inStream);
	if (verbose) {
		printf("There was a total of %g pkts that matched the filter.\n",pktCount);
	}
	return 0;
}


/* ------------------------SUB-routines-------------------------------*/
/* ****************************************************************** */
/*
  Created:      2002-12-16 ??:??, Patrik.Carlsson@bth.se
  Latest edit:  2003-02-20 10:30, Patrik.Carlsson@bth.se

  Function:
  void pktArrival(*)
  Return value:
  None

  Arguments (In Order):
  Arrival time of Packet.
  Size of Packet/Payload, this is used to calculate when the packet/payload started.
  Currently this only works when the payload is at the end!
  I.e If p indicated payload, H packet headers/or other uninteresting info.
  HHHHpppppp OK
  Hppppppppp OK
  HHHpppHHHH NOT OK
  ppppppHHHH NOT OK
  Link Capacity in bps.
  Array that contains the number of bits that arrived in a sample interval, defined in ST.
  Array that contains the sample times, ST[0] is the NEXT sample, ST[1] the most recent.
  ST[N] is the sample that is to be "releases"
  Number of samples/bins, i.e. N in the definition above.
  Inter sample time, or 1/fS.

  Description:
  In order to correctly estimate where a packets bits arrive, this to sample correctly, we calculate when the first packet/payload
  bit arrived. Once we have the packets 'real' arrival time(tStart), and termination time (tArr). We can overlay this time span
  on the sample time line, the bits of the packet are then 'dropped' down into the corresponding bin. Since we allow arbitrary
  sample frequency (double sets the limit), we can zoom down to sample intervals smaller than the transfer time of a bit. However
  at this level of detail, the time stamp of the packet is _crucial_. Depending on its inaccuracy there might be more than one bit
  in an interval. This routine DOES NOT account for this, it MAY report if there is more bits than possible in a interval.




*/
void pktArrival(double tArr, int pktSize, double linkCapacity,qd_real *BITS,qd_real *STy,int bins, double tSample){
	qd_real tTransfer, tStart;              // Transfer time of packet, start time of packet
	int j;                                  // Yee, can it be a index variable..
	qd_real bits;                           // Temporary variable, holds number of bits in a, or parts of, sample interval

#ifdef debug
	printf("pktArrival() %d bytes ",pktSize);
#endif
	tTransfer=(pktSize*8.0)/linkCapacity;   // Estimate the transfer wire transfer time
	tStart=tArr-tTransfer;                  // Estimate when the packet started arriving

#ifdef debug
	cout << "  [tStart " << setiosflags(ios::fixed) << setprecision(12) << (double)tStart << " -- tTrfs " << (double)tTransfer << "  ---- " << (double)tArr << " tT/tS " << (double)tTransfer/tSample << endl;

#endif
	if(fractionalPDU==0){        // Do not consider fractional pdus, just wack them in the array.
		BITS[0]+=pktSize*8;
	} else {       // Normal treatemant

		if(tArr<STy[0] && tArr>STy[1]) {        // Pkt arrives in the "next" sample.. NORMAL behaviour.
#ifdef debug
			printf("<<PKT arrived Correctly>>");
#endif
			if(tStart>STy[1]) {                  // Packet arrives completely next sample interval, completely since we also know tArr<STy[0]
#ifdef debug
				printf("bin 0 %d bits (ONE).\n",pktSize*8);
#endif
				BITS[0]+=pktSize*8;
			} else {
				if(tStart>STy[2] ){               // Packet started arriving in previous sample interval, so its split inbetween the 'previous' and 'next' intervals.
#ifdef debug
					cout << "bin 0 +" << (tArr-STy[1])*linkCapacity << " bin 1 += " << (STy[1]-tStart)*linkCapacity << " (TWO)" << endl;
#endif
					BITS[0]+=(tArr-STy[1])*linkCapacity;    // Calculate how many bits that will arrive in the next interval.
					BITS[1]+=(STy[1]-tStart)*linkCapacity;  // and how many bits that arrived in the previous.
				} else {                                  // Packet spans more than 3 intervals
					if(ceil(to_double(tTransfer/tSample))>=bins) {  // Calculate how many bins that the packet needs to be completely measured.
						cout << "  [tStart " << tStart << " -- tTrfs " << tTransfer << "  ---- " << tArr << " tT/tS " << tTransfer/tSample << endl;
						printf("Error: pkt to large...\n");      // It needs more than we got, ERROR!! Increase the noBins in the init..
						return;                                  // Each extra bin, allows us to handle packets that are
					}                                            // offset by tSample seconds. Thus 10 extra bins, allows pkts to be offset 10tSample

					bits=pktSize*8.0-(tArr-STy[1])*linkCapacity; // How many bits will remain of the packet after the once that go to
					// next interval has been removed?
					BITS[0]+=(tArr-STy[1])*linkCapacity;         // How many bits will end up in the next interval ?
					j=1;
#ifdef debug
					cout << "bin 0 +" << (tArr-STy[1])*linkCapacity << "( "<< tArr << " - " << STy[1]<< " = " << tArr-STy[1] << ") BIN[" << BITS[0]<< "]"<<endl;
#endif
					while(bits>tSample*linkCapacity){            // While bits>tSample*linkCapacity=MaxIntervalBits of the packet keep filling the intervals.
#ifdef debug                                           // We use MIB to not endup with negative bits in the last interval.
						cout << "bin " << j << "  BIN["<< BITS[j] << "] " << endl;
#endif
						BITS[j]+=tSample*linkCapacity;            // add B bits in interval j
						bits-=tSample*linkCapacity;               // calculate how many bits that remain
#ifdef debug
						cout << " += " << tSample*linkCapacity << " (" << STy[j+1] << " -- "<< STy[j] << " ) BIN[" << BITS[j] << "]" << endl;
#endif
						j++;
					}
#ifdef debug
					cout << "Lbin " << j << " BIN[ " << BITS[j] << "] "<< endl;//The remaining bits goes here. The range of bits should be 0<= bits <MIB
#endif
					if(bits>0.0) {
						BITS[j]+=bits;
					}
					if(bits<0.0) {                                // Just a precausion, and probably an ERROR!
						BITS[j-1]-=(-1.0)*bits;
						cout << "  [tStart " << tStart << " -- tTrfs " << tTransfer << "  ---- " << tArr << " tT/tS " << tTransfer/tSample << endl;
						printf("ERROR: negative bits in the last bin. Multiple bins coverd!!");
						return;
					}
#ifdef debug
					cout << " += " << bits << " ("<< STy[j+1] << " -- "<< STy[j] << " ) BIN[ " << BITS[j] << "]" << endl;

#endif
				}
			}
		}
		if(STy[2]<tArr && tArr<STy[1]) {                    // Packet should have arrived in the previous sample..
			printf("**PKT MISSED IT sample**\n");            // This is the almost the same routines as above, only adjusted to account for the
			// shifted arrival time of the packet.
			if(ceil(to_double(tTransfer/tSample))>=bins-1) {
				printf("Error: Pkt missed by one interval and pkt to large...\n");
				return;
			}

			if(tStart>STy[2]) {                             // Packet arived completely in the previous interval
#ifdef debug
				printf("Full pkt of %d bits in bin 1.\n",pktSize*8);
#endif
				BITS[1]+=pktSize*8;
			} else {
				if(tStart>STy[3] ){                               // Pkt split from previous sample and this
					BITS[1]+=(tArr-STy[2])*linkCapacity;
					BITS[2]+=(STy[2]-tStart)*linkCapacity;
				} else {                                          // Pkt spans more than 3 intervals
					BITS[1]+=(tArr-STy[2])*linkCapacity;
					j=2;
					bits=pktSize*8.0-(tArr-STy[2])*linkCapacity;    // How many bits will remain of the packet after the once that go to
					while(bits>tSample*linkCapacity && j<bins-1){
						BITS[j]+=tSample*linkCapacity;
						bits-=tSample*linkCapacity;                   // calculate how many bits that remain
						j++;
					}
					if(bits>0.0) {
						BITS[j]+=bits;
					}
					if(bits<0.0) {                                  // Just a precausion, and probably an ERROR!
						BITS[j-1]-=(-1.0)*bits;
						cout << "  [tStart " << tStart << " -- tTrfs " << tTransfer << "  ---- " << tArr << " tT/tS " << tTransfer/tSample << endl;
						printf("ERROR: negative bits in the last bin. Multiple bins coverd(Shifted pkt)!!");
						return;
					}
				}
			}
		}
		if(tArr<STy[2]) {                               // Packet is off by two samples.. BIG ERRROR!!
#ifdef debug
			cout << "ERROR: packet (" << tArr <<") arrived " << (STy[1]-tArr)/tSample << " samples to late.\n" << endl;
#endif
		}
	}
#ifdef debug
	printf("<-pktArrival()  ");
#endif
  return;
}

/*
  Created:      2002-12-16 ??:??, Patrik.Carlsson@bth.se
  Latest edit:  2003-02-20 10:30, Patrik.Carlsson@bth.se

  Function:
  void sample(*)
  Return value:
  bitrate estimate at sample ST[N]

  Arguments (In Order):
  Array that contains the number of bits that arrived in a sample interval, defined in ST.
  Array that contains the sample times, ST[0] is the NEXT sample, ST[1] the most recent.
  ST[N] is the sample that is to be "releases"
  Number of samples/bins, i.e. N in the definition above.
  Inter sample time, or 1/fS.
  Link Capacity in bps.

  Description:
  Based on the sample intervals filled in by pktArrival() this routine takes the interval that happend 'earliest' ST[N], and
  calculates the bitrate obtained in this interval. The result, bitEst, is the returned value. Then it shifts both arrays
  this to create a new sample interval at ST[0].



*/


double sampleBINS(qd_real *BITS, qd_real *STy, int bins,double tSample, double linkCapacity) {
  double bitEst;
  int i;
  qd_real tSample2=tSample;
  bitEst=to_double(BITS[bins-1]/tSample2);    // Calculate for the bit rate estimate for the last bin, which cannot get any more samples.

#ifdef debug_sample
  printf("sample()\t at %12.12f \n",STy[0]);
#endif
  if(bitEst/1e6>linkCapacity ||  bitEst<0.0) {
    printf("########bit estimation of %f Mbps\n", bitEst/1e6);
  }
#ifdef debug_bps_estm
  printf("S[%12.12f]bps = %f / %f = %f\n",STy[bins-1],BITS[bins-1],tSample,bitEst);
#endif
  for(i=bins-1;i>0;i--){        // Shift the bins; bin[i]=bin[i-1]
    STy[i]=STy[i-1];
    BITS[i]=BITS[i-1];
  }
  BITS[0]=0.0;                    // Initialize the first bin to zero, and the arrival time of the next sample.
  STy[0]=STy[1]+tSample;

#ifdef debug_sample
  printf("--bins--\n");
  for(i=0;i<bins;i++){
    cout << "[" << i << "]" << setiosflags(ios::fixed) << setprecision(12) << (double)STy[i] << "=  " << (double)BITS[i] << endl;
  }

  cout << " next at " << setiosflags(ios::fixed) << setprecision(12) << (double)STy[0] << "<-sample()" << endl;
#endif


  return bitEst;

}

/*
  Created:      2003-02-20 12:40, Patrik.Carlsson@bth.se
  Latest edit:  2003-02-20 10:30, Patrik.Carlsson@bth.se

  Function:
  int payLoadExtraction(*)
  Return value:
  Payload at given level.

  Arguments (In Order):
  Desired level, 0 physical, 1 link, 2 network, 3 transport
  Description:
  Returns the number of bytes a packet contains at level L



*/
//caphead is new one, data is old.

int payLoadExtraction(int level, char* data)
{
#ifdef debug
  // cout << "->payLoadExtraction(" << level <<", data)" << endl;
#endif

  int payLoadSize=0;
  struct ethhdr *ether=0;
  struct ip *ip_hdr=0;
  struct tcphdr *tcp=0;
  struct udphdr *udp=0;
   cap_header *caphead=(cap_header*)data;

  if(level==0) { payLoadSize=caphead->len; };                             // payload size at physical (ether+network+transport+app)
  ether=(struct ethhdr*)(data +sizeof(cap_header));
  if(level==1) { payLoadSize=caphead->len-sizeof(struct ethhdr); }; // payload size at link  (network+transport+app)

 switch(ntohs(ether->h_proto)) {
  case ETHERTYPE_IP:/* Packet contains an IP, PASS TWO! */
    ip_hdr=(struct ip*)(data +sizeof(cap_header)+sizeof(struct ethhdr));
    if(level==2) { payLoadSize=ntohs(ip_hdr->ip_len)-4*ip_hdr->ip_hl; }; // payload size at network  (transport+app)
    switch(ip_hdr->ip_p) { /* Test what transport protocol is present */
    case IPPROTO_TCP: /* TCP */
      tcp=(struct tcphdr*)(data +sizeof(cap_header)+sizeof(struct ethhdr)+4*ip_hdr->ip_hl);
      if(level==3) { payLoadSize=ntohs(ip_hdr->ip_len)-4*tcp->doff-4*ip_hdr->ip_hl; };  // payload size at transport  (app)
      break;
    case IPPROTO_UDP: /* UDP */
      udp=(struct udphdr*)(data +sizeof(cap_header)+sizeof(struct ethhdr)+4*ip_hdr->ip_hl);
      if(level==3) { payLoadSize=(u_int16_t)(ntohs(udp->len)-8); };                     // payload size at transport  (app)
      break;
    default:
      if(level==3) { payLoadSize=(u_int16_t)(ntohs(udp->len)-8); };                     // payload size at transport  (app)
                       printf("Unknown transport protocol: %d \n", ip_hdr->ip_p);
      break;
   } //End switch(ip_hdr->ip_p)
    break;
  default:      /* Packet contains unknown link . */
    if(level==2) { payLoadSize=ntohs(ip_hdr->ip_len)-4*ip_hdr->ip_hl; };                         // payload size at network  (transport+app)
   // printf("Unknown link type 0x%0x \n", ntohs(ether->h_proto));
   break;
  }//End switch(ntohs(ether->ether_type))

 //payLoadSize=caphead->len;
 // cout << "<-payLoadExtraction:" << payLoadSize << " ." << endl;

  return payLoadSize;
}




//370 in 370 a printf statement is enabled, and in 694 printf is disabled. 364 ret == 0


