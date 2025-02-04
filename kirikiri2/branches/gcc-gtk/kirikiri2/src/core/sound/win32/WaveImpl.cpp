//---------------------------------------------------------------------------
/*
	TVP2 ( T Visual Presenter 2 )  A script authoring tool
	Copyright (C) 2000-2005  W.Dee <dee@kikyou.info>

	See details of license at "license.txt"
*/
//---------------------------------------------------------------------------
// Wave Player implementation
//---------------------------------------------------------------------------
#include "tjsCommHead.h"

#include <mmsystem.h>
#include <mmreg.h>
#include <syncobjs.hpp>
#include <math.h>
#include "MainFormUnit.h"
#include "DebugIntf.h"
#include "MsgIntf.h"
#include "StorageIntf.h"
#include "WaveImpl.h"
#include "PluginImpl.h"
#include "SysInitIntf.h"
#include "ThreadIntf.h"
#include "Random.h"
#include "UtilStreams.h"

#ifdef TVP_SUPPORT_OLD_WAVEUNPACKER
	#include "oldwaveunpacker.h"
#endif

#pragma pack(push, 8)
	#include "tvpsnd.h"
#pragma pack(pop)

#ifdef TVP_SUPPORT_KPI
	#include "kmp_pi.h"
#endif

//---------------------------------------------------------------------------
// Options management
//---------------------------------------------------------------------------
static tTVPThreadPriority TVPDecodeThreadHighPriority = ttpHigher;
static tTVPThreadPriority TVPDecodeThreadLowPriority = ttpLowest;
static bool TVPSoundOptionsInit = false;
static bool TVPControlPrimaryBufferRun = true;
static bool TVPUseSoftwareBuffer = false;
static bool TVPAlwaysRecreateSoundBuffer = false;
static bool TVPPrimaryFloat = false;
static tjs_int TVPPriamrySBFrequency = 44100;
static tjs_int TVPPrimarySBBits = 16;
static tTVPSoundGlobalFocusMode TVPSoundGlobalFocusModeByOption = sgfmNeverMute;
static tjs_int TVPSoundGlobalFocusMuteVolume = 0;
static enum tTVPForceConvertMode { fcmNone, fcm16bit, fcm16bitMono }
	TVPForceConvertMode = fcmNone;
static tjs_int TVPPrimarySBCreateTryLevel = -1;
static bool TVPExpandToQuad = false;
static tjs_int TVPL1BufferLength = 1000; // in ms
static tjs_int TVPL2BufferLength = 4000; // in ms
//---------------------------------------------------------------------------
static void TVPInitSoundOptions()
{
	if(TVPSoundOptionsInit) return;

	// retrieve options from commandline
/*
 ttpIdle = 0
 ttpLowest = 1
 ttpLower = 2
 ttpNormal = 3
 ttpHigher = 4
 ttpHighest = 5
 ttpTimeCritical = 6
*/

	tTJSVariant val;
	if(TVPGetCommandLine(TJS_W("-wsdecpri"), &val))
	{
		tjs_int v = val;
		if(v < 0) v = 0;
		if(v > 5) v = 5; // tpTimeCritical is dangerous...
		TVPDecodeThreadLowPriority = (tTVPThreadPriority)v;
		if(TVPDecodeThreadHighPriority<TVPDecodeThreadLowPriority)
			TVPDecodeThreadHighPriority = TVPDecodeThreadLowPriority;
	}

	if(TVPGetCommandLine(TJS_W("-wscontrolpri"), &val))
	{
		if(ttstr(val) == TJS_W("yes"))
			TVPControlPrimaryBufferRun = true;
		else
			TVPControlPrimaryBufferRun = false;
	}

	if(TVPGetCommandLine(TJS_W("-wssoft"), &val))
	{
		if(ttstr(val) == TJS_W("yes"))
			TVPUseSoftwareBuffer = true;
		else
			TVPUseSoftwareBuffer = false;
	}

	if(TVPGetCommandLine(TJS_W("-wsrecreate"), &val))
	{
		if(ttstr(val) == TJS_W("yes"))
			TVPAlwaysRecreateSoundBuffer = true;
		else
			TVPAlwaysRecreateSoundBuffer = false;
	}

	if(TVPGetCommandLine(TJS_W("-wsfreq"), &val))
	{
		TVPPriamrySBFrequency = val;
	}

	if(TVPGetCommandLine(TJS_W("-wsbits"), &val))
	{
		ttstr sval(val);
		if(sval == TJS_W("f32"))
		{
			TVPPrimaryFloat = true;
			TVPPrimarySBBits = 32;
		}
		else if(sval[0] == TJS_W('i'))
		{
			TVPPrimaryFloat = false;
			TVPPrimarySBBits = TJS_atoi(sval.c_str() + 1);
		}
	}

	if(TVPGetCommandLine(TJS_W("-wspritry"), &val))
	{
		ttstr sval(val);
		if(sval == TJS_W("all"))
			TVPPrimarySBCreateTryLevel = -1;
		else
			TVPPrimarySBCreateTryLevel = val;
	}

	if(TVPGetCommandLine(TJS_W("-wsforcecnv"), &val))
	{
		ttstr sval(val);
		if(sval == TJS_W("i16"))
			TVPForceConvertMode = fcm16bit;
		else if(sval == TJS_W("i16m"))
			TVPForceConvertMode = fcm16bitMono;
		else
			TVPForceConvertMode = fcmNone;
	}

	if(TVPGetCommandLine(TJS_W("-wsexpandquad"), &val))
	{
		if(ttstr(val) == TJS_W("yes"))
			TVPExpandToQuad = true;
		else
			TVPExpandToQuad = false;
	}

	if(TVPGetCommandLine(TJS_W("-wsmute"), &val))
	{
		ttstr str(val);
		if(str == TJS_W("no") || str == TJS_W("never"))
			TVPSoundGlobalFocusModeByOption = sgfmNeverMute;
		else if(str == TJS_W("minimize"))
			TVPSoundGlobalFocusModeByOption = sgfmMuteOnMinimize;
		else if(str == TJS_W("yes") || str == TJS_W("deactive"))
			TVPSoundGlobalFocusModeByOption = sgfmMuteOnDeactivate;
	}

	if(TVPGetCommandLine(TJS_W("-wsmutevol"), &val))
	{
		tjs_int n = (tjs_int)val;
		if(n >= 0 && n <= 100) TVPSoundGlobalFocusMuteVolume = n * 1000;
	}

	if(TVPGetCommandLine(TJS_W("-wsl1len"), &val))
	{
		tjs_int n = (tjs_int)val;
		if(n > 0 && n < 600000) TVPL1BufferLength = n;
	}

	if(TVPGetCommandLine(TJS_W("-wsl2len"), &val))
	{
		tjs_int n = (tjs_int)val;
		if(n > 0 && n < 600000) TVPL2BufferLength = n;
	}

	TVPSoundOptionsInit = true;
}
//---------------------------------------------------------------------------



//---------------------------------------------------------------------------
// TSS plug-in interface
//---------------------------------------------------------------------------
class tTVPTSSWaveDecoder : public tTVPWaveDecoder
{
	ITSSWaveDecoder *Decoder;

public:

	tTVPTSSWaveDecoder(ITSSWaveDecoder *decoder) { Decoder = decoder; }
	~tTVPTSSWaveDecoder() { Decoder->Release(); };

	void GetFormat(tTVPWaveFormat & format)
	{
		TSSWaveFormat f;
		if(FAILED(Decoder->GetFormat(&f)))
		{
			TVPThrowExceptionMessage(TVPPluginError,
				TJS_W("ITSSWaveDecoder::GetFormat failed."));
		}
		format.SamplesPerSec = f.dwSamplesPerSec;
		format.Channels = f.dwChannels;
		if(f.dwBitsPerSample >= 0x10000)
		{
			// floating-point format since 2.17 beta 5
			format.IsFloat = true;
			format.BitsPerSample = f.dwBitsPerSample - 0x10000;
		}
		else
		{
			format.IsFloat = false;
			format.BitsPerSample = f.dwBitsPerSample;
		}
		format.BytesPerSample = format.BitsPerSample / 8;
		format.TotalSamples = f.ui64TotalSamples;
		format.TotalTime = f.dwTotalTime;
		format.Seekable = f.dwSeekable;
		format.SpeakerConfig = 0;
	}

	bool Render(void *buf, tjs_uint bufsamplelen, tjs_uint& rendered)
	{
		HRESULT hr;
		unsigned long rend;
		unsigned long st;
		hr = Decoder->Render(buf, bufsamplelen, &rend, &st);
		rendered = rend; // count of rendered samples
		if(FAILED(hr)) return false;
		return (bool)st;
	}

	bool SetPosition(tjs_uint64 samplepos)
	{
		HRESULT hr;
		hr = Decoder->SetPosition(samplepos);
		if(FAILED(hr)) return false;
		return true;
	}
};
//---------------------------------------------------------------------------
class tTVPTSSWaveDecoderCreator : public tTVPWaveDecoderCreator
{
public:
	tTVPWaveDecoder * Create(const ttstr & storagename,	const ttstr &extension)
	{
		ITSSWaveDecoder * dec = TVPSearchAvailTSSWaveDecoder(storagename, extension);
		if(!dec) return NULL;

		return new tTVPTSSWaveDecoder(dec);
	}
} static TVPTSSWaveDecoderCreator;
//---------------------------------------------------------------------------
static bool TVPTSSWaveDecoderCreatorRegistered = false;
void TVPRegisterTSSWaveDecoderCreator()
{
	if(!TVPTSSWaveDecoderCreatorRegistered)
	{
		TVPRegisterWaveDecoderCreator(&TVPTSSWaveDecoderCreator);
		TVPTSSWaveDecoderCreatorRegistered = true;
	}
}
//---------------------------------------------------------------------------



#ifdef TVP_SUPPORT_OLD_WAVEUNPACKER
//---------------------------------------------------------------------------
// old WaveUnpacker plug-in
//---------------------------------------------------------------------------
class tTVPWaveUnpacker : public tTVPWaveDecoder
{
	IWaveUnpacker *Unpacker;
	IStream *Stream;
	tjs_uint SampleSize;

public:

	tTVPWaveUnpacker(IWaveUnpacker *unpacker, IStream *stream)
		{ Unpacker = unpacker; Stream = stream; }
	~tTVPWaveUnpacker() { Unpacker->Release(); Stream->Release();};

	void GetFormat(tTVPWaveFormat & format)
	{
		long samplespersec, channels, bitspersample;

		if(FAILED(Unpacker->GetWaveFormat(&samplespersec, &channels, &bitspersample)))
		{
			TVPThrowExceptionMessage(TVPPluginError,
				TJS_W("IWaveUnpacker::GetWaveFormat failed."));
		}
		SampleSize = channels * (format.BytesPerSample = bitspersample / 8);
		format.SamplesPerSec = samplespersec;
		format.Channels = channels;
		format.BitsPerSample = bitspersample;
		format.TotalSamples = 0;
		format.TotalTime = 0;
		format.Seekable = false;
		format.SpeakerConfig = 0;
		format.IsFloat = false;
	}

	bool Render(void *buf, tjs_uint bufsamplelen, tjs_uint& rendered)
	{
		HRESULT hr;
		long samples = bufsamplelen * SampleSize;
		long rend;
		hr = Unpacker->Render(buf, samples, &rend);
		rendered = rend / SampleSize;
		if(FAILED(hr)) return false;
		return !(rendered < bufsamplelen);
	}

	bool SetPosition(tjs_uint64 samplepos)
	{
		if(samplepos == 0)
		{
			Unpacker->SetCurrentPosition(0);
			return true;
		}
		return false; // does not support
	}
};
//---------------------------------------------------------------------------
class tTVPWaveUnpackerCreator : public tTVPWaveDecoderCreator
{
public:
	tTVPWaveDecoder * Create(const ttstr & storagename, const ttstr & extension)
	{
		IStream *stream;
		IWaveUnpacker * dec = TVPSearchAvailWaveUnpacker(storagename, &stream);
		if(!dec) return NULL;

		return new tTVPWaveUnpacker(dec, stream);
	}
} static TVPWaveUnpackerCreator;
//---------------------------------------------------------------------------
static bool TVPWaveUnpackerCreatorRegistered = false;
void TVPRegisterWaveUnpackerCreator()
{
	if(!TVPWaveUnpackerCreatorRegistered)
	{
		TVPRegisterWaveDecoderCreator(&TVPWaveUnpackerCreator);
		TVPWaveUnpackerCreatorRegistered = true;
	}
}
//---------------------------------------------------------------------------
#endif



