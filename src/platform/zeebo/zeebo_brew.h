// zeebo_brew.h - Qualcomm BREW 4.0.2 / Zeebo ABI definitions.
//
// Self-contained clean-room definitions matching the BREW 4.0.2 / Zeebo vtable
// contracts implemented by the console and by zeebx-emu (ZeebxTeam/zeebx-emu).
// Keeping these isolated inside src/platform/zeebo/ ensures zero BREW/AEE
// types leak into src/game/ or src/marni/ public headers.
#pragma once

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// ---------------------------------------------------------------------------
// Basic AEE / BREW types and status codes
// ---------------------------------------------------------------------------
typedef uint32_t AEECLSID;
typedef uint32_t AEEIID;
typedef uint16_t AEEEvent;
typedef uint16_t AECHAR;
typedef uint8_t  boolean;

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

#define AEE_SUCCESS          0
#define EFAILED              1
#define ENOMEMORY            2
#define ECLASSNOTSUPPORT     3
#define EVERSIONNOTSUPPORT   4
#define EALREADYLOADED       5
#define EUNABLETOLOAD        6
#define EBADPARM             16
#define EFILENOEXISTS        26
#define EFILEEXISTS          27

// ---------------------------------------------------------------------------
// Standard BREW / Zeebo Class IDs
// ---------------------------------------------------------------------------
#define AEECLSID_DISPLAY         0x01001002u
#define AEECLSID_FILEMGR         0x01001003u
#define AEECLSID_SOUND           0x01001056u
#define AEECLSID_MEDIAUTIL       0x0100550Du
#define AEECLSID_HID             0x0106C411u
#define AEECLSID_EGL             0x01014BC4u
#define AEECLSID_GL              0x01014BC3u
#define AEECLSID_GLES11          0x0103D8EBu
#define AEEIID_IDIB              0x01001045u

// Application Class ID for Resident Evil 1 Zeebo port (custom homebrew range)
#define AEECLSID_RE1_ZEEBO       0x01099001u

// ---------------------------------------------------------------------------
// BREW Events
// ---------------------------------------------------------------------------
#define EVT_APP_START            0x0000u
#define EVT_APP_STOP             0x0001u
#define EVT_APP_SUSPEND          0x0002u
#define EVT_APP_RESUME           0x0003u
#define EVT_KEY                  0x0100u
#define EVT_KEY_PRESS            0x0101u
#define EVT_KEY_RELEASE          0x0102u

// BREW Virtual Key Codes (AVK_*)
#define AVK_UP                   0xE031u
#define AVK_DOWN                 0xE032u
#define AVK_LEFT                 0xE033u
#define AVK_RIGHT                0xE034u
#define AVK_SELECT               0xE035u
#define AVK_CLR                  0xE030u
#define AVK_SOFT1                0xE036u
#define AVK_SOFT2                0xE037u
#define AVK_SEND                 0xE038u
#define AVK_END                  0xE039u
#define AVK_0                    0xE021u
#define AVK_1                    0xE022u
#define AVK_2                    0xE023u
#define AVK_3                    0xE024u
#define AVK_4                    0xE025u
#define AVK_5                    0xE026u
#define AVK_6                    0xE027u
#define AVK_7                    0xE028u
#define AVK_8                    0xE029u
#define AVK_9                    0xE02Au
#define AVK_STAR                 0xE02Bu
#define AVK_POUND                0xE02Cu

// ---------------------------------------------------------------------------
// Forward interface declarations
// ---------------------------------------------------------------------------
typedef struct IShell    IShell;
typedef struct IModule   IModule;
typedef struct IApplet   IApplet;
typedef struct IFileMgr  IFileMgr;
typedef struct IFile     IFile;
typedef struct IDisplay  IDisplay;
typedef struct IBitmap   IBitmap;
typedef struct IDIB      IDIB;
typedef struct IEGL      IEGL;
typedef struct IGL       IGL;
typedef struct IHID      IHID;
typedef struct IHIDDevice IHIDDevice;
typedef struct ISound    ISound;
typedef struct IMediaUtil IMediaUtil;
typedef struct IMedia     IMedia;

