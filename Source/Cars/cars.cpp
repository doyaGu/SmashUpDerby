#define WIN32_LEAN_AND_MEAN // Exclude rarely-used stuff from Windows headers
#include <windows.h>
#include <tchar.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "CKAll.h"

#include "resource.h"

#define MAX_LOADSTRING 100

// Global Variables for Interface
HINSTANCE g_hInstance;
HWND g_hWnd;
TCHAR szTitle[MAX_LOADSTRING]; // The title bar text

int g_Width = 640;     // Default Window width
int g_Height = 480;    // Default Window Height
int g_Bpp = 16;        // Default FullScreen Bit Per Pixel
int g_RefreshRate = 0; // Default Fullscreen refresh rate

int g_NewWidth;
int g_NewHeight;
int g_NewBpp;
int g_NewRefreshRate;

// Global Variables for Player
CKContext *TheCKContext = NULL;
CKTimeManager *TheTimeManager = NULL;
CKMessageManager *TheMessageManager = NULL;
CKRenderManager *TheRenderManager = NULL;
CKRenderContext *TheRenderContext = NULL;

VxDriverDesc *g_DriverDesc = NULL;
int g_Driver = 0;
int g_Mode = -1;
BOOL g_Fullscreen = FALSE;
BOOL g_SwitchDisplayMode = FALSE;
BOOL g_GoFullScreen = FALSE;

TCHAR szResourcesRoot[MAX_PATH];
TCHAR szVirtoolsDllRoot[MAX_PATH];
TCHAR szDriverName[MAX_PATH];

INT_PTR CALLBACK SetupDialogProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK DialogProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

void Process();

static BOOL FindDisplayMode(int width, int height, int bpp, int refreshRate);
static BOOL AdjustWindow(HWND hWnd);

