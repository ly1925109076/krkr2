//---------------------------------------------------------------------------
/*
	TVP2 ( T Visual Presenter 2 )  A script authoring tool
	Copyright (C) 2000-2004  W.Dee <dee@kikyou.info>

	See details of license at "license.txt"
*/
//---------------------------------------------------------------------------
// Layer Management
//---------------------------------------------------------------------------
#ifndef LayerIntfH
#define LayerIntfH


#include "tjsNative.h"
#include "tvpfontstruc.h"
#include "ComplexRect.h"
#include "drawable.h"
#include "tvpinputdefs.h"
#include "TransIntf.h"
#include "EventIntf.h"
#include "ObjectList.h"




//---------------------------------------------------------------------------
// global flags
//---------------------------------------------------------------------------
extern bool TVPFreeUnusedLayerCache;


//---------------------------------------------------------------------------
// initial bitmap holder ( since tTVPBaseBitmap cannot create empty bitmap )
//---------------------------------------------------------------------------
const tTVPBaseBitmap & TVPGetInitialBitmap();
//---------------------------------------------------------------------------


//---------------------------------------------------------------------------
// global options
//---------------------------------------------------------------------------
enum tTVPGraphicSplitOperationType
{ gsotNone, gsotSimple, gsotInterlace, gsotBiDirection };
extern tTVPGraphicSplitOperationType TVPGraphicSplitOperationType;
//---------------------------------------------------------------------------


/*[*/
//---------------------------------------------------------------------------
// drawn face types
//---------------------------------------------------------------------------
enum tTVPDrawFace
{
	dfBoth,
	dfMain,
	dfMask,
	dfProvince
};
//---------------------------------------------------------------------------
/*]*/



/*[*/
//---------------------------------------------------------------------------
// layer hit test type
//---------------------------------------------------------------------------
enum tTVPHitType {htMask, htProvince};
//---------------------------------------------------------------------------
/*]*/



/*[*/
//---------------------------------------------------------------------------
// color key types
//---------------------------------------------------------------------------
#define TVP_clAdapt			((tjs_uint32)(0x1ffffff)
#define TVP_clNone			((tjs_uint32)(0x2ffffff)
#define TVP_Is_clPalIdx(n)	((tjs_uint32)(((n)&0xff000000) == 0x3000000)
#define TVP_get_clPalIdx(n) ((tjs_uint32)((n)&0xff)
//---------------------------------------------------------------------------
/*]*/



//---------------------------------------------------------------------------
// TVPToActualColor :  convert color identifier to actual color
//---------------------------------------------------------------------------
TJS_EXP_FUNC_DEF(tjs_uint32, TVPToActualColor, (tjs_uint32 col));
	// implement in each platform
TJS_EXP_FUNC_DEF(tjs_uint32, TVPFromActualColor, (tjs_uint32 col));
	// implement in each platform
//---------------------------------------------------------------------------





