//---------------------------------------------------------------------------
/*
	TVP2 ( T Visual Presenter 2 )  A script authoring tool
	Copyright (C) 2000-2004  W.Dee <dee@kikyou.info>

	See details of license at "license.txt"
*/
//---------------------------------------------------------------------------
// Script Event/Window Handling and Dispatching / System Idle Event Delivering
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
#ifndef EventIntfH
#define EventIntfH

#include "tjsNative.h"



//---------------------------------------------------------------------------
// Event dispatching
//---------------------------------------------------------------------------
extern void TVPDeliverAllEvents(); // called from (indirectly) the OS

extern bool TVPEventDisabled;  // do not write to this variable directly

extern void TVPInvokeEvents();
	// implement this in each platform,
	// to ensure calling "TVPDeliverAllEvents" when the Application is
	// ready to deliver the events.
extern void TVPEventReceived();
	// implement this in each platform.
	// notifies that events are delivered and ensure being ready for next event.

extern void TVPCallDeliverAllEventsOnIdle();
	// implement this in each platform.
	// once return control to OS, and set to call TVPInvokeEvents() after it.



//---------------------------------------------------------------------------
// somewhat public

TJS_EXP_FUNC_DEF(void, TVPBreathe, ());
	// implement this in each platform
	// to handle OS's message events
	// this should be called when in a long time processing, something like a
	// network access, to reply to user's Windowing message, repainting, etc...
	// in TVPBreathe, TVP events must not be invoked. ( happened events over the
	// long time processing are pending until next timing of message delivering. )

TJS_EXP_FUNC_DEF(bool, TVPGetBreathing, ()); // return whether now is in event breathing

TJS_EXP_FUNC_DEF(void, TVPSetSystemEventDisabledState, (bool en));
	/*
		sets whether system overall event handling is enabled.
		this works distinctly from TVPEventDisabled.
	*/

TJS_EXP_FUNC_DEF(bool, TVPGetSystemEventDisabledState, ());
//---------------------------------------------------------------------------




/*[*/
//---------------------------------------------------------------------------
// Script Event Related
//---------------------------------------------------------------------------
#define TVP_EPT_POST			0x00  // normal post, simply add to queue
#define TVP_EPT_REMOVE_POST		0x01
		// remove event in pending queue that has same target, source, tag and
		// name before post
#define TVP_EPT_IMMEDIATE		0x02
		// the event will be delivered immediately

#define TVP_EPT_DISCARDABLE		0x10
		// the event can be discarded when event system is disabled

#define TVP_EPT_EXCLUSIVE		0x20
		// (with TVP_EPT_POST only)
		// the event is given priority and other posted events are not processed
		// until the exclusive event is processed.

#define TVP_EPT_METHOD_MASK		0x0f
/*]*/
//---------------------------------------------------------------------------
TJS_EXP_FUNC_DEF(void, TVPPostEvent, (iTJSDispatch2 * source, iTJSDispatch2 *target,
	ttstr &eventname, tjs_uint32 tag, tjs_uint32 flag,
	tjs_uint numargs, tTJSVariant *args));
		// posts TVP event. this function itself is thread-safe.

TJS_EXP_FUNC_DEF(tjs_int, TVPCancelEvents, (iTJSDispatch2 * source, iTJSDispatch2 *target,
	const ttstr &eventname, tjs_uint32 tag = 0));
		// removes events that has specified source/target/name/tag.
		// tag == 0 removes all tag from queue.
		// returns count of removed events.

TJS_EXP_FUNC_DEF(bool, TVPAreEventsInQueue, (iTJSDispatch2 * source, iTJSDispatch2 *target,
	const ttstr &eventname, tjs_uint32 tag));
		// returns whether the events are in queue that have specified
		// source/target/name/tag.
		// tag == 0 matches all tag in queue.

TJS_EXP_FUNC_DEF(tjs_int, TVPCountEventsInQueue, (iTJSDispatch2 * source, iTJSDispatch2 *target,
	const ttstr &eventname, tjs_uint32 tag));
		// returns count of the events in queue that have specified
		// source/target/name/tag.
		// tag == 0 matches all tag in queue.

