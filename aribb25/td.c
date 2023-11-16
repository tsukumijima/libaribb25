#define VERSION_STRING "0.2.9"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <math.h>

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#if defined(_WIN32)
	#include <io.h>
	#include <windows.h>
	#include <crtdbg.h>
	#include <tchar.h>
#else
	#define __STDC_FORMAT_MACROS
	#define TCHAR char
	#define _T(X) X
	#define _ftprintf fprintf
	#define _ttoi atoi
	#define _tmain main
	#define _topen _open
	#include <unistd.h>
	#include <sys/time.h>
#endif

#include "arib_std_b25.h"
#include "arib_std_b25_error_code.h"
#include "b_cas_card.h"

typedef struct {
	int32_t round;
	int32_t strip;
	int32_t emm;
	int32_t verbose;
	int32_t power_ctrl;
	int32_t simd_instruction;
	int32_t benchmark;
} OPTION;

static void show_usage();
static int parse_arg(OPTION *dst, int argc, TCHAR **argv);
#ifdef ENABLE_ARIB_STREAM_TEST
static void test_arib_std_b25(OPTION *opt);
#else
static void test_arib_std_b25(const TCHAR *src, const TCHAR *dst, OPTION *opt);
#endif
static void show_bcas_power_on_control_info(B_CAS_CARD *bcas);
static void run_multi2_benchmark_test(OPTION *opt);

int _tmain(int argc, TCHAR **argv)
{
	int n;
	OPTION opt;

#if defined(_WIN32)
	_CrtSetReportMode( _CRT_WARN, _CRTDBG_MODE_FILE );
	_CrtSetReportFile( _CRT_WARN, _CRTDBG_FILE_STDOUT );
	_CrtSetReportMode( _CRT_ERROR, _CRTDBG_MODE_FILE );
	_CrtSetReportFile( _CRT_ERROR, _CRTDBG_FILE_STDOUT );
	_CrtSetReportMode( _CRT_ASSERT, _CRTDBG_MODE_FILE );
	_CrtSetReportFile( _CRT_ASSERT, _CRTDBG_FILE_STDOUT );
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF|_CRTDBG_DELAY_FREE_MEM_DF|_CRTDBG_CHECK_ALWAYS_DF|_CRTDBG_LEAK_CHECK_DF);
#endif

	n = parse_arg(&opt, argc, argv);
#ifdef ENABLE_ARIB_STREAM_TEST
	if(n != argc && opt.benchmark == 0){
#else
	if(n+2 > argc && opt.benchmark == 0){
#endif
		show_usage();
		exit(EXIT_FAILURE);
	}
	if(opt.benchmark == 1){
		run_multi2_benchmark_test(&opt);
		exit(EXIT_FAILURE);
	}

#ifdef ENABLE_ARIB_STREAM_TEST
	test_arib_std_b25(&opt);
#else
	for(;n<=(argc-2);n+=2){
		test_arib_std_b25(argv[n+0], argv[n+1], &opt);
	}
#endif

#if defined(_WIN32)
	_CrtDumpMemoryLeaks();
#endif

	return EXIT_SUCCESS;
}