int APIENTRY WinMain(HINSTANCE hInstance,
                     HINSTANCE hPrevInstance,
                     LPSTR lpCmdLine,
                     int nCmdShow)
{
    g_SwitchDisplayMode = FALSE;

    HANDLE hMutex = CreateMutex(NULL, FALSE, TEXT("Cars, Tatanka"));
    if (GetLastError())
        return 0;

    LoadString(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    HWND hDlg = CreateDialogParam(hInstance, (LPCTSTR)IDD_DIALOG2, NULL, DialogProc, 0);

    RECT rc;
    GetWindowRect(hDlg, &rc);
    SetWindowPos(hDlg,
                 NULL,
                 (GetSystemMetrics(SM_CXSCREEN) - (rc.right - rc.left)) / 2,
                 (GetSystemMetrics(SM_CYSCREEN) - (rc.bottom - rc.top)) / 2,
                 0,
                 0,
                 SWP_NOSIZE | SWP_NOZORDER);
    ShowWindow(hDlg, SW_SHOW);
    UpdateWindow(hDlg);

    HKEY hkResult;
    DWORD dwType, cbData;
    TCHAR buffer[MAX_PATH];
    if (LoadString(hInstance, IDS_REG_KEY, buffer, MAX_PATH) &&
        RegOpenKeyEx(HKEY_LOCAL_MACHINE, buffer, 0, KEY_EXECUTE, &hkResult) == ERROR_SUCCESS)
    {
        cbData = MAX_PATH;
        if (RegQueryValueEx(hkResult, TEXT("resources root"), 0, &dwType, (LPBYTE)buffer, &cbData) == ERROR_SUCCESS)
        {
            _tcscpy(szResourcesRoot, buffer);
        }

        cbData = MAX_PATH;
        if (RegQueryValueEx(hkResult, TEXT("virtools dll root"), 0, &dwType, (LPBYTE)buffer, &cbData) == ERROR_SUCCESS)
        {
            _tcscpy(szVirtoolsDllRoot, buffer);
        }

        cbData = MAX_PATH;
        if (RegQueryValueEx(hkResult, TEXT("driver name"), 0, &dwType, (LPBYTE)buffer, &cbData) == ERROR_SUCCESS)
        {
            _tcscpy(szDriverName, buffer);
        }

        cbData = MAX_PATH;
        if (RegQueryValueEx(hkResult, TEXT("screen width"), 0, &dwType, (LPBYTE)buffer, &cbData) == ERROR_SUCCESS)
        {
            g_Width = *(int *)buffer;
        }

        cbData = MAX_PATH;
        if (RegQueryValueEx(hkResult, TEXT("screen height"), 0, &dwType, (LPBYTE)buffer, &cbData) == ERROR_SUCCESS)
        {
            g_Height = *(int *)buffer;
        }

        cbData = MAX_PATH;
        if (RegQueryValueEx(hkResult, TEXT("screen bpp"), 0, &dwType, (LPBYTE)buffer, &cbData) == ERROR_SUCCESS)
        {
            g_Bpp = *(int *)buffer;
        }

        cbData = MAX_PATH;
        if (RegQueryValueEx(hkResult, TEXT("screen hz"), 0, &dwType, (LPBYTE)buffer, &cbData) == ERROR_SUCCESS)
        {
            g_RefreshRate = *(int *)buffer;
        }

        cbData = MAX_PATH;
        if (RegQueryValueEx(hkResult, TEXT("fullscreen"), 0, &dwType, (LPBYTE)buffer, &cbData) == ERROR_SUCCESS)
        {
            g_Fullscreen = *(BOOL *)buffer;
        }

        RegCloseKey(hkResult);
    }

    WNDCLASSEX wcex;
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = (WNDPROC)WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(hInstance, (LPCTSTR)IDI_PLAYER);
    wcex.hCursor = NULL;
    wcex.hbrBackground = NULL;
    wcex.lpszMenuName = NULL;
    wcex.lpszClassName = TEXT("VirtoolsTatankaPlayerWndClass");
    wcex.hIconSm = LoadIcon(wcex.hInstance, (LPCTSTR)IDI_SMALL);
    RegisterClassEx(&wcex);

    DWORD dwStyle = WS_POPUP | WS_CAPTION;
    DWORD dwExStyle = WS_EX_RIGHTSCROLLBAR;
    if (g_Fullscreen)
    {
        dwStyle = WS_POPUP;
        dwExStyle = WS_EX_TOPMOST;
    }

    dwStyle |= WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
    g_GoFullScreen = TRUE;

    if (g_Fullscreen)
    {
        SetRect(&rc, 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN));
    }
    else
    {
        SetRect(&rc, 0, 0, g_Width, g_Height);
        AdjustWindowRect(&rc, dwStyle, FALSE);

        int x = GetSystemMetrics(SM_CXSCREEN) - (rc.right - rc.left);
        if (x < 0)
        {
            rc.left = 0;
            rc.right = GetSystemMetrics(SM_CXSCREEN);
        }
        else
        {
            rc.left += x / 2;
            rc.right += x / 2;
        }

        int y = GetSystemMetrics(SM_CYSCREEN) - (rc.bottom - rc.top);
        if (y < 0)
        {
            rc.top = 0;
            rc.bottom = GetSystemMetrics(SM_CYSCREEN);
        }
        else
        {
            rc.top += y / 2;
            rc.bottom += y / 2;
        }
    }

    g_hWnd = CreateWindowExA(
        dwExStyle,
        (LPCSTR)TEXT("VirtoolsTatankaPlayerWndClass"),
        (LPCSTR)szTitle,
        dwStyle,
        rc.left,
        rc.top,
        rc.right - rc.left,
        rc.bottom - rc.top,
        NULL,
        NULL,
        hInstance,
        0);

    SendMessage(hDlg, WM_CLOSE, 0, 0);
    SetCursor(NULL);

    // CoInitialize(NULL);
    // for (int i = 0; i < 8; ++i)
    // {
    //     _stprintf(buffer, TEXT("%s/movies/intro%d.avi"), szResourcesRoot, i + 1);
    //     // subs_402E90(g_hWnd, buffer);
    // }
    // CoUninitialize();

    TCHAR *path;
    TCHAR szVirtoolsDll[MAX_PATH];
    _tcscpy(szVirtoolsDll, szVirtoolsDllRoot);
    path = &szVirtoolsDll[_tcslen(szVirtoolsDll)];

    CKStartUp();
    CKPluginManager *pm = CKGetPluginManager();

    _tcscpy(path, TEXT("/RenderEngines"));
    pm->ParsePlugins(szVirtoolsDll);
    _tcscpy(path, TEXT("/Managers"));
    pm->ParsePlugins(szVirtoolsDll);
    _tcscpy(path, TEXT("/BuildingBlocks"));
    pm->ParsePlugins(szVirtoolsDll);
    _tcscpy(path, TEXT("/Plugins"));
    pm->ParsePlugins(szVirtoolsDll);

    if (pm->GetPluginCount(CKPLUGIN_RENDERENGINE_DLL) == 0)
    {
        CKShutdown();
        MessageBox(NULL, TEXT("Unable to load a RenderEngine"), szTitle, MB_OK | MB_ICONERROR);
        return -1;
    }

    if (CKCreateContext(&TheCKContext, g_hWnd, 0, 6))
    {
        CKShutdown();
        return -1;
    }

    TheMessageManager = TheCKContext->GetMessageManager();
    TheTimeManager = TheCKContext->GetTimeManager();
    TheRenderManager = TheCKContext->GetRenderManager();

    int driverCount = TheRenderManager->GetRenderDriverCount();
    g_Driver = 0;
    if (_tcscmp(szDriverName, TEXT("")) != 0)
    {
        for (int i = 0; i < driverCount; ++i)
        {
            g_DriverDesc = TheRenderManager->GetRenderDriverDescription(g_Driver);
            if (!g_DriverDesc)
            {
                CKCloseContext(TheCKContext);
                CKShutdown();
                MessageBox(NULL, TEXT("Unable to initialize a render driver"), szTitle, MB_OK | MB_ICONERROR);
                return -1;
            }
            if (strcmpi(szDriverName, g_DriverDesc->DriverName) == 0)
            {
                g_Driver = i;
                break;
            }
        }
    }

    if (g_Driver == driverCount)
    {
        g_Driver = 0;
        g_DriverDesc = TheRenderManager->GetRenderDriverDescription(g_Driver);
    }

    _tcscpy(szDriverName, g_DriverDesc->DriverName);
    if (!FindDisplayMode(g_Width, g_Height, g_Bpp, g_RefreshRate))
    {
        g_Width = 640;
        g_Height = 480;
        g_Bpp = 16;
        g_RefreshRate = 0;
        FindDisplayMode(g_Width, g_Height, g_Bpp, g_RefreshRate);
    }

    ShowWindow(g_hWnd, SW_SHOW);
    UpdateWindow(g_hWnd);
    SetRect(&rc, 0, 0, g_Width, g_Height);
    CKRECT rect = {rc.left, rc.top, rc.right, rc.bottom};
    if (g_Fullscreen)
        TheRenderContext = TheRenderManager->CreateRenderContext(
            g_hWnd,
            g_Driver,
            &rect,
            TRUE,
            g_Bpp,
            -1,
            -1,
            g_RefreshRate);
    else
        TheRenderContext = TheRenderManager->CreateRenderContext(
            g_hWnd,
            g_Driver,
            &rect,
            FALSE,
            0,
            -1,
            -1,
            0);

    if (!TheRenderContext)
    {
        CKCloseContext(TheCKContext);
        TheCKContext = NULL;
        CKShutdown();
        MessageBox(NULL, TEXT("Cannot initialize Render Context (try switching to 16 or 24/32 bits display)"), szTitle, MB_OK | MB_ICONWARNING);
        return -1;
    }

    CKDWORD renderOptions = TheRenderContext->GetCurrentRenderOptions();
    renderOptions |= CK_RENDER_FOREGROUNDSPRITES;
    TheRenderContext->ChangeCurrentRenderOptions(renderOptions, 0);

    TCHAR szResources[MAX_PATH];
    _tcscpy(szResources, szResourcesRoot);
    path = &szResources[_tcslen(szResources)];

    _tcscpy(path, TEXT("/Textures"));
    TheCKContext->GetPathManager()->AddPath(BITMAP_PATH_IDX, XString(szResources));
    _tcscpy(path, TEXT("/Sounds"));
    TheCKContext->GetPathManager()->AddPath(SOUND_PATH_IDX, XString(szResources));
    _tcscpy(path, TEXT("/3D Entities"));
    TheCKContext->GetPathManager()->AddPath(DATA_PATH_IDX, XString(szResources));

    CKObjectArray *array = CreateCKObjectArray();

    _tcscpy(path, TEXT("/cars.vmo"));
    if (TheCKContext->Load(szResources, array) != CK_OK)
    {
        TheCKContext->Reset();
        TheCKContext->ClearAll();
        TheRenderManager->DestroyRenderContext(TheRenderContext);
        TheRenderContext = NULL;
        CKCloseContext(TheCKContext);
        TheCKContext = NULL;
        CKShutdown();
        MessageBox(NULL, TEXT("Cannot load: cars.vmo"), szTitle, MB_OK | MB_ICONWARNING);
        return -1;
    }

    CKLevel *level = TheCKContext->GetCurrentLevel();
    level->AddRenderContext(TheRenderContext, TRUE);

    CK_ID *camIds = TheCKContext->GetObjectsListByClassID(CKCID_CAMERA);
    if (!camIds)
        camIds = TheCKContext->GetObjectsListByClassID(CKCID_TARGETCAMERA);
    if (camIds)
    {
        CKCamera *camera = (CKCamera *)TheCKContext->GetObject(camIds[0]);
        if (camera)
            TheRenderContext->AttachViewpointToCamera(camera);
    }

    level->LaunchScene(NULL);

    DeleteCKObjectArray(array);

    TheCKContext->Reset();
    TheCKContext->Play();

    level->SetAsWaitingForMessages(TRUE);

    MSG msg;
    HACCEL hAccel = LoadAccelerators(hInstance, (LPCTSTR)IDR_ACCEL);
    while (true)
    {
        while (!PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            if (g_SwitchDisplayMode)
            {
                if (TheRenderContext && FindDisplayMode(g_NewWidth, g_NewHeight, g_NewBpp, g_NewRefreshRate))
                {
                    if (!g_GoFullScreen)
                    {
                        AdjustWindow(g_hWnd);
                        g_SwitchDisplayMode = FALSE;
                        Process();
                        continue;
                    }

                    TheRenderContext->StopFullScreen();
                    if (TheCKContext)
                        TheCKContext->Pause();
                    TheRenderContext->GoFullScreen(
                        g_DriverDesc->DisplayModes[g_Mode].Width,
                        g_DriverDesc->DisplayModes[g_Mode].Height,
                        g_DriverDesc->DisplayModes[g_Mode].Bpp,
                        g_Driver,
                        g_DriverDesc->DisplayModes[g_Mode].RefreshRate);
                    g_Width = g_NewWidth;
                    g_Height = g_NewHeight;
                    g_RefreshRate = g_NewRefreshRate;
                    g_Bpp = g_NewBpp;
                    if (TheCKContext)
                        TheCKContext->Play();
                }
                g_SwitchDisplayMode = FALSE;
            }

            Process();
        }

        if (msg.message == WM_QUIT)
            break;

        if (!TranslateAccelerator(msg.hwnd, hAccel, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    if (TheCKContext)
    {
        TheCKContext->Reset();
        TheCKContext->ClearAll();
        if (TheRenderContext)
        {
            TheRenderManager->DestroyRenderContext(TheRenderContext);
            TheRenderContext = NULL;
        }
        CKCloseContext(TheCKContext);
        TheCKContext = NULL;
    }

    if (LoadString(hInstance, 104, buffer, MAX_PATH) && !RegOpenKeyEx(HKEY_LOCAL_MACHINE, buffer, 0, KEY_WRITE, &hkResult))
    {
        RegSetValueEx(hkResult, TEXT("driver name"), 0, REG_SZ, (LPBYTE)szDriverName, _tclen(szDriverName));
        RegSetValueEx(hkResult, TEXT("screen width"), 0, REG_DWORD, (LPBYTE)&g_Width, sizeof(int));
        RegSetValueEx(hkResult, TEXT("screen height"), 0, REG_DWORD, (LPBYTE)&g_Height, sizeof(int));
        RegSetValueEx(hkResult, TEXT("screen bpp"), 0, REG_DWORD, (LPBYTE)&g_Bpp, sizeof(int));
        RegSetValueEx(hkResult, TEXT("screen hz"), 0, REG_DWORD, (LPBYTE)&g_RefreshRate, sizeof(int));
        RegSetValueEx(hkResult, TEXT("fullscreen"), 0, REG_DWORD, (LPBYTE)&g_Fullscreen, sizeof(int));
        RegCloseKey(hkResult);
    }

    CKShutdown();
    return msg.wParam;
}

INT_PTR CALLBACK SetupDialogProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    WORD wNotifyCode = HIWORD(wParam);
    int wID = LOWORD(wParam);
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        int count = TheRenderManager->GetRenderDriverCount();
        int i, index;
        char Chaine[256];

        // Fill List with existing drivers
        for (i = 0; i < count; i++)
        {
            VxDriverDesc *desc = TheRenderManager->GetRenderDriverDescription(i);
            index = SendDlgItemMessage(hDlg, IDC_LISTDRIVER, LB_ADDSTRING, 0, (LPARAM)desc->DriverName);
            SendDlgItemMessage(hDlg, IDC_LISTDRIVER, LB_SETITEMDATA, index, i);
            if (i == g_Driver)
                SendDlgItemMessage(hDlg, IDC_LISTDRIVER, LB_SETCURSEL, index, 0);
        }

        // Fill List with availables Modes	for this driver
        VxDriverDesc *MainDesc = TheRenderManager->GetRenderDriverDescription(g_Driver);
        int prevWidth = 0, prevHeight = 0, prevBpp = 0, prevRefreshRate = 0;
        for (i = 0; i < MainDesc->DisplayModeCount; i++)
        {
            if (MainDesc->DisplayModes[i].Bpp > 8)
            {
                if (!((MainDesc->DisplayModes[i].Width == prevWidth) &&
                      (MainDesc->DisplayModes[i].Height == prevHeight) &&
                      (MainDesc->DisplayModes[i].Bpp == prevBpp) &&
                      (MainDesc->DisplayModes[i].RefreshRate == prevRefreshRate)))
                {
                    sprintf(Chaine, "%d x %d x %d %d",
                            MainDesc->DisplayModes[i].Width,
                            MainDesc->DisplayModes[i].Height,
                            MainDesc->DisplayModes[i].Bpp,
                            MainDesc->DisplayModes[i].RefreshRate);
                    index = SendDlgItemMessage(hDlg, IDC_LISTMODE, LB_ADDSTRING, 0, (LPARAM)Chaine);
                    SendDlgItemMessage(hDlg, IDC_LISTMODE, LB_SETITEMDATA, index, i);
                    prevWidth = MainDesc->DisplayModes[i].Width;
                    prevHeight = MainDesc->DisplayModes[i].Height;
                    prevBpp = MainDesc->DisplayModes[i].Bpp;
                    prevRefreshRate = MainDesc->DisplayModes[i].RefreshRate;

                    if (i == g_Mode)
                    {
                        SendDlgItemMessage(hDlg, IDC_LISTMODE, LB_SETCURSEL, index, 0);
                        SendDlgItemMessage(hDlg, IDC_LISTMODE, LB_SETTOPINDEX, index, 0);
                    }
                }
            }
        }
    }
        return TRUE;

    case WM_COMMAND:
        if (wNotifyCode == LBN_SELCHANGE)
            if (wID == IDC_LISTDRIVER)
            {
                char Chaine[256];
                int index = SendDlgItemMessage(hDlg, IDC_LISTDRIVER, LB_GETCURSEL, 0, 0);
                g_Fullscreen = SendDlgItemMessage(hDlg, IDC_LISTDRIVER, LB_GETITEMDATA, index, 0);
                VxDriverDesc *MainDesc = TheRenderManager->GetRenderDriverDescription(g_Fullscreen);

                SendDlgItemMessage(hDlg, IDC_LISTMODE, LB_RESETCONTENT, 0, 0);

                int prevWidth = 0, prevHeight = 0, prevBpp = 0, prevRefreshRate = 0;
                for (int i = 0; i < MainDesc->DisplayModeCount; i++)
                {
                    if (MainDesc->DisplayModes[i].Bpp > 8)
                    {
                        if (!((MainDesc->DisplayModes[i].Width == prevWidth) &&
                              (MainDesc->DisplayModes[i].Height == prevHeight) &&
                              (MainDesc->DisplayModes[i].Bpp == prevBpp) &&
                              (MainDesc->DisplayModes[i].RefreshRate == prevRefreshRate)))
                        {
                            sprintf(Chaine, "%d x %d x %d %d",
                                    MainDesc->DisplayModes[i].Width,
                                    MainDesc->DisplayModes[i].Height,
                                    MainDesc->DisplayModes[i].Bpp,
                                    MainDesc->DisplayModes[i].RefreshRate);
                            index = SendDlgItemMessage(hDlg, IDC_LISTMODE, LB_ADDSTRING, 0, (LPARAM)Chaine);
                            SendDlgItemMessage(hDlg, IDC_LISTMODE, LB_SETITEMDATA, index, i);
                            prevWidth = MainDesc->DisplayModes[i].Width;
                            prevHeight = MainDesc->DisplayModes[i].Height;
                            prevBpp = MainDesc->DisplayModes[i].Bpp;
                            prevRefreshRate = MainDesc->DisplayModes[i].RefreshRate;

                            if (i == g_Mode)
                            {
                                SendDlgItemMessage(hDlg, IDC_LISTMODE, LB_SETCURSEL, index, 0);
                                SendDlgItemMessage(hDlg, IDC_LISTMODE, LB_SETTOPINDEX, index, 0);
                            }
                        }
                    }
                }
            }

        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            int index = SendDlgItemMessage(hDlg, IDC_LISTDRIVER, LB_GETCURSEL, 0, 0);
            if (index >= 0)
                g_Fullscreen = SendDlgItemMessage(hDlg, IDC_LISTDRIVER, LB_GETITEMDATA, index, 0);
            index = SendDlgItemMessage(hDlg, IDC_LISTMODE, LB_GETCURSEL, 0, 0);
            if (index >= 0)
                g_Mode = SendDlgItemMessage(hDlg, IDC_LISTMODE, LB_GETITEMDATA, index, 0);
            EndDialog(hDlg, LOWORD(wParam));
            return TRUE;
        }
        break;
    }
    return FALSE;
}

