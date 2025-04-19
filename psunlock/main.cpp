#include <windows.h>
#include <tlhelp32.h>
#include <string>
#include <iostream>
#include <string>
#include <chrono>
#include "resource.h"

#define ID_TRAY_EXIT  1001
#define ID_TRAY_ABOUT 1002
#define WM_TRAYICON (WM_USER + 1)

static NOTIFYICONDATA nid = { 0 };
static HMENU hMenu;
static bool NeedsUnlock = false;

struct Context_t
{
    DWORD pid;
    HWND hwnd;
};

uint64_t GetTimeMS()
{
    auto now = std::chrono::system_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
}

HWND FindMainWindow(DWORD pid) 
{
    Context_t context = { pid, nullptr };

    auto EnumProc = [](HWND hwnd, LPARAM lParam) -> BOOL 
    {
        DWORD winPid = 0;
        GetWindowThreadProcessId(hwnd, &winPid);
        auto ctx = (decltype(&context))lParam;
        if (winPid == ctx->pid && GetWindow(hwnd, GW_OWNER) == nullptr && IsWindowVisible(hwnd)) 
        {
            ctx->hwnd = hwnd;
            return FALSE;
        }
        return TRUE;
    };

    EnumWindows(EnumProc, (LPARAM)&context);
    return context.hwnd;
}

DWORD GetProcessIdByName(const std::wstring& name) 
{
    PROCESSENTRY32W entry = { sizeof(entry) };
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) 
        return 0;

    while (Process32NextW(snapshot, &entry)) 
    {
        if (name == entry.szExeFile) 
        {
            CloseHandle(snapshot);
            return entry.th32ProcessID;
        }
    }

    CloseHandle(snapshot);
    return 0;
}

BOOL CALLBACK EnumTargetChild(HWND hwnd, LPARAM lParam) 
{
    PostMessage(hwnd, WM_CLOSE, 0, 0);
    return TRUE;
}

BOOL CALLBACK EnumChildToFindChrome(HWND hwnd, LPARAM lParam) 
{
    wchar_t className[256];
    GetClassNameW(hwnd, className, 256);
    std::wstring cname = className;
    if (cname.find(L"Chrome_WidgetWin_") != std::wstring::npos)
    {
        EnumChildWindows(hwnd, EnumTargetChild, 0);
        PostMessage(hwnd, WM_CLOSE, 0, 0);
        NeedsUnlock = true;
        return FALSE;
    }
    return TRUE;
}

BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam)
{
    DWORD pid;
    wchar_t className[256];
    GetClassNameW(hwnd, className, 256);
    std::wstring cname = className;
   
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid == *(DWORD*)lParam)
    {
        EnumChildWindows(hwnd, EnumChildToFindChrome, 0);
        if (cname == L"DesktopApp")
        {
            PostMessage(hwnd, WM_CLOSE, 0, 0);
            NeedsUnlock = true;
        }
    }
    return TRUE;
}

bool TryToKillPopup(DWORD pid)
{
    NeedsUnlock = false;
    EnumWindows(EnumWindowsProc, (LPARAM)&pid);
    return NeedsUnlock;
}

void PSToForeground()
{
    HWND hwnd = FindMainWindow(GetProcessIdByName(L"Photoshop.exe"));
    if (!IsWindow(hwnd))
        return;
    ShowWindow(hwnd, SW_RESTORE);
    SetForegroundWindow(hwnd);
    SetActiveWindow(hwnd);
    SetFocus(hwnd);
}

void UnlockPhotoshop(DWORD pid)
{
    HWND hwnd = FindMainWindow(pid);
    if (!IsWindow(hwnd))
        return;

    EnableWindow(hwnd, TRUE);
    SetForegroundWindow(hwnd);
    SetActiveWindow(hwnd);
    SetFocus(hwnd);
}