static void show_usage()
{
#ifdef ENABLE_ARIB_STREAM_TEST
// arib-b1-stream-test・arib-b25-stream-test
#ifdef ENABLE_ARIB_STD_B1
	_ftprintf(stderr, _T("arib-b1-stream-test - ARIB STD-B1 test program version %s\n"), _T(VERSION_STRING));
	_ftprintf(stderr, _T("usage: arib-b1-stream-test [options] \n"));
#else
	_ftprintf(stderr, _T("arib-b25-stream-test - ARIB STD-B25 test program version %s\n"), _T(VERSION_STRING));
	_ftprintf(stderr, _T("usage: arib-b25-stream-test [options] \n"));
#endif
#else
// b1・b25
#ifdef ENABLE_ARIB_STD_B1
	_ftprintf(stderr, _T("b1 - ARIB STD-B1 test program version %s\n"), _T(VERSION_STRING));
	_ftprintf(stderr, _T("usage: b1 [options] src.m2t dst.m2t [more pair ..]\n"));
#else
	_ftprintf(stderr, _T("b25 - ARIB STD-B25 test program version %s\n"), _T(VERSION_STRING));
	_ftprintf(stderr, _T("usage: b25 [options] src.m2t dst.m2t [more pair ..]\n"));
#endif
#endif
	_ftprintf(stderr, _T("options:\n"));
	_ftprintf(stderr, _T("  -r round (integer, default=4)\n"));
	_ftprintf(stderr, _T("  -s strip\n"));
	_ftprintf(stderr, _T("     0: keep null(padding) stream (default)\n"));
	_ftprintf(stderr, _T("     1: strip null stream\n"));
// EMM・通電制御情報は未サポート
#ifndef ENABLE_ARIB_STD_B1
	_ftprintf(stderr, _T("  -m EMM\n"));
	_ftprintf(stderr, _T("     0: ignore EMM (default)\n"));
	_ftprintf(stderr, _T("     1: send EMM to B-CAS card\n"));
	_ftprintf(stderr, _T("  -p power_on_control_info\n"));
	_ftprintf(stderr, _T("     0: do nothing additionally\n"));
	_ftprintf(stderr, _T("     1: show B-CAS EMM receiving request (default)\n"));
#endif
	_ftprintf(stderr, _T("  -v verbose\n"));
	_ftprintf(stderr, _T("     0: silent\n"));
	_ftprintf(stderr, _T("     1: show processing status (default)\n"));
#ifdef ENABLE_MULTI2_SIMD
	_ftprintf(stderr, _T("  -i instruction\n"));
	_ftprintf(stderr, _T("     0: use no SIMD instruction\n"));
	_ftprintf(stderr, _T("     1: use SSE2 instruction if available\n"));
	_ftprintf(stderr, _T("     2: use SSSE3 instruction if available\n"));
	_ftprintf(stderr, _T("     3: use AVX2 instruction if available (default)\n"));
	_ftprintf(stderr, _T("  -b MULTI2 benchmark test for SIMD\n"));
#endif
	_ftprintf(stderr, _T("\n"));
}

static int parse_arg(OPTION *dst, int argc, TCHAR **argv)
{
	int i;

	dst->round = 4;
	dst->strip = 0;
	dst->emm = 0;
#ifdef ENABLE_ARIB_STD_B1
	dst->power_ctrl = 0;
#else
	dst->power_ctrl = 1;
#endif
	dst->verbose = 1;
	dst->simd_instruction = 3;
	dst->benchmark = 0;

	for(i=1;i<argc;i++){
		if(argv[i][0] != '-'){
			break;
		}
		switch(argv[i][1]){
// EMM・通電制御情報は未サポート
#ifndef ENABLE_ARIB_STD_B1
		case 'm':
			if(argv[i][2]){
				dst->emm = _ttoi(argv[i]+2);
			}else{
				dst->emm = _ttoi(argv[i+1]);
				i += 1;
			}
			break;
		case 'p':
			if(argv[i][2]){
				dst->power_ctrl = _ttoi(argv[i]+2);
			}else{
				dst->power_ctrl = _ttoi(argv[i+1]);
				i += 1;
			}
			break;
#endif
		case 'r':
			if(argv[i][2]){
				dst->round = _ttoi(argv[i]+2);
			}else{
				dst->round = _ttoi(argv[i+1]);
				i += 1;
			}
			break;
		case 's':
			if(argv[i][2]){
				dst->strip = _ttoi(argv[i]+2);
			}else{
				dst->strip = _ttoi(argv[i+1]);
				i += 1;
			}
			break;
		case 'v':
			if(argv[i][2]){
				dst->verbose = _ttoi(argv[i]+2);
			}else{
				dst->verbose = _ttoi(argv[i+1]);
				i += 1;
			}
			break;
#ifdef ENABLE_MULTI2_SIMD
		case 'i':
			if(argv[i][2]){
				dst->simd_instruction = _ttoi(argv[i]+2);
			}else{
				dst->simd_instruction = _ttoi(argv[i+1]);
				i += 1;
			}
			break;
		case 'b':
			dst->benchmark = 1;
			break;
#endif
		default:
			_ftprintf(stderr, _T("error - unknown option '-%c'\n"), argv[i][1]);
#ifdef ENABLE_ARIB_STREAM_TEST
			return -1;  // show usage
#else
			return argc;
#endif
		}
	}

	return i;
}