//---------------------------------------------------------------------------
// tTJSNI_BaseLayer implementation
//---------------------------------------------------------------------------
class tTJSNI_BaseWindow;
class tTVPBaseBitmap;
class tTVPLayerManager;
class tTJSNI_BaseLayer :
	public tTJSNativeInstance, public tTVPDrawable,
	public tTVPCompactEventCallbackIntf
{
	friend class tTVPLayerManager;

	iTJSDispatch2 *Owner;
	tTJSVariantClosure ActionOwner;

	//---------------------------------------------- object lifetime stuff --
public:
	tTJSNI_BaseLayer(void);
	~tTJSNI_BaseLayer();
	tjs_error TJS_INTF_METHOD Construct(tjs_int numparams, tTJSVariant **param,
		iTJSDispatch2 *tjs_obj);
	void TJS_INTF_METHOD Invalidate();

	iTJSDispatch2 * GetOwnerNoAddRef() const { return Owner; }

	tTJSVariantClosure GetActionOwnerNoAddRef() const { return ActionOwner; }


private:
	bool Shutdown; // true when shutting down
	bool CompactEventHookInit;
	void RegisterCompactEventHook();
	void TJS_INTF_METHOD OnCompact(tjs_int level); // method from tTVPCompactEventCallbackIntf

	//----------------------------------------------- interface to manager --
private:
	tTVPLayerManager *Manager;
public:
	tTVPLayerManager *GetManager() const { return Manager; }
	tTJSNI_BaseWindow * GetWindow() const;

	//---------------------------------------------------- tree management --
private:
	tTJSNI_BaseLayer *Parent;
	tObjectList<tTJSNI_BaseLayer> Children;
	ttstr Name;
	bool Visible;
	tjs_int Opacity;
	tjs_uint OrderIndex;
		// index in parent's 'Children' array.
		// do not access this variable directly; use GetOrderIndex() instead.
	tjs_uint OverallOrderIndex;
		// index in overall tree.
		// use GetOverallOrderIndex instead
	bool AbsoluteOrderMode; // manage Z order in absolute Z position
	tjs_int AbsoluteOrderIndex;
		// do not access this variable directly; use GetAbsoluteOrderIndex() instead. 

	bool ChildrenOrderIndexValid;

	tjs_int VisibleChildrenCount;
	iTJSDispatch2 *ChildrenArray; // Array object which holds children array ...
	iTJSDispatch2 *ArrayClearMethod; // holds Array.clear method
	bool ChildrenArrayValid;

	void Join(tTJSNI_BaseLayer *parent); // join to the parent
	void Part(); // part from the parent

	void AddChild(tTJSNI_BaseLayer *child);
	void SeverChild(tTJSNI_BaseLayer *child);

public:
	iTJSDispatch2 * GetChildrenArrayObjectNoAddRef();

private:

	tTJSNI_BaseLayer* GetAncestorChild(tTJSNI_BaseLayer *ancestor);
		// retrieve "ancestor"'s child that is ancestor of this ( can be thisself )
	bool IsAncestor(tTJSNI_BaseLayer *ancestor);
		// is "ancestor" is ancestor of this layer ? (cannot be itself)
	bool IsAncestorOrSelf(tTJSNI_BaseLayer *ancestor)
		{ return ancestor == this || IsAncestor(ancestor); }
			// same as IsAncestor (but can be itself)

	void RecreateOverallOrderIndex(tjs_uint& index,
		std::vector<tTJSNI_BaseLayer*>& nodes);

	void Exchange(tTJSNI_BaseLayer *target, bool keepchild = false);
		// exchange this for the other layer
	void Swap(tTJSNI_BaseLayer *target) // swap this for the other layer
		{ Exchange(target, true); }

	tjs_int GetVisibleChildrenCount()
	{ if(VisibleChildrenCount == -1) CheckChildrenVisibleState();
		return VisibleChildrenCount; }

	void CheckChildrenVisibleState();
	void NotifyChildrenVisualStateChanged();

	void RecreateOrderIndex();

public:
	tjs_uint GetOrderIndex()
	{
		if(!Parent) return 0;
		if(Parent->ChildrenOrderIndexValid) return OrderIndex;
		Parent->RecreateOrderIndex();
		return OrderIndex;
	}

	// SetOrderIndex are below

	tjs_uint GetOverallOrderIndex();

	tTJSNI_BaseLayer * GetParent() const { return Parent; }
	void SetParent(tTJSNI_BaseLayer *parent) { Join(parent); }

	tjs_uint GetCount() { return Children.GetActualCount(); }

	tTJSNI_BaseLayer * GetChildren(tjs_int idx)
		{ Children.Compact(); return Children[idx]; }

	const ttstr & GetName() const { return Name; }
	void SetName(const ttstr &name) { Name = name; }
	bool GetVisible() const { return Visible; }
	void SetVisible(bool st);
	tjs_int GetOpacity() const { return Opacity; }
	void SetOpacity(tjs_int opa);
	bool IsSeen() const { return Visible && Opacity != 0; }
	bool IsPrimary() const;

	bool GetParentVisible() const; // is parent visible? this does not check opacity
	bool GetNodeVisible() { return GetParentVisible() && Visible; } // this does not check opacity

	tTJSNI_BaseLayer * GetNeighborAbove(bool loop = false);
	tTJSNI_BaseLayer * GetNeighborBelow(bool loop = false);

private:
	void CheckZOrderMoveRule(tTJSNI_BaseLayer *lay);
	void ChildChangeOrder(tjs_int from, tjs_int to);
	void ChildChangeAbsoluteOrder(tjs_int from, tjs_int abs_to);


public:
	void MoveBefore(tTJSNI_BaseLayer *lay); // move before sibling : lay
	void MoveBehind(tTJSNI_BaseLayer *lay); // move behind sibling : lay

	void SetOrderIndex(tjs_int index);

	void BringToBack(); // to most back position
	void BringToFront(); // to most front position

	tjs_int GetAbsoluteOrderIndex(); // retrieve order index in absolute position
	void SetAbsoluteOrderIndex(tjs_int index);

	bool GetAbsoluteOrderMode() const { return AbsoluteOrderMode; }
	void SetAbsoluteOrderMode(bool b);

public:
	void DumpStructure(int level = 0);  // dump layer structure to dms



	//--------------------------------------------- layer type management --
private:
	tTVPLayerType Type;
	tTVPLayerType DrawType;

	void NotifyLayerTypeChange();

public:
	tTVPLayerType GetType() const { return Type; }
	void SetType(tTVPLayerType type);

	bool GetHasAlpha() const { return DrawType == ltTransparent; }

	//-------------------------------------------- geographical management --
private:
	tTVPRect Rect;
	bool ExposedRegionValid;
	tTVPComplexRect ExposedRegion; // exposed region (no-children-overlapped-region)
	tTVPComplexRect OverlappedRegion; // overlapped region (overlapped by children)
		// above two shuld not be accessed directly

	void SetToCreateExposedRegion() { ExposedRegionValid = false; }
	void CreateExposedRegion(); // create exposed/overlapped region information
	const tTVPComplexRect & GetExposedRegion()
		{ if(!ExposedRegionValid) CreateExposedRegion();
		  return ExposedRegion; }
	const tTVPComplexRect & GetOverlappedRegion()
		{ if(!ExposedRegionValid) CreateExposedRegion();
		  return OverlappedRegion; }


	void InternalSetSize(tjs_uint width, tjs_uint height);
	void InternalSetBounds(const tTVPRect &rect);

	void ConvertToParentRects(tTVPComplexRect &dest, const tTVPComplexRect &src);

private:

public:
	const tTVPRect & GetRect() const { return Rect; }
	tjs_int GetLeft() const { return Rect.left; }
	tjs_int GetTop() const { return Rect.top; }
	tjs_uint GetWidth() const { return (tjs_uint)Rect.get_width(); }
	tjs_uint GetHeight() const { return (tjs_uint)Rect.get_height(); }


	void SetLeft(tjs_int left);
	void SetTop(tjs_int top);
	void SetPosition(tjs_int left, tjs_int top);
	void SetWidth(tjs_uint width);
	void SetHeight(tjs_uint height);
	void SetSize(tjs_uint width, tjs_uint height);
	void SetBounds(const tTVPRect &rect);

	void ParentRectToChildRect(tTVPRect &rect);

	void ToPrimaryCoordinates(tjs_int &x, tjs_int &y) const;
	void FromPrimaryCoordinates(tjs_int &x, tjs_int &y) const;

	//-------------------------------------------- image buffer management --
private:
	tTVPBaseBitmap *MainImage;
	tTVPBaseBitmap *ProvinceImage;
	tjs_uint32 NeutralColor;

	void ChangeImageSize(tjs_uint width, tjs_uint height);

	tjs_int ImageLeft; // image offset left
	tjs_int ImageTop; // image offset top

	void AllocateImage();
	void DeallocateImage();
	void AllocateProvinceImage();
	void DeallocateProvinceImage();
	void AllocateDefaultImage();

public:
	void AssignImages(tTJSNI_BaseLayer *src); // assign image content

	void SetNeutralColor(tjs_uint32 color) { NeutralColor = color; }
	tjs_uint32 GetNeutralColor() const { return NeutralColor; }

public:
	void SetImageLeft(tjs_int left);
	tjs_int GetImageLeft() const;
	void SetImageTop(tjs_int top);
	tjs_int GetImageTop() const;
	void SetImagePosition(tjs_int left, tjs_int top);
	void SetImageWidth(tjs_uint width);
	tjs_uint GetImageWidth() const;
	void SetImageHeight(tjs_uint height);
	tjs_uint GetImageHeight() const;

private:
	void InternalSetImageSize(tjs_uint width, tjs_uint height);
public:
	void SetImageSize(tjs_uint width, tjs_uint height);
private:
	void ImageLayerSizeChanged(); // called from geographical management
public:
	tTVPBaseBitmap * GetMainImage() { ApplyFont(); return MainImage; }
	tTVPBaseBitmap * GetProvinceImage() { ApplyFont(); return ProvinceImage; }
		// exporting of these two members is a bit dangerous
		// in the manner of protecting
		// ( user can resize the image with the returned object !? )

	void IndependMainImage(bool copy = true);
	void IndependProvinceImage(bool copy = true);

	void SaveLayerImage(const ttstr &name, const ttstr &type);

	void LoadImages(const ttstr &name, tjs_uint32 colorkey);

	void LoadProvinceImage(const ttstr &name);

	tjs_uint32 GetMainPixel(tjs_int x, tjs_int y) const;
	void SetMainPixel(tjs_int x, tjs_int y, tjs_uint32 color);
	tjs_int GetMaskPixel(tjs_int x, tjs_int y) const;
	void SetMaskPixel(tjs_int x, tjs_int y, tjs_int mask);
	tjs_int GetProvincePixel(tjs_int x, tjs_int y) const;
	void SetProvincePixel(tjs_int x, tjs_int y, tjs_int n);

	const void * GetMainImagePixelBuffer() const;
	void * GetMainImagePixelBufferForWrite();
	tjs_int GetMainImagePixelBufferPitch() const;
	const void * GetProvinceImagePixelBuffer() const;
	void * GetProvinceImagePixelBufferForWrite();
	tjs_int GetProvinceImagePixelBufferPitch() const;

	//---------------------------------- input event / hit test management --
private:
	tTVPHitType HitType;
	tjs_int HitThreshold;
	bool OnHitTest_Work;
	bool Enabled; // is layer enabled for input?
	bool EnabledWork; // work are for delivering onNodeEnabled or onNodeDisabled
	bool Focusable; // is layer focusable ?
	bool JoinFocusChain; // does layer join the focus chain ?
	tTJSNI_BaseLayer *FocusWork;

	tjs_int AttentionLeft; // attention position
	tjs_int AttentionTop;
	tjs_int UseAttention;

	tTVPImeMode ImeMode; // ime mode

	tjs_int Cursor; // mouse cursor
	tjs_int CursorX_Work; // holds x-coordinate in SetCursorX and SetCursorY

public:
	void SetCursorByStorage(const ttstr &storage);
	void SetCursorByNumber(tjs_int num);


private:
	tjs_int GetLayerActiveCursor(); // return layer's actual (active) mouse cursor
	void SetCurrentCursorToWindow(); // set current layer cusor to the window

private:
	bool _HitTestNoVisibleCheck(tjs_int x, tjs_int y);

public:
	bool HitTestNoVisibleCheck(tjs_int x, tjs_int y);
	void SetHitTestWork(bool b) { OnHitTest_Work = b; }

	tjs_int GetCursor() const { return Cursor; }

private:
	ttstr Hint; // layer hint text
	bool ShowParentHint; // show parent's hint ?

	void SetCurrentHintToWindow(); // set current hint to the window

public:
	const ttstr & GetHint() const { return Hint; }
	void SetHint(const ttstr & hint);
	bool GetShowParentHint() const { return ShowParentHint; }
	void SetShowParentHint(bool b) { ShowParentHint = b; }


public:
	tjs_int GetAttentionLeft() const { return AttentionLeft; }
	void SetAttentionLeft(tjs_int l);
	tjs_int GetAttentionTop() const { return AttentionTop; }
	void SetAttentionTop(tjs_int t);
	void SetAttentionPoint(tjs_int l, tjs_int t);
	bool GetUseAttention() const { return UseAttention; }
	void SetUseAttention(bool b);

private:

public:
	void SetImeMode(tTVPImeMode mode);
	tTVPImeMode GetImeMode() const { return ImeMode; }

public:
	// these mouse cursor position functions are based on layer's coordinates,
	// not layer image's coordinates.
	tjs_int GetCursorX();
	void SetCursorX(tjs_int x);
	tjs_int GetCursorY();
	void SetCursorY(tjs_int y);
	void SetCursorPos(tjs_int x, tjs_int y);

private:
	bool GetMostFrontChildAt(tjs_int x, tjs_int y, tTJSNI_BaseLayer **lay,
		const tTJSNI_BaseLayer *except = NULL, bool get_disabled = false);

public:
	tTJSNI_BaseLayer * GetMostFrontChildAt(tjs_int x, tjs_int y, bool exclude_self = false,
		bool get_disabled = false);

	tTVPHitType GetHitType() const { return HitType; }
	void SetHitType(tTVPHitType type) { HitType = type; }

	tjs_int GetHitThreshold() const { return HitThreshold; }
	void SetHitThreshold(tjs_int t) { HitThreshold = t; }

private:
	void FireClick(tjs_int x, tjs_int y);
	void FireDoubleClick(tjs_int x, tjs_int y);
	void FireMouseDown(tjs_int x, tjs_int y, tTVPMouseButton mb, tjs_uint32 flags);
	void FireMouseUp(tjs_int x, tjs_int y, tTVPMouseButton mb, tjs_uint32 flags);
	void FireMouseMove(tjs_int x, tjs_int y, tjs_uint32 flags);
	void FireMouseEnter();
	void FireMouseLeave();

public:
	void ReleaseCapture();

private:
	tTJSNI_BaseLayer *SearchFirstFocusable();

	tTJSNI_BaseLayer *_GetPrevFocusable(); // search next focusable layer backward
	tTJSNI_BaseLayer *_GetNextFocusable(); // search next focusable layer backward

public:
	tTJSNI_BaseLayer *GetPrevFocusable();
	tTJSNI_BaseLayer *GetNextFocusable();
	void SetFocusWork(tTJSNI_BaseLayer *lay)
		{ FocusWork = lay; }

public:

	tTJSNI_BaseLayer * FocusPrev();
	tTJSNI_BaseLayer * FocusNext();

	void SetFocusable(bool b);
	bool GetFocusable() const { return Focusable; }

	void SetJoinFocusChain(bool b) { JoinFocusChain = b; }
	bool GetJoinFocusChain() const { return JoinFocusChain; }

	bool CheckFocusable() const { return Focusable && Visible && Enabled; }
	bool ParentFocusable() const;
	bool GetNodeFocusable()
		{ return CheckFocusable() && ParentFocusable() && !IsDisabledByMode(); }

	bool SetFocus(bool direction = true);

	bool GetFocused();

private:
	void FireBlur(tTJSNI_BaseLayer *prevfocused);
	void FireFocus(tTJSNI_BaseLayer *prevfocused, bool direction);
	tTJSNI_BaseLayer * FireBeforeFocus(tTJSNI_BaseLayer *prevfocused, bool direction);

public:
	void SetMode();
	void RemoveMode();

	bool IsDisabledByMode();  // is "this" layer disable by modal layer?

private:

public:
	void NotifyNodeEnabledState();
	void SaveEnabledWork();
	void SetEnabled(bool b);
	bool ParentEnabled();
	bool GetEnabled() const { return Enabled; }

	bool GetNodeEnabled()
		{ return GetEnabled() && ParentEnabled() && !IsDisabledByMode(); }

private:
	void FireKeyDown(tjs_uint key, tjs_uint32 shift);
	void FireKeyUp(tjs_uint key, tjs_uint32 shift);
	void FireKeyPress(tjs_char key);
	void FireMouseWheel(tjs_uint32 shift, tjs_int delta, tjs_int x, tjs_int y);

public:
	void DefaultKeyDown(tjs_uint key, tjs_uint32 shift); // default keyboard behavior
	void DefaultKeyUp(tjs_uint key, tjs_uint32 shift); // default keyboard behavior
	void DefaultKeyPress(tjs_char key); // default keyboard behavior

	//--------------------------------------------------- cache management --
private:
	tTVPBaseBitmap *CacheBitmap;
	tTVPComplexRect CachedRegion;

	void AllocateCache();
	void ResizeCache();
	void DeallocateCache();
	void DispSizeChanged(); // is called from geographical management
	void CompactCache(); // free cache image if the cache is not needed

	tjs_uint CacheEnabledCount;

public:
	bool GetCacheEnabled() const { return CacheEnabledCount!=0; }

	tjs_uint IncCacheEnabledCount();
	tjs_uint DecCacheEnabledCount();

	//--------------------------------------------- drawing function stuff --
private:
	tTVPDrawFace DrawFace; // current drawing layer face
	bool ImageModified; // flag to know modification of layer image
	tTVPRect ClipRect; // clipping rectangle
public:
	void SetClip(tjs_int left, tjs_int top, tjs_int width, tjs_int height);
	tjs_int GetClipLeft() const { return ClipRect.left; }
	void SetClipLeft(tjs_int left);
	tjs_int GetClipTop() const { return ClipRect.top; }
	void SetClipTop(tjs_int top);
	tjs_int GetClipWidth() const { return ClipRect.right - ClipRect.left; }
	void SetClipWidth(tjs_int width);
	tjs_int GetClipHeight() const { return ClipRect.bottom - ClipRect.top; }
	void SetClipHeight(tjs_int height);

private:
	bool ClipDestPointAndSrcRect(tjs_int &dx, tjs_int &dy,
		tTVPRect &srcrectout, const tTVPRect &srcrect) const;

public:
	void SetDrawFace(tTVPDrawFace f) { DrawFace = f; }
	tTVPDrawFace GetDrawFace() const { return DrawFace; }

	void FillRect(const tTVPRect &rect, tjs_uint32 color);
	void ColorRect(const tTVPRect &rect, tjs_uint32 color, tjs_int opa);

	void DrawText(tjs_int x, tjs_int y, const ttstr &text, tjs_uint32 color,
		tjs_int opa, bool aa, tjs_int shadowlevel, tjs_uint32 shadowcolor,
			tjs_int shadowwidth,
		tjs_int shadowofsx, tjs_int shadowofsy);

	void PiledCopy(tjs_int dx, tjs_int dy, tTJSNI_BaseLayer *src,
		const tTVPRect &rect);

	void CopyRect(tjs_int dx, tjs_int dy, tTJSNI_BaseLayer *src,
		const tTVPRect &rect);

	void StretchCopy(const tTVPRect &destrect, tTJSNI_BaseLayer *src,
		const tTVPRect &rect, tTVPBBStretchType mode = stNearest);

	void AffineCopy(const t2DAffineMatrix &matrix, tTJSNI_BaseLayer *src,
		const tTVPRect &srcrect, tTVPBBStretchType mode = stNearest);

	void AffineCopy(const tTVPPoint *points, tTJSNI_BaseLayer *src,
		const tTVPRect &srcrect, tTVPBBStretchType mode = stNearest);

	void PileRect(tjs_int dx, tjs_int dy, tTJSNI_BaseLayer *src,
		const tTVPRect &rect, tjs_int opacity = 255, bool hda = true);

	void BlendRect(tjs_int dx, tjs_int dy, tTJSNI_BaseLayer *src,
		const tTVPRect &rect, tjs_int opacity = 255, bool hda = true);

	void OperateRect(tjs_int dx, tjs_int dy, tTJSNI_BaseLayer *src,
		const tTVPRect &rect, tTVPBlendOperationMode mode,
			tjs_int opacity = 255, bool hda = true);

	void StretchPile(const tTVPRect &destrect, tTJSNI_BaseLayer *src,
		const tTVPRect &srcrect, tjs_int opacity = 255,
			tTVPBBStretchType mode = stNearest, bool hda = true);

	void StretchBlend(const tTVPRect &destrect, tTJSNI_BaseLayer *src,
		const tTVPRect &srcrect, tjs_int opacity = 255,
			tTVPBBStretchType mode = stNearest, bool hda = true);

	void AffinePile(const t2DAffineMatrix &matrix, tTJSNI_BaseLayer *src,
		const tTVPRect &srcrect, tjs_int opacity = 255,
		tTVPBBStretchType mode = stNearest, bool hda = true);

	void AffinePile(const tTVPPoint *points, tTJSNI_BaseLayer *src,
		const tTVPRect &srcrect, tjs_int opacity = 255,
		tTVPBBStretchType mode = stNearest, bool hda = true);

	void AffineBlend(const t2DAffineMatrix &matrix, tTJSNI_BaseLayer *src,
		const tTVPRect &srcrect, tjs_int opacity = 255,
		tTVPBBStretchType mode = stNearest, bool hda = true);

	void AffineBlend(const tTVPPoint *points, tTJSNI_BaseLayer *src,
		const tTVPRect &srcrect, tjs_int opacity = 255,
		tTVPBBStretchType mode = stNearest, bool hda = true);

	void AdjustGamma(const tTVPGLGammaAdjustData & data);
	void DoGrayScale();
	void LRFlip();
	void UDFlip();

	bool GetImageModified() const { return ImageModified; }
	void SetImageModified(bool b) { ImageModified = b; }


	//------------------------------------------- interface to font object --

private:
	tTVPFont Font;
	bool FontChanged;
	iTJSDispatch2 *FontObject;

	void ApplyFont();

public:
	void SetFontFace(const ttstr & face);
	ttstr GetFontFace() const;
	void SetFontHeight(tjs_int height);
	tjs_int GetFontHeight() const;
	void SetFontAngle(tjs_int angle);
	tjs_int GetFontAngle() const;
	void SetFontBold(bool b);
	bool GetFontBold() const;
	void SetFontItalic(bool b);
	bool GetFontItalic() const;
	void SetFontStrikeout(bool b);
	bool GetFontStrikeout() const;
	void SetFontUnderline(bool b);
	bool GetFontUnderline() const;


	tjs_int GetTextWidth(const ttstr & text);
	tjs_int GetTextHeight(const ttstr & text);
	double GetEscWidthX(const ttstr & text);
	double GetEscWidthY(const ttstr & text);
	double GetEscHeightX(const ttstr & text);
	double GetEscHeightY(const ttstr & text);
	bool DoUserFontSelect(tjs_uint32 flags, const ttstr &caption,
		const ttstr &prompt, const ttstr &samplestring);

	void MapPrerenderedFont(const ttstr & storage);
	void UnmapPrerenderedFont();

	iTJSDispatch2 * GetFontObjectNoAddRef();


	//------------------------------------------------ updating management --
private:
	tjs_int UpdateOfsX, UpdateOfsY;
	tTVPRect UpdateRectForChild; // to be used in tTVPDrawable::GetDrawTargetBitmap
	tjs_int UpdateRectForChildOfsX;
	tjs_int UpdateRectForChildOfsY;
	tTVPBaseBitmap *UpdateBitmapForChild; // to be used in tTVPDrawable::GetDrawTargetBitmap
	tTVPRect UpdateExcludeRect; // rectangle whose update is not be needed

	tTVPComplexRect CacheRecalcRegion; // region that must be reconstructed

	bool CallOnPaint; // call onPaint event when flaged

	void UpdateChildRegion(tTJSNI_BaseLayer *child, const tTVPComplexRect &region,
		bool tempupdate, bool targnodevisible, bool addtoprimary);

	void InternalUpdate(const tTVPRect &rect, bool tempupdate = false);

public:
	void Update(tTVPComplexRect &rects, bool tempupdate = false);
	void Update(const tTVPRect &rect, bool tempupdate = false);
	void Update(bool tempupdate = false);

	void UpdateByScript(const tTVPRect &rect) { CallOnPaint = true; Update(rect); }
	void UpdateByScript() { CallOnPaint = true; Update(); }

	void SetCallOnPaint(bool b) { CallOnPaint = b; }
	bool GetCallOnPaint() const { return CallOnPaint; }

private:
	void ParentUpdate();  // called when layer moves


	bool InCompletion; // update/completion pipe line is processing
	void BeforeCompletion(); // called before the drawing is processed
	void AfterCompletion(); // called after the drawing is processed

	void QueryUpdateExcludeRect(tTVPRect &rect, bool parentvisible);
		// query update exclude rect ( checks completely opaque area )

	static void BltImage(tTVPBaseBitmap *dest, bool destalpha, tjs_int destx,
		tjs_int desty, tTVPBaseBitmap *src, const tTVPRect &srcrect,
		tTVPLayerType drawtype, tjs_int opacity);

	void DrawSelf(tTVPDrawable *target, tTVPRect &pr, 
		tTVPRect &cr);
	void CopySelfForRect(tTVPBaseBitmap *dest, tjs_int destx, tjs_int desty,
		const tTVPRect &srcrect);
	void CopySelf(tTVPBaseBitmap *dest, tjs_int destx, tjs_int desty,
		const tTVPRect &r);
	void EffectImage(tTVPBaseBitmap *dest, const tTVPRect & destrect);

	void Draw(tTVPDrawable *target, const tTVPRect &r, bool visiblecheck = true);

	// these 3 below are methods from tTVPDrawable
	tTVPBaseBitmap * GetDrawTargetBitmap(const tTVPRect &rect,
		tTVPRect &cliprect);
	bool GetDrawTargetHasAlpha();
	void DrawCompleted(const tTVPRect&destrect, tTVPBaseBitmap *bmp,
		const tTVPRect &cliprect,
		tTVPLayerType type, tjs_int opacity);

	void InternalComplete2(tTVPComplexRect & updateregion, tTVPDrawable *drawable);
	void InternalComplete(tTVPComplexRect & updateregion, tTVPDrawable *drawable);
	void CompleteForWindow(tTVPDrawable *drawable);
public:
private:
	tTVPBaseBitmap * Complete(const tTVPRect & rect);
	tTVPBaseBitmap * Complete(); // complete entire area of the layer


	//---------------------------------------------- transition management --
private:
	iTVPDivisibleTransHandler * DivisibleTransHandler;
	iTVPGiveUpdateTransHandler * GiveUpdateTransHandler;

	tTJSNI_BaseLayer * TransDest; // transition destination
	iTJSDispatch2 *TransDestObj;
	tTJSNI_BaseLayer * TransSrc; // transition source
	iTJSDispatch2 *TransSrcObj;

	bool InTransition; // is transition processing?
	bool TransWithChildren; // is transition with children?

	tjs_uint64 TransTick; // current tick count

	tTVPTransType TransType; // current transition type
	tTVPTransUpdateType TransUpdateType; // current transition update type

	tTVPScanLineProviderForBaseBitmap * DestSLP; // destination scan line provider
	tTVPScanLineProviderForBaseBitmap * SrcSLP; // source scan line provider

	bool TransCompEventPrevented; // whether "onTransitionCompleted" event is prevented

public:
	void StartTransition(const ttstr &name, bool withchildren,
		tTJSNI_BaseLayer *transsource, tTJSVariantClosure options);

private:
	void InternalStopTransition();

public:
	void StopTransition();

private:
	void StopTransitionByHandler();

	void InvokeTransition(tjs_uint64 tick); // called frequanctly

	void DoDivisibleTransition(tTVPBaseBitmap *dest, tjs_int dx, tjs_int dy,
		const tTVPRect &srcrect);

	struct tTransDrawable : public tTVPDrawable
	{
		// tTVPDrawable class for Transition pipe line rearrangement
		tTJSNI_BaseLayer * Owner;
		tTVPBaseBitmap * Target;
		tTVPRect TargetRect;
		tTVPDrawable *OrgDrawable;

		tTVPBaseBitmap *Src1Bmp; // tutDivisible
		tTVPBaseBitmap *Src2Bmp; // tutDivisible

		void Init(tTJSNI_BaseLayer *owner, tTVPDrawable *org)
		{
			Owner = owner;
			OrgDrawable = org;
			Target = NULL;
		}

		tTVPBaseBitmap * GetDrawTargetBitmap(const tTVPRect &rect,
			tTVPRect &cliprect);
		bool GetDrawTargetHasAlpha();
		void DrawCompleted(const tTVPRect&destrect, tTVPBaseBitmap *bmp,
			const tTVPRect &cliprect,
			tTVPLayerType type, tjs_int opacity);
	} TransDrawable;
	friend class tTransDrawable;

	struct tTransIdleCallback : public tTVPContinuousEventCallbackIntf
	{
		tTJSNI_BaseLayer * Owner;
		void TJS_INTF_METHOD OnContinuousCallback(tjs_uint64 tick)
			{ Owner->InvokeTransition(tick); }
		// from tTVPIdleEventCallbackIntf
	} TransIdleCallback;
	friend class tTransIdleCallback;

};
//---------------------------------------------------------------------------