#ifdef TVP_SUPPORT_KPI
//---------------------------------------------------------------------------
// KMP plug-in interface
//---------------------------------------------------------------------------
class tTVPKMPWaveDecoder : public tTVPWaveDecoder
{
	HKMP Handle;
	KMPMODULE *Module;
	SOUNDINFO Info;
	tjs_int SampleSize;
	tjs_uint8 *RenderBuffer;
	tjs_uint RenderBufferRemain;
	bool Ended;

public:

	tTVPKMPWaveDecoder(HKMP handle, KMPMODULE *module, const SOUNDINFO &info)
	{
		Handle = handle, Module = module, Info = info;
		SampleSize = Info.dwChannels * Info.dwBitsPerSample / 8;
		RenderBufferRemain = 0;
		Ended = false;
		RenderBuffer = new tjs_uint8[Info.dwUnitRender];
	}

	~tTVPKMPWaveDecoder()
	{
		delete [] RenderBuffer;
		Module->Close(Handle);
	}

	void GetFormat(tTVPWaveFormat & format)
	{
		format.SamplesPerSec = Info.dwSamplesPerSec;
		format.Channels = Info.dwChannels;
		format.BitsPerSample = Info.dwBitsPerSample;
		format.BytesPerSample = format.BitsPerSample / 8;
		format.TotalSamples = 0; // unknown
		format.TotalTime = Info.dwLength;
		format.Seekable = Info.dwSeekable;
		format.SpeakerConfig = 0;
		format.IsFloat = false;
	}

	bool Render(void *buf, tjs_uint bufsamplelen, tjs_uint& rend_ret)
	{
		tjs_uint8 *buffer = (tjs_uint8*)buf;
		tjs_uint bufsize = bufsamplelen * SampleSize;
		tjs_uint rendered = 0;

		while(bufsize)
		{
			if(RenderBufferRemain)
			{
				// previous decoded samples are in RenderBuffer
				tjs_uint remain =
					bufsize < RenderBufferRemain ? bufsize : RenderBufferRemain;
				memcpy(buffer + rendered,
					RenderBuffer + (Info.dwUnitRender - RenderBufferRemain),
					remain);
				bufsize -= remain;
				RenderBufferRemain -= remain;
				rendered += remain;
			}
			else if(bufsize >= Info.dwUnitRender)
			{
				// directly decode to destination buffer
				if(Ended) break;
				DWORD one_rend =
					Module->Render(Handle, buffer + rendered, Info.dwUnitRender);
				bufsize -= one_rend;
				rendered += one_rend;
				if(one_rend < Info.dwUnitRender) { Ended = true; break; } // decode ended
			}
			else
			{
				// render to RenderBuffer
				if(Ended) break;
				DWORD one_rend =
					Module->Render(Handle, RenderBuffer, Info.dwUnitRender);
				// copy to destination buffer
				tjs_uint copy_bytes = one_rend < bufsize ? one_rend:bufsize;
				memcpy(buffer + rendered, RenderBuffer, copy_bytes);
				RenderBufferRemain = one_rend - copy_bytes;
				bufsize -= copy_bytes;
				rendered += copy_bytes;
				if(one_rend < Info.dwUnitRender && one_rend < bufsize)
					{ Ended = true; break; }
				if(RenderBufferRemain == 0) { Ended = true; break; }
			}
		}

		rend_ret = rendered / SampleSize;
		return !(rend_ret < bufsamplelen);
	}

	bool SetPosition(tjs_uint64 samplepos)
	{
		if(samplepos == 0)
		{
			Module->SetPosition(Handle, 0);
			Ended = false;
			return true;
		}
		return false; // does not support
	}
};
//---------------------------------------------------------------------------
class tTVPKMPWaveDecoderCreator : public tTVPWaveDecoderCreator
{
public:
	tTVPWaveDecoder * Create(const ttstr & storagename, const ttstr &extension)
	{
		SOUNDINFO info;
		KMPMODULE *module;
		HKMP handle = TVPSearchAvailKMPWaveDecoder(storagename,
			 &module, &info);
		if(!handle) return NULL;

		return new tTVPKMPWaveDecoder(handle, module, info);
	}
} static TVPKMPWaveDecoderCreator;
//---------------------------------------------------------------------------
static bool TVPKMPWaveDecoderCreatorRegistered = false;
void TVPRegisterKMPWaveDecoderCreator()
{
	if(!TVPKMPWaveDecoderCreatorRegistered)
	{
		TVPRegisterWaveDecoderCreator(&TVPKMPWaveDecoderCreator);
		TVPKMPWaveDecoderCreatorRegistered = true;
	}
}
//---------------------------------------------------------------------------
#endif


//---------------------------------------------------------------------------
// Log Table for DirectSound volume
//---------------------------------------------------------------------------
static bool TVPLogTableInit = false;
static tjs_int TVPLogTable[101];
static void TVPInitLogTable()
{
	if(TVPLogTableInit) return;
	TVPLogTableInit = true;
	tjs_int i;
	TVPLogTable[0] = -10000;
	for(i = 1; i <= 100; i++)
	{
		TVPLogTable[i] = log10((double)i/100.0)*5000.0;
	}
}
//---------------------------------------------------------------------------



