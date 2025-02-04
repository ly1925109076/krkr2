//---------------------------------------------------------------------------
/*
	TVP2 ( T Visual Presenter 2 )  A script authoring tool
	Copyright (C) 2000-2004  W.Dee <dee@kikyou.info>

	See details of license at "license.txt"
*/
//---------------------------------------------------------------------------
// Definition of Messages and Message Related Utilities
//---------------------------------------------------------------------------
// 2003/11/13 W.Dee          Changed TVPGetVersion to use TVPGetFileVersionOf.
//---------------------------------------------------------------------------
#include "tjsCommHead.h"
#pragma hdrstop

#include "MsgIntf.h"
#include "MsgImpl.h"
#include "PluginImpl.h"

//---------------------------------------------------------------------------
// version retrieving
//---------------------------------------------------------------------------
void TVPGetVersion(void)
{
	static bool DoGet=true;
	if(DoGet)
	{
		DoGet = false;

		TVPVersionMajor = 0;
		TVPVersionMinor = 0;
		TVPVersionRelease = 0;
		TVPVersionBuild = 0;

		TVPGetFileVersionOf(_argv[0], TVPVersionMajor, TVPVersionMinor,
			TVPVersionRelease, TVPVersionBuild);
	}
}
//---------------------------------------------------------------------------