//---------------------------------------------------------------------------
// tTVPLayerManager
//---------------------------------------------------------------------------
// layer mamager which is created in window's context
//---------------------------------------------------------------------------
class tTVPLayerManager
{
	tTJSNI_BaseWindow * Window;

	tTJSNI_BaseLayer * CaptureOwner;
	tTJSNI_BaseLayer * LastMouseMoveSent;

	std::vector<tTJSNI_BaseLayer *> ModalLayerVector;
		// pointer to modal layer vector
	tTJSNI_BaseLayer * FocusedLayer; // pointer to current focused layer
	tTJSNI_BaseLayer * Primary; // primary layer
	bool OverallOrderIndexValid;
	std::vector<tTJSNI_BaseLayer*> AllNodes;
		// hold overall nodes;
		// use GetAllNodes to retrieve the newest information of this
	tTVPComplexRect UpdateRegion; // window update region

	bool FocusChangeLock;

	bool VisualStateChanged;
		// flag for visual
		// state changing ( see tTJSNI_BaseLaye::NotifyChildrenVisualStateChanged)

	tjs_int LastMouseMoveX;
	tjs_int LastMouseMoveY;

	bool ReleaseCaptureCalled;

	bool InNotifyingHintOrCursorChange;

public:
	tTVPLayerManager(tTJSNI_BaseWindow *window);
	~tTVPLayerManager();


public:
	void AttachPrimary(tTJSNI_BaseLayer *pri); // attach primary layer to the manager
	void DetachPrimary(); // detach primary layer from the manager