typedef void (*PFNNOTIFY)(void* pUser);

// ---------------------------------------------------------------------------
// AEEHelperFuncs (exact BREW SDK 4.0.2 / zeebx-core aee_helpers::HELPERS order)
// ---------------------------------------------------------------------------
typedef struct AEEHelperFuncs {
    void* (*pfnMemmove)(void* dst, const void* src, size_t n);   // 0
    void* (*pfnMemset)(void* dst, int c, size_t n);              // 1
    char* (*pfnStrcpy)(char* dst, const char* src);              // 2
    char* (*pfnStrcat)(char* dst, const char* src);              // 3
    int   (*pfnStrcmp)(const char* a, const char* b);            // 4
    size_t(*pfnStrlen)(const char* s);                           // 5
    void* _reserved6[20];                                        // 6..25
    void* (*pfnMalloc)(uint32_t dwSize);                         // 26 (0x68)
    void  (*pfnFree)(void* p);                                   // 27 (0x6C)
    void* _reserved28;                                           // 28
    void* (*pfnRealloc)(void* p, uint32_t dwSize);               // 29 (0x74)
    void* _reserved30[9];                                        // 30..38
    void  (*pfnDbgPrintf)(const char* fmt, ...);                 // 39 (0x9C)
    void* _reserved40[4];                                        // 40..43
    uint32_t (*pfnGetUpTimeMS)(void);                            // 44 (0xB0)
} AEEHelperFuncs;

// ---------------------------------------------------------------------------
// IModule & IApplet
// ---------------------------------------------------------------------------
typedef struct IModuleVtbl {
    uint32_t (*AddRef)(IModule* po);
    uint32_t (*Release)(IModule* po);
    int      (*CreateInstance)(IModule* po, IShell* pIShell, AEECLSID ClsId, void** ppObj);
    void     (*FreeResources)(IModule* po, IFileMgr* pfm, void* pRes);
} IModuleVtbl;

struct IModule {
    const IModuleVtbl* pvt;
};

typedef struct IAppletVtbl {
    uint32_t (*AddRef)(IApplet* po);
    uint32_t (*Release)(IApplet* po);
    boolean  (*HandleEvent)(IApplet* po, AEEEvent eCode, uint16_t wParam, uint32_t dwParam);
} IAppletVtbl;

struct IApplet {
    const IAppletVtbl* pvt;
};