#ifdef ENABLE_ARIB_STREAM_TEST
static void test_arib_std_b25(OPTION *opt)
#else
static void test_arib_std_b25(const TCHAR *src, const TCHAR *dst, OPTION *opt)
#endif
{
#ifdef ENABLE_ARIB_STREAM_TEST
	int code,i,n;
#else
	int code,i,n,m;
#endif
	int sfd,dfd;

#ifndef ENABLE_ARIB_STREAM_TEST
	int64_t total;
#endif
	int32_t offset;
#if defined(_WIN32)
	unsigned long tick,tock;
#else
	struct timeval tick,tock;
	double millisec;
#endif
	double mbps;

	ARIB_STD_B25 *b25;
	B_CAS_CARD   *bcas;

	ARIB_STD_B25_PROGRAM_INFO pgrm;

	uint8_t data[64*1024];
	uint8_t *_data;

	ARIB_STD_B25_BUFFER sbuf;
	ARIB_STD_B25_BUFFER dbuf;

#ifdef ENABLE_ARIB_STREAM_TEST
	sfd = 0; // stdin
	dfd = 1; // stdout
#else
	sfd = -1;
	dfd = -1;
#endif
	b25 = NULL;
	bcas = NULL;
	_data = NULL;

#ifdef ENABLE_ARIB_STREAM_TEST
#if defined(_WIN32)
	_setmode(_fileno(stdin), _O_BINARY);
	_setmode(_fileno(stdout), _O_BINARY);
#endif
#else
	sfd = _topen(src, _O_BINARY|_O_RDONLY|_O_SEQUENTIAL);
	if(sfd < 0){
		_ftprintf(stderr, _T("error - failed on _open(%s) [src]\n"), src);
		goto LAST;
	}

	_lseeki64(sfd, 0, SEEK_END);
	total = _telli64(sfd);
	_lseeki64(sfd, 0, SEEK_SET);
#endif

	b25 = create_arib_std_b25();
	if(b25 == NULL){
		_ftprintf(stderr, _T("error - failed on create_arib_std_b25()\n"));
		goto LAST;
	}

	code = b25->set_multi2_round(b25, opt->round);
	if(code < 0){
		_ftprintf(stderr, _T("error - failed on ARIB_STD_B25::set_multi2_round() : code=%d\n"), code);
		goto LAST;
	}

	code = b25->set_strip(b25, opt->strip);
	if(code < 0){
		_ftprintf(stderr, _T("error - failed on ARIB_STD_B25::set_strip() : code=%d\n"), code);
		goto LAST;
	}

	code = b25->set_emm_proc(b25, opt->emm);
	if(code < 0){
		_ftprintf(stderr, _T("error - failed on ARIB_STD_B25::set_emm_proc() : code=%d\n"), code);
		goto LAST;
	}

#ifdef ENABLE_MULTI2_SIMD
	code = b25->set_simd_mode(b25, opt->simd_instruction);
	if(code < 0){
		_ftprintf(stderr, _T("error - failed on ARIB_STD_B25::set_simd_mode() : code=%d\n"), code);
		goto LAST;
	}
#endif

	bcas = create_b_cas_card();
	if(bcas == NULL){
		_ftprintf(stderr, _T("error - failed on create_b_cas_card()\n"));
		goto LAST;
	}

	code = bcas->init(bcas);
	if(code < 0){
		_ftprintf(stderr, _T("error - failed on B_CAS_CARD::init() : code=%d\n"), code);
		goto LAST;
	}

	code = b25->set_b_cas_card(b25, bcas);
	if(code < 0){
		_ftprintf(stderr, _T("error - failed on ARIB_STD_B25::set_b_cas_card() : code=%d\n"), code);
		goto LAST;
	}

#ifndef ENABLE_ARIB_STREAM_TEST
	dfd = _topen(dst, _O_BINARY|_O_WRONLY|_O_SEQUENTIAL|_O_CREAT|_O_TRUNC, _S_IREAD|_S_IWRITE);
	if(dfd < 0){
		_ftprintf(stderr, _T("error - failed on _open(%s) [dst]\n"), dst);
		goto LAST;
	}
#endif

	offset = 0;
#if defined(_WIN32)
	tock = GetTickCount();
#else
	gettimeofday(&tock, NULL);
#endif
	while( (n = _read(sfd, data, sizeof(data))) > 0 ){
		sbuf.data = data;
		sbuf.size = n;

		code = b25->put(b25, &sbuf);
		if(code < 0){
			_ftprintf(stderr, _T("error - failed on ARIB_STD_B25::put() : code=%d\n"), code);
			dbuf.data = data;
			dbuf.size = n;
			if(code < ARIB_STD_B25_ERROR_NO_ECM_IN_HEAD_32M){
				uint8_t *p = NULL;
				b25->withdraw(b25, &sbuf);
				if(sbuf.size > 0){
					if(_data != NULL){
						free(_data);
						_data = NULL;
					}
					p = (uint8_t *)malloc(sbuf.size + n);
				}
				if(p != NULL){
					memcpy(p, sbuf.data, sbuf.size);
					memcpy(p + sbuf.size, data, n);
					dbuf.data = p;
					dbuf.size = sbuf.size + n;
					_data = p;
				}
			}
		}else{
			code = b25->get(b25, &dbuf);
			if(code < 0){
				_ftprintf(stderr, _T("error - failed on ARIB_STD_B25::get() : code=%d\n"), code);
				goto LAST;
			}
		}

		if(dbuf.size > 0){
			n = _write(dfd, dbuf.data, dbuf.size);
			if(n != dbuf.size){
				_ftprintf(stderr, _T("error failed on _write(%d)\n"), dbuf.size);
				goto LAST;
			}
		}

		offset += sbuf.size;
		if(opt->verbose != 0){
#ifndef ENABLE_ARIB_STREAM_TEST
			m = (int)(10000ULL*offset/total);
#endif
			mbps = 0.0;
#if defined(_WIN32)
			tick = GetTickCount();
			if (tick-tock > 100) {
				mbps = offset;
				mbps /= 1024;
				mbps /= (tick-tock);
			}
#else
			gettimeofday(&tick, NULL);
			millisec = (tick.tv_sec - tock.tv_sec) * 1000;
			millisec += (tick.tv_usec - tock.tv_usec) / 1000;
			if(millisec > 100.0) {
				mbps = offset;
				mbps /= 1024;
				mbps /= millisec;
			}
#endif
#ifdef ENABLE_ARIB_STREAM_TEST
			_ftprintf(stderr, _T("\rprocessing: %6.2f MB/sec"), mbps);
#else
			_ftprintf(stderr, _T("\rprocessing: %2d.%02d%% [%6.2f MB/sec]"), m/100, m%100, mbps);
#endif
		}
	}

	code = b25->flush(b25);
	if(code < 0){
		_ftprintf(stderr, _T("error - failed on ARIB_STD_B25::flush() : code=%d\n"), code);
		goto LAST;
	}

	code = b25->get(b25, &dbuf);
	if(code < 0){
		_ftprintf(stderr, _T("error - failed on ARIB_STD_B25::get() : code=%d\n"), code);
		goto LAST;
	}

	if(dbuf.size > 0){
		n = _write(dfd, dbuf.data, dbuf.size);
		if(n != dbuf.size){
			_ftprintf(stderr, _T("error - failed on _write(%d)\n"), dbuf.size);
			goto LAST;
		}
	}

	if(opt->verbose != 0){
		mbps = 0.0;
#if defined(_WIN32)
		tick = GetTickCount();
		if(tick-tock > 100){
			mbps = offset;
			mbps /= 1024;
			mbps /= (tick-tock);
		}
#else
		gettimeofday(&tick, NULL);
		millisec = (tick.tv_sec - tock.tv_sec) * 1000;
		millisec += (tick.tv_usec - tock.tv_usec) / 1000;
		if(millisec > 100.0){
			mbps = offset;
			mbps /= 1024;
			mbps /= millisec;
		}
#endif
		_ftprintf(stderr, _T("\rprocessing: finish  [%6.2f MB/sec]\n"), mbps);
		fflush(stderr);
		fflush(stdout);
	}

	n = b25->get_program_count(b25);
	if(n < 0){
		_ftprintf(stderr, _T("error - failed on ARIB_STD_B25::get_program_count() : code=%d\n"), code);
		goto LAST;
	}
	for(i=0;i<n;i++){
		code = b25->get_program_info(b25, &pgrm, i);
		if(code < 0){
			_ftprintf(stderr, _T("error - failed on ARIB_STD_B25::get_program_info(%d) : code=%d\n"), i, code);
			goto LAST;
		}
		if(pgrm.ecm_unpurchased_count > 0){
			_ftprintf(stderr, _T("warning - unpurchased ECM is detected\n"));
			_ftprintf(stderr, _T("  channel:               %d\n"), pgrm.program_number);
			_ftprintf(stderr, _T("  unpurchased ECM count: %d\n"), pgrm.ecm_unpurchased_count);
			_ftprintf(stderr, _T("  last ECM error code:   %04x\n"), pgrm.last_ecm_error_code);
			_ftprintf(stderr, _T("  undecrypted TS packet: %" PRId64 "\n"), pgrm.undecrypted_packet_count);
			_ftprintf(stderr, _T("  total TS packet:       %" PRId64 "\n"), pgrm.total_packet_count);
		}
	}

	if(opt->power_ctrl != 0){
		show_bcas_power_on_control_info(bcas);
	}

LAST:

	if(_data != NULL){
		free(_data);
		_data = NULL;
	}

#ifndef ENABLE_ARIB_STREAM_TEST
	if(sfd >= 0){
		_close(sfd);
		sfd = -1;
	}

	if(dfd >= 0){
		_close(dfd);
		dfd = -1;
	}
#endif

	if(b25 != NULL){
		b25->release(b25);
		b25 = NULL;
	}

	if(bcas != NULL){
		bcas->release(bcas);
		bcas = NULL;
	}
}