	tTJSNI_BaseLayer * GetPrimaryLayer() const { return Primary; }
	bool IsPrimaryLayerAttached() const { return Primary != NULL; } 

	void NotifyPart(tTJSNI_BaseLayer *lay); // notifies layer parting from its parent

	tTVPComplexRect & GetUpdateRegionForCompletion() { return UpdateRegion; }

private:

	void _RecreateOverallOrderIndex(tjs_uint& index,
		std::vector<tTJSNI_BaseLayer*>& nodes);

public:
	void InvalidateOverallIndex();
	void RecreateOverallOrderIndex();

	std::vector<tTJSNI_BaseLayer*> & GetAllNodes();

	void NotifyVisualStateChanged() { VisualStateChanged = true; }
	bool GetVisualStateChanged() { return VisualStateChanged; }
	void QueryUpdateExcludeRect();


public:

	void NotifyMouseCursorChange(tTJSNI_BaseLayer *layer, tjs_int cursor);
	void SetMouseCursor(tjs_int cursor);

	void GetCursorPos(tjs_int &x, tjs_int &y);
		// get cursor position in primary coordinates
	void SetCursorPos(tjs_int x, tjs_int y);
		// set cursor position in primary coordinates

	void NotifyHintChange(tTJSNI_BaseLayer *layer, const ttstr & hint);
	void SetHint(const ttstr &hint);
		// set layer hint to current window
	void NotifyLayerResize();  // layer -> window
	void NotifyWindowInvalidation(); // layer -> window

public:
	tTJSNI_BaseWindow * GetWindow() const { return Window; }
	void SetWindow(tTJSNI_BaseWindow *window);
	void NotifyResizeFromWindow(tjs_uint w, tjs_uint h); // window -> layer
	void NotifyInvalidationFromWindow(const tTVPRect &r); // window -> layer