//---------------------------------------------------------------------------
// DirectSound management
//---------------------------------------------------------------------------
static LPDIRECTSOUND TVPDirectSound = NULL;
static LPDIRECTSOUNDBUFFER TVPPrimaryBuffer = NULL;
static bool TVPPrimaryBufferPlayingByProgram = false;
static HMODULE TVPDirectSoundDLL = NULL;
static TTimer *TVPPrimaryDelayedStopperTimer = NULL;
static bool TVPDirectSoundShutdown = false;
//---------------------------------------------------------------------------
static void TVPEnsurePrimaryBufferPlay()
{
	if(!TVPControlPrimaryBufferRun) return;

	if(TVPPrimaryBuffer)
	{
		if(TVPPrimaryDelayedStopperTimer)
			TVPPrimaryDelayedStopperTimer->Enabled = false;
		if(!TVPPrimaryBufferPlayingByProgram)
		{
			TVPPrimaryBuffer->Play(0, 0, DSBPLAY_LOOPING);
			TVPPrimaryBufferPlayingByProgram = true;
		}
	}
}
//---------------------------------------------------------------------------
static void TVPStopPrimaryBuffer()
{
	// this will not immediately stop the buffer
	if(!TVPControlPrimaryBufferRun) return;

	if(TVPPrimaryBuffer)
	{
		if(TVPPrimaryDelayedStopperTimer)
		{
			TVPPrimaryDelayedStopperTimer->Enabled = false; // once disable the timer
			TVPPrimaryDelayedStopperTimer->Enabled = true;
		}
	}
}
//---------------------------------------------------------------------------
class tTVPPrimaryDelayedStopper
{
public:
	void __fastcall OnTimer(TObject *Sender)
	{
		if(TVPPrimaryDelayedStopperTimer)
			TVPPrimaryDelayedStopperTimer->Enabled = false;
		if(TVPPrimaryBuffer)
		{
			if(TVPPrimaryBufferPlayingByProgram)
			{
				TVPPrimaryBuffer->Stop();
				TVPPrimaryBufferPlayingByProgram = false;
			}
		}
	}
} TVPPrimaryDelayedStopper;
//---------------------------------------------------------------------------
static ttstr TVPGetSoundBufferFormatString(const WAVEFORMATEXTENSIBLE &wfx)
{
	ttstr debuglog(TJS_W("format container = "));

	if(wfx.Format.wFormatTag == WAVE_FORMAT_PCM)
		debuglog += TJS_W("WAVE_FORMAT_PCM, ");
	else if(wfx.Format.wFormatTag == WAVE_FORMAT_IEEE_FLOAT)
		debuglog += TJS_W("WAVE_FORMAT_IEEE_FLOAT, ");
	else if(wfx.Format.wFormatTag == WAVE_FORMAT_EXTENSIBLE)
		debuglog += TJS_W("WAVE_FORMAT_EXTENSIBLE, ");
	else
		debuglog += TJS_W("unknown format tag 0x") +
			TJSInt32ToHex(wfx.Format.wFormatTag) + TJS_W(", ");

	debuglog +=
		TJS_W("frequency = ") + ttstr((tjs_int)wfx.Format.nSamplesPerSec) + TJS_W("Hz, ") +
		TJS_W("bits = ") + ttstr((tjs_int)wfx.Format.wBitsPerSample) + TJS_W("bits, ") +
		TJS_W("channels = ") + ttstr((tjs_int)wfx.Format.nChannels);

	if(wfx.Format.wFormatTag == WAVE_FORMAT_EXTENSIBLE)
	{
		debuglog += TJS_W(", ");
		debuglog += TJS_W("valid bits = ") +
			ttstr((tjs_int)wfx.Samples.wValidBitsPerSample) +
				TJS_W("bits, ");
		debuglog += TJS_W("channel mask = 0x") +
			TJSInt32ToHex((tjs_uint32)wfx.dwChannelMask) +
				TJS_W(", ");

		if(!memcmp(&wfx.SubFormat,
			&TVP_GUID_KSDATAFORMAT_SUBTYPE_IEEE_FLOAT, 16))
			debuglog += TJS_W("sub type = KSDATAFORMAT_SUBTYPE_IEEE_FLOAT");
		else if(!memcmp(&wfx.SubFormat,
			&TVP_GUID_KSDATAFORMAT_SUBTYPE_PCM, 16))
			debuglog += TJS_W("sub type = KSDATAFORMAT_SUBTYPE_PCM");
		else
		{
			wchar_t buf[101];
			StringFromGUID2((const _GUID &)wfx.SubFormat,
				buf, 100);
			debuglog += TJS_W("unknown sub type ") + ttstr(buf);
		}
	}

	return debuglog;
}
//---------------------------------------------------------------------------
static BOOL CALLBACK DSoundEnumCallback( GUID* pGUID, const char * strDesc,
	const char * strDrvName,  VOID* pContext )
{
	ttstr log(TJS_W("(info) DirectSound Driver/Device found : "));
	if(strDesc) log += ttstr(strDesc);
	if(strDrvName && strDrvName[0])
	{
		log += TJS_W(" [");

		char driverpath[1024];
		char *driverpath_filename = NULL;
		bool success = SearchPath(NULL, strDrvName, NULL, 1023, driverpath, &driverpath_filename);
		if(!success)
		{
			char syspath[1024];
			GetSystemDirectory(syspath, 1023);
			strcat(syspath, "\\drivers"); // SystemDir\drivers
			success = SearchPath(syspath, strDrvName, NULL, 1023, driverpath, &driverpath_filename);
		}

		if(!success)
		{
			char syspath[1024];
			GetWindowsDirectory(syspath, 1023);
			strcat(syspath, "\\system32"); // WinDir\system32
			success = SearchPath(syspath, strDrvName, NULL, 1023, driverpath, &driverpath_filename);
		}

		if(!success)
		{
			char syspath[1024];
			GetWindowsDirectory(syspath, 1023);
			strcat(syspath, "\\system32\\drivers"); // WinDir\system32\drivers
			success = SearchPath(syspath, strDrvName, NULL, 1023, driverpath, &driverpath_filename);
		}

		if(success)
		{
			log += ttstr(driverpath);
			tjs_int major, minor, release, build;
			if(TVPGetFileVersionOf(driverpath, major, minor, release, build))
			{
				char tmp[256];
				wsprintf(tmp, " version %d.%d.%d.%d", (int)major, (int)minor, (int)release, (int)build);
				log += tmp;
			}
			else
			{
				log += TJS_W(" version unknown");
			}
		}
		else
		{
			log += ttstr(strDrvName);
			log += TJS_W(" ... is not found in search path");
		}

		log += TJS_W("]");
	}

	TVPAddImportantLog(log);

	return TRUE;
}
//---------------------------------------------------------------------------
static void TVPInitDirectSound()
{
	TVPInitSoundOptions();

	if(TVPDirectSound) return;
	if(TVPDirectSoundShutdown) return;

	if(TVPDirectSoundDLL == NULL)
	{
		// map dsound.dll
		TVPDirectSoundDLL = LoadLibrary("dsound.dll");
		if(!TVPDirectSoundDLL)
		{
			TVPThrowExceptionMessage(TVPCannotInitDirectSound,
				TJS_W("Cannot load dsound.dll."));
		}

		// Enum DirectSound devices
		try
		{
			HRESULT WINAPI (*DirectSoundEnumerateA)(LPDSENUMCALLBACKA, LPVOID);
			DirectSoundEnumerateA = (HRESULT WINAPI (*)
				(LPDSENUMCALLBACKA, LPVOID)) GetProcAddress(TVPDirectSoundDLL, "DirectSoundEnumerateA");
			if(DirectSoundEnumerateA)
				DirectSoundEnumerateA(DSoundEnumCallback, NULL);
		}
		catch(...)
		{
			// ignore errors
		}
	}

	try
	{
		if(TVPControlPrimaryBufferRun)
		{
			if(!TVPPrimaryDelayedStopperTimer)
			{
				// create timer to stop primary buffer playing at delayed timing
				TVPPrimaryDelayedStopperTimer = new TTimer(Application);
				TVPPrimaryDelayedStopperTimer->Interval = 4000;
				TVPPrimaryDelayedStopperTimer->Enabled = false;
				TVPPrimaryDelayedStopperTimer->OnTimer =
					TVPPrimaryDelayedStopper.OnTimer;
			}
		}

		// get procDirectSoundCreate's address
		HRESULT WINAPI (*procDirectSoundCreate)(LPGUID, LPDIRECTSOUND *, LPUNKNOWN);
		procDirectSoundCreate = (HRESULT (WINAPI*)(_GUID *,IDirectSound **,IUnknown*))
			GetProcAddress(TVPDirectSoundDLL, "DirectSoundCreate");
		if(!procDirectSoundCreate)
		{
			TVPThrowExceptionMessage(TVPCannotInitDirectSound,
				TJS_W("Missing DirectSoundCreate in dsound.dll."));
		}

		// create DirectSound Object
		HRESULT hr;
		hr = procDirectSoundCreate(NULL, &TVPDirectSound, NULL);
		if(FAILED(hr))
		{
			TVPThrowExceptionMessage(TVPCannotInitDirectSound,
				ttstr(TJS_W("DirectSoundCreate failed./HR=")) +
				TJSInt32ToHex((tjs_uint32)hr));
		}

		// set cooperative level
		hr = TVPDirectSound->SetCooperativeLevel(Application->Handle,
			DSSCL_PRIORITY);
		if(FAILED(hr))
		{
			TVPThrowExceptionMessage(TVPCannotInitDirectSound,
				ttstr(TJS_W("IDirectSound::SetCooperativeLevel failed./HR="))+
				TJSInt32ToHex((tjs_uint32)hr));
		}

		// create primary buffer
		TVPPrimaryBufferPlayingByProgram = false;

		DSBUFFERDESC dsbd;

		ZeroMemory(&dsbd, sizeof(dsbd));
		dsbd.dwSize = sizeof(dsbd);
		dsbd.dwFlags = DSBCAPS_PRIMARYBUFFER;
		if(TVPUseSoftwareBuffer)
			dsbd.dwFlags |= DSBCAPS_LOCSOFTWARE;

		hr = TVPDirectSound->CreateSoundBuffer(&dsbd, &TVPPrimaryBuffer, NULL);

		bool pri_normal = false;
		if(FAILED(hr) || TVPPrimaryBuffer == NULL)
		{
			// cannot create DirectSound primary buffer.
			// try to set cooperative level to DSSCL_NORMAL.
			TVPPrimaryBuffer = NULL;
			hr = TVPDirectSound->SetCooperativeLevel(Application->Handle,
				DSSCL_NORMAL);
			if(FAILED(hr))
			{
				// failed... here assumes this a failure.
				TVPThrowExceptionMessage(TVPCannotInitDirectSound,
					ttstr(TJS_W("IDirectSound::SetCooperativeLevel failed. "
						"(after creation of primary buffer failed)/HR="))+
					TJSInt32ToHex((tjs_uint32)hr));
			}
			TVPAddLog(TJS_W("Warning: Cannot create DirectSound primary buffer. "
				"Force not to use it."));
			pri_normal = true;
		}

		// set primary buffer 's sound format
		if(!pri_normal && TVPPrimaryBuffer)
		{
			WAVEFORMATEXTENSIBLE wfx;

			// number of channels is decided using GetSpeakerConfig
			DWORD sp_config;
			wfx.dwChannelMask = KSAUDIO_SPEAKER_STEREO;
			wfx.Format.nChannels = 2;

			if(SUCCEEDED(TVPDirectSound->GetSpeakerConfig(&sp_config)))
			{
				switch(DSSPEAKER_CONFIG(sp_config))
				{
				case DSSPEAKER_HEADPHONE:
					wfx.Format.nChannels = 2;
					wfx.dwChannelMask = KSAUDIO_SPEAKER_STEREO;
					break;
				case DSSPEAKER_MONO:
					wfx.Format.nChannels = 1;
					wfx.dwChannelMask = KSAUDIO_SPEAKER_MONO;
					break;
				case DSSPEAKER_QUAD:
					wfx.Format.nChannels = 4;
					wfx.dwChannelMask = KSAUDIO_SPEAKER_QUAD;
					break;
				case DSSPEAKER_STEREO:
					wfx.Format.nChannels = 2;
					wfx.dwChannelMask = KSAUDIO_SPEAKER_STEREO;
					break;
				case DSSPEAKER_SURROUND:
					wfx.Format.nChannels = 4;
					wfx.dwChannelMask = KSAUDIO_SPEAKER_SURROUND;
					break;
				case DSSPEAKER_5POINT1:
					wfx.Format.nChannels = 6;
					wfx.dwChannelMask = KSAUDIO_SPEAKER_5POINT1;
					break;
				case 0x00000007:  // DSSPEAKER_7POINT1
					wfx.Format.nChannels = 8;
					wfx.dwChannelMask = KSAUDIO_SPEAKER_7POINT1;
					break;
				}
			}

			wfx.Format.cbSize = 22;
			wfx.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
			wfx.Format.nSamplesPerSec = TVPPriamrySBFrequency;
			wfx.Format.wBitsPerSample = TVPPrimarySBBits;
			wfx.Samples.wValidBitsPerSample = wfx.Format.wBitsPerSample;
			wfx.Format.nBlockAlign = (WORD)(wfx.Format.wBitsPerSample/8 * wfx.Format.nChannels);
			wfx.Format.nAvgBytesPerSec = wfx.Format.nSamplesPerSec * wfx.Format.nBlockAlign;

			memcpy(&wfx.SubFormat, TVPPrimaryFloat ?
				TVP_GUID_KSDATAFORMAT_SUBTYPE_IEEE_FLOAT:
				TVP_GUID_KSDATAFORMAT_SUBTYPE_PCM, 16);

			switch(TVPPrimarySBCreateTryLevel)
			{
			case 2: goto level2;
			case 1: goto level1;
			case 0: goto level0;
			}

			// first try using WAVEFORMATEXTENSIBLE
			hr = TVPPrimaryBuffer->SetFormat((const WAVEFORMATEX*)&wfx);

			if(FAILED(hr))
			{
		level2:
				// second try using WAVEFORMATEX
				wfx.Format.cbSize = 0;
				wfx.Format.wFormatTag =
					TVPPrimaryFloat ? WAVE_FORMAT_IEEE_FLOAT :
						WAVE_FORMAT_PCM;
				wfx.Format.nBlockAlign = (WORD)(wfx.Format.wBitsPerSample/8 * wfx.Format.nChannels);
				wfx.Format.nAvgBytesPerSec = wfx.Format.nSamplesPerSec * wfx.Format.nBlockAlign;
				hr = TVPPrimaryBuffer->SetFormat((const WAVEFORMATEX*)&wfx);
			}

			if(FAILED(hr))
			{
		level1:
				// third try using 16bit pcm
				wfx.Format.cbSize = 0;
				wfx.Format.wBitsPerSample = 16;
				wfx.Samples.wValidBitsPerSample = 16;
				wfx.Format.wFormatTag = WAVE_FORMAT_PCM;
				wfx.Format.nBlockAlign = (WORD)(wfx.Format.wBitsPerSample/8 * wfx.Format.nChannels);
				wfx.Format.nAvgBytesPerSec = wfx.Format.nSamplesPerSec * wfx.Format.nBlockAlign;
				hr = TVPPrimaryBuffer->SetFormat((const WAVEFORMATEX*)&wfx);
			}

			if(FAILED(hr))
			{
				// specified parameter is denied
				TVPAddImportantLog(ttstr(TJS_W("Warning: IDirectSoundBuffer::SetFormat failed. "
						"(") + ttstr(TVPPriamrySBFrequency) + TJS_W("Hz, ") +
						ttstr(TVPPrimarySBBits) + TJS_W("bits, ") +
						ttstr((tjs_int)wfx.Format.nChannels) + TJS_W("channels)/HR="))+
						TJSInt32ToHex((tjs_uint32)hr));
				TVPAddImportantLog(TJS_W("Retrying with 44100Hz, "
						"16bits, 2channels"));

		level0:
				// fourth very basic buffer format
				WAVEFORMATEX wfx;
				wfx.cbSize = 0;
				wfx.wFormatTag = WAVE_FORMAT_PCM;
				wfx.nChannels = 2;
				wfx.nSamplesPerSec = 44100;
				wfx.wBitsPerSample = 16;
				wfx.nBlockAlign = (WORD)(wfx.wBitsPerSample/8*wfx.nChannels);
				wfx.nAvgBytesPerSec = wfx.nSamplesPerSec*wfx.nBlockAlign;
				TVPPrimaryBuffer->SetFormat(&wfx); // here does not check errors
			}


			hr = TVPPrimaryBuffer->GetFormat((WAVEFORMATEX*)&wfx, sizeof(WAVEFORMATEX), NULL);
			if(FAILED(hr))
				hr = TVPPrimaryBuffer->GetFormat((WAVEFORMATEX*)&wfx, sizeof(WAVEFORMATEXTENSIBLE), NULL);

			if(SUCCEEDED(hr))
			{
				ttstr debuglog(TJS_W("(info) Accepted DirectSound primary buffer format : "));
				debuglog += TVPGetSoundBufferFormatString(wfx);
				TVPAddImportantLog(debuglog);
			}

		}
	}
	catch(...)
	{
		if(TVPPrimaryBuffer) TVPPrimaryBuffer->Release(), TVPPrimaryBuffer = NULL;
		if(TVPDirectSound) TVPDirectSound->Release(), TVPDirectSound = NULL;
		if(TVPDirectSoundDLL) FreeLibrary(TVPDirectSoundDLL), TVPDirectSoundDLL = NULL;
		throw;
	}
}
//---------------------------------------------------------------------------
static void TVPUninitDirectSound()
{
	// release all objects except for secondary buffers.
	if(TVPPrimaryBuffer) TVPPrimaryBuffer->Release(), TVPPrimaryBuffer = NULL;
	if(TVPDirectSound) TVPDirectSound->Release(), TVPDirectSound = NULL;
	if(TVPDirectSoundDLL) FreeLibrary(TVPDirectSoundDLL), TVPDirectSoundDLL = NULL;
}
static void TVPUninitDirectSoundAtExitProc()
{
	TVPUninitDirectSound();
	TVPDirectSoundShutdown = true;
}
static tTVPAtExit TVPUninitDirectSoundAtExit
	(TVP_ATEXIT_PRI_RELEASE, TVPUninitDirectSoundAtExitProc);
//---------------------------------------------------------------------------
IDirectSound * TVPGetDirectSound()
{
	try
	{
		TVPInitDirectSound();
	}
	catch(...)
	{
		return NULL;
	}
	if(TVPDirectSound) TVPDirectSound->AddRef();
	return TVPDirectSound; // note that returned TVPDirectSound is AddRefed
}
//---------------------------------------------------------------------------




