/* This file contains some Vista SDK stuff that is missing from the current
   mingw-w64 headers */

#ifndef WINEXTRA_H
#define WINEXTRA_H

/* winuser.h */

#if (_WIN32_WINNT >= 0x0600)
  WINUSERAPI WINBOOL WINAPI SetProcessDPIAware(VOID);
  WINUSERAPI WINBOOL WINAPI IsProcessDPIAware(VOID);
#endif

/* commctrl.h */

#ifndef NOTASKDIALOG
#if (NTDDI_VERSION >= NTDDI_VISTA)
#include <pshpack1.h>
#define TD_WARNING_ICON     MAKEINTRESOURCEW(-1)
#define TD_ERROR_ICON       MAKEINTRESOURCEW(-2)
#define TD_INFORMATION_ICON MAKEINTRESOURCEW(-3)
#define TD_SHIELD_ICON      MAKEINTRESOURCEW(-4)

  typedef HRESULT (CALLBACK *PFTASKDIALOGCALLBACK)(HWND hwnd,UINT uNotification,WPARAM wParam,LPARAM lParam,LONG_PTR dwRefData);

  enum _TASKDIALOG_FLAGS {
    TDF_ENABLE_HYPERLINKS           = 0x0001,
    TDF_USE_HICON_MAIN              = 0x0002,
    TDF_USE_HICON_FOOTER            = 0x0004,
    TDF_ALLOW_DIALOG_CANCELLATION   = 0x0008,
    TDF_USE_COMMAND_LINKS           = 0x0010,
    TDF_USE_COMMAND_LINKS_NO_ICON   = 0x0020,
    TDF_EXPAND_FOOTER_AREA          = 0x0040,
    TDF_EXPANDED_BY_DEFAULT         = 0x0080,
    TDF_VERIFICATION_FLAG_CHECKED   = 0x0100,
    TDF_SHOW_PROGRESS_BAR           = 0x0200,
    TDF_SHOW_MARQUEE_PROGRESS_BAR   = 0x0400,
    TDF_CALLBACK_TIMER              = 0x0800,
    TDF_POSITION_RELATIVE_TO_WINDOW = 0x1000,
    TDF_RTL_LAYOUT                  = 0x2000,
    TDF_NO_DEFAULT_RADIO_BUTTON     = 0x4000,
    TDF_CAN_BE_MINIMIZED            = 0x8000,
    TDIF_SIZE_TO_CONTENT            = 0x1000000,
    TDF_SIZE_TO_CONTENT             = 0x1000000
  };
  typedef int TASKDIALOG_FLAGS;

  enum _TASKDIALOG_COMMON_BUTTON_FLAGS {
    TDCBF_OK_BUTTON     = 0x01,
    TDCBF_YES_BUTTON    = 0x02,
    TDCBF_NO_BUTTON     = 0x04,
    TDCBF_CANCEL_BUTTON = 0x08,
    TDCBF_RETRY_BUTTON  = 0x10,
    TDCBF_CLOSE_BUTTON  = 0x20
  };
  typedef int TASKDIALOG_COMMON_BUTTON_FLAGS;

  typedef enum _TASKDIALOG_NOTIFICATIONS {
    TDN_CREATED                = 0,
    TDN_NAVIGATED              = 1,
    TDN_BUTTON_CLICKED         = 2,
    TDN_HYPERLINK_CLICKED      = 3,
    TDN_TIMER                  = 4,
    TDN_DESTROYED              = 5,
    TDN_RADIO_BUTTON_CLICKED   = 6,
    TDN_DIALOG_CONSTRUCTED     = 7,
    TDN_VERIFICATION_CLICKED   = 8,
    TDN_HELP                   = 9,
    TDN_EXPANDO_BUTTON_CLICKED = 10
  } TASKDIALOG_NOTIFICATIONS;

  typedef enum _TASKDIALOG_MESSAGES {
    TDM_NAVIGATE_PAGE = WM_USER + 101,
    TDM_CLICK_BUTTON = WM_USER + 102,
    TDM_SET_MARQUEE_PROGRESS_BAR = WM_USER + 103,
    TDM_SET_PROGRESS_BAR_STATE = WM_USER + 104,
    TDM_SET_PROGRESS_BAR_RANGE = WM_USER + 105,
    TDM_SET_PROGRESS_BAR_POS = WM_USER + 106,
    TDM_SET_PROGRESS_BAR_MARQUEE = WM_USER + 107,
    TDM_SET_ELEMENT_TEXT = WM_USER + 108,
    TDM_CLICK_RADIO_BUTTON = WM_USER + 110,
    TDM_ENABLE_BUTTON = WM_USER + 111,
    TDM_ENABLE_RADIO_BUTTON = WM_USER + 112,
    TDM_CLICK_VERIFICATION = WM_USER + 113,
    TDM_UPDATE_ELEMENT_TEXT = WM_USER + 114,
    TDM_SET_BUTTON_ELEVATION_REQUIRED_STATE = WM_USER + 115,
    TDM_UPDATE_ICON = WM_USER + 116
  } TASKDIALOG_MESSAGES;

  typedef enum _TASKDIALOG_ELEMENTS {
    TDE_CONTENT,
    TDE_EXPANDED_INFORMATION,
    TDE_FOOTER,
    TDE_MAIN_INSTRUCTION
  } TASKDIALOG_ELEMENTS;

  typedef enum _TASKDIALOG_ICON_ELEMENTS {
    TDIE_ICON_MAIN,
    TDIE_ICON_FOOTER
  } TASKDIALOG_ICON_ELEMENTS;

  typedef struct _TASKDIALOG_BUTTON {
    int nButtonID;
    PCWSTR pszButtonText;
  } TASKDIALOG_BUTTON;

  typedef struct _TASKDIALOGCONFIG {
    UINT cbSize;
    HWND hwndParent;
    HINSTANCE hInstance;
    TASKDIALOG_FLAGS dwFlags;
    TASKDIALOG_COMMON_BUTTON_FLAGS dwCommonButtons;
    PCWSTR pszWindowTitle;
    __C89_NAMELESS union {
      HICON hMainIcon;
      PCWSTR pszMainIcon;
    } DUMMYUNIONNAME;
    PCWSTR pszMainInstruction;
    PCWSTR pszContent;
    UINT cButtons;
    const TASKDIALOG_BUTTON *pButtons;
    int nDefaultButton;
    UINT cRadioButtons;
    const TASKDIALOG_BUTTON *pRadioButtons;
    int nDefaultRadioButton;
    PCWSTR pszVerificationText;
    PCWSTR pszExpandedInformation;
    PCWSTR pszExpandedControlText;
    PCWSTR pszCollapsedControlText;
    __C89_NAMELESS union {
      HICON hFooterIcon;
      PCWSTR pszFooterIcon;
    } DUMMYUNIONNAME2;
    PCWSTR pszFooter;
    PFTASKDIALOGCALLBACK pfCallback;
    LONG_PTR lpCallbackData;
    UINT cxWidth;
  } TASKDIALOGCONFIG;

  WINCOMMCTRLAPI HRESULT WINAPI TaskDialog(HWND hwndParent,HINSTANCE hInstance,PCWSTR pszWindowTitle,PCWSTR pszMainInstruction,PCWSTR pszContent,TASKDIALOG_COMMON_BUTTON_FLAGS dwCommonButtons,PCWSTR pszIcon,int *pnButton);
  WINCOMMCTRLAPI HRESULT WINAPI TaskDialogIndirect(const TASKDIALOGCONFIG *pTaskConfig,int *pnButton,int *pnRadioButton,BOOL *pfVerificationFlagChecked);
#include <poppack.h>
#endif
#endif

/* dwmapi.h */

HRESULT WINAPI DwmFlush(VOID);

#endif