	void NotifyClick(tjs_int x, tjs_int y) { PrimaryClick(x, y); }
	void NotifyDoubleClick(tjs_int x, tjs_int y) { PrimaryDoubleClick(x, y); }
	void NotifyMouseDown(tjs_int x, tjs_int y, tTVPMouseButton mb, tjs_uint32 flags)
		{ PrimaryMouseDown(x, y, mb, flags); }
	void NotifyMouseUp(tjs_int x, tjs_int y, tTVPMouseButton mb, tjs_uint32 flags)
		{ PrimaryMouseUp(x, y, mb, flags); }
	void NotifyMouseMove(tjs_int x, tjs_int y, tjs_uint32 flags)
		{ PrimaryMouseMove(x, y, flags); }
	void NotifyForceMouseLeave()
		{ ForceMouseLeave(); }
	void NotifyKeyDown(tjs_uint key, tjs_uint32 shift)
		{ PrimaryKeyDown(key, shift); }
	void NotifyKeyUp(tjs_uint key, tjs_uint32 shift)
		{ PrimaryKeyUp(key, shift); }
	void NotifyKeyPress(tjs_char key)
		{ PrimaryKeyPress(key); }
	void NotifyMouseWheel(tjs_uint32 shift, tjs_int delta, tjs_int x, tjs_int y)
		{ PrimaryMouseWheel(shift, delta, x, y); }

