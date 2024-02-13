#include "b_cas_card.h"
#include "b_cas_card_error_code.h"

#include <stdlib.h>
#include <string.h>

#include <math.h>

#include <winscard.h>
#if defined(_WIN32)
#  include <windows.h>
#  include <tchar.h>
#else
#  define TCHAR char
#  if !defined(__CYGWIN__)
#    include <wintypes.h>
#  endif
#  if defined(DEBUG)
#    include <stdio.h>
#  endif
#  define _tcslen strlen
#  define _tcscmp strcmp
#  define _T(x) x
#endif

#if defined(_WIN32)
	// ref: https://stackoverflow.com/a/6924293/17124142
	EXTERN_C IMAGE_DOS_HEADER __ImageBase;
#endif

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 inner structures
 ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
typedef struct {

	SCARDCONTEXT            mng;
	SCARDHANDLE             card;

	uint8_t                *pool;
	LPTSTR                  reader;

	uint8_t                *sbuf;
	uint8_t                *rbuf;

	B_CAS_INIT_STATUS       stat;

	B_CAS_ID                id;
	int32_t                 id_max;

	B_CAS_PWR_ON_CTRL_INFO  pwc;
	int32_t                 pwc_max;

} B_CAS_CARD_PRIVATE_DATA;

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 constant values
 ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
static const uint8_t INITIAL_SETTING_CONDITIONS_CMD[] = {
#ifdef ENABLE_ARIB_STD_B1
	// CLA・INSを修正
	0x80, 0x5e, 0x00, 0x00, 0x00,
#else
	0x90, 0x30, 0x00, 0x00, 0x00,
#endif
};

static const uint8_t CARD_ID_INFORMATION_ACQUIRE_CMD[] = {
#ifdef ENABLE_ARIB_STD_B1
	// CLA・INSを修正
	0x80, 0x5e, 0x00, 0x00, 0x00,
#else
	0x90, 0x32, 0x00, 0x00, 0x00,
#endif
};

static const uint8_t POWER_ON_CONTROL_INFORMATION_REQUEST_CMD[] = {
	0x90, 0x80, 0x00, 0x00, 0x01, 0x00, 0x00,
};

static const uint8_t ECM_RECEIVE_CMD_HEADER[] = {
#ifdef ENABLE_ARIB_STD_B1
	// CLAを修正
	0x80, 0x34, 0x00, 0x00,
#else
	0x90, 0x34, 0x00, 0x00,
#endif
};

static const uint8_t EMM_RECEIVE_CMD_HEADER[] = {
#ifdef ENABLE_ARIB_STD_B1
	// CLAを修正
	0x80, 0x36, 0x00, 0x00,
#else
	0x90, 0x36, 0x00, 0x00,
#endif
};

#define B_CAS_BUFFER_MAX (4*1024)

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 function prototypes (interface method)
 ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
static void release_b_cas_card(void *bcas);
static int init_b_cas_card(void *bcas);
static int init_b_cas_card_with_name(void *bcas, const char * card_reader_name);
static int get_init_status_b_cas_card(void *bcas, B_CAS_INIT_STATUS *stat);
static int get_id_b_cas_card(void *bcas, B_CAS_ID *dst);
static int get_pwr_on_ctrl_b_cas_card(void *bcas, B_CAS_PWR_ON_CTRL_INFO *dst);
static int proc_ecm_b_cas_card(void *bcas, B_CAS_ECM_RESULT *dst, uint8_t *src, int len);
static int proc_emm_b_cas_card(void *bcas, uint8_t *src, int len);

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 global function implementation
 ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