// ---------------------------------------------------------------------------
// IShell (52 slots matching zeebx-core aee_slots::SHELL)
// ---------------------------------------------------------------------------
typedef struct IShellVtbl {
    uint32_t (*AddRef)(IShell* po);
    uint32_t (*Release)(IShell* po);
    int      (*CreateInstance)(IShell* po, AEECLSID ClsId, void** ppObj);
    int      (*QueryClass)(IShell* po, AEECLSID ClsId, void* pInfo);
    void     (*GetDeviceInfo)(IShell* po, void* pInfo);
    int      (*StartApplet)(IShell* po, AEECLSID clsID);
    int      (*CloseApplet)(IShell* po, boolean bReturnToIdle);
    boolean  (*CanStartApplet)(IShell* po, AEECLSID clsID);
    AEECLSID (*ActiveApplet)(IShell* po);
    int      (*EnumAppletInit)(IShell* po);
    boolean  (*EnumNextApplet)(IShell* po, void* pInfo);
    int      (*SetTimer)(IShell* po, int32_t dwMsecs, PFNNOTIFY pfn, void* pUser);
    int      (*CancelTimer)(IShell* po, PFNNOTIFY pfn, void* pUser);
    uint32_t (*GetTimerExpiration)(IShell* po, PFNNOTIFY pfn, void* pUser);
    int      (*CreateDialog)(IShell* po, const char* pszResFile, uint16_t nResID, void* pHandler);
    void*    (*GetActiveDialog)(IShell* po);
    int      (*EndDialog)(IShell* po);
    int      (*LoadResString)(IShell* po, const char* pszResFile, uint16_t nResID, AECHAR* pBuff, int nSize);
    void*    (*LoadResData)(IShell* po, const char* pszResFile, uint16_t nResID, uint16_t nType);
    int      (*LoadResObject)(IShell* po, const char* pszResFile, uint16_t nResID, uint16_t nType, void** ppObj);
    void     (*FreeResData)(IShell* po, void* pData);
    boolean  (*SendEvent)(IShell* po, uint32_t dwFlags, AEECLSID clsID, AEEEvent evt, uint16_t wp, uint32_t dwp);
    void     (*Beep)(IShell* po, uint32_t nBeepType, boolean bLoud);
    int      (*GetPrefs)(IShell* po, AEECLSID clsID, uint16_t wVer, void* pCfg, uint16_t nSize);
    int      (*SetPrefs)(IShell* po, AEECLSID clsID, uint16_t wVer, void* pCfg, uint16_t nSize);
    int      (*GetItemStyle)(IShell* po, uint32_t* pStyle, void* pNormal, void* pSel);
    boolean  (*Prompt)(IShell* po, const char* pszResFile, uint16_t wTitle, uint16_t wText, uint16_t* pBtns, uint16_t wDefBtn, uint16_t wType);
    boolean  (*MessageBox)(IShell* po, const char* pszResFile, uint16_t wTitle, uint16_t wText, uint16_t wType);
    boolean  (*MessageBoxText)(IShell* po, const AECHAR* pszTitle, const AECHAR* pszText, uint16_t wType);
    int      (*SetAlarm)(IShell* po, AEECLSID clsID, uint16_t nCode, uint32_t nSeconds);
    int      (*CancelAlarm)(IShell* po, AEECLSID clsID, uint16_t nCode);
    boolean  (*AlarmsActive)(IShell* po);
    void*    (*GetHandler)(IShell* po, AEEEvent evt, uint32_t dwParam);
    void     (*RegisterHandler)(IShell* po, AEECLSID clsID, uint32_t dwMask);
    int      (*RegisterNotify)(IShell* po, AEECLSID clsNotifier, AEECLSID clsID, uint32_t dwMask);
    int      (*Notify)(IShell* po, AEECLSID clsNotifier, uint32_t dwMask, void* pData);
    int      (*Resume)(IShell* po, void* pcb);
    void     (*ForceExit)(IShell* po);
    void     (*GetPosition)(IShell* po, int* pnX, int* pnY);
    boolean  (*CheckPrivLevel)(IShell* po, uint32_t dwPriv, boolean bPrompt);
    boolean  (*IsValidResource)(IShell* po, const char* pszResFile, uint16_t nResID, uint16_t nType);
    int      (*LoadResDataEx)(IShell* po, const char* pszResFile, uint16_t nResID, uint16_t nType, void* pBuf, uint32_t* pdwSize);
    void     (*RegisterSystemCallback)(IShell* po, void* pcb, uint32_t dwMask);
    int      (*DetectType)(IShell* po, const void* cpBuf, uint32_t* pdwSize, const char* cpszName, const char** pcpszMIME);
    int      (*GetDeviceInfoEx)(IShell* po, uint32_t nItem, void* pBuff, int* pnSize);
    uint16_t (*GetClassItemID)(IShell* po, AEECLSID clsID, uint16_t wID);
    void     (*Obsolete)(IShell* po);
    int      (*GetProperty)(IShell* po, uint32_t dwPropID, uint32_t* pdwVal);
    int      (*SetProperty)(IShell* po, uint32_t dwPropID, uint32_t dwVal);
    int      (*RegisterEvent)(IShell* po, AEECLSID clsID, uint32_t dwEvent);
    void     (*Reset)(IShell* po);
    boolean  (*AppIsInGroup)(IShell* po, uint32_t dwGroup);
} IShellVtbl;

struct IShell {
    const IShellVtbl* pvt;
};

#define ISHELL_CreateInstance(p, clsid, pp)   ((p)->pvt->CreateInstance((p), (clsid), (pp)))
#define ISHELL_SetTimer(p, ms, pfn, usr)      ((p)->pvt->SetTimer((p), (ms), (pfn), (usr)))
#define ISHELL_CancelTimer(p, pfn, usr)       ((p)->pvt->CancelTimer((p), (pfn), (usr)))
#define ISHELL_CloseApplet(p, bIdle)          ((p)->pvt->CloseApplet((p), (bIdle)))

