// libaribb25.h: CB25Decoder クラスのインターフェイス
//
//////////////////////////////////////////////////////////////////////
#pragma once

#include <windows.h>
#include <ctime>
#include <mutex>

#include "IB25Decoder.h"
#include "arib_std_b25.h"
#include "arib_std_b25_error_code.h"

#define RETRY_INTERVAL	10	// 10sec interval

class CB25Decoder : public IB25Decoder2
{
public:
// IB25Decoder
	virtual const BOOL Initialize(DWORD dwRound = 4);
	virtual const BOOL Decode(BYTE *pSrcBuf, const DWORD dwSrcSize, BYTE **ppDstBuf, DWORD *pdwDstSize);
	virtual const BOOL Flush(BYTE **ppDstBuf, DWORD *pdwDstSize);
	virtual const BOOL Reset(void);

// IB25Decoder2
	virtual void DiscardNullPacket(const bool bEnable = true);
	virtual void DiscardScramblePacket(const bool bEnable = true);
	virtual void EnableEmmProcess(const bool bEnable = true);
	virtual void SetMulti2Round(const int32_t round = 4);
	virtual void SetSimdMode(const int32_t instruction = 2);
	virtual const DWORD GetDescramblingState(const WORD wProgramID);
	virtual void ResetStatistics(void);
	virtual const DWORD GetPacketStride(void);
	virtual const DWORD GetInputPacketNum(const WORD wPID = TS_INVALID_PID);
	virtual const DWORD GetOutputPacketNum(const WORD wPID = TS_INVALID_PID);
	virtual const DWORD GetSyncErrNum(void);
	virtual const DWORD GetFormatErrNum(void);
	virtual const DWORD GetTransportErrNum(void);
	virtual const DWORD GetContinuityErrNum(const WORD wPID = TS_INVALID_PID);
	virtual const DWORD GetScramblePacketNum(const WORD wPID = TS_INVALID_PID);
	virtual const DWORD GetEcmProcessNum(void);
	virtual const DWORD GetEmmProcessNum(void);

// CB25Decoder
	CB25Decoder(void);
	virtual ~CB25Decoder(void);
	void Release(void);
	static CB25Decoder *m_pThis;

private:
	std::mutex _mtx;
	B_CAS_CARD *_bcas;
	ARIB_STD_B25 *_b25;
	BYTE *_data;
	time_t _errtime;
};
