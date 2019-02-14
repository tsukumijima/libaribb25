// IB25Decoder.h: IB25Decoder クラスのインターフェイス
//
//////////////////////////////////////////////////////////////////////


#pragma once

#include <stdint.h>

/////////////////////////////////////////////////////////////////////////////
// 定数定義
/////////////////////////////////////////////////////////////////////////////

#define TS_INVALID_PID	0xFFFFU		// 無効PID


/////////////////////////////////////////////////////////////////////////////
// B25デコーダインタフェース
/////////////////////////////////////////////////////////////////////////////

class IB25Decoder
{
public:
	virtual const BOOL Initialize(DWORD dwRound = 4) = 0;
	virtual void Release(void) = 0;

	virtual const BOOL Decode(BYTE *pSrcBuf, const DWORD dwSrcSize, BYTE **ppDstBuf, DWORD *pdwDstSize) = 0;
	virtual const BOOL Flush(BYTE **ppDstBuf, DWORD *pdwDstSize) = 0;
	virtual const BOOL Reset(void) = 0;
};


/////////////////////////////////////////////////////////////////////////////
// B25デコーダインタフェース2
/////////////////////////////////////////////////////////////////////////////

class IB25Decoder2	: public IB25Decoder
{
public:
	enum	// GetDescramblerState() リターンコード
	{
		DS_NO_ERROR			= 0x00000000UL,		// エラーなし正常
		DS_BCAS_ERROR		= 0x00000001UL,		// B-CASカードエラー
		DS_NOT_CONTRACTED	= 0x00000002UL		// 視聴未契約
	};

	virtual void DiscardNullPacket(const bool bEnable = true) = 0;
	virtual void DiscardScramblePacket(const bool bEnable = true) = 0;
	virtual void EnableEmmProcess(const bool bEnable = true) = 0;
	virtual void SetMulti2Round(const int32_t round = 4) = 0;		// オリジナルに追加
	virtual void SetSimdMode(const int32_t instruction = 3) = 0;	// オリジナルに追加

	virtual const DWORD GetDescramblingState(const WORD wProgramID) = 0;

	virtual void ResetStatistics(void) = 0;

	virtual const DWORD GetPacketStride(void) = 0;
	virtual const DWORD GetInputPacketNum(const WORD wPID = TS_INVALID_PID) = 0;
	virtual const DWORD GetOutputPacketNum(const WORD wPID = TS_INVALID_PID) = 0;
	virtual const DWORD GetSyncErrNum(void) = 0;
	virtual const DWORD GetFormatErrNum(void) = 0;
	virtual const DWORD GetTransportErrNum(void) = 0;
	virtual const DWORD GetContinuityErrNum(const WORD wPID = TS_INVALID_PID) = 0;
	virtual const DWORD GetScramblePacketNum(const WORD wPID = TS_INVALID_PID) = 0;
	virtual const DWORD GetEcmProcessNum(void) = 0;
	virtual const DWORD GetEmmProcessNum(void) = 0;
};