TJS_EXP_FUNC_DEF(void, TVPCancelEventsByTag, (iTJSDispatch2 * source, iTJSDispatch2 *target,
	tjs_uint32 tag = 0));
		// removes all events which have the same source/target/tag.
		// tag == 0 removes all tag from queue.

TJS_EXP_FUNC_DEF(void, TVPCancelSourceEvents, (iTJSDispatch2 * source));
		// removes all events that has specified source.
//---------------------------------------------------------------------------




//---------------------------------------------------------------------------
// Window update event related
//---------------------------------------------------------------------------
class tTJSNI_BaseWindow;
extern void TVPPostWindowUpdate(tTJSNI_BaseWindow *window);
extern void TVPRemoveWindowUpdate(tTJSNI_BaseWindow *window);
extern void TVPDeliverWindowUpdateEvents();
//---------------------------------------------------------------------------




//---------------------------------------------------------------------------
// User input event related
//---------------------------------------------------------------------------
class tTVPBaseInputEvent // base user input event class
{
	void * Source;
public:
	tTVPBaseInputEvent(void *source) { Source = source; }
	virtual ~tTVPBaseInputEvent() {};
	virtual void Deliver() const = 0;
	void * GetSource() const { return Source; }
};
//---------------------------------------------------------------------------
extern void TVPPostInputEvent(tTVPBaseInputEvent *ev, tjs_uint32 flags = 0);
extern void TVPCancelInputEvents(void * source);
extern tjs_int TVPGetInputEventCount();
//---------------------------------------------------------------------------






//---------------------------------------------------------------------------
// TVPCreateEventObject
//---------------------------------------------------------------------------
TJS_EXP_FUNC_DEF(iTJSDispatch2 *, TVPCreateEventObject, (const tjs_char *type,
	iTJSDispatch2 *targthis, iTJSDispatch2 *targ));
//---------------------------------------------------------------------------


//---------------------------------------------------------------------------
// some macros for driving "action" method
//---------------------------------------------------------------------------
extern ttstr TVPActionName;
#define TVP_ACTION_INVOKE_BEGIN(argnum, name, object) \
	{ \
		if(numparams < (argnum)) return TJS_E_BADPARAMCOUNT; \
		tjs_int arg_count = 0; \
		iTJSDispatch2 *evobj = TVPCreateEventObject(TJS_W(name), (object), \
			(object)); \
		tTJSVariant evval(evobj, evobj); \
		evobj->Release(); \

#define TVP_ACTION_INVOKE_MEMBER(name) \
	{\
		static ttstr member_name(TJS_W(name)); \
		evobj->PropSet(TJS_MEMBERENSURE|TJS_IGNOREPROP, \
			member_name.c_str(), member_name.GetHint(), param[arg_count++], \
			evobj); \
	}

#define TVP_ACTION_INVOKE_END(object) \
		tTJSVariant *pevval = &evval; \
		(object).FuncCall(0, TVPActionName.c_str(), TVPActionName.GetHint(), \
			result, 1, &pevval, NULL); \
	}

#define TVP_ACTION_INVOKE_END_NAME(object, name, hint) \
		tTJSVariant *pevval = &evval; \
		(object).FuncCall(0, (name), (hint), \
			result, 1, &pevval, NULL); \
	}
//---------------------------------------------------------------------------




//---------------------------------------------------------------------------
// Continuous Event related
//---------------------------------------------------------------------------
/*[*/
class tTVPContinuousEventCallbackIntf // callback class for continuous event delivering
{
public:
	virtual void TJS_INTF_METHOD OnContinuousCallback(tjs_uint64 tick) = 0;
};
/*]*/
//---------------------------------------------------------------------------
TJS_EXP_FUNC_DEF(void, TVPAddContinuousEventHook, (tTVPContinuousEventCallbackIntf *cb));
TJS_EXP_FUNC_DEF(void, TVPRemoveContinuousEventHook, (tTVPContinuousEventCallbackIntf *cb));

extern void TVPBeginContinuousEvent();
	// must be implemented in each platforms
	// this must begin calling TVPDeliverContinuousEvent