INT_PTR CALLBACK DialogProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    if (uMsg != WM_INITDIALOG)
    {
        if (uMsg != WM_COMMAND || wParam != WM_CREATE && wParam != WM_DESTROY)
            return FALSE;
        EndDialog(hDlg, LOWORD(wParam));
    }
    return TRUE;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    case WM_SIZE:
        if (wParam == SIZE_MAXHIDE || wParam == SIZE_MINIMIZED)
        {
            if (TheCKContext)
                TheCKContext->Pause();
            break;
        }

        if (TheRenderContext && !TheRenderContext->IsFullScreen())
            TheRenderContext->Resize();

        if (TheCKContext)
            TheCKContext->Play();
        break;

    case WM_PAINT:
        if (TheRenderContext && !TheRenderContext->IsFullScreen())
            TheRenderContext->Render();
        break;

    case WM_ERASEBKGND:
    {
        RECT rc;
        GetClientRect(hWnd, &rc);
        DWORD rop = (hWnd == g_hWnd) ? BLACKNESS : WHITENESS;
        PatBlt((HDC)wParam, 0, 0, rc.right - rc.left, rc.bottom - rc.top, rop);
    }
        return 1;

    case WM_ACTIVATEAPP:
        if (TheRenderContext && !g_GoFullScreen && !g_SwitchDisplayMode)
        {
            if (wParam)
            {
                if (TheCKContext)
                    TheCKContext->Play();
                TheRenderContext->StopFullScreen();
                TheRenderContext->GoFullScreen(
                    g_DriverDesc->DisplayModes[g_Mode].Width,
                    g_DriverDesc->DisplayModes[g_Mode].Height,
                    g_DriverDesc->DisplayModes[g_Mode].Bpp,
                    g_Driver,
                    g_DriverDesc->DisplayModes[g_Mode].RefreshRate);
            }
            else if (TheCKContext)
            {
                TheCKContext->Pause();
            }
            return 0;
        }
        break;

    case WM_SETCURSOR:
        if (TheRenderContext)
            SetCursor(NULL);
        return 1;

    case WM_GETMINMAXINFO:
    {
        LPMINMAXINFO lpmmi = (LPMINMAXINFO)lParam;
        if (lpmmi)
        {
            lpmmi->ptMinTrackSize.x = 400;
            lpmmi->ptMinTrackSize.y = 200;
        }
    }
    break;

    case WM_NCHITTEST:
        if (TheRenderContext && TheRenderContext->IsFullScreen())
            return 1;
        break;

    case WM_ENTERMENULOOP:
        if (TheCKContext)
            TheCKContext->Pause();
        break;

    case WM_EXITMENULOOP:
        if (TheCKContext)
            TheCKContext->Play();
        break;

    case WM_POWERBROADCAST:
        if (wParam == 0 || wParam == PBT_APMRESUMESUSPEND)
            return 1;
        break;

    case WM_LBUTTONDOWN:
        SetFocus(hWnd);
        break;

    case WM_COMMAND:
    {
        int wmId = LOWORD(wParam);
        int wmEvent = HIWORD(wParam);

        switch (wmId)
        {
        case IDM_EXIT:
            DestroyWindow(hWnd);
            return 0;

        case ID_NEMOPLAYER_FULLSCREEN:
        {
            if (!TheRenderContext)
                return 0;

            WINDOWPLACEMENT wndpl;
            if (TheRenderContext->IsFullScreen())
            {
                TheRenderContext->StopFullScreen();
                if (!TheRenderContext->IsFullScreen())
                {
                    SetWindowLong(hWnd, GWL_STYLE, WS_POPUP | WS_CAPTION);
                    if (g_GoFullScreen)
                    {
                        SetWindowPlacement(hWnd, &wndpl);
                        SetWindowPos(hWnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE | SWP_DRAWFRAME | SWP_SHOWWINDOW);
                    }
                    else
                    {
                        AdjustWindow(hWnd);
                    }
                }

                g_GoFullScreen = FALSE;
            }
            else
            {
                memset(&wndpl, 0, sizeof(WINDOWPLACEMENT));
                wndpl.length = sizeof(WINDOWPLACEMENT);
                GetWindowPlacement(hWnd, &wndpl);

                TheRenderContext->GoFullScreen(
                    g_DriverDesc->DisplayModes[g_Mode].Width,
                    g_DriverDesc->DisplayModes[g_Mode].Height,
                    g_DriverDesc->DisplayModes[g_Mode].Bpp,
                    g_Driver,
                    g_DriverDesc->DisplayModes[g_Mode].RefreshRate);

                if (TheRenderContext->IsFullScreen())
                {
                    SetWindowLongA(hWnd, GWL_STYLE, WS_POPUP);
                    SetWindowPos(
                        hWnd,
                        HWND_TOPMOST,
                        0,
                        0,
                        g_DriverDesc->DisplayModes[g_Mode].Width,
                        g_DriverDesc->DisplayModes[g_Mode].Height,
                        SWP_DRAWFRAME | SWP_SHOWWINDOW);
                }

                g_GoFullScreen = TRUE;
            }
            return 0;
        }
        break;

        case ID_NEMOPLAYER_ABOUT:
            if (TheRenderContext && !TheRenderContext->IsFullScreen())
            {
                DialogBoxParam(g_hInstance, (LPCTSTR)IDD_DIALOG1, hWnd, SetupDialogProc, 0);
                return 0;
            }
            break;

        default:
            break;
        }
    }
    break;

    case WM_SYSCOMMAND:
        if (wParam == SC_SIZE ||
            wParam == SC_MOVE ||
            wParam == SC_MAXIMIZE ||
            wParam == SC_KEYMENU ||
            wParam == SC_MONITORPOWER)
            if (TheRenderContext && TheRenderContext->IsFullScreen())
                return 1;
        break;

    default:
        break;
    }
    return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

