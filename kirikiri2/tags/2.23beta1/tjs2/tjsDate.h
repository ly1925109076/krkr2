//---------------------------------------------------------------------------
/*
	TJS2 Script Engine
	Copyright (C) 2000-2004  W.Dee <dee@kikyou.info>

	See details of license at "license.txt"
*/
//---------------------------------------------------------------------------
// Date class implementation
//---------------------------------------------------------------------------

#ifndef tjsDateH
#define tjsDateH

#include <time.h>
#include "tjsNative.h"

namespace TJS
{
//---------------------------------------------------------------------------
class tTJSNI_Date : public tTJSNativeInstance
{
public:
	tTJSNI_Date();
	time_t DateTime;
private:
};

//---------------------------------------------------------------------------
class tTJSNC_Date : public tTJSNativeClass
{
public:
	tTJSNC_Date();

	static tjs_uint32 ClassID;

private:
	tTJSNativeInstance *CreateNativeInstance();
};
//---------------------------------------------------------------------------
} // namespace TJS

#endif