//---------------------------------------------------------------------------
// Format related functions
//---------------------------------------------------------------------------
static void TVPWaveFormatToWAVEFORMATEXTENSIBLE(const tTVPWaveFormat *in,
	WAVEFORMATEXTENSIBLE *out, bool use3d)
{
	// convert tTVPWaveFormat structure to WAVEFORMATEXTENSIBLE structure.

	ZeroMemory(out, sizeof(WAVEFORMATEXTENSIBLE));
	out->Format.nChannels = in->Channels;
	out->Format.nSamplesPerSec = in->SamplesPerSec;
	out->Format.wBitsPerSample = in->BytesPerSample * 8;
	out->Samples.wValidBitsPerSample = out->Format.wBitsPerSample;

	bool fraction_found = in->BitsPerSample != in->BytesPerSample * 8;

	const WORD cbextsize = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);

	if(in->SpeakerConfig == 0)
	{
		if(in->Channels == 1)
		{
			// mono
			if(TVPExpandToQuad && !use3d)
			{
				out->Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
				out->Format.cbSize = cbextsize;
				out->dwChannelMask =
					SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT | SPEAKER_BACK_LEFT |
					SPEAKER_BACK_RIGHT;
				out->Format.nChannels = 4;
				out->Samples.wValidBitsPerSample = in->BitsPerSample;
			}
			else
			{
				if(in->BytesPerSample >= 3 || fraction_found)
				{
					out->Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
					out->Format.cbSize = cbextsize;
					out->dwChannelMask = KSAUDIO_SPEAKER_MONO;
					out->Samples.wValidBitsPerSample = in->BitsPerSample;
				}
				else
				{
					out->Format.wFormatTag =
						in->IsFloat ? WAVE_FORMAT_IEEE_FLOAT:WAVE_FORMAT_PCM;
					out->Format.cbSize = 0;
				}
			}
		}
		else if(in->Channels == 2)
		{
			// stereo
			if(TVPExpandToQuad)
			{
				out->Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
				out->Format.cbSize = cbextsize;
				out->dwChannelMask =
					SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT | SPEAKER_BACK_LEFT |
					SPEAKER_BACK_RIGHT;
				out->Format.nChannels = 4;
				out->Samples.wValidBitsPerSample = in->BitsPerSample;
			}
			else
			{
				if(in->BytesPerSample >= 3 || fraction_found)
				{
					out->Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
					out->Format.cbSize = cbextsize;
					out->dwChannelMask = KSAUDIO_SPEAKER_STEREO;
					out->Samples.wValidBitsPerSample = in->BitsPerSample;
				}
				else
				{
					out->Format.wFormatTag =
						in->IsFloat ? WAVE_FORMAT_IEEE_FLOAT:WAVE_FORMAT_PCM;
					out->Format.cbSize = 0;
				}
			}
		}
		else if(in->Channels == 4)
		{
			// assumed as FL, FR, BL, BR
			out->Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
			out->Format.cbSize = cbextsize;
			out->dwChannelMask =
				SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT | SPEAKER_BACK_LEFT |
				SPEAKER_BACK_RIGHT;
			out->Samples.wValidBitsPerSample = in->BitsPerSample;
		}
		else if(in->Channels == 6)
		{
			// assumed as 5 point 1
			out->Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
			out->Format.cbSize = cbextsize;
			out->dwChannelMask = KSAUDIO_SPEAKER_5POINT1;
			out->Samples.wValidBitsPerSample = in->BitsPerSample;
		}
		else
		{
			throw ttstr(TJS_W("Unsupported channel count : "))+
				ttstr((tjs_int)(in->Channels));
		}
	}
	else
	{
		out->Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
		out->Format.cbSize = cbextsize;
		out->dwChannelMask = in->SpeakerConfig;
		out->Samples.wValidBitsPerSample = in->BitsPerSample;
	}

	memcpy(&out->SubFormat, in->IsFloat ?
		TVP_GUID_KSDATAFORMAT_SUBTYPE_IEEE_FLOAT:TVP_GUID_KSDATAFORMAT_SUBTYPE_PCM, 16);

	out->Format.nBlockAlign =
		out->Format.wBitsPerSample * out->Format.nChannels / 8;
	out->Format.nAvgBytesPerSec =
		out->Format.nSamplesPerSec * out->Format.nBlockAlign;
}
//---------------------------------------------------------------------------
static void TVPWaveFormatToWAVEFORMATEXTENSIBLE2(const tTVPWaveFormat *in,
	WAVEFORMATEXTENSIBLE *out, bool use3d)
{
	// suggest 2nd expression of WAVEFORMATEXTENSIBLE.
	TVPWaveFormatToWAVEFORMATEXTENSIBLE(in, out, use3d);

	bool fraction_found = in->BitsPerSample != in->BytesPerSample * 8;

	if(in->SpeakerConfig == 0)
	{
		if(in->Channels == 1)
		{
			// mono
			out->Format.nChannels = 1;
			if(in->BytesPerSample >= 3 && !fraction_found)
			{
				out->Format.wFormatTag =
					in->IsFloat ? WAVE_FORMAT_IEEE_FLOAT:WAVE_FORMAT_PCM;
				out->Format.cbSize = 0;
			}
		}
		else if(in->Channels == 2)
		{
			// stereo
			out->Format.nChannels = 2;
			if(in->BytesPerSample >= 3 && !fraction_found)
			{
				out->Format.wFormatTag =
					in->IsFloat ? WAVE_FORMAT_IEEE_FLOAT:WAVE_FORMAT_PCM;
				out->Format.cbSize = 0;
			}
		}
	}

	out->Format.nBlockAlign =
		out->Format.wBitsPerSample * out->Format.nChannels / 8;
	out->Format.nAvgBytesPerSec =
		out->Format.nSamplesPerSec * out->Format.nBlockAlign;
}
//---------------------------------------------------------------------------
static void TVPWaveFormatToWAVEFORMATEXTENSIBLE16bits(const tTVPWaveFormat *in,
	WAVEFORMATEXTENSIBLE *out, bool use3d)
{
	// suggest 16bit output format.
	TVPWaveFormatToWAVEFORMATEXTENSIBLE(in, out, use3d);

	out->Format.wBitsPerSample = 16;
	out->Samples.wValidBitsPerSample = 16;

	if(out->Format.wFormatTag == WAVE_FORMAT_EXTENSIBLE)
	{
		memcpy(&out->SubFormat, TVP_GUID_KSDATAFORMAT_SUBTYPE_PCM, 16);
	}
	else
	{
		out->Format.wFormatTag = WAVE_FORMAT_PCM;
	}

	out->Format.nBlockAlign =
		out->Format.wBitsPerSample * out->Format.nChannels / 8;
	out->Format.nAvgBytesPerSec =
		out->Format.nSamplesPerSec * out->Format.nBlockAlign;
}
//---------------------------------------------------------------------------
static void TVPWaveFormatToWAVEFORMATEXTENSIBLE16bitsMono(const tTVPWaveFormat *in,
	WAVEFORMATEXTENSIBLE *out, bool use3d)
{
	// suggest 16bit output format.
	TVPWaveFormatToWAVEFORMATEXTENSIBLE(in, out, use3d);

	out->Format.wBitsPerSample = 16;
	out->Samples.wValidBitsPerSample = 16;
	out->Format.nChannels = 1;
	memcpy(&out->SubFormat, TVP_GUID_KSDATAFORMAT_SUBTYPE_PCM, 16);
	out->Format.wFormatTag = WAVE_FORMAT_PCM;
	out->Format.cbSize = 0;

	out->Format.nBlockAlign =
		out->Format.wBitsPerSample * out->Format.nChannels / 8;
	out->Format.nAvgBytesPerSec =
		out->Format.nSamplesPerSec * out->Format.nBlockAlign;
}
//---------------------------------------------------------------------------
#pragma pack(push, 1)
static void TVPConvertWaveFormatToDestinationFormat(void *dest, const void *src,
	tjs_int count, const WAVEFORMATEXTENSIBLE *df, const  tTVPWaveFormat*sf)
{
	// convert PCM format descripted in "sf" to the destination format "df"

	// check whether to convert to 16bit integer
	bool convert_to_16 = false;
	if(df->Format.wBitsPerSample == 16)
	{
		if(df->Format.wFormatTag == WAVE_FORMAT_PCM)
		{
			convert_to_16 = true;
		}
		else if(df->Format.wFormatTag == WAVE_FORMAT_EXTENSIBLE)
		{
			if(!memcmp(&df->SubFormat, TVP_GUID_KSDATAFORMAT_SUBTYPE_PCM, 16))
				convert_to_16 = true;
		}
	}

	if(convert_to_16)
	{
		// convert to 16bit sample
		if(df->Format.nChannels == 1 && sf->Channels != 1)
			TVPConvertPCMTo16bits((tjs_int16*)dest, src, *sf, count, true);
		else
			TVPConvertPCMTo16bits((tjs_int16*)dest, src, *sf, count, false);
	}
	else
	{
		// intact copy
		memcpy(dest, src, count * sf->BytesPerSample * sf->Channels);
	}


	struct type_24bit { tjs_uint8 data[3]; };

	#define PROCESS_BY_SAMPLESIZE \
		if(df->Format.wBitsPerSample == 8) \
		{ \
			typedef tjs_int8 type; \
			PROCESS; \
		} \
		else if(df->Format.wBitsPerSample == 16) \
		{ \
			typedef tjs_int16 type; \
			PROCESS;\
		} \
		else if(df->Format.wBitsPerSample == 24) \
		{ \
			typedef type_24bit type; \
			PROCESS; \
		} \
		else if(df->Format.wBitsPerSample == 32) \
		{ \
			typedef tjs_int32 type; \
			PROCESS; \
		} \

	if(df->Format.nChannels == 4 && sf->Channels == 1)
	{
		// expand mono to quadphone
		#define PROCESS \
			type *ps = (type *)dest + (count-1); \
			type *pd = (type *)dest + (count-1) * 4; \
			for(tjs_int i = 0; i < count; i++) \
			{ \
				pd[0] = pd[1] = pd[2] = pd[3] = ps[0]; \
				pd -= 4; \
				ps -= 1; \
			}

		PROCESS_BY_SAMPLESIZE

		#undef PROCESS
	}

	if(df->Format.nChannels == 4 && sf->Channels == 2)
	{
		// expand stereo to quadphone
		#define PROCESS \
			type *ps = (type *)dest + (count-1) * 2; \
			type *pd = (type *)dest + (count-1) * 4; \
			for(tjs_int i = 0; i < count; i++) \
			{ \
				pd[0] = pd[2] = ps[0]; \
				pd[1] = pd[3] = ps[1]; \
				pd -= 4; \
				ps -= 2; \
			}

		PROCESS_BY_SAMPLESIZE

		#undef PROCESS
	}

	if(sf->SpeakerConfig == 0 && sf->Channels == 6 && df->Format.nChannels == 6)
	{
		// if the "channels" is 6 and speaker configuration is not specified,
		// input data is assumued that the order is :
		// FL FC FR BL BR LF
		// is to be reordered as :
		// FL FR FC LF BL BR (which WAVEFORMATEXTENSIBLE expects)

		#define PROCESS \
			type *op = (type *)dest; \
			for(tjs_int i = 0; i < count; i++) \
			{ \
				type fc = op[1]; \
				type bl = op[3]; \
				type br = op[4]; \
				op[1] = op[2]; \
				op[2] = fc; \
				op[3] = op[5]; \
				op[4] = bl; \
				op[5] = br; \
				op += 6; \
			}

		PROCESS_BY_SAMPLESIZE


		#undef PROCESS
	}
}
#pragma pack(pop)
//---------------------------------------------------------------------------
static void TVPMakeSilentWaveBytes(void *dest, tjs_int bytes, const WAVEFORMATEXTENSIBLE *format)
{
	if(format->Format.wBitsPerSample == 8)
	{
		// 0x80
		memset(dest, 0x80, bytes);
	}
	else
	{
		// 0x00
		memset(dest, 0x00, bytes);
	}
}
//---------------------------------------------------------------------------
static void TVPMakeSilentWave(void *dest, tjs_int count, const WAVEFORMATEXTENSIBLE *format)
{
	tjs_int bytes = count * format->Format.nBlockAlign;
	TVPMakeSilentWaveBytes(dest, bytes, format);
}
//---------------------------------------------------------------------------




//---------------------------------------------------------------------------
// Buffer management
//---------------------------------------------------------------------------
std::vector<tTJSNI_WaveSoundBuffer *> TVPWaveSoundBufferVector;
tTJSCriticalSection TVPWaveSoundBufferVectorCS;

//---------------------------------------------------------------------------
// tTVPWaveSoundBufferThread : playing thread
//---------------------------------------------------------------------------
class tTVPWaveSoundBufferThread : public tTVPThread
{
	tTVPThreadEvent Event;

public:
	tTVPWaveSoundBufferThread();
	~tTVPWaveSoundBufferThread();

protected:
	void Execute(void);

public:
	void Start(void);
	void CheckBufferSleep();

} static *TVPWaveSoundBufferThread = NULL;
//---------------------------------------------------------------------------
tTVPWaveSoundBufferThread::tTVPWaveSoundBufferThread()
	: tTVPThread(true)
{
	SetPriority(ttpHighest);
	Resume();
}
//---------------------------------------------------------------------------
tTVPWaveSoundBufferThread::~tTVPWaveSoundBufferThread()
{
	SetPriority(ttpNormal);
	Terminate();
	Resume();
	Event.Set();
	WaitFor();
}
//---------------------------------------------------------------------------
#define TVP_WSB_THREAD_SLEEP_TIME 60
void tTVPWaveSoundBufferThread::Execute(void)
{
	while(!GetTerminated())
	{
		// thread loop for playing thread
		DWORD time = GetTickCount();
		TVPPushEnvironNoise(&time, sizeof(time));

		{	// thread-protected
			tTJSCriticalSectionHolder holder(TVPWaveSoundBufferVectorCS);
			std::vector<tTJSNI_WaveSoundBuffer *>::iterator i;
			for(i = TVPWaveSoundBufferVector.begin();
				i != TVPWaveSoundBufferVector.end(); i++)
			{
				if((*i)->ThreadCallbackEnabled)
					(*i)->FillBuffer(); // fill sound buffer
			}
		}	// end-of-thread-protected
		time = GetTickCount() - time;

		if(time < TVP_WSB_THREAD_SLEEP_TIME)
			Event.WaitFor(TVP_WSB_THREAD_SLEEP_TIME - time);
		else
			Event.WaitFor(1);
	}
}
//---------------------------------------------------------------------------
void tTVPWaveSoundBufferThread::Start()
{
	Event.Set();
	Resume();
}
//---------------------------------------------------------------------------
void tTVPWaveSoundBufferThread::CheckBufferSleep()
{
	tTJSCriticalSectionHolder holder(TVPWaveSoundBufferVectorCS);
	tjs_uint size, nonwork_count;
	nonwork_count = 0;
	size = TVPWaveSoundBufferVector.size();
	std::vector<tTJSNI_WaveSoundBuffer *>::iterator i;
	for(i = TVPWaveSoundBufferVector.begin();
		i != TVPWaveSoundBufferVector.end(); i++)
	{
		if(!(*i)->ThreadCallbackEnabled)
			nonwork_count ++;
	}
	if(nonwork_count == size)
	{
		Suspend(); // all buffers are sleeping...
		TVPStopPrimaryBuffer();
	}
}
//---------------------------------------------------------------------------