void Process()
{
    float beforeRender = 0, beforeProcess = 0;
    TheTimeManager->GetTimeToWaitForLimits(beforeRender, beforeProcess);

    if (beforeProcess <= 0)
    {
        TheTimeManager->ResetChronos(FALSE, TRUE);
        TheCKContext->Process();

        CKLevel *level = TheCKContext->GetCurrentLevel();
        if (level && TheMessageManager)
        {
            int msgCount = level->GetLastFrameMessageCount();
            for (int i = 0; i < msgCount; ++i)
            {
                CKMessage *msg = level->GetLastFrameMessage(i);
                CKSTRING name = TheMessageManager->GetMessageTypeName(msg->GetMsgType());
                if (strcmpi(name, "bye bye") == 0)
                {
                    PostMessage(g_hWnd, WM_CLOSE, 0, 0);
                }
                else if (strcmpi(name, "change screen resolution") == 0)
                {
                    CKParameter *param;
                    param = msg->GetParameter(0);
                    param->GetValue(&g_NewWidth);
                    param = msg->GetParameter(1);
                    param->GetValue(&g_NewHeight);
                    param = msg->GetParameter(2);
                    param->GetValue(&g_NewBpp);
                    param = msg->GetParameter(3);
                    param->GetValue(&g_NewRefreshRate);

                    VxDisplayMode *dm = &g_DriverDesc->DisplayModes[g_Mode];
                    if (dm->Width != g_NewWidth ||
                        dm->Height != g_NewHeight ||
                        dm->Bpp != g_NewBpp ||
                        dm->RefreshRate != g_NewRefreshRate)
                    {
                        g_SwitchDisplayMode = TRUE;
                    }
                }
                else if (strcmpi(name, "pause game") == 0)
                {
                    if (TheCKContext)
                        TheCKContext->GetTimeManager()->SetTimeScaleFactor(0);
                }
                else if (strcmpi(name, "play game") == 0)
                {
                    if (TheCKContext)
                        TheCKContext->GetTimeManager()->SetTimeScaleFactor(1);
                }
            }
        }
    }
    if (beforeRender <= 0)
    {
        TheTimeManager->ResetChronos(TRUE, FALSE);
        TheRenderContext->Render();
    }
}