	tTJSNI_BaseLayer * GetMostFrontChildAt(tjs_int x, tjs_int y,
		tTJSNI_BaseLayer *except = NULL, bool get_disabled = false);

	void PrimaryClick(tjs_int x, tjs_int y);
	void PrimaryDoubleClick(tjs_int x, tjs_int y);

	void PrimaryMouseDown(tjs_int x, tjs_int y, tTVPMouseButton mb, tjs_uint32 flags);
	void PrimaryMouseUp(tjs_int x, tjs_int y, tTVPMouseButton mb, tjs_uint32 flags);

	void PrimaryMouseMove(tjs_int x, tjs_int y, tjs_uint32 flags);
	void ForceMouseLeave();
	void LeaveMouseFromTree(tTJSNI_BaseLayer *root); // force to leave mouse

	void ReleaseCapture();
	bool BlurTree(tTJSNI_BaseLayer *root); // remove focus from "root"
	tTJSNI_BaseLayer *SearchFirstFocusable(); // search first focusable layer


	tTJSNI_BaseLayer *GetFocusedLayer() const { return FocusedLayer; }
	void CheckTreeFocusableState(tTJSNI_BaseLayer *root);
		// check newly added tree's focusable state
	bool SetFocusTo(tTJSNI_BaseLayer *layer, bool direction = true);
		// set focus to layer
	tTJSNI_BaseLayer *FocusPrev(); // focus to previous layer
	tTJSNI_BaseLayer *FocusNext(); // focus to next layer
	void ReleaseAllModalLayer(); // release all modal layer on invalidation
	void SetModeTo(tTJSNI_BaseLayer *layer); // set mode to layer
	void RemoveModeFrom(tTJSNI_BaseLayer *layer); // remove mode from layer
	void RemoveTreeModalState(tTJSNI_BaseLayer *root);
		// remove modal state from given tree
	tTJSNI_BaseLayer *GetCurrentModalLayer() const;

private:
	bool SearchAttentionPoint(tTJSNI_BaseLayer *target, tjs_int &x, tjs_int &y);
	void SetAttentionPointOf(tTJSNI_BaseLayer *layer);
	void DisableAttentionPoint();

public:
	void NotifyAttentionStateChanged(tTJSNI_BaseLayer *from);

private:
	void SetImeModeOf(tTJSNI_BaseLayer *layer);
	void ResetImeMode();

public:
	void NotifyImeModeChanged(tTJSNI_BaseLayer *from);

private:
	tjs_int EnabledWorkRefCount;
public:
	void SaveEnabledWork();
	void NotifyNodeEnabledState();
	void PrimaryKeyDown(tjs_uint key, tjs_uint32 shift);
	void PrimaryKeyUp(tjs_uint key, tjs_uint32 shift);
	void PrimaryKeyPress(tjs_char key);
	void PrimaryMouseWheel(tjs_uint32 shift, tjs_int delta, tjs_int x, tjs_int y);