//---------------------------------------------------------------------------
static void TVPReleaseSoundBuffers(bool disableevent = true)
{
	// release all secondary buffers and DierctSound.
	tTJSCriticalSectionHolder holder(TVPWaveSoundBufferVectorCS);
	std::vector<tTJSNI_WaveSoundBuffer *>::iterator i;
	for(i = TVPWaveSoundBufferVector.begin();
		i != TVPWaveSoundBufferVector.end(); i++)
	{
		(*i)->FreeDirectSoundBuffer(disableevent);
	}
}
//---------------------------------------------------------------------------
static void TVPShutdownWaveSoundBuffers()
{
	// clean up soundbuffers at exit
	if(TVPWaveSoundBufferThread)
		delete TVPWaveSoundBufferThread, TVPWaveSoundBufferThread = NULL;
	TVPReleaseSoundBuffers();
}
static tTVPAtExit TVPShutdownWaveSoundBuffersAtExit
	(TVP_ATEXIT_PRI_PREPARE, TVPShutdownWaveSoundBuffers);
//---------------------------------------------------------------------------
static void TVPEnsureWaveSoundBufferWorking()
{
	if(!TVPWaveSoundBufferThread)
		TVPWaveSoundBufferThread = new tTVPWaveSoundBufferThread();
	TVPWaveSoundBufferThread->Start();
}
//---------------------------------------------------------------------------
static void TVPCheckSoundBufferAllSleep()
{
	if(TVPWaveSoundBufferThread)
		TVPWaveSoundBufferThread->CheckBufferSleep();
}
//---------------------------------------------------------------------------
static void TVPAddWaveSoundBuffer(tTJSNI_WaveSoundBuffer * buffer)
{
	tTJSCriticalSectionHolder holder(TVPWaveSoundBufferVectorCS);
	TVPWaveSoundBufferVector.push_back(buffer);
}
//---------------------------------------------------------------------------
static void TVPRemoveWaveSoundBuffer(tTJSNI_WaveSoundBuffer * buffer)
{
	bool bufferempty;

	{
		tTJSCriticalSectionHolder holder(TVPWaveSoundBufferVectorCS);
		std::vector<tTJSNI_WaveSoundBuffer *>::iterator i;
		i = std::find(TVPWaveSoundBufferVector.begin(),
			TVPWaveSoundBufferVector.end(),
			buffer);
		if(i != TVPWaveSoundBufferVector.end())
			TVPWaveSoundBufferVector.erase(i);
		bufferempty = TVPWaveSoundBufferVector.size() == 0;
	}

	if(bufferempty)
	{
		if(TVPWaveSoundBufferThread)
			delete TVPWaveSoundBufferThread, TVPWaveSoundBufferThread = NULL;
	}
}
//---------------------------------------------------------------------------
void TVPResetVolumeToAllSoundBuffer()
{
	// call each SoundBuffer's SetVolumeToSoundBuffer
	tTJSCriticalSectionHolder holder(TVPWaveSoundBufferVectorCS);
	std::vector<tTJSNI_WaveSoundBuffer *>::iterator i;
	for(i = TVPWaveSoundBufferVector.begin();
		i != TVPWaveSoundBufferVector.end(); i++)
	{
		(*i)->SetVolumeToSoundBuffer();
	}
}
//---------------------------------------------------------------------------
void TVPReleaseDirectSound()
{
	TVPReleaseSoundBuffers(false);
	TVPUninitDirectSound();
}
//---------------------------------------------------------------------------






//---------------------------------------------------------------------------
// tTVPWaveSoundBufferDcodeThread : decoding thread
//---------------------------------------------------------------------------
class tTVPWaveSoundBufferDecodeThread : public tTVPThread
{
	tTJSNI_WaveSoundBuffer * Owner;
	tTVPThreadEvent Event;

public:
	tTVPWaveSoundBufferDecodeThread(tTJSNI_WaveSoundBuffer * owner);
	~tTVPWaveSoundBufferDecodeThread();

	void Execute(void);
};
//---------------------------------------------------------------------------
tTVPWaveSoundBufferDecodeThread::tTVPWaveSoundBufferDecodeThread(
	tTJSNI_WaveSoundBuffer * owner)
	: tTVPThread(true)
{
	TVPInitSoundOptions();

	Owner = owner;
	SetPriority(TVPDecodeThreadHighPriority);
	Resume();
}
//---------------------------------------------------------------------------
tTVPWaveSoundBufferDecodeThread::~tTVPWaveSoundBufferDecodeThread()
{
	SetPriority(TVPDecodeThreadHighPriority);
	Terminate();
	Resume();
	Event.Set();
	WaitFor();
}
//---------------------------------------------------------------------------
#define TVP_WSB_DECODE_THREAD_SLEEP_TIME 110
void tTVPWaveSoundBufferDecodeThread::Execute(void)
{
	DWORD st = GetTickCount();
	while(!GetTerminated())
	{
		// decoder thread main loop
		bool wait = !Owner->FillL2Buffer(false, true); // fill
		if(GetTerminated()) break;
		DWORD et = GetTickCount();
		TVPPushEnvironNoise(&et, sizeof(et));

		if(wait)
		{
			// buffer is full; sleep longer
			DWORD elapsed = et -st;
			if(elapsed < TVP_WSB_DECODE_THREAD_SLEEP_TIME)
				Event.WaitFor(TVP_WSB_DECODE_THREAD_SLEEP_TIME - elapsed);
		}
		else
		{
			// buffer is not full; sleep shorter
			Sleep(1);
			if(!GetTerminated()) SetPriority(TVPDecodeThreadLowPriority);
		}
		st = et;
	}
}
//---------------------------------------------------------------------------