B_CAS_CARD *create_b_cas_card(void)
{
	int n;

	B_CAS_CARD *r;
	B_CAS_CARD_PRIVATE_DATA *prv;

	n = sizeof(B_CAS_CARD) + sizeof(B_CAS_CARD_PRIVATE_DATA);
	prv = (B_CAS_CARD_PRIVATE_DATA *)calloc(1, n);
	if(prv == NULL){
		return NULL;
	}

	r = (B_CAS_CARD *)(prv+1);

	r->private_data = prv;

	r->release = release_b_cas_card;
	r->init = init_b_cas_card;
	r->get_init_status = get_init_status_b_cas_card;
	r->get_id = get_id_b_cas_card;
	r->get_pwr_on_ctrl = get_pwr_on_ctrl_b_cas_card;
	r->proc_ecm = proc_ecm_b_cas_card;
	r->proc_emm = proc_emm_b_cas_card;

	return r;
}

static char pattern[1024] = "";
int override_card_reader_name_pattern(const char * name) {
	if (_tcslen(name) > 0 && _tcslen(name) < 1024) {
		strcpy(pattern, name);
		return 0;
	} else {
		return -1;
	}
}


/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 function prototypes (private method)
 ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
static B_CAS_CARD_PRIVATE_DATA *private_data(void *bcas);
static void teardown(B_CAS_CARD_PRIVATE_DATA *prv);
static int change_id_max(B_CAS_CARD_PRIVATE_DATA *prv, int max);
static int change_pwc_max(B_CAS_CARD_PRIVATE_DATA *prv, int max);
static int connect_card(B_CAS_CARD_PRIVATE_DATA *prv, LPCTSTR reader_name);
static void extract_power_on_ctrl_response(B_CAS_PWR_ON_CTRL *dst, uint8_t *src);
static void extract_mjd(int *yy, int *mm, int *dd, int mjd);
static int setup_ecm_receive_command(uint8_t *dst, uint8_t *src, int len);
static int setup_emm_receive_command(uint8_t *dst, uint8_t *src, int len);
static int32_t load_be_uint16(uint8_t *p);
static int64_t load_be_uint48(uint8_t *p);

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 interface method implementation
 ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
static void release_b_cas_card(void *bcas)
{
	B_CAS_CARD_PRIVATE_DATA *prv;

	prv = private_data(bcas);
	if(prv == NULL){
		/* do nothing */
		return;
	}

	teardown(prv);
	free(prv);
}

static int init_b_cas_card(void *bcas)
{
#if defined(_WIN32)
	// この dll/exe と拡張子なしファイル名が同じ ini ファイルのパスを取得
	// ini ファイルは以下のような構造
	// [CardReader]
	// Name=SCM Microsystems Inc. SCR33x USB Smart Card Reader 0
	TCHAR ini_file_path[MAX_PATH];
	GetModuleFileName((HINSTANCE)&__ImageBase, ini_file_path, MAX_PATH);
	_tcscpy_s(_tcsrchr(ini_file_path, _T('.')), MAX_PATH, _T(".ini"));

	OutputDebugString(TEXT("libaribb25: ini file path:"));
	OutputDebugString(ini_file_path);

	// card_reader_name に GetPrivateProfileString() で取得したカードリーダー名を入れる
	// ini ファイルや値がないなどカードリーダー名を取得できなかった場合は、card_reader_name はNULLになる
	TCHAR *card_reader_name;
	card_reader_name = (TCHAR *)malloc(1024);
	GetPrivateProfileString(_T("CardReader"), _T("Name"), _T(""), card_reader_name, 1024, ini_file_path);

	if(card_reader_name == NULL){
		OutputDebugString(TEXT("libaribb25: no card reader name specified in ini file."));
	} else {
		OutputDebugString(TEXT("libaribb25: specified card reader name:"));
		OutputDebugString(card_reader_name);
	}
#endif

	if (pattern != NULL && _tcslen(pattern) > 0 && _tcslen(pattern) < 1024) {
		return init_b_cas_card_with_name(bcas, pattern);
	}
#if defined(_WIN32)
	else if (card_reader_name != NULL && _tcslen(card_reader_name) > 0 && _tcslen(card_reader_name) < 1024) {
		int code = init_b_cas_card_with_name(bcas, card_reader_name);
		free(card_reader_name);
		return code;
	}
#endif
	else {
		return init_b_cas_card_with_name(bcas, "");
	}
}