extern void TVPEndContinuousEvent();
	// must be implemented in each platforms
	// this must stop calling TVPDeliverContinuousEvent

extern void TVPDeliverContinuousEvent();
	// must be called by each platforms's implementation

extern void TVPAddContinuousHandler(tTJSVariantClosure clo);
	// add callback function written in TJS
extern void TVPRemoveContinuousHandler(tTJSVariantClosure clo);
	// remove callback function added by TVPAddIdleHandler
//---------------------------------------------------------------------------




/*[*/
//---------------------------------------------------------------------------
// System "Compact" Event related
//---------------------------------------------------------------------------
#define TVP_COMPACT_LEVEL_IDLE        5  // the application is in idle state
#define TVP_COMPACT_LEVEL_DEACTIVATE 10  // the application had been deactivated
#define TVP_COMPACT_LEVEL_MINIMIZE   15  // the application had been minimized
#define TVP_COMPACT_LEVEL_MAX       100  // strongest level, should clear all caches
//---------------------------------------------------------------------------
class tTVPCompactEventCallbackIntf // callback class for compact event delivering
{
public:
	virtual void TJS_INTF_METHOD OnCompact(tjs_int level) = 0;
};
/*]*/
//---------------------------------------------------------------------------
TJS_EXP_FUNC_DEF(void, TVPAddCompactEventHook, (tTVPCompactEventCallbackIntf *cb));
TJS_EXP_FUNC_DEF(void, TVPRemoveCompactEventHook, (tTVPCompactEventCallbackIntf *cb));

extern void TVPDeliverCompactEvent(tjs_int level);
	// must be called by each platforms's implementation
//---------------------------------------------------------------------------





/*
	AsyncTrigger is a class for invoking events at asynchronous.
	Script can trigger event but the event is not delivered immediately,
	is delivered at next event flush phase.
*/
/*[*/
//---------------------------------------------------------------------------
// AsyncTrigger related
//---------------------------------------------------------------------------
enum tTVPAsyncTriggerMode
{
	atmNormal, atmExclusive, atmAtIdle
};
/*]*/

//---------------------------------------------------------------------------
// tTJSNI_AsyncTrigger : TJS AsyncTrigger native instance
//---------------------------------------------------------------------------
class tTJSNI_AsyncTrigger : public tTJSNativeInstance,
	tTVPContinuousEventCallbackIntf
{
	typedef tTJSNativeInstance inherited;

protected:
	iTJSDispatch2 *Owner;
	tTJSVariantClosure ActionOwner; // object to send action
	ttstr ActionName;

	bool Cached; // cached operation
	tTVPAsyncTriggerMode Mode; // event mode
	tjs_int IdlePendingCount;

public:
	tTJSNI_AsyncTrigger();
	tjs_error TJS_INTF_METHOD
		Construct(tjs_int numparams, tTJSVariant **param,
			iTJSDispatch2 *tjs_obj);
	void TJS_INTF_METHOD Invalidate();

protected:
	void TJS_INTF_METHOD OnContinuousCallback(tjs_uint64 tick);


public:
	tTJSVariantClosure GetActionOwnerNoAddRef() const { return ActionOwner; }
	ttstr & GetActionName() { return ActionName; }

	void Trigger();
	void Cancel();

	bool GetCached() const { return Cached; }
	void SetCached(bool b);

	tTVPAsyncTriggerMode GetMode() const { return Mode; }
	void SetMode(tTVPAsyncTriggerMode m);
};
//---------------------------------------------------------------------------



//---------------------------------------------------------------------------
// tTJSNC_AsyncTrigger : TJS AsyncTrigger class
//---------------------------------------------------------------------------
class tTJSNC_AsyncTrigger : public tTJSNativeClass
{
	typedef tTJSNativeClass inherited;

public:
	tTJSNC_AsyncTrigger();
	static tjs_uint32 ClassID;

protected:
	tTJSNativeInstance *CreateNativeInstance();
};
//---------------------------------------------------------------------------
extern tTJSNativeClass * TVPCreateNativeClass_AsyncTrigger();
//---------------------------------------------------------------------------



#endif