INT_PTR CALLBACK DialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) 
{
    switch (message) 
    {
    case WM_INITDIALOG:
    {
        RECT rc;
        GetWindowRect(hDlg, &rc);
        int dlgWidth = rc.right - rc.left;
        int dlgHeight = rc.bottom - rc.top;

        int scrWidth = GetSystemMetrics(SM_CXSCREEN);
        int scrHeight = GetSystemMetrics(SM_CYSCREEN);

        SetWindowPos(hDlg, NULL, (scrWidth - dlgWidth) / 2, (scrHeight - dlgHeight) / 2, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
        return TRUE;
    }
    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) 
        {
            EndDialog(hDlg, LOWORD(wParam));
            return TRUE;
        }
        break;
    }
    return FALSE;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) 
    {
    case WM_CREATE:
        nid.cbSize = sizeof(nid);
        nid.hWnd = hWnd;
        nid.uID = 1;
        nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
        nid.uCallbackMessage = WM_TRAYICON;
        nid.hIcon = LoadIcon(GetModuleHandleA(NULL), MAKEINTRESOURCE(IDI_ICON1));
        lstrcpy(nid.szTip, TEXT("PS Unlock (Waiting for Photoshop...)"));
        Shell_NotifyIcon(NIM_ADD, &nid);
        break;

    case WM_TRAYICON:
        if (lParam == WM_RBUTTONUP) 
        {
            POINT pt;
            GetCursorPos(&pt);
            SetForegroundWindow(hWnd);
            hMenu = CreatePopupMenu();
            AppendMenu(hMenu, MF_STRING, ID_TRAY_ABOUT, TEXT("About"));
            AppendMenu(hMenu, MF_STRING, ID_TRAY_EXIT, TEXT("Exit"));
            TrackPopupMenu(hMenu, TPM_BOTTOMALIGN | TPM_LEFTALIGN, pt.x, pt.y, 0, hWnd, NULL);
            DestroyMenu(hMenu);
        }
        if (lParam == WM_LBUTTONDBLCLK)
            PSToForeground();
        break;

    case WM_COMMAND:
        if (LOWORD(wParam) == ID_TRAY_EXIT) 
        {
            Shell_NotifyIcon(NIM_DELETE, &nid);
            PostQuitMessage(0);
        }
        else if (LOWORD(wParam) == ID_TRAY_ABOUT)
        {
            DialogBox(GetModuleHandleA(NULL), MAKEINTRESOURCE(IDD_ABOUT), hWnd, DialogProc);
        }
        break;

    case WM_DESTROY:
        Shell_NotifyIcon(NIM_DELETE, &nid);
        PostQuitMessage(0);
        break;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int) 
{
    HANDLE hUniqueMutex = CreateMutex(NULL, FALSE, TEXT("Global\\PSUnlockUnique"));
    if (GetLastError() == ERROR_ALREADY_EXISTS) 
        return -1;
    
    WNDCLASS wc = { 0 };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = TEXT("TrayClass");
    RegisterClass(&wc);

    HWND hWnd = CreateWindow(TEXT("TrayClass"), TEXT(""), WS_OVERLAPPEDWINDOW,
        0, 0, 0, 0, NULL, NULL, hInst, NULL);

    MSG msg;
    uint64_t lastMonitored = GetTimeMS();
    DWORD pid = 0;
    bool psFound = false;
    while (true)
    {
        if (!psFound && pid)
        {
            lstrcpy(nid.szTip, TEXT("PS Unlock (Monitoring for popups...)"));
            nid.uFlags = NIF_TIP;
            Shell_NotifyIcon(NIM_MODIFY, &nid);
            psFound = true;
        }

        if (pid && GetTimeMS() - lastMonitored >= 100)
        {
            if (TryToKillPopup(pid))
                UnlockPhotoshop(pid);
            lastMonitored = GetTimeMS();
        }

        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
                break;
        }

        pid = GetProcessIdByName(L"Photoshop.exe");
        if (psFound && !pid)
            break;
    }

    Shell_NotifyIcon(NIM_DELETE, &nid);
    CloseHandle(hUniqueMutex);
    return 0;
}