static int init_b_cas_card_with_name(void *bcas, const char * card_reader_name)
{
	int m;
	long ret;
	unsigned long len;

	B_CAS_CARD_PRIVATE_DATA *prv;

	prv = private_data(bcas);
	if(prv == NULL){
		return B_CAS_CARD_ERROR_INVALID_PARAMETER;
	}

	teardown(prv);

	ret = SCardEstablishContext(SCARD_SCOPE_USER, NULL, NULL, &(prv->mng));
	if(ret != SCARD_S_SUCCESS){
		return B_CAS_CARD_ERROR_NO_SMART_CARD_READER;
	}

	ret = SCardListReaders(prv->mng, NULL, NULL, &len);
	if(ret != SCARD_S_SUCCESS){
		return B_CAS_CARD_ERROR_NO_SMART_CARD_READER;
	}
	len += 256;

	m = (sizeof(TCHAR)*len) + (2*B_CAS_BUFFER_MAX) + (sizeof(int64_t)*16) + (sizeof(B_CAS_PWR_ON_CTRL)*16);
	prv->pool = (uint8_t *)malloc(m);
	if(prv->pool == NULL){
		return B_CAS_CARD_ERROR_NO_ENOUGH_MEMORY;
	}

	prv->reader = (LPTSTR)(prv->pool);
	prv->sbuf = prv->pool + len;
	prv->rbuf = prv->sbuf + B_CAS_BUFFER_MAX;
	prv->id.data = (int64_t *)(prv->rbuf + B_CAS_BUFFER_MAX);
	prv->id_max = 16;
	prv->pwc.data = (B_CAS_PWR_ON_CTRL *)(prv->id.data + prv->id_max);
	prv->pwc_max = 16;

	ret = SCardListReaders(prv->mng, NULL, prv->reader, &len);
	if(ret != SCARD_S_SUCCESS){
		return B_CAS_CARD_ERROR_NO_SMART_CARD_READER;
	}

	while( prv->reader[0] != 0 ){

#if defined(_WIN32)
		OutputDebugString(TEXT("libaribb25: detected card reader name:"));
		OutputDebugString(prv->reader);
#elif defined(DEBUG)
		fprintf(stderr, "libaribb25: detected card reader name:\n");
		fprintf(stderr, "%.1024s\n", prv->reader);
#endif

		// 取得したカードリーダー名のカードリーダーなら接続を試みる
		// もしカードリーダー名が空文字列ならすべてのカードリーダーに接続を試み、最初に見つかったカードリーダーに接続する
		if(_tcscmp(card_reader_name, prv->reader) == 0 || _tcscmp(card_reader_name, _T("")) == 0){
			if(connect_card(prv, prv->reader)){
#if defined(_WIN32)
				OutputDebugString(TEXT("libaribb25: connected card reader name:"));
				OutputDebugString(prv->reader);
#elif defined(DEBUG)
				fprintf(stderr, "libaribb25: connected card reader name:\n");
				fprintf(stderr, "%.1024s\n", prv->reader);
#endif
				break;
			} else {
#if defined(_WIN32)
				OutputDebugString(TEXT("libaribb25: failed to connect card reader name:"));
				OutputDebugString(prv->reader);
#elif defined(DEBUG)
				fprintf(stderr, "libaribb25: failed to connect card reader name:\n");
				fprintf(stderr, "%.1024s\n", prv->reader);
#endif
			}
		}

		prv->reader += (_tcslen(prv->reader) + 1);
	}

	if(prv->card == 0){
#if defined(_WIN32)
		OutputDebugString(TEXT("libaribb25: all the attempts failed."));
#elif defined(DEBUG)
		fprintf(stderr, "libaribb25: all the attempts failed.\n");
#endif
		return B_CAS_CARD_ERROR_ALL_READERS_CONNECTION_FAILED;
	}

	return 0;
}