static void show_bcas_power_on_control_info(B_CAS_CARD *bcas)
{
	int code;
	int i,w;
	B_CAS_PWR_ON_CTRL_INFO pwc;

	code = bcas->get_pwr_on_ctrl(bcas, &pwc);
	if(code < 0){
		_ftprintf(stderr, _T("error - failed on B_CAS_CARD::get_pwr_on_ctrl() : code=%d\n"), code);
		return;
	}

	if(pwc.count == 0){
		_ftprintf(stderr, _T("no EMM receiving request\n"));
		return;
	}

	_ftprintf(stderr, _T("total %d EMM receiving request\n"), pwc.count);
	for(i=0;i<pwc.count;i++){
		_ftprintf(stderr, _T("+ [%d] : tune "), i);
		switch(pwc.data[i].network_id){
		case 4:
			w = pwc.data[i].transport_id;
			_ftprintf(stderr, _T("BS-%d/TS-%d "), ((w >> 4) & 0x1f), (w & 7));
			break;
		case 6:
		case 7:
			w = pwc.data[i].transport_id;
			_ftprintf(stderr, _T("ND-%d/TS-%d "), ((w >> 4) & 0x1f), (w & 7));
			break;
		default:
			_ftprintf(stderr, _T("unknown(b:0x%02x,n:0x%04x,t:0x%04x) "), pwc.data[i].broadcaster_group_id, pwc.data[i].network_id, pwc.data[i].transport_id);
			break;
		}
		_ftprintf(stderr, _T("between %04d %02d/%02d "), pwc.data[i].s_yy, pwc.data[i].s_mm, pwc.data[i].s_dd);
		_ftprintf(stderr, _T("to %04d %02d/%02d "), pwc.data[i].l_yy, pwc.data[i].l_mm, pwc.data[i].l_dd);
		_ftprintf(stderr, _T("least %d hours\n"), pwc.data[i].hold_time);
	}
}