//---------------------------------------------------------------------------
// tTJSNI_WaveSoundBuffer
//---------------------------------------------------------------------------
tjs_int tTJSNI_WaveSoundBuffer::GlobalVolume = 100000;
tTVPSoundGlobalFocusMode tTJSNI_WaveSoundBuffer::GlobalFocusMode = sgfmNeverMute;
//---------------------------------------------------------------------------
tTJSNI_WaveSoundBuffer::tTJSNI_WaveSoundBuffer()
{
	TVPInitSoundOptions();
	TVPRegisterTSSWaveDecoderCreator();
#ifdef TVP_SUPPORT_OLD_WAVEUNPACKER
	TVPRegisterWaveUnpackerCreator();
#endif
#ifdef TVP_SUPPORT_KPI
	TVPRegisterKMPWaveDecoderCreator();
#endif
	TVPInitLogTable();
	Decoder = NULL;
	Thread = NULL;
	UseVisBuffer = false;
	VisBuffer = NULL;
	ThreadCallbackEnabled = false;
	Level2Buffer = NULL;
	Level2BufferSize = 0;
	Volume =  100000;
	Volume2 = 100000;
	Use3D = false;
	BufferCanControlPan = false;
	Pan = 0;
	SoundBuffer = NULL;
	L2BufferDecodedSamplesInUnit = NULL;
	L2BufferSamplePositions = NULL;
	L1BufferSamplePositions = NULL;
	L1BufferUnits = 0;
	L2BufferUnits = 0;
	TVPAddWaveSoundBuffer(this);
	ZeroMemory(&C_InputFormat, sizeof(C_InputFormat));
	ZeroMemory(&InputFormat, sizeof(InputFormat));
	ZeroMemory(&Format, sizeof(Format));
	Looping = false;
}
//---------------------------------------------------------------------------
tjs_error TJS_INTF_METHOD
tTJSNI_WaveSoundBuffer::Construct(tjs_int numparams, tTJSVariant **param,
		iTJSDispatch2 *tjs_obj)
{
	tjs_error hr = inherited::Construct(numparams, param, tjs_obj);
	if(TJS_FAILED(hr)) return hr;

	return TJS_S_OK;
}
//---------------------------------------------------------------------------
void TJS_INTF_METHOD tTJSNI_WaveSoundBuffer::Invalidate()
{
	inherited::Invalidate();

	Clear();
	DestroySoundBuffer();

	TVPRemoveWaveSoundBuffer(this);
}
//---------------------------------------------------------------------------
void tTJSNI_WaveSoundBuffer::ThrowSoundBufferException(const ttstr &reason)
{
	TVPThrowExceptionMessage(TVPCannotCreateDSSecondaryBuffer,
		reason, ttstr().printf(TJS_W("frequency=%d/channels=%d/bits=%d"),
		InputFormat.SamplesPerSec, InputFormat.Channels,
		InputFormat.BitsPerSample));
}
//---------------------------------------------------------------------------
void tTJSNI_WaveSoundBuffer::TryCreateSoundBuffer()
{
	// release previous sound buffer
	if(SoundBuffer) SoundBuffer->Release(), SoundBuffer = NULL;

	// compute buffer bytes
	AccessUnitSamples = Format.Format.nSamplesPerSec / TVP_WSB_ACCESS_FREQ;
	AccessUnitBytes = AccessUnitSamples * Format.Format.nBlockAlign;

	L1BufferUnits = TVPL1BufferLength / (1000 / TVP_WSB_ACCESS_FREQ);
	if(L1BufferUnits <= 1) L1BufferUnits = 2;
	if(L1BufferSamplePositions) delete [] L1BufferSamplePositions, L1BufferSamplePositions = NULL;
	L1BufferSamplePositions = new tjs_uint64[L1BufferUnits];
	BufferBytes = AccessUnitBytes * L1BufferUnits;
		// l1 buffer bytes

	if(BufferBytes <= 0)
		ThrowSoundBufferException(TJS_W("Invalid format."));

	// allocate visualization buffer
	if(UseVisBuffer) ResetVisBuffer();

	// allocate level2 buffer ( 4sec. buffer )
	L2BufferUnits = TVPL2BufferLength / (1000 / TVP_WSB_ACCESS_FREQ);
	if(L2BufferUnits <= 1) L2BufferUnits = 2;

	if(L2BufferDecodedSamplesInUnit) delete [] L2BufferDecodedSamplesInUnit, L2BufferDecodedSamplesInUnit = NULL;
	if(L2BufferSamplePositions) delete [] L2BufferSamplePositions, L2BufferSamplePositions = NULL;
	L2BufferDecodedSamplesInUnit = new tjs_int[L2BufferUnits];
	L2BufferSamplePositions = new tjs_uint64[L2BufferUnits];

	L2AccessUnitBytes = AccessUnitSamples * InputFormat.BytesPerSample * InputFormat.Channels;
	Level2BufferSize = L2AccessUnitBytes * L2BufferUnits;
	if(Level2Buffer) delete [] Level2Buffer, Level2Buffer = NULL;
	Level2Buffer = new tjs_uint8[Level2BufferSize];

	// setup parameters
	DSBUFFERDESC dsbd;
	ZeroMemory(&dsbd, sizeof(dsbd));
	dsbd.dwSize = sizeof(dsbd);
	dsbd.dwFlags = 	DSBCAPS_GETCURRENTPOSITION2 |
		DSBCAPS_CTRLVOLUME;
	if(!Use3D)
		dsbd.dwFlags |= DSBCAPS_CTRLPAN, BufferCanControlPan = true;
	else
		BufferCanControlPan = false;
	if(!(TVPSoundGlobalFocusMuteVolume == 0 &&
		TVPSoundGlobalFocusModeByOption >= sgfmMuteOnDeactivate))
		dsbd.dwFlags |= DSBCAPS_GLOBALFOCUS;
	if(TVPUseSoftwareBuffer)
		dsbd.dwFlags |= DSBCAPS_LOCSOFTWARE;

	dsbd.dwBufferBytes = BufferBytes;

	dsbd.lpwfxFormat = (WAVEFORMATEX*)&Format;

	// create sound buffer
	HRESULT hr;
	hr = TVPDirectSound->CreateSoundBuffer(&dsbd, &SoundBuffer, NULL);
	if(FAILED(hr) && BufferCanControlPan)
	{
		dsbd.dwFlags &= ~ DSBCAPS_CTRLPAN;
		BufferCanControlPan = false;
		hr = TVPDirectSound->CreateSoundBuffer(&dsbd, &SoundBuffer, NULL);
	}

	if(FAILED(hr))
	{
		SoundBuffer = NULL;
		delete [] Level2Buffer;
		Level2Buffer = NULL;
		ThrowSoundBufferException(
			ttstr(TJS_W("IDirectSound::CreateSoundBuffer "
				"(on to create a secondary buffer) failed./HR=") +
				TJSInt32ToHex(hr)));
	}
}
//---------------------------------------------------------------------------
void tTJSNI_WaveSoundBuffer::CreateSoundBuffer()
{
	// create a direct sound secondary buffer which has given format.

	TVPInitDirectSound(); // ensure DirectSound object

	bool format_is_not_identical = TVPAlwaysRecreateSoundBuffer ||
		C_InputFormat.SamplesPerSec		!= InputFormat.SamplesPerSec ||
		C_InputFormat.Channels			!= InputFormat.Channels ||
		C_InputFormat.BitsPerSample		!= InputFormat.BitsPerSample ||
		C_InputFormat.BytesPerSample	!= InputFormat.BytesPerSample ||
		C_InputFormat.SpeakerConfig		!= InputFormat.SpeakerConfig ||
		C_InputFormat.IsFloat			!= InputFormat.IsFloat;

	if(format_is_not_identical)
	{
		try
		{
			ttstr msg;
			bool failed;
			bool firstfailed = false;
			ttstr firstformat;
			int forcemode = 0;

			if(TVPForceConvertMode == fcm16bit) goto try16bits;
			if(TVPForceConvertMode == fcm16bitMono) goto try16bits_mono;

			failed = false;
			TVPWaveFormatToWAVEFORMATEXTENSIBLE(&InputFormat, &Format, false);
			try
			{
				TryCreateSoundBuffer();
			}
			catch(eTJSError &e)
			{
				failed = true;
				msg = e.GetMessage();
			}


			if(failed)
			{
				failed = false;
				TVPWaveFormatToWAVEFORMATEXTENSIBLE2(&InputFormat, &Format, false);
				try
				{
					TryCreateSoundBuffer();
				}
				catch(eTJSError &e)
				{
					firstformat = TVPGetSoundBufferFormatString(Format);
					failed = true;
					firstfailed = true;
					msg = e.GetMessage();
				}
			}

			if(failed)
			{
		try16bits:
				failed = false;
				TVPWaveFormatToWAVEFORMATEXTENSIBLE16bits(&InputFormat, &Format, false);
				try
				{
					TryCreateSoundBuffer();
				}
				catch(eTJSError &e)
				{
					failed = true;
					msg = e.GetMessage();
				}
				if(!failed) forcemode = 1;
			}

			if(failed)
			{
		try16bits_mono:
				failed = false;
				TVPWaveFormatToWAVEFORMATEXTENSIBLE16bitsMono(&InputFormat, &Format, false);
				try
				{
					TryCreateSoundBuffer();
				}
				catch(eTJSError &e)
				{
					failed = true;
					msg = e.GetMessage();
				}
				if(!failed) forcemode = 2;
			}

			if(failed)
				TVPThrowExceptionMessage(msg.c_str());


			// log
			if(SoundBuffer)
			{
				WAVEFORMATEXTENSIBLE wfx;
				HRESULT hr = SoundBuffer->GetFormat(
					(WAVEFORMATEX*)&wfx, sizeof(WAVEFORMATEX), NULL);
				if(FAILED(hr))
					hr = SoundBuffer->GetFormat(
						(WAVEFORMATEX*)&wfx, sizeof(WAVEFORMATEXTENSIBLE), NULL);

				ttstr log(TJS_W("(info) Accepted DirectSound secondary buffer format : "));
				if(SUCCEEDED(hr))
					log += TVPGetSoundBufferFormatString(wfx);
				else
					log += TJS_W("unknown format");
				if(firstfailed)
				{
					log += TJS_W(" (") + firstformat +
						TJS_W(" was requested but denied. Continuing operation with ");
					if(forcemode == 1)
						log += TJS_W("16bit mode");
					else if(forcemode == 2)
						log += TJS_W("16bit mono mode");
					log += TJS_W(".)");
				}

				if(firstfailed)
					TVPAddImportantLog(log);
				else
					TVPAddLog(log);
				}
		}
		catch(ttstr & e)
		{
			ThrowSoundBufferException(e);
		}
		catch(...)
		{
			throw;
		}

	}

	// reset volume
	SetVolumeToSoundBuffer();

	// reset sound buffer
	ResetSoundBuffer();

	C_InputFormat = InputFormat;
}
//---------------------------------------------------------------------------
void tTJSNI_WaveSoundBuffer::DestroySoundBuffer()
{
	if(SoundBuffer)
	{
		SoundBuffer->Stop();
		SoundBuffer->Release();
		SoundBuffer = NULL;
	}

	DSBufferPlaying = false;
	BufferPlaying = false;
	CurrentPos = 0;

	if(L1BufferSamplePositions) delete [] L1BufferSamplePositions, L1BufferSamplePositions = NULL;
	if(L2BufferDecodedSamplesInUnit) delete [] L2BufferDecodedSamplesInUnit, L2BufferDecodedSamplesInUnit = NULL;
	if(L2BufferSamplePositions) delete [] L2BufferSamplePositions, L2BufferSamplePositions = NULL;
	if(Level2Buffer) delete [] Level2Buffer, Level2Buffer = NULL;
	L1BufferUnits = 0;
	L2BufferUnits = 0;

	ZeroMemory(&C_InputFormat, sizeof(C_InputFormat));

	Level2BufferSize = 0;

	DeallocateVisBuffer();
}
//---------------------------------------------------------------------------
void tTJSNI_WaveSoundBuffer::ResetSoundBuffer()
{
	if(SoundBuffer)
	{
		// fill the buffer with silence

		SoundBuffer->SetCurrentPosition(0);
		BYTE *p1,*p2;
		DWORD b1,b2;
		HRESULT hr;
		hr = SoundBuffer->Lock(0, BufferBytes, (void**)&p1,
			&b1, (void**)&p2, &b2, 0);
		if(hr == DSERR_BUFFERLOST)
		{
			// retry after restoring lost buffer memory
			SoundBuffer->Restore();
			hr = SoundBuffer->Lock(0, BufferBytes, (void**)&p1,
				&b1, (void**)&p2, &b2, 0);
		}

		if(SUCCEEDED(hr))
		{
			TVPMakeSilentWaveBytes(p1, BufferBytes, &Format);

			SoundBuffer->Unlock((void*)p1, b1, (void*)p2, b2);
		}

		SoundBuffer->SetCurrentPosition(0);

		// fill level2 buffer with silence
		TVPMakeSilentWaveBytes(Level2Buffer, Level2BufferSize, &Format);
	}

	ResetSamplePositions();
}
//---------------------------------------------------------------------------
void tTJSNI_WaveSoundBuffer::ResetSamplePositions()
{
	// reset L2BufferSamplePositions and L1BufferSamplePositions
	if(L2BufferSamplePositions)
	{
		for(int i = 0; i < L2BufferUnits; i++)
			L2BufferSamplePositions[i] = (tjs_uint64)(tjs_int64)-1;
	}
	if(L1BufferSamplePositions)
	{
		for(int i = 0; i < L1BufferUnits; i++)
			L1BufferSamplePositions[i] = (tjs_uint64)(tjs_int64)-1;
	}
}
//---------------------------------------------------------------------------
void tTJSNI_WaveSoundBuffer::Clear()
{
	// clear all status and unload current decoder
	Stop();
	ThreadCallbackEnabled = false;
	TVPCheckSoundBufferAllSleep();
	if(Thread) delete Thread, Thread = NULL;
	if(Decoder) delete Decoder, Decoder = NULL;
	LoopLength = LoopStart = LoopEnd = 0;
	CurrentPos = 0;
	BufferPlaying = false;
	DSBufferPlaying = false;
	Paused = false;

	ResetSamplePositions();

	SetStatus(ssUnload);
}
//---------------------------------------------------------------------------
tjs_uint tTJSNI_WaveSoundBuffer::_Decode(void *buffer, tjs_uint bufsamplelen)
{
	tjs_uint written = 0;
	tjs_int samplesize = InputFormat.BytesPerSample * InputFormat.Channels;
	while(written < bufsamplelen)
	{
		tjs_uint w;
		bool cont;

		try
		{
			// decode
			cont = Decoder->Render((tjs_uint8*)buffer +
				written * samplesize,
				bufsamplelen - written, w);
		}
		catch(...)
		{
			w = 0;
			cont = false;
		}

		written += w;
		CurrentPos += w;

		if(!cont)
		{
			// decoding had been finished

			if(LoopLength == 0 && Looping)
			{
				// loop information is not loaded. do simple loop.
				// set decode position to zero.
				Decoder->SetPosition(0);
				CurrentPos = 0;
			}
			else
			{
				return written;
			}
		}
	}

	return written;
}
//---------------------------------------------------------------------------
tjs_uint tTJSNI_WaveSoundBuffer::Decode(void *buffer, tjs_uint bufsamplelen)
{
	// decode PCM.
	if(LoopLength == 0 || !Looping ||
		(LoopLength != 0 && CurrentPos >= LoopEnd))
			return _Decode(buffer, bufsamplelen);

	// loop information processing
	tjs_uint written = 0;
	tjs_uint remain = bufsamplelen;
	tjs_int samplesize = InputFormat.BytesPerSample * InputFormat.Channels;
	while(remain)
	{
		tjs_uint l;  // decode size at once

		// determine size to decode
		if(LoopEnd - CurrentPos < 0x10000000L)
			l = LoopEnd - CurrentPos;
		else
			l = 0x10000000; // we must care well when converting from i64 to i32.

		if(remain < l) l = remain;

		tjs_uint decoded = _Decode((tjs_uint8*)buffer + written * samplesize, l);

		written += decoded;
		remain -= decoded;
		if(decoded < l) return written; // may be a decode error


		if(CurrentPos == LoopEnd)
		{
			Decoder->SetPosition(LoopStart); // seek to loop start point
			CurrentPos = LoopStart;
		}
	}

	return written;
}
//---------------------------------------------------------------------------
bool tTJSNI_WaveSoundBuffer::FillL2Buffer(bool firstwrite, bool fromdecodethread)
{
	if(!fromdecodethread && Thread)
		Thread->SetPriority(ttpHighest);
			// make decoder thread priority high, before entering critical section

	tTJSCriticalSectionHolder holder(L2BufferCS);

	if(firstwrite)
	{
		// only main thread runs here
		L2BufferReadPos = L2BufferWritePos = L2BufferRemain = 0;
		L2BufferEnded = false;
		for(tjs_int i = 0; i<L2BufferUnits; i++)
			L2BufferDecodedSamplesInUnit[i] = 0;
	}

	{
		tTVPThreadPriority ttpbefore = TVPDecodeThreadHighPriority;
		bool retflag = false;
		if(Thread)
		{
			ttpbefore = Thread->GetPriority();
			Thread->SetPriority(TVPDecodeThreadHighPriority);
		}
		{
			tTJSCriticalSectionHolder holder(L2BufferRemainCS);
			if(L2BufferRemain == L2BufferUnits) retflag = true;
		}
		if(Thread) Thread->SetPriority(ttpbefore);
		if(retflag) return false; // buffer is full
	}

	if(L2BufferEnded)
	{
		L2BufferDecodedSamplesInUnit[L2BufferWritePos] = 0;
	}
	else
	{
        tjs_uint64 currentpcmpos = CurrentPos;

		tjs_uint decoded = Decode(L2BufferWritePos * L2AccessUnitBytes + Level2Buffer,
			AccessUnitSamples);

		if(decoded < (tjs_uint) AccessUnitSamples) L2BufferEnded = true;

		L2BufferDecodedSamplesInUnit[L2BufferWritePos] = decoded;
        L2BufferSamplePositions[L2BufferWritePos] = currentpcmpos;
	}

	L2BufferWritePos++;
	if(L2BufferWritePos >= L2BufferUnits) L2BufferWritePos = 0;

	{
		tTVPThreadPriority ttpbefore = TVPDecodeThreadHighPriority;
		if(Thread)
		{
			ttpbefore = Thread->GetPriority();
			Thread->SetPriority(TVPDecodeThreadHighPriority);
		}
		{
			tTJSCriticalSectionHolder holder(L2BufferRemainCS);
			L2BufferRemain++;
		}
		if(Thread) Thread->SetPriority(ttpbefore);
	}

	return true;
}
//---------------------------------------------------------------------------
void tTJSNI_WaveSoundBuffer::PrepareToReadL2Buffer(bool firstread)
{
	if(L2BufferRemain == 0 && !L2BufferEnded)
		FillL2Buffer(firstread, false);

	if(Thread) Thread->SetPriority(TVPDecodeThreadHighPriority);
			// make decoder thread priority higher than usual,
			// before entering critical section
}
//---------------------------------------------------------------------------
tjs_uint tTJSNI_WaveSoundBuffer::ReadL2Buffer(void *buffer, tjs_uint64 &pcmpos)
{
	tjs_uint decoded = L2BufferDecodedSamplesInUnit[L2BufferReadPos];


	TVPConvertWaveFormatToDestinationFormat(buffer,
		L2BufferReadPos * L2AccessUnitBytes + Level2Buffer, decoded,
		&Format, &InputFormat);

    pcmpos = L2BufferSamplePositions[L2BufferReadPos];

	if(decoded < (tjs_uint)AccessUnitSamples)
	{
		// fill rest with silence
		TVPMakeSilentWave((tjs_uint8*)buffer + decoded*Format.Format.nBlockAlign,
			AccessUnitSamples - decoded, &Format);
	}

	L2BufferReadPos++;
	if(L2BufferReadPos >= L2BufferUnits) L2BufferReadPos = 0;

	{
		tTJSCriticalSectionHolder holder(L2BufferRemainCS);
		L2BufferRemain--;
	}

	return decoded;
}
//---------------------------------------------------------------------------
void tTJSNI_WaveSoundBuffer::FillDSBuffer(tjs_int writepos, tjs_uint64 * pcmpos)
{
	BYTE *p1, *p2;
	DWORD b1, b2;

	HRESULT hr;
	hr = SoundBuffer->Lock(writepos, AccessUnitBytes,
		(void**)&p1, &b1, (void**)&p2, &b2, 0);
	if(hr == DSERR_BUFFERLOST)
	{
		// retry after restoring lost buffer memory
		SoundBuffer->Restore();
		hr = SoundBuffer->Lock(writepos, AccessUnitBytes,
			(void**)&p1, &b1, (void**)&p2, &b2, 0);
	}

	if(SUCCEEDED(hr))
	{
		tjs_uint decoded;

		if(UseVisBuffer)
		{
			decoded = ReadL2Buffer(VisBuffer + writepos, *pcmpos);
			memcpy(p1, VisBuffer + writepos, AccessUnitBytes);
		}
		else
		{
			decoded = ReadL2Buffer(p1, *pcmpos);
		}

		if(PlayStopPos == -1 && decoded < (tjs_uint)AccessUnitSamples)
		{
			// decoding was finished
			PlayStopPos = writepos + decoded*Format.Format.nBlockAlign;
				// set stop position
		}

		SoundBuffer->Unlock((void*)p1, b1, (void*)p2, b2);
	}
}
//---------------------------------------------------------------------------
bool tTJSNI_WaveSoundBuffer::FillBuffer(bool firstwrite, bool allowpause)
{
	// fill DirectSound secondary buffer with one render unit.

	tTJSCriticalSectionHolder holder(BufferCS);

	if(!SoundBuffer) return true;
	if(!Decoder) return true;
	if(!BufferPlaying) return true;

	// check paused state
	if(allowpause)
	{
		if(Paused)
		{
			if(DSBufferPlaying)
			{
				SoundBuffer->Stop();
				DSBufferPlaying = false;
			}
			return true;
		}
		else
		{
			if(!DSBufferPlaying)
			{
				SoundBuffer->Play(0, 0, DSBPLAY_LOOPING);
				DSBufferPlaying = true;
			}
		}
	}

	// check decoder thread status
	tjs_int bufferremain;
	{
		tTJSCriticalSectionHolder holder(L2BufferRemainCS);
		bufferremain = L2BufferRemain;
	}

	if(Thread && bufferremain < L2BufferUnits / 4)
		Thread->SetPriority(ttpNormal);

	// check buffer playing position
	tjs_int writepos;
	tjs_uint64 * pcmpos;

	DWORD pp = 0, wp = 0; // write pos and read pos
	if(FAILED(SoundBuffer->GetCurrentPosition(&pp, &wp))) return true;


	TVPPushEnvironNoise(&pp, sizeof(pp));
	TVPPushEnvironNoise(&wp, sizeof(wp));
		// drift between main clock and clocks which come from other sources
		// is a good environ noise.


	if(firstwrite)
	{
		writepos = 0;
		pcmpos = L1BufferSamplePositions + 0;
		PlayStopPos = -1;
		SoundBufferWritePos = 1;
		SoundBufferPrevReadPos = 0;
	}
	else
	{
		if(PlayStopPos != -1)
		{
			// check whether the buffer playing position passes over PlayStopPos
			if(SoundBufferPrevReadPos > (tjs_int)pp)
			{
				if(PlayStopPos >= SoundBufferPrevReadPos ||
					PlayStopPos < (tjs_int)pp)
				{
					SoundBuffer->Stop();
					ResetSamplePositions();
					DSBufferPlaying = false;
					BufferPlaying = false;
					CurrentPos = 0;
					return true;
				}
			}
			else
			{
				if(PlayStopPos >= SoundBufferPrevReadPos &&
					PlayStopPos < (tjs_int)pp)
				{
					SoundBuffer->Stop();
					ResetSamplePositions();
					DSBufferPlaying = false;
					BufferPlaying = false;
					CurrentPos = 0;
					return true;
				}
			}
		}

		tjs_int wpb = wp / AccessUnitBytes;
		tjs_int ppb = pp / AccessUnitBytes;

		tjs_int d = wpb - SoundBufferWritePos;
		if(d < 0) d += L1BufferUnits;

		wpb -= ppb;
		if(wpb < 0) wpb += L1BufferUnits;

		if(d <= wpb)
		{
			// pp thru wp is currently playing position; cannot write there
			return true;
		}

		writepos = SoundBufferWritePos * AccessUnitBytes;
		pcmpos = L1BufferSamplePositions + SoundBufferWritePos;
		SoundBufferWritePos ++;
		if(SoundBufferWritePos >= L1BufferUnits)
			SoundBufferWritePos = 0;
	}

	SoundBufferPrevReadPos = pp;

	if(bufferremain > 1) // buffer is ready
	{
		// with no locking operations
		FillDSBuffer(writepos, pcmpos);
	}
	else
	{
		PrepareToReadL2Buffer(false); // complete decoding before reading from L2

		{
			tTJSCriticalSectionHolder l2holder(L2BufferCS);
			FillDSBuffer(writepos, pcmpos);
		}
	}
	return false;
}
//---------------------------------------------------------------------------
void tTJSNI_WaveSoundBuffer::StartPlay()
{
	if(!Decoder) return;

	TVPEnsurePrimaryBufferPlay(); // let primary buffer to start running

	// play from first

	{	// thread protected block
		if(Thread) { Thread->SetPriority(TVPDecodeThreadHighPriority); }
		tTJSCriticalSectionHolder holder(BufferCS);
		tTJSCriticalSectionHolder l2holder(L2BufferCS);

		CreateSoundBuffer();

		// fill sound buffer with some first samples
		BufferPlaying = true;
		FillL2Buffer(true, false);
		FillBuffer(true, false);
		FillBuffer(false, false);
		FillBuffer(false, false);
		FillBuffer(false, false);

		// start playing
		if(!Paused)
		{
			SoundBuffer->Play(0, 0, DSBPLAY_LOOPING);
			DSBufferPlaying = true;
		}


	}	// end of thread protected block

	// ensure thread
	TVPEnsureWaveSoundBufferWorking();
	if(!Thread)
	{
		ThreadCallbackEnabled = true;
		Thread = new tTVPWaveSoundBufferDecodeThread(this);
	}

}
//---------------------------------------------------------------------------
void tTJSNI_WaveSoundBuffer::StopPlay()
{
	if(!Decoder) return;
	if(!SoundBuffer) return;

	if(Thread) { Thread->SetPriority(TVPDecodeThreadHighPriority);}
	tTJSCriticalSectionHolder holder(BufferCS);
	tTJSCriticalSectionHolder l2holder(L2BufferCS);

	SoundBuffer->Stop();
	DSBufferPlaying = false;
	BufferPlaying = false;
}
//---------------------------------------------------------------------------
void tTJSNI_WaveSoundBuffer::Play()
{
	// play from first or current position
	if(!Decoder) return;
	if(BufferPlaying) return;

	StopPlay();

	TVPEnsurePrimaryBufferPlay(); // let primary buffer to start running

	if(Thread) { Thread->SetPriority(TVPDecodeThreadHighPriority);}
	tTJSCriticalSectionHolder holder(BufferCS);
	tTJSCriticalSectionHolder l2holder(L2BufferCS);

	if(!Decoder->SetPosition(CurrentPos))
		Decoder->SetPosition(CurrentPos = 0);

	StartPlay();
	SetStatus(ssPlay);
}
//---------------------------------------------------------------------------
void tTJSNI_WaveSoundBuffer::Stop()
{
	// stop playing
	StopPlay();

	// delete thread
	ThreadCallbackEnabled = false;
	TVPCheckSoundBufferAllSleep();
	if(Thread) delete Thread, Thread = NULL;

	// set status
	if(Status != ssUnload) SetStatus(ssStop);

	// rewind
	CurrentPos = 0;
}
//---------------------------------------------------------------------------
void tTJSNI_WaveSoundBuffer::SetPaused(bool b)
{
	if(Thread)
		{/*orgpri = Thread->Priority;*/
			Thread->SetPriority(TVPDecodeThreadHighPriority); }
	tTJSCriticalSectionHolder holder(BufferCS);
	tTJSCriticalSectionHolder l2holder(L2BufferCS);

	Paused = b;
}
//---------------------------------------------------------------------------
void tTJSNI_WaveSoundBuffer::TimerBeatHandler()
{
	inherited::TimerBeatHandler();

	// check buffer stopping
	if(Status == ssPlay && !BufferPlaying)
	{
		// buffer was stopped
		ThreadCallbackEnabled = false;
		TVPCheckSoundBufferAllSleep();
		if(Thread) delete Thread, Thread = NULL;
		SetStatusAsync(ssStop);
	}
}
//---------------------------------------------------------------------------
void tTJSNI_WaveSoundBuffer::Open(const ttstr & storagename)
{
	// open a storage and prepare to play
	TVPEnsurePrimaryBufferPlay(); // let primary buffer to start running

	Clear();

	Decoder = TVPCreateWaveDecoder(storagename);

	// open loop information file
	ttstr sliname = storagename + TJS_W(".sli");
	if(TVPIsExistentStorage(sliname))
	{
		tTVPStreamHolder slistream(sliname);
		tjs_uint8 *buffer;
		tjs_uint size;
		buffer = new tjs_uint8 [ (size = slistream->GetSize()) +1];
		try
		{
			slistream->ReadBuffer(buffer, size);
			buffer[size] = 0;
			char *p;
			p = strstr((char*)buffer, "LoopStart=");
			if(!p) TVPThrowExceptionMessage(TVPInvalidLoopInformation, sliname);
			p += 10; // length of "LoopStart="
			LoopStart = _atoi64(p);

			p = strstr((char*)buffer, "LoopLength=");
			if(!p) TVPThrowExceptionMessage(TVPInvalidLoopInformation, sliname);
			p += 11; // length of "LoopLength="
			LoopLength = _atoi64(p);

			LoopEnd = LoopStart + LoopLength;
		}
		catch(...)
		{
			delete [] buffer;
			Clear();
			throw;
		}
		delete [] buffer;
	}

	// retrieve format
	try
	{
		Decoder->GetFormat(InputFormat);
	}
	catch(...)
	{
		Clear();
		throw;
	}

	if(LoopLength && InputFormat.TotalSamples)
	{
		// if loop information is alive and totalsamples are known
		if(LoopStart + LoopLength > InputFormat.TotalSamples)
		{
			// loop length exceeds total length
			Clear();
			TVPThrowExceptionMessage(TVPInvalidLoopInformation, sliname);
		}
	}

	// set status to stop
	SetStatus(ssStop);
}
//---------------------------------------------------------------------------
tjs_uint64 tTJSNI_WaveSoundBuffer::GetPosition()
{
	if(!Decoder) return 0L;
	if(!SoundBuffer) return 0L;

	tTJSCriticalSectionHolder holder(BufferCS);

	DWORD wp, pp;
	if(FAILED(SoundBuffer->GetCurrentPosition(&pp, &wp))) return 0L;

	tjs_int rblock = pp / AccessUnitBytes;

	if(L1BufferSamplePositions[rblock] == (tjs_uint64)(tjs_int64)-1)
		return 0L;

	tjs_int offset = pp % AccessUnitBytes;

	offset /= Format.Format.nBlockAlign;

	tjs_uint64 pos = L1BufferSamplePositions[rblock] + offset;

	if(Looping)
	{
		if(LoopLength != 0)
		{
			while(pos >= LoopEnd) pos -= LoopLength;
		}
		else
		{
			// simple loop
			if(InputFormat.TotalSamples != 0)
				while(pos >= InputFormat.TotalSamples) pos -= InputFormat.TotalSamples;
		}
	}
	else
	{
		// not looping
		if(InputFormat.TotalSamples != 0)
			while(pos >= InputFormat.TotalSamples) pos -= InputFormat.TotalSamples;
	}

	return pos * 1000 / Format.Format.nSamplesPerSec;
}
//---------------------------------------------------------------------------
void tTJSNI_WaveSoundBuffer::SetPosition(tjs_uint64 pos)
{
	tjs_uint64 possamples = pos * Format.Format.nSamplesPerSec / 1000; // in samples

	if(InputFormat.TotalSamples && InputFormat.TotalSamples <= possamples) return;

	if(BufferPlaying && DSBufferPlaying)
	{
		StopPlay();
		if(Decoder->SetPosition(possamples))
			CurrentPos = possamples;
		StartPlay();
	}
	else
	{
		if(Decoder->SetPosition(possamples)) CurrentPos = possamples;
	}
}
//---------------------------------------------------------------------------
tjs_uint64 tTJSNI_WaveSoundBuffer::GetTotalTime()
{
	return InputFormat.TotalSamples * 1000 / Format.Format.nSamplesPerSec;
}
//---------------------------------------------------------------------------
void tTJSNI_WaveSoundBuffer::SetVolumeToSoundBuffer()
{
	// set current volume/pan to DirectSound buffer
	if(SoundBuffer)
	{
		tjs_int v;
		tjs_int mutevol = 100000;
		if(TVPSoundGlobalFocusModeByOption >= sgfmMuteOnDeactivate &&
			TVPSoundGlobalFocusMuteVolume == 0)
		{
			// no mute needed here;
			// muting will be processed in DirectSound framework.
			;
		}
		else
		{
			// mute mode is choosen from GlobalFocusMode or
			// TVPSoundGlobalFocusModeByOption which is more restrictive.
			tTVPSoundGlobalFocusMode mode =
				GlobalFocusMode > TVPSoundGlobalFocusModeByOption ?
					GlobalFocusMode : TVPSoundGlobalFocusModeByOption;
			switch(mode)
			{
			case sgfmNeverMute:
				;
				break;
			case sgfmMuteOnMinimize:
				if(!  TVPMainForm->GetApplicationNotMinimizing())
					mutevol = TVPSoundGlobalFocusMuteVolume;
				break;
			case sgfmMuteOnDeactivate:
				if(!
					(  TVPMainForm->GetApplicationActivating() &&
					   TVPMainForm->GetApplicationNotMinimizing()))
					mutevol = TVPSoundGlobalFocusMuteVolume;
				break;
			}
		}

		// compute volume for each buffer
		v = (Volume / 10) * (Volume2 / 10) / 1000;
		v = (v / 10) * (GlobalVolume / 10) / 1000;
		v = (v / 10) * (mutevol / 10) / 1000;
		v = v / 1000;
		if(v > 100) v = 100;
		if(v < 0 ) v = 0;
		SoundBuffer->SetVolume(TVPLogTable[v]);

		if(BufferCanControlPan)
		{
			tjs_int v = Pan / 1000;
			LONG pan;
			if(v < -100) v = -100;
			if(v > 100) v = 100;
			if(v < 0)
				pan = TVPLogTable[100 - (-v)];
			else
				pan = -TVPLogTable[100 - v];

			SoundBuffer->SetPan(pan);
		}
	}
}
//---------------------------------------------------------------------------
void tTJSNI_WaveSoundBuffer::SetVolume(tjs_int v)
{
	if(v < 0) v = 0;
	if(v > 100000) v = 100000;

	if(Volume != v)
	{
		Volume = v;
		SetVolumeToSoundBuffer();
	}
}
//---------------------------------------------------------------------------
void tTJSNI_WaveSoundBuffer::SetVolume2(tjs_int v)
{
	if(v < 0) v = 0;
	if(v > 100000) v = 100000;

	if(Volume2 != v)
	{
		Volume2 = v;
		SetVolumeToSoundBuffer();
	}
}
//---------------------------------------------------------------------------
void tTJSNI_WaveSoundBuffer::SetPan(tjs_int v)
{
	if(v < -100000) v = -100000;
	if(v > 100000) v = 100000;
	if(Pan != v)
	{
		Pan = v;
		SetVolumeToSoundBuffer();
	}
}
//---------------------------------------------------------------------------
void tTJSNI_WaveSoundBuffer::SetGlobalVolume(tjs_int v)
{
	if(v < 0) v = 0;
	if(v > 100000) v = 100000;

	if(GlobalVolume != v)
	{
		GlobalVolume = v;
		TVPResetVolumeToAllSoundBuffer();
	}
}
//---------------------------------------------------------------------------
void tTJSNI_WaveSoundBuffer::SetGlobalFocusMode(tTVPSoundGlobalFocusMode b)
{
	if(GlobalFocusMode != b)
	{
		GlobalFocusMode = b;
		TVPResetVolumeToAllSoundBuffer();
	}
}
//---------------------------------------------------------------------------
void tTJSNI_WaveSoundBuffer::SetUseVisBuffer(bool b)
{
	tTJSCriticalSectionHolder holder(BufferCS);

	if(b)
	{
		UseVisBuffer = true;

		if(SoundBuffer) ResetVisBuffer();
	}
	else
	{
		DeallocateVisBuffer();
		UseVisBuffer = false;
	}
}
//---------------------------------------------------------------------------
void tTJSNI_WaveSoundBuffer::ResetVisBuffer()
{
	// reset or recreate visualication buffer
	tTJSCriticalSectionHolder holder(BufferCS);

	DeallocateVisBuffer();

	VisBuffer = new tjs_uint8 [BufferBytes];
	UseVisBuffer = true;
}
//---------------------------------------------------------------------------
void tTJSNI_WaveSoundBuffer::DeallocateVisBuffer()
{
	tTJSCriticalSectionHolder holder(BufferCS);

	if(VisBuffer) delete [] VisBuffer, VisBuffer = NULL;
	UseVisBuffer = false;
}
//---------------------------------------------------------------------------
void tTJSNI_WaveSoundBuffer::CopyVisBuffer(tjs_int16 *dest, const tjs_uint8 *src,
	tjs_int numsamples, tjs_int channels)
{
	bool isfloat = Format.Format.wFormatTag == WAVE_FORMAT_IEEE_FLOAT ||
		(Format.Format.wFormatTag == WAVE_FORMAT_EXTENSIBLE &&
			!memcmp(&Format.SubFormat, &TVP_GUID_KSDATAFORMAT_SUBTYPE_IEEE_FLOAT, 16));

	if(channels == 1)
	{
		TVPConvertPCMTo16bits(dest, (const void*)src, Format.Format.nChannels,
			Format.Format.wBitsPerSample / 8, Format.Samples.wValidBitsPerSample,
			isfloat, numsamples, true);
	}
	else if(channels == Format.Format.nChannels)
	{
		TVPConvertPCMTo16bits(dest, (const void*)src, Format.Format.nChannels,
			Format.Format.wBitsPerSample / 8, Format.Samples.wValidBitsPerSample,
			isfloat, numsamples, false);
	}
}
//---------------------------------------------------------------------------
tjs_int tTJSNI_WaveSoundBuffer::GetVisBuffer(tjs_int16 *dest, tjs_int numsamples,
	tjs_int channels, tjs_int aheadsamples)
{
	// read visualization buffer samples
	if(!UseVisBuffer) return 0;
	if(!VisBuffer) return 0;
	if(!Decoder) return 0;
	if(!SoundBuffer) return 0;
	if(!DSBufferPlaying || !BufferPlaying) return 0;

	if(channels != Format.Format.nChannels && channels != 1) return 0;

	// retrieve current playing position
	DWORD wp, pp;
	{
		tTJSCriticalSectionHolder holder(BufferCS);

		if(FAILED(SoundBuffer->GetCurrentPosition(&pp, &wp))) return 0;
	}


	pp += aheadsamples * Format.Format.nBlockAlign;
	pp = pp % BufferBytes;

	if(L1BufferSamplePositions[pp/AccessUnitBytes] == (tjs_uint64)(tjs_int64)-1)
		return 0;

	pp /= Format.Format.nBlockAlign;

	tjs_int buffersamples = BufferBytes / Format.Format.nBlockAlign;

	// copy to distination buffer
	tjs_int writtensamples = 0;
	if(numsamples > 0)
	{
		while(true)
		{
			tjs_int bufrest = buffersamples - pp;
			tjs_int copysamples = (bufrest > numsamples ? numsamples : bufrest);

			CopyVisBuffer(dest, VisBuffer + pp * Format.Format.nBlockAlign,
				copysamples, channels);

			numsamples -= copysamples;
			writtensamples += copysamples;
			if(numsamples <= 0) break;

			dest += channels * copysamples;
			pp += copysamples;
			pp = pp % buffersamples;
		}
	}

	return writtensamples;
}
//---------------------------------------------------------------------------