static int get_init_status_b_cas_card(void *bcas, B_CAS_INIT_STATUS *stat)
{
	B_CAS_CARD_PRIVATE_DATA *prv;

	prv = private_data(bcas);
	if( (prv == NULL) || (stat == NULL) ){
		return B_CAS_CARD_ERROR_INVALID_PARAMETER;
	}

	if(prv->card == 0){
		return B_CAS_CARD_ERROR_NOT_INITIALIZED;
	}

	memcpy(stat, &(prv->stat), sizeof(B_CAS_INIT_STATUS));

	return 0;
}

static int get_id_b_cas_card(void *bcas, B_CAS_ID *dst)
{
	long ret;

	unsigned long slen;
	unsigned long rlen;

	int i,num;

	uint8_t *p;
	uint8_t *tail;

	B_CAS_CARD_PRIVATE_DATA *prv;

	prv = private_data(bcas);
	if( (prv == NULL) || (dst == NULL) ){
		return B_CAS_CARD_ERROR_INVALID_PARAMETER;
	}

	if(prv->card == 0){
		return B_CAS_CARD_ERROR_NOT_INITIALIZED;
	}

	slen = sizeof(CARD_ID_INFORMATION_ACQUIRE_CMD);
	memcpy(prv->sbuf, CARD_ID_INFORMATION_ACQUIRE_CMD, slen);
	rlen = B_CAS_BUFFER_MAX;

	ret = SCardTransmit(prv->card, SCARD_PCI_T1, prv->sbuf, slen, NULL, prv->rbuf, &rlen);
	if( (ret != SCARD_S_SUCCESS) || (rlen < 19) ){
		return B_CAS_CARD_ERROR_TRANSMIT_FAILED;
	}

	p = prv->rbuf + 6;
	tail = prv->rbuf + rlen;
	if( p+1 > tail ){
		return B_CAS_CARD_ERROR_TRANSMIT_FAILED;
	}

	num = p[0];
	if(num > prv->id_max){
		if(change_id_max(prv, num+4) < 0){
			return B_CAS_CARD_ERROR_NO_ENOUGH_MEMORY;
		}
	}

	p += 1;
	for(i=0;i<num;i++){
		if( p+10 > tail ){
			return B_CAS_CARD_ERROR_TRANSMIT_FAILED;
		}

		prv->id.data[i] = load_be_uint48(p+2);
		p += 10;
	}

	prv->id.count = num;

	memcpy(dst, &(prv->id), sizeof(B_CAS_ID));

	return 0;
}