#ifdef USE_BENCHMARK
#define BENCHMARK_ROUND 200000
#define MAX_INSTRUCTION 4
static void run_multi2_benchmark_test(OPTION *opt)
{
	int code;

	const TCHAR *INSTRUCTION_NAMES[MAX_INSTRUCTION] = {_T("normal"), _T("SSE2"), _T("SSSE3"), _T("AVX2")};
	int64_t totals[MAX_INSTRUCTION];
	int64_t base_time;
	int64_t max_time;
	int32_t time_percentage;
	int32_t supported_mode;
	uint32_t test_count;

	ARIB_STD_B25 *b25;

	b25 = create_arib_std_b25();
	if(b25 == NULL){
		_ftprintf(stderr, _T("error - failed on create_arib_std_b25()\n"));
		goto LAST;
	}
	supported_mode = b25->get_simd_mode(b25);

	code = b25->set_multi2_round(b25, opt->round);
	if(code < 0){
		_ftprintf(stderr, _T("error - failed on ARIB_STD_B25::set_multi2_round() : code=%d\n"), code);
		goto LAST;
	}

	code = b25->set_strip(b25, opt->strip);
	if(code < 0){
		_ftprintf(stderr, _T("error - failed on ARIB_STD_B25::set_strip() : code=%d\n"), code);
		goto LAST;
	}

	_ftprintf(stderr, _T("running - MULTI2 benchmark test\n"));

	memset(totals, 0, sizeof(totals));
	max_time = INT64_MIN;
	test_count = 0;
	do{
		for(int32_t i=0;i<MAX_INSTRUCTION;i++){
			code = test_multi2_decryption(b25, &totals[i], i, BENCHMARK_ROUND);
			if(code >= 0){
				if(totals[i] > max_time){
					max_time = totals[i];
				}
			}
		}
		test_count += BENCHMARK_ROUND;
	}while(max_time < 1500);

	_ftprintf(stderr, _T("complete - MULTI2 benchmark test (count=%u)\n"), test_count);
	base_time = totals[0];
	for(int32_t i=0;i<MAX_INSTRUCTION;i++){
		_ftprintf(stderr, _T("  %-6s: %5" PRId64 " ms [%8d packets/s]"), INSTRUCTION_NAMES[i], totals[i], (int32_t)round(test_count*1000LL/(double)totals[i]));
		if(i == 0){
			_ftprintf(stderr, _T("\n"));
			continue;
		}
		if(totals[i] < base_time){
			time_percentage =  (int32_t)(base_time*100/totals[i]) - 100;
		}else{
			time_percentage = -(int32_t)(totals[i]*100/base_time) - 100;
		}

		_ftprintf(stderr, _T(" (%3d%% faster)\n"), time_percentage);
	}

LAST:

	if(b25 != NULL){
		b25->release(b25);
		b25 = NULL;
	}
}
#else
static void run_multi2_benchmark_test(OPTION *opt)
{
	_ftprintf(stderr, _T("MULTI2 benchmark test is disabled\n"));
}
#endif