// ---------------------------------------------------------------------------
// IFileMgr & IFile (matching zeebx-core aee_slots::FILEMGR / FILE)
// ---------------------------------------------------------------------------
typedef enum {
    _OFM_READ       = 0x0001,
    _OFM_READWRITE  = 0x0002,
    _OFM_CREATE     = 0x0004,
    _OFM_APPEND     = 0x0008
} OpenFileMode;

typedef enum {
    _SEEK_START   = 0,
    _SEEK_END     = 1,
    _SEEK_CURRENT = 2
} FileSeekType;

typedef struct AEEFileInfo {
    uint32_t attrib;         // +0x00 (char attrib + 3 bytes pad)
    uint32_t dwCreationDate; // +0x04
    uint32_t dwSize;         // +0x08
    char     szName[64];     // +0x0C
} AEEFileInfo;

typedef struct IFileMgrVtbl {
    uint32_t (*AddRef)(IFileMgr* po);
    uint32_t (*Release)(IFileMgr* po);
    IFile*   (*OpenFile)(IFileMgr* po, const char* pszFile, OpenFileMode mode);
    int      (*GetInfo)(IFileMgr* po, const char* pszName, AEEFileInfo* pInfo);
    int      (*Remove)(IFileMgr* po, const char* pszName);
    int      (*MkDir)(IFileMgr* po, const char* pszDir);
    int      (*RmDir)(IFileMgr* po, const char* pszDir);
    int      (*Test)(IFileMgr* po, const char* pszName);
    uint32_t (*GetFreeSpace)(IFileMgr* po, uint32_t* pdwTotal);
    int      (*GetLastError)(IFileMgr* po);
    int      (*EnumInit)(IFileMgr* po, const char* pszDir, boolean bDirs);
    boolean  (*EnumNext)(IFileMgr* po, AEEFileInfo* pInfo);
    int      (*Rename)(IFileMgr* po, const char* pszSrc, const char* pszDest);
} IFileMgrVtbl;

struct IFileMgr {
    const IFileMgrVtbl* pvt;
};

typedef struct IFileVtbl {
    uint32_t (*AddRef)(IFile* po);
    uint32_t (*Release)(IFile* po);
    void     (*Readable)(IFile* po, void* pfn, void* pUser);
    int32_t  (*Read)(IFile* po, void* pBuffer, uint32_t dwCount);
    void     (*Cancel)(IFile* po, void* pfn, void* pUser);
    uint32_t (*Write)(IFile* po, const void* pBuffer, uint32_t dwCount);
    int      (*GetInfo)(IFile* po, AEEFileInfo* pInfo);
    int      (*Seek)(IFile* po, FileSeekType seek, int32_t position);
    int      (*Truncate)(IFile* po, uint32_t truncate_pos);
} IFileVtbl;

struct IFile {
    const IFileVtbl* pvt;
};

// ---------------------------------------------------------------------------
// IDisplay / IBitmap (for BREW 4.0.2 NativeWindowType = IDIB* in eglCreateWindowSurface)
// ---------------------------------------------------------------------------
typedef struct IBitmapVtbl {
    uint32_t (*AddRef)(IBitmap* po);
    uint32_t (*Release)(IBitmap* po);
    int      (*QueryInterface)(IBitmap* po, AEEIID id, void** ppNew);
} IBitmapVtbl;

struct IBitmap {
    const IBitmapVtbl* pvt;
};

typedef struct IDisplayVtbl {
    uint32_t (*AddRef)(IDisplay* po);
    uint32_t (*Release)(IDisplay* po);
    void*    reserved_2_15[14];
    int      (*GetDeviceBitmap)(IDisplay* po, IBitmap** ppIBitmap);
} IDisplayVtbl;

struct IDisplay {
    const IDisplayVtbl* pvt;
};