static int get_pwr_on_ctrl_b_cas_card(void *bcas, B_CAS_PWR_ON_CTRL_INFO *dst)
{
#ifdef ENABLE_ARIB_STD_B1
	// 通電制御情報取得は未サポート
	return B_CAS_CARD_ERROR_INVALID_PARAMETER;
#endif

	long ret;

	unsigned long slen;
	unsigned long rlen;

	int i,num,code;

	B_CAS_CARD_PRIVATE_DATA *prv;

	memset(dst, 0, sizeof(B_CAS_PWR_ON_CTRL_INFO));

	prv = private_data(bcas);
	if( (prv == NULL) || (dst == NULL) ){
		return B_CAS_CARD_ERROR_INVALID_PARAMETER;
	}

	if(prv->card == 0){
		return B_CAS_CARD_ERROR_NOT_INITIALIZED;
	}

	slen = sizeof(POWER_ON_CONTROL_INFORMATION_REQUEST_CMD);
	memcpy(prv->sbuf, POWER_ON_CONTROL_INFORMATION_REQUEST_CMD, slen);
	prv->sbuf[5] = 0;
	rlen = B_CAS_BUFFER_MAX;

	ret = SCardTransmit(prv->card, SCARD_PCI_T1, prv->sbuf, slen, NULL, prv->rbuf, &rlen);
	if( (ret != SCARD_S_SUCCESS) || (rlen < 18) || (prv->rbuf[6] != 0) ){
		return B_CAS_CARD_ERROR_TRANSMIT_FAILED;
	}

	code = load_be_uint16(prv->rbuf+4);
	if(code == 0xa101){
		/* no data */
		return 0;
	}else if(code != 0x2100){
		return B_CAS_CARD_ERROR_TRANSMIT_FAILED;
	}

	num = (prv->rbuf[7] + 1);
	if(prv->pwc_max < num){
		if(change_pwc_max(prv, num+4) < 0){
			return B_CAS_CARD_ERROR_NO_ENOUGH_MEMORY;
		}
	}

	extract_power_on_ctrl_response(prv->pwc.data+0, prv->rbuf);

	for(i=1;i<num;i++){
		prv->sbuf[5] = (uint8_t)i;
		rlen = B_CAS_BUFFER_MAX;

		ret = SCardTransmit(prv->card, SCARD_PCI_T1, prv->sbuf, slen, NULL, prv->rbuf, &rlen);
		if( (ret != SCARD_S_SUCCESS) || (rlen < 18) || (prv->rbuf[6] != i) ){
			return B_CAS_CARD_ERROR_TRANSMIT_FAILED;
		}

		extract_power_on_ctrl_response(prv->pwc.data+i, prv->rbuf);
	}

	prv->pwc.count = num;

	memcpy(dst, &(prv->pwc), sizeof(B_CAS_PWR_ON_CTRL_INFO));

	return 0;
}