static BOOL FindDisplayMode(int width, int height, int bpp, int refreshRate)
{

    if (g_DriverDesc->DisplayModeCount <= 0)
        return FALSE;

    for (int i = 0; i < g_DriverDesc->DisplayModeCount; i++)
    {
        if (g_DriverDesc->DisplayModes[i].Width == g_Width &&
            g_DriverDesc->DisplayModes[i].Height == g_Height &&
            g_Bpp == g_DriverDesc->DisplayModes[i].Bpp &&
            (!g_RefreshRate || (g_RefreshRate == g_DriverDesc->DisplayModes[i].RefreshRate)))
        {
            g_Mode = i;
            return TRUE;
        }
    }

    return FALSE;
}

static BOOL AdjustWindow(HWND hWnd)
{
    RECT rc;
    SetRect(&rc, 0, 0, g_DriverDesc->DisplayModes[g_Mode].Width, g_DriverDesc->DisplayModes[g_Mode].Height);
    AdjustWindowRect(&rc, WS_POPUP | WS_CAPTION, FALSE);

    int x = GetSystemMetrics(SM_CXSCREEN) - (rc.right - rc.left);
    if (x < 0)
    {
        rc.left = 0;
        rc.right = GetSystemMetrics(SM_CXSCREEN);
    }
    else
    {
        rc.left += x / 2;
        rc.right += x / 2;
    }

    int y = GetSystemMetrics(SM_CYSCREEN) - (rc.bottom - rc.top);
    if (y < 0)
    {
        rc.top = 0;
        rc.bottom = GetSystemMetrics(SM_CYSCREEN);
    }
    else
    {
        rc.top += y / 2;
        rc.bottom += y / 2;
    }

    return SetWindowPos(hWnd, HWND_NOTOPMOST, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, SWP_DRAWFRAME | SWP_SHOWWINDOW);
}