// ---------------------------------------------------------------------------
// IEGL & IGL (Legacy BREW GL/EGL interface where methods omit `this`!)
// ---------------------------------------------------------------------------
typedef void* AEEEGLDisplay;
typedef void* AEEEGLSurface;
typedef void* AEEEGLContext;
typedef void* AEEEGLConfig;
typedef int   AEEEGLint;
typedef unsigned int AEEEGLBoolean;

typedef struct IEGLVtbl {
    uint32_t      (*AddRef)(IEGL* po);
    uint32_t      (*Release)(IEGL* po);
    int           (*QueryInterface)(IEGL* po, AEEIID id, void** ppNew);
    AEEEGLint     (*eglGetError)(void);
    AEEEGLDisplay (*eglGetDisplay)(int display_id);
    AEEEGLBoolean (*eglInitialize)(AEEEGLDisplay dpy, AEEEGLint* major, AEEEGLint* minor);
    AEEEGLBoolean (*eglTerminate)(AEEEGLDisplay dpy);
    const char*   (*eglQueryString)(AEEEGLDisplay dpy, AEEEGLint name);
    void*         (*eglGetProcAddress)(const char* procname);
    AEEEGLBoolean (*eglGetConfigs)(AEEEGLDisplay dpy, AEEEGLConfig* configs, AEEEGLint config_size, AEEEGLint* num_config);
    AEEEGLBoolean (*eglChooseConfig)(AEEEGLDisplay dpy, const AEEEGLint* attrib_list, AEEEGLConfig* configs, AEEEGLint config_size, AEEEGLint* num_config);
    AEEEGLBoolean (*eglGetConfigAttrib)(AEEEGLDisplay dpy, AEEEGLConfig config, AEEEGLint attribute, AEEEGLint* value);
    AEEEGLSurface (*eglCreateWindowSurface)(AEEEGLDisplay dpy, AEEEGLConfig config, void* win, const AEEEGLint* attrib_list);
    AEEEGLSurface (*eglCreatePixmapSurface)(AEEEGLDisplay dpy, AEEEGLConfig config, void* pixmap, const AEEEGLint* attrib_list);
    AEEEGLSurface (*eglCreatePbufferSurface)(AEEEGLDisplay dpy, AEEEGLConfig config, const AEEEGLint* attrib_list);
    AEEEGLBoolean (*eglDestroySurface)(AEEEGLDisplay dpy, AEEEGLSurface surface);
    AEEEGLBoolean (*eglQuerySurface)(AEEEGLDisplay dpy, AEEEGLSurface surface, AEEEGLint attribute, AEEEGLint* value);
    AEEEGLContext (*eglCreateContext)(AEEEGLDisplay dpy, AEEEGLConfig config, AEEEGLContext share_context, const AEEEGLint* attrib_list);
    AEEEGLBoolean (*eglDestroyContext)(AEEEGLDisplay dpy, AEEEGLContext ctx);
    AEEEGLBoolean (*eglMakeCurrent)(AEEEGLDisplay dpy, AEEEGLSurface draw, AEEEGLSurface read, AEEEGLContext ctx);
    AEEEGLContext (*eglGetCurrentContext)(void);
    AEEEGLSurface (*eglGetCurrentSurface)(AEEEGLint readdraw);
    AEEEGLDisplay (*eglGetCurrentDisplay)(void);
    AEEEGLBoolean (*eglQueryContext)(AEEEGLDisplay dpy, AEEEGLContext ctx, AEEEGLint attribute, AEEEGLint* value);
    AEEEGLBoolean (*eglWaitGL)(void);
    AEEEGLBoolean (*eglWaitNative)(AEEEGLint engine);
    AEEEGLBoolean (*eglSwapBuffers)(AEEEGLDisplay dpy, AEEEGLSurface surface);
    AEEEGLBoolean (*eglCopyBuffers)(AEEEGLDisplay dpy, AEEEGLSurface surface, void* target);
} IEGLVtbl;

struct IEGL {
    const IEGLVtbl* pvt;
};