//---------------------------------------------------------------------------
// tTJSNC_WaveSoundBuffer
//---------------------------------------------------------------------------
tTJSNativeInstance *tTJSNC_WaveSoundBuffer::CreateNativeInstance()
{
	return new tTJSNI_WaveSoundBuffer();
}
//---------------------------------------------------------------------------



//---------------------------------------------------------------------------
// TVPCreateNativeClass_WaveSoundBuffer
//---------------------------------------------------------------------------
tTJSNativeClass * TVPCreateNativeClass_WaveSoundBuffer()
{
	tTJSNativeClass *cls = new tTJSNC_WaveSoundBuffer();
	static tjs_uint32 TJS_NCM_CLASSID;
	TJS_NCM_CLASSID = tTJSNC_WaveSoundBuffer::ClassID;

//----------------------------------------------------------------------
// methods
//----------------------------------------------------------------------
TJS_BEGIN_NATIVE_METHOD_DECL(/*func. name*/freeDirectSound)  /* static */
{
	// release directsound
	TVPReleaseDirectSound();

	return TJS_S_OK;
}
TJS_END_NATIVE_METHOD_DECL_OUTER(/*object to register*/cls,
	/*func. name*/freeDirectSound)
//----------------------------------------------------------------------
TJS_BEGIN_NATIVE_METHOD_DECL(/*func. name*/getVisBuffer)  /* static */
{
	// get samples for visualization 
	TJS_GET_NATIVE_INSTANCE(/*var. name*/_this,
		/*var. type*/tTJSNI_WaveSoundBuffer);

	if(numparams < 3) return TJS_E_BADPARAMCOUNT;
	tjs_int16 *dest = (tjs_int16*)(tjs_int)(*param[0]);

	tjs_int ahead = 0;
	if(numparams >= 4) ahead = (tjs_int)*param[3];

	tjs_int res = _this->GetVisBuffer(dest, *param[1], *param[2], ahead);

	if(result) *result = res;

	return TJS_S_OK;
}
TJS_END_NATIVE_METHOD_DECL_OUTER(/*object to register*/cls,
	/*func. name*/getVisBuffer)
//----------------------------------------------------------------------



//----------------------------------------------------------------------
// properties
//----------------------------------------------------------------------
TJS_BEGIN_NATIVE_PROP_DECL(useVisBuffer)
{
	TJS_BEGIN_NATIVE_PROP_GETTER
	{
		TJS_GET_NATIVE_INSTANCE(/*var. name*/_this,
			/*var. type*/tTJSNI_WaveSoundBuffer);

		*result = _this->GetUseVisBuffer();
		return TJS_S_OK;
	}
	TJS_END_NATIVE_PROP_GETTER

	TJS_BEGIN_NATIVE_PROP_SETTER
	{
		TJS_GET_NATIVE_INSTANCE(/*var. name*/_this,
			/*var. type*/tTJSNI_WaveSoundBuffer);

		_this->SetUseVisBuffer((tjs_int)*param);

		return TJS_S_OK;
	}
	TJS_END_NATIVE_PROP_SETTER
}
TJS_END_NATIVE_PROP_DECL_OUTER(cls, useVisBuffer)
//----------------------------------------------------------------------
	return cls;
}
//---------------------------------------------------------------------------

