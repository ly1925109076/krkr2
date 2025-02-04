//---------------------------------------------------------------------------
/*
	TVP2 ( T Visual Presenter 2 )  A script authoring tool
	Copyright (C) 2000-2004  W.Dee <dee@kikyou.info>

	See details of license at "license.txt"
*/
//---------------------------------------------------------------------------
// Wave Player interface
//---------------------------------------------------------------------------
#ifndef WaveIntfH
#define WaveIntfH

#include "tjsNative.h"
#include "SoundBufferBaseIntf.h"


//---------------------------------------------------------------------------
// GUID identifying WAVEFORMATEXTENSIBLE sub format
//---------------------------------------------------------------------------
extern tjs_uint8 TVP_GUID_KSDATAFORMAT_SUBTYPE_PCM[16];
extern tjs_uint8 TVP_GUID_KSDATAFORMAT_SUBTYPE_IEEE_FLOAT[16];
//---------------------------------------------------------------------------


//---------------------------------------------------------------------------
// PCM data format (internal use)
//---------------------------------------------------------------------------
struct tTVPWaveFormat
{
	tjs_uint SamplesPerSec; // sample granule per sec
	tjs_uint Channels;
	tjs_uint BitsPerSample; // per one sample
	tjs_uint BytesPerSample; // per one sample
	tjs_uint64 TotalSamples; // in sample granule; unknown for zero
	tjs_uint64 TotalTime; // in ms; unknown for zero
	tjs_uint32 SpeakerConfig; // bitwise OR of SPEAKER_* constants
	bool IsFloat; // true if the data is IEEE floating point
	bool Seekable;
};
//---------------------------------------------------------------------------



//---------------------------------------------------------------------------
// PCM bit depth converter
//---------------------------------------------------------------------------
extern void TVPConvertPCMTo16bits(tjs_int16 *output, const void *input,
	const tTVPWaveFormat &format, tjs_int count, bool downmix);
extern void TVPConvertPCMTo16bits(tjs_int16 *output, const void *input,
	tjs_int channels, tjs_int bytespersample, tjs_int bitspersample, bool isfloat,
	tjs_int count, bool downmix);

//---------------------------------------------------------------------------



//---------------------------------------------------------------------------
// tTVPWaveDecoder interface
//---------------------------------------------------------------------------
class tTVPWaveDecoder
{
public:
	virtual ~tTVPWaveDecoder() {};

	virtual void GetFormat(tTVPWaveFormat & format) = 0;
		/* Retrieve PCM format, etc. */

	virtual bool Render(void *buf, tjs_uint bufsamplelen, tjs_uint& rendered) = 0;
		/*
			Render PCM from current position.
			where "buf" is a destination buffer, "bufsamplelen" is the buffer's
			length in sample granule, "rendered" is to be an actual number of
			written sample granule.
			returns whether the decoding is to be continued.
			because "redered" can be lesser than "bufsamplelen", the player
			should not end until the returned value becomes false.
		*/

	virtual bool SetPosition(tjs_uint64 samplepos) = 0;
		/*
			Seek to "samplepos". "samplepos" must be given in unit of sample granule.
			returns whether the seeking is succeeded.
		*/
};
//---------------------------------------------------------------------------
class tTVPWaveDecoderCreator
{
public:
	virtual tTVPWaveDecoder * Create(const ttstr & storagename,
		const ttstr &extension) = 0;
		/*
			Create tTVPWaveDecoder instance. returns NULL if failed.
		*/
};
//---------------------------------------------------------------------------




//---------------------------------------------------------------------------
// tTVPWaveDecoder interface management
//---------------------------------------------------------------------------
extern void TVPRegisterWaveDecoderCreator(tTVPWaveDecoderCreator *d);
extern void TVPUnregisterWaveDecoderCreator(tTVPWaveDecoderCreator *d);
extern tTVPWaveDecoder *  TVPCreateWaveDecoder(const ttstr & storagename);
//---------------------------------------------------------------------------




//---------------------------------------------------------------------------
// tTJSNI_BaseWaveSoundBuffer
//---------------------------------------------------------------------------
class tTJSNI_BaseWaveSoundBuffer : public tTJSNI_SoundBuffer
{
	typedef tTJSNI_SoundBuffer inherited;

public:
	tTJSNI_BaseWaveSoundBuffer();
	tjs_error TJS_INTF_METHOD
	Construct(tjs_int numparams, tTJSVariant **param,
		iTJSDispatch2 *tjs_obj);
	void TJS_INTF_METHOD Invalidate();

protected:
public:
};
//---------------------------------------------------------------------------

#include "WaveImpl.h" // must define tTJSNI_WaveSoundBuffer class

//---------------------------------------------------------------------------





//---------------------------------------------------------------------------
// tTJSNC_WaveSoundBuffer : TJS WaveSoundBuffer class
//---------------------------------------------------------------------------
class tTJSNC_WaveSoundBuffer : public tTJSNativeClass
{
public:
	tTJSNC_WaveSoundBuffer();
	static tjs_uint32 ClassID;

protected:
	tTJSNativeInstance *CreateNativeInstance();
};
//---------------------------------------------------------------------------
extern tTJSNativeClass * TVPCreateNativeClass_WaveSoundBuffer();
//---------------------------------------------------------------------------

#endif