static int proc_ecm_b_cas_card(void *bcas, B_CAS_ECM_RESULT *dst, uint8_t *src, int len)
{
	int retry_count;

	long ret;
	unsigned long slen;
	unsigned long rlen;

	B_CAS_CARD_PRIVATE_DATA *prv;

	prv = private_data(bcas);
	if( (prv == NULL) ||
			(dst == NULL) ||
			(src == NULL) ||
			(len < 1) ){
		return B_CAS_CARD_ERROR_INVALID_PARAMETER;
	}

	if(prv->card == 0){
		return B_CAS_CARD_ERROR_NOT_INITIALIZED;
	}

	slen = setup_ecm_receive_command(prv->sbuf, src, len);
	rlen = B_CAS_BUFFER_MAX;

	retry_count = 0;
	ret = SCardTransmit(prv->card, SCARD_PCI_T1, prv->sbuf, slen, NULL, prv->rbuf, &rlen);
#ifdef ENABLE_ARIB_STD_B1
	while( ((ret != SCARD_S_SUCCESS) || (rlen < 22)) && (retry_count < 2) ){
#else
	while( ((ret != SCARD_S_SUCCESS) || (rlen < 25)) && (retry_count < 2) ){
#endif
		retry_count += 1;
//		if(!connect_card(prv, prv->reader)){
//			continue;
//		}
//		slen = setup_ecm_receive_command(prv->sbuf, src, len);
		rlen = B_CAS_BUFFER_MAX;

		ret = SCardTransmit(prv->card, SCARD_PCI_T1, prv->sbuf, slen, NULL, prv->rbuf, &rlen);
	}

#ifdef ENABLE_ARIB_STD_B1
 	// 結果の判定方法を変更
 	if( (ret != SCARD_S_SUCCESS) ){
#else
	if( (ret != SCARD_S_SUCCESS) || (rlen < 25) ){
#endif
		return B_CAS_CARD_ERROR_TRANSMIT_FAILED;
	}

#ifdef ENABLE_ARIB_STD_B1
 	if(rlen < 22){
 		dst->return_code = 0xa103;
 	}else{
 		const static uint8_t ffff[16] = {0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff};
 		memcpy(dst->scramble_key, prv->rbuf, 16);
 		switch (load_be_uint16(prv->rbuf+18)){
		case 0xc001:
			dst->return_code = 0x0800;
			break;
		case 0xc000:
			dst->return_code = 0xa901;
			break;
		// 他にどんなコードがあるか不明なのでとりあえずff..ffかどうかでチェック
		default:
			if(!memcmp(dst->scramble_key, ffff, 16)){
				dst->return_code = 0xa902;
			}else{
				dst->return_code = 0x0800;
			}
			break;
 		}
 	}
#else
	memcpy(dst->scramble_key, prv->rbuf+6, 16);
	dst->return_code = load_be_uint16(prv->rbuf+4);
#endif

	return 0;
}

static int proc_emm_b_cas_card(void *bcas, uint8_t *src, int len)
{
#ifdef ENABLE_ARIB_STD_B1
 	// EMM 処理は未サポート
 	return B_CAS_CARD_ERROR_INVALID_PARAMETER;
#endif

	int retry_count;

	long ret;
	unsigned long slen;
	unsigned long rlen;

	B_CAS_CARD_PRIVATE_DATA *prv;

	prv = private_data(bcas);
	if( (prv == NULL) ||
			(src == NULL) ||
			(len < 1) ){
		return B_CAS_CARD_ERROR_INVALID_PARAMETER;
	}

	if(prv->card == 0){
		return B_CAS_CARD_ERROR_NOT_INITIALIZED;
	}

	slen = setup_emm_receive_command(prv->sbuf, src, len);
	rlen = B_CAS_BUFFER_MAX;

	retry_count = 0;
	ret = SCardTransmit(prv->card, SCARD_PCI_T1, prv->sbuf, slen, NULL, prv->rbuf, &rlen);
	while( ((ret != SCARD_S_SUCCESS) || (rlen < 6)) && (retry_count < 2) ){
		retry_count += 1;
//		if(!connect_card(prv, prv->reader)){
//			continue;
//		}
//		slen = setup_emm_receive_command(prv->sbuf, src, len);
		rlen = B_CAS_BUFFER_MAX;

		ret = SCardTransmit(prv->card, SCARD_PCI_T1, prv->sbuf, slen, NULL, prv->rbuf, &rlen);
	}

	if( (ret != SCARD_S_SUCCESS) || (rlen < 6) ){
		return B_CAS_CARD_ERROR_TRANSMIT_FAILED;
	}

	return 0;
}

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 private method implementation
 ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
static B_CAS_CARD_PRIVATE_DATA *private_data(void *bcas)
{
	B_CAS_CARD_PRIVATE_DATA *r;
	B_CAS_CARD *p;

	p = (B_CAS_CARD *)bcas;
	if(p == NULL){
		return NULL;
	}

	r = (B_CAS_CARD_PRIVATE_DATA *)(p->private_data);
	if( ((void *)(r+1)) != ((void *)p) ){
		return NULL;
	}

	return r;
}

static void teardown(B_CAS_CARD_PRIVATE_DATA *prv)
{
	if(prv->card != 0){
		SCardDisconnect(prv->card, SCARD_LEAVE_CARD);
		prv->card = 0;
	}

	if(prv->mng != 0){
		SCardReleaseContext(prv->mng);
		prv->mng = 0;
	}

	if(prv->pool != NULL){
		free(prv->pool);
		prv->pool = NULL;
	}

	prv->reader = NULL;
	prv->sbuf = NULL;
	prv->rbuf = NULL;
	prv->id.data = NULL;
	prv->id_max = 0;
}

static int change_id_max(B_CAS_CARD_PRIVATE_DATA *prv, int max)
{
	intptr_t m;
	intptr_t reader_size;
	int pwctrl_size;

	uint8_t *p;
	uint8_t *old_reader;
	uint8_t *old_pwctrl;

	reader_size = prv->sbuf - prv->pool;
	pwctrl_size = prv->pwc.count * sizeof(B_CAS_PWR_ON_CTRL);

	m = reader_size;
	m += (2*B_CAS_BUFFER_MAX);
	m += (max*sizeof(int64_t));
	m += (prv->pwc_max*sizeof(B_CAS_PWR_ON_CTRL));
	p = (uint8_t *)malloc(m);
	if(p == NULL){
		return B_CAS_CARD_ERROR_NO_ENOUGH_MEMORY;
	}

	old_reader = (uint8_t *)(prv->reader);
	old_pwctrl = (uint8_t *)(prv->pwc.data);

	prv->reader = (LPTSTR)p;
	prv->sbuf = prv->pool + reader_size;
	prv->rbuf = prv->sbuf + B_CAS_BUFFER_MAX;
	prv->id.data = (int64_t *)(prv->rbuf + B_CAS_BUFFER_MAX);
	prv->id_max = max;
	prv->pwc.data = (B_CAS_PWR_ON_CTRL *)(prv->id.data + prv->id_max);

	memcpy(prv->reader, old_reader, reader_size);
	memcpy(prv->pwc.data, old_pwctrl, pwctrl_size);

	free(prv->pool);
	prv->pool = p;

	return 0;
}

static int change_pwc_max(B_CAS_CARD_PRIVATE_DATA *prv, int max)
{
	intptr_t m;
	intptr_t reader_size;
	int cardid_size;

	uint8_t *p;
	uint8_t *old_reader;
	uint8_t *old_cardid;

	reader_size = prv->sbuf - prv->pool;
	cardid_size = prv->id.count * sizeof(int64_t);

	m = reader_size;
	m += (2*B_CAS_BUFFER_MAX);
	m += (prv->id_max*sizeof(int64_t));
	m += (max*sizeof(B_CAS_PWR_ON_CTRL));
	p = (uint8_t *)malloc(m);
	if(p == NULL){
		return B_CAS_CARD_ERROR_NO_ENOUGH_MEMORY;
	}

	old_reader = (uint8_t *)(prv->reader);
	old_cardid = (uint8_t *)(prv->id.data);

	prv->reader = (LPTSTR)p;
	prv->sbuf = prv->pool + reader_size;
	prv->rbuf = prv->sbuf + B_CAS_BUFFER_MAX;
	prv->id.data = (int64_t *)(prv->rbuf + B_CAS_BUFFER_MAX);
	prv->pwc.data = (B_CAS_PWR_ON_CTRL *)(prv->id.data + prv->id_max);
	prv->pwc_max = max;

	memcpy(prv->reader, old_reader, reader_size);
	memcpy(prv->id.data, old_cardid, cardid_size);

	free(prv->pool);
	prv->pool = p;

	return 0;
}

static int connect_card(B_CAS_CARD_PRIVATE_DATA *prv, LPCTSTR reader_name)
{
	int m,n;

	long ret;
	unsigned long rlen,protocol;

	uint8_t *p;

	if(prv->card != 0){
		SCardDisconnect(prv->card, SCARD_RESET_CARD);
		prv->card = 0;
	}

	ret = SCardConnect(prv->mng, reader_name, SCARD_SHARE_SHARED, SCARD_PROTOCOL_T1, &(prv->card), &protocol);
	if(ret != SCARD_S_SUCCESS){
		return 0;
	}

	m = sizeof(INITIAL_SETTING_CONDITIONS_CMD);
	memcpy(prv->sbuf, INITIAL_SETTING_CONDITIONS_CMD, m);
	rlen = B_CAS_BUFFER_MAX;
	ret = SCardTransmit(prv->card, SCARD_PCI_T1, prv->sbuf, m, NULL, prv->rbuf, &rlen);
	if(ret != SCARD_S_SUCCESS){
		return 0;
	}

#ifdef ENABLE_ARIB_STD_B1
	if(rlen < 46){
#else
	if(rlen < 57){
#endif
		return 0;
	}

	p = prv->rbuf;

#ifdef ENABLE_ARIB_STD_B1
 	n = load_be_uint16(p+44);
 	if(n != 0x9000){ // return code missmatch
 		// 最終2バイトがリターンコードかどうか未確認なのでエラーとはしない
 		// return 0;
	}
#else
	n = load_be_uint16(p+4);
	if(n != 0x2100){ // return code missmatch
		return 0;
	}
#endif

#ifdef ENABLE_ARIB_STD_B1
	memcpy(prv->stat.system_key, p+8, 32);
	memcpy(prv->stat.init_cbc, p+8, 8);
	prv->stat.ca_system_id = load_be_uint16(p);
	prv->stat.card_status = 0;
#else
	memcpy(prv->stat.system_key, p+16, 32);
	memcpy(prv->stat.init_cbc, p+48, 8);
	prv->stat.bcas_card_id = load_be_uint48(p+8);
	prv->stat.card_status = load_be_uint16(p+2);
	prv->stat.ca_system_id = load_be_uint16(p+6);
#endif

	return 1;
}

static void extract_power_on_ctrl_response(B_CAS_PWR_ON_CTRL *dst, uint8_t *src)
{
	int referrence;
	int start;
	int limit;

	dst->broadcaster_group_id = src[8];
	referrence = (src[9]<<8)|src[10];
	start = referrence - src[11];
	limit = start + (src[12]-1);

	extract_mjd(&(dst->s_yy), &(dst->s_mm), &(dst->s_dd), start);
	extract_mjd(&(dst->l_yy), &(dst->l_mm), &(dst->l_dd), limit);

	dst->hold_time = src[13];
	dst->network_id = (src[14]<<8)|src[15];
	dst->transport_id = (src[16]<<8)|src[17];
}

static void extract_mjd(int *yy, int *mm, int *dd, int mjd)
{
	int a1,m1;
	int a2,m2;
	int a3,m3;
	int a4,m4;
	int mw;
	int dw;
	int yw;

	mjd -= 51604; // 2000,3/1
	if(mjd < 0){
		mjd += 0x10000;
	}

	a1 = mjd / 146097;
	m1 = mjd % 146097;
	a2 = m1 / 36524;
	m2 = m1 - (a2 * 36524);
	a3 = m2 / 1461;
	m3 = m2 - (a3 * 1461);
	a4 = m3 / 365;
	if(a4 > 3){
		a4 = 3;
	}
	m4 = m3 - (a4 * 365);

	mw = (1071*m4+450) >> 15;
	dw = m4 - ((979*mw+16) >> 5);

	yw = a1*400 + a2*100 + a3*4 + a4 + 2000;
	mw += 3;
	if(mw > 12){
		mw -= 12;
		yw += 1;
	}
	dw += 1;

	*yy = yw;
	*mm = mw;
	*dd = dw;
}

static int setup_ecm_receive_command(uint8_t *dst, uint8_t *src, int len)
{
	int r;

	r = sizeof(ECM_RECEIVE_CMD_HEADER);
	memcpy(dst+0, ECM_RECEIVE_CMD_HEADER, r);
	dst[r] = (uint8_t)(len & 0xff);
	r += 1;
	memcpy(dst+r, src, len);
	r += len;
	dst[r] = 0;
	r += 1;

	return r;
}

static int setup_emm_receive_command(uint8_t *dst, uint8_t *src, int len)
{
	int r;

	r = sizeof(EMM_RECEIVE_CMD_HEADER);
	memcpy(dst+0, EMM_RECEIVE_CMD_HEADER, r);
	dst[r] = (uint8_t)(len & 0xff);
	r += 1;
	memcpy(dst+r, src, len);
	r += len;
	dst[r] = 0;
	r += 1;

	return r;
}

static int32_t load_be_uint16(uint8_t *p)
{
	return ((p[0]<<8)|p[1]);
}

static int64_t load_be_uint48(uint8_t *p)
{
	int i;
	int64_t r;

	r = p[0];
	for(i=1;i<6;i++){
		r <<= 8;
		r |= p[i];
	}

	return r;
}