// ---------------------------------------------------------------------------
// IHID & IHIDDevice (matching zeebx-core aee_slots::HID / HIDDEVICE)
// ---------------------------------------------------------------------------
#define AEEHID_UID_JOYSTICK_DEVICE  0x0106c3fdu
#define AEEHID_UID_KEYBOARD_DEVICE  0x0106c3fcu

typedef struct AEEHIDDeviceInfo {
    uint32_t dwType;
    uint16_t wProductID;
    uint16_t wVendorID;
    boolean  bBluetooth;
} AEEHIDDeviceInfo;

typedef struct AEEHIDButtonInfo {
    uint32_t dwID;
    uint32_t dwState;
    uint32_t dwUID;
    uint32_t dwMin;
    uint32_t dwMax;
} AEEHIDButtonInfo;

typedef struct IHIDVtbl {
    uint32_t (*AddRef)(IHID* po);
    uint32_t (*Release)(IHID* po);
    int      (*QueryInterface)(IHID* po, AEEIID id, void** ppNew);
    int      (*CreateDevice)(IHID* po, int nDevHandle, IHIDDevice** ppDevice);
    int      (*GetDeviceInfo)(IHID* po, int nDevHandle, AEEHIDDeviceInfo* pInfo);
    int      (*GetNextConnectEvent)(IHID* po, int* pnDevHandle, boolean* pbConnected, uint32_t* pdwTimestamp);
    int      (*RegisterForConnectEvents)(IHID* po, void* pSignal);
    int      (*GetConnectedDevices)(IHID* po, int nDeviceType, int* pnHandles, int nLen, int* pnLenReq);
} IHIDVtbl;

struct IHID {
    const IHIDVtbl* pvt;
};

typedef struct IHIDDeviceVtbl {
    uint32_t (*AddRef)(IHIDDevice* po);
    uint32_t (*Release)(IHIDDevice* po);
    int      (*QueryInterface)(IHIDDevice* po, AEEIID id, void** ppNew);
    int      (*GetDeviceInfo)(IHIDDevice* po, AEEHIDDeviceInfo* pInfo);
    int      (*GetDeviceStatus)(IHIDDevice* po, uint32_t* pdwStatus);
    int      (*RegisterForStatusChange)(IHIDDevice* po, void* pSignal);
    int      (*GetButtonInfo)(IHIDDevice* po, int nButton, AEEHIDButtonInfo* pInfo);
    int      (*GetNumberOfButtons)(IHIDDevice* po, int* pnButtons);
    int      (*RegisterForButtonEvent)(IHIDDevice* po, void* pSignal);
    int      (*GetNextButtonEvent)(IHIDDevice* po, AEEHIDButtonInfo* pInfo, uint32_t* pdwTimestamp, boolean* pbDropped);
    int      (*GetPositionState)(IHIDDevice* po, int32_t* pnAxes);
    int      (*GetMinPositionInfo)(IHIDDevice* po, int32_t* pnMin);
    int      (*GetMaxPositionInfo)(IHIDDevice* po, int32_t* pnMax);
    int      (*GetAxesInfo)(IHIDDevice* po, void* pAxesInfo);
    int      (*RegisterForPositionChange)(IHIDDevice* po, void* pSignal);
    int      (*SetExclusiveLevel)(IHIDDevice* po, int nLevel);
    int      (*GetExclusiveLevel)(IHIDDevice* po, int* pnLevel);
    int      (*Rumble)(IHIDDevice* po, uint32_t dwPattern, uint32_t dwDuration);
    int      (*GetRumbleStatus)(IHIDDevice* po, boolean* pbActive);
} IHIDDeviceVtbl;

struct IHIDDevice {
    const IHIDDeviceVtbl* pvt;
};

// ---------------------------------------------------------------------------
// IMediaUtil & IMedia (matching zeebx-core aee_slots::MEDIAUTIL / MEDIA)
// ---------------------------------------------------------------------------
#define MMD_FILE_NAME       0u
#define MMD_BUFFER          1u
#define MM_PARM_MEDIA_DATA  1u
#define MM_PARM_VOLUME      4u
#define MM_PARM_PLAY_REPEAT 11u
#define MM_STATE_READY      2u
#define MM_STATE_PLAY       3u

