/*
 *  SPCFile.cpp
 *  C700
 *
 *  Created by osoumen on 12/10/10.
 *  Copyright 2012 __MyCompanyName__. All rights reserved.
 *
 */

#include "DataBuffer.h"
#include "SPCFile.h"
#include "brrcodec.h"
#include <string.h>

//-----------------------------------------------------------------------------
SPCFile::SPCFile( const char *path, bool isWriteable )
: FileAccess(path, isWriteable)
{
    mSpcPlay.init();
}

//-----------------------------------------------------------------------------
SPCFile::~SPCFile()
{
}

//-----------------------------------------------------------------------------
bool SPCFile::Load()
{
    DataBuffer  fileData(mPath);
    if (fileData.GetDataSize() < SPC_READ_SIZE) {
        return false;
    }
	
    blargg_err_t err;
    err = mSpcPlay.load_spc(fileData.GetDataPtr(), SPC_READ_SIZE);
    if (err) {
        return false;
    }
    // The DIR register is normally already valid in an SPC snapshot. Running
    // the emulated driver for a few seconds before sampling RAM can overwrite
    // sample data (e.g. Akao/RS3 streams samples into ARAM), so only fall back
    // to emulation when the snapshot has no usable DIR.
    if (mSpcPlay.GetDspReg(0x5d) == 0) {
        err = mSpcPlay.play(32000*5, NULL);   //空動作させる
        if (err) {
            return false;
        }
    }
    unsigned char *ramData = mSpcPlay.GetRam();
	
	mSrcTableAddr = (int)mSpcPlay.GetDspReg(0x5d) << 8;
	for (int i=0; i<128; i++ ) {
		int	startaddr;
		int	loopaddr;
		startaddr	= (int)ramData[mSrcTableAddr + i*4];
		startaddr	+= (int)ramData[mSrcTableAddr + i*4 + 1] << 8;
		loopaddr	= (int)ramData[mSrcTableAddr + i*4 + 2];
		loopaddr	+= (int)ramData[mSrcTableAddr + i*4 + 3] << 8;
		mSampleStart[i]	= startaddr;
		mLoopSize[i]	= loopaddr-startaddr;
		mIsLoop[i]		= checkbrrsize(&ramData[startaddr], &mSampleBytes[i]) == 1?true:false;
		
		// ループポイントの検証は、実際にループする波形(末端ブロックにループ
		// フラグがあるもの)に限る。Akao系(例: Romancing SaGa 3)のSPCは、
		// ループしないワンショット波形のDIRにも範囲外のループアドレスが
		// 残っていることがあり、それを不正として波形ごと捨てると、その
		// 楽器が読み込まれず異常な発音(ノコギリ波状の音)になる。
		if ( startaddr == 0 || startaddr == 0xffff || mLoopSize[i] < 0 ||
			( mIsLoop[i] && ( mSampleBytes[i] < mLoopSize[i] || (mLoopSize[i]%9) != 0 ) ) ) {
			mSampleBytes[i] = 0;
		}
	}
	
	mIsLoaded = true;

	return true;
}

//-----------------------------------------------------------------------------
unsigned char *SPCFile::GetSampleIndex( int sampleIndex, int *size )
{
	if ( mIsLoaded ) {
        unsigned char *ramData = mSpcPlay.GetRam();
		if ( size ) {
			*size = mSampleBytes[sampleIndex];
		}
		if ( mSampleBytes[sampleIndex] == 0 ) {
			return NULL;
		}
		return &ramData[mSampleStart[sampleIndex]];
	}
	return NULL;
}

//-----------------------------------------------------------------------------
int SPCFile::GetLoopSizeIndex( int samleIndex )
{
	if ( mIsLoaded ) {
		if ( mIsLoop[samleIndex] ) {
			return mLoopSize[samleIndex];
		}
		else {
			return -1;
		}
	}
	return 0;
}

