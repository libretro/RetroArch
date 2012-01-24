#ifndef _MENU_XUI_H_
#define _MENU_XUI_H_

#include <xui.h>
#include <xuiapp.h>

class CSSNES : public CXuiModule
{
protected:
	/* Override so that Cssnes can register classes */
	virtual HRESULT RegisterXuiClasses();
	/* Override so that Cssnes can unregister classes */
	virtual HRESULT UnregisterXuiClasses();
};

class CSSNESMain: public CXuiSceneImpl
{
protected:
	CXuiControl m_filebrowser;
	CXuiControl m_settings;
	CXuiControl m_quit;
	CXuiTextElement m_title;
	CXuiTextElement m_core;
public:
	HRESULT OnInit( XUIMessageInit* pInitData, BOOL& bHandled );
	HRESULT OnNotifyPress( HXUIOBJ hObjPressed,  BOOL& bHandled );

	XUI_BEGIN_MSG_MAP()
		XUI_ON_XM_INIT( OnInit)
		XUI_ON_XM_NOTIFY_PRESS( OnNotifyPress )
	XUI_END_MSG_MAP();

	XUI_IMPLEMENT_CLASS(CSSNESMain, L"SSNESMain", XUI_CLASS_SCENE)
};

class CSSNESSettings: public CXuiSceneImpl
{
protected:
	CXuiControl m_rewind;
	CXuiControl m_back;
public:
	HRESULT OnInit( XUIMessageInit* pInitData, BOOL& bHandled );
	HRESULT OnNotifyPress( HXUIOBJ hObjPressed,  BOOL& bHandled );

	XUI_BEGIN_MSG_MAP()
		XUI_ON_XM_INIT( OnInit)
		XUI_ON_XM_NOTIFY_PRESS( OnNotifyPress )
	XUI_END_MSG_MAP();

	XUI_IMPLEMENT_CLASS(CSSNESSettings, L"SSNESSettings", XUI_CLASS_SCENE)
};

int menu_init (void);
void menu_loop (void);

extern CSSNES app;

#endif