typedef struct AEEMediaData {
    uint32_t clsData;
    void*    pData;
    uint32_t dwSize;
} AEEMediaData;

typedef struct IMediaUtilVtbl {
    uint32_t (*AddRef)(IMediaUtil* po);
    uint32_t (*Release)(IMediaUtil* po);
    int      (*QueryInterface)(IMediaUtil* po, AEEIID id, void** ppNew);
    int      (*CreateMedia)(IMediaUtil* po, AEEMediaData* pmd, IMedia** ppm);
    int      (*EncodeMedia)(IMediaUtil* po, void* pInfo, IMedia** ppm);
    int      (*CreateMediaEx)(IMediaUtil* po, AEEMediaData* pmd, void* pEx, IMedia** ppm);
} IMediaUtilVtbl;

struct IMediaUtil {
    const IMediaUtilVtbl* pvt;
};

typedef struct IMediaVtbl {
    uint32_t (*AddRef)(IMedia* po);
    uint32_t (*Release)(IMedia* po);
    int      (*QueryInterface)(IMedia* po, AEEIID id, void** ppNew);
    int      (*RegisterNotify)(IMedia* po, void* pfn, void* pUser);
    int      (*SetMediaParm)(IMedia* po, int nParmID, int32_t p1, int32_t p2);
    int      (*GetMediaParm)(IMedia* po, int nParmID, int32_t* p1, int32_t* p2);
    int      (*Play)(IMedia* po);
    int      (*Record)(IMedia* po);
    int      (*Stop)(IMedia* po);
    int      (*Seek)(IMedia* po, int nMode, int32_t lOffset);
    int      (*Pause)(IMedia* po);
    int      (*Resume)(IMedia* po);
    int      (*GetTotalTime)(IMedia* po);
    int      (*GetState)(IMedia* po, boolean* pbStateChanging);
} IMediaVtbl;

struct IMedia {
    const IMediaVtbl* pvt;
};

// ---------------------------------------------------------------------------
// Global Zeebo BREW runtime context (owned by zeebo_brew.cpp)
// ---------------------------------------------------------------------------
typedef struct ZeeboRuntime {
    IShell*         pShell;
    AEEHelperFuncs* pHelpers;
    IFileMgr*       pFileMgr;
    IDisplay*       pDisplay;
    IEGL*           pEGL;
    void*           pGL;
    IHID*           pHID;
    ISound*         pSound;
    IMediaUtil*     pMediaUtil;
    IApplet*        pApplet;
    boolean         bRunning;
} ZeeboRuntime;

extern ZeeboRuntime g_Zeebo;

static inline void* Zeebo_GetNativeWindow(void)
{
    if (!g_Zeebo.pDisplay || !g_Zeebo.pDisplay->pvt || !g_Zeebo.pDisplay->pvt->GetDeviceBitmap) {
        return NULL;
    }
    IBitmap* pDevBmp = NULL;
    if (g_Zeebo.pDisplay->pvt->GetDeviceBitmap(g_Zeebo.pDisplay, &pDevBmp) != AEE_SUCCESS || !pDevBmp) {
        return NULL;
    }
    IDIB* pDIB = NULL;
    if (pDevBmp->pvt && pDevBmp->pvt->QueryInterface) {
        if (pDevBmp->pvt->QueryInterface(pDevBmp, AEEIID_IDIB, (void**)&pDIB) == AEE_SUCCESS && pDIB) {
            return (void*)pDIB;
        }
    }
    return (void*)pDevBmp;
}

// Entry point called by BREW / zeebx-emu loader
int AEEMod_Load(IShell* ps, void* ph, IModule** ppMod);

// Frame pump & event callbacks implemented in main.cpp / input.cpp
boolean Zeebo_OnAppStart(void);
void    Zeebo_OnAppStop(void);
void    Zeebo_OnFrameTick(void);
boolean Zeebo_OnKeyEvent(AEEEvent evt, uint16_t keyCode, uint32_t dwParam);
void    Zeebo_Log(const char* fmt, ...);

#ifdef __cplusplus
}
#endif