	void AddUpdateRegion(const tTVPComplexRect &rects);
	void AddUpdateRegion(const tTVPRect & rect);
	void PrimaryUpdateByWindow(const tTVPRect &rect);
	void UpdateToWindow();
	void NotifyUpdateRegionFixed();


	void DumpPrimaryStructure();
};
//---------------------------------------------------------------------------


#include "LayerImpl.h"


//---------------------------------------------------------------------------
// tTJSNC_Layer : TJS Layer class
//---------------------------------------------------------------------------
class tTJSNC_Layer : public tTJSNativeClass
{
public:
	tTJSNC_Layer();
	static tjs_uint32 ClassID;

protected:
	tTJSNativeInstance *CreateNativeInstance();
	/*
		implement this in each platform.
		this must return a proper instance of tTJSNI_Layer.
	*/
};
//---------------------------------------------------------------------------
extern tTJSNativeClass * TVPCreateNativeClass_Layer();
//---------------------------------------------------------------------------




//---------------------------------------------------------------------------
// tTJSNI_Font : Font Native Object
//---------------------------------------------------------------------------
class tTJSNI_Font : public tTJSNativeInstance
{
	typedef tTJSNativeInstance inherited;

	tTJSNI_BaseLayer * Layer;

public:
	tTJSNI_Font();
	~tTJSNI_Font();
	tjs_error TJS_INTF_METHOD Construct(tjs_int numparams, tTJSVariant **param,
		iTJSDispatch2 *tjs_obj);
	void TJS_INTF_METHOD Invalidate();

	tTJSNI_BaseLayer * GetLayer() const { return Layer; }

};
//---------------------------------------------------------------------------



//---------------------------------------------------------------------------
// tTJSNC_Font : TJS Font class
//---------------------------------------------------------------------------
class tTJSNC_Font : public tTJSNativeClass
{
public:
	tTJSNC_Font();
	static tjs_uint32 ClassID;

protected:
	tTJSNativeInstance *CreateNativeInstance() { return new tTJSNI_Font(); }
};
//---------------------------------------------------------------------------
iTJSDispatch2 * TVPCreateFontObject(iTJSDispatch2 * layer);
//---------------------------------------------------------------------------


#endif
