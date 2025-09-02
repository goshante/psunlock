#include <windows.h>
#include <tlhelp32.h>
#include <string>
#include <iostream>
#include <string>
#include <chrono>
#include <map>
#include <sys/stat.h>
#include "resource.h"

#define ID_TRAY_EXIT  1001
#define ID_TRAY_ABOUT 1002
#define WM_TRAYICON (WM_USER + 1)

static NOTIFYICONDATA nid = { 0 };
static HMENU hMenu;
static bool NeedsUnlock = false;

bool FileExists(const std::string& path)
{
    struct stat buffer;
    return stat(path.c_str(), &buffer) == 0;
}

bool RunExecutable(const std::wstring& path)
{
    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    return CreateProcessW(path.c_str(), NULL, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)
        && CloseHandle(pi.hProcess) && CloseHandle(pi.hThread);
}

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

BOOL CALLBACK UnlockChildren(HWND hwnd, LPARAM lParam)
{
    EnableWindow(hwnd, TRUE);
    return TRUE;
}

BOOL CALLBACK EnumChildToFindDroverLord(HWND hwnd, LPARAM lParam)
{
    wchar_t className[256];
    GetClassNameW(hwnd, className, 256);
    std::wstring cname = className;
    if (cname.find(L"DroverLord - Window Class") != std::wstring::npos)
    {
        EnumChildWindows(hwnd, UnlockChildren, 0);
        EnableWindow(hwnd, TRUE);
        return FALSE;
    }
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

BOOL CALLBACK EnumWindowsProcDROV(HWND hwnd, LPARAM lParam)
{
    DWORD pid;
    wchar_t className[256];
    GetClassNameW(hwnd, className, 256);
    std::wstring cname = className;

    GetWindowThreadProcessId(hwnd, &pid);
    if (pid == *(DWORD*)lParam)
    {
        EnumChildWindows(hwnd, EnumChildToFindDroverLord, 0);
        if (cname.find(L"DroverLord - Window Class") != std::wstring::npos)
        {
            EnableWindow(hwnd, TRUE);
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

void AppToForeground(const std::wstring& appName)
{
    DWORD pid = GetProcessIdByName(appName.c_str());
    if (!pid)
        return;

    HWND hwnd = FindMainWindow(pid);
    if (!IsWindow(hwnd))
        return;

    ShowWindow(hwnd, SW_RESTORE);
    SetForegroundWindow(hwnd);
    SetActiveWindow(hwnd);
    SetFocus(hwnd);
}

//Why the fuck they call this shit like that lmao
void UnlockDroverLord(DWORD pid)
{
    EnumWindows(EnumWindowsProcDROV, (LPARAM)&pid);
}

void UnlockApplication(DWORD pid, bool DroverLord)
{
    HWND hwnd = FindMainWindow(pid);
    if (!IsWindow(hwnd))
        return;

    EnableWindow(hwnd, TRUE);
    SetForegroundWindow(hwnd);
    SetActiveWindow(hwnd);
    SetFocus(hwnd);

    if (DroverLord)
        UnlockDroverLord(pid);
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
        lstrcpy(nid.szTip, TEXT("PS Unlock (Waiting for PS/Premiere/AE...)"));
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
        {
            AppToForeground(L"AfterFX.exe");
            AppToForeground(L"Adobe Premiere Pro.exe");
            AppToForeground(L"Photoshop.exe");
        }
            

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

bool UnlockChecksForApp(const std::wstring& app)
{
    DWORD pid = GetProcessIdByName(app.c_str());
    static std::map<std::wstring, uint64_t> lastMonitored;
    if (lastMonitored.empty())
    {
        lastMonitored.emplace(L"Photoshop.exe", 0);
        lastMonitored.emplace(L"Adobe Premiere Pro.exe", 0);
        lastMonitored.emplace(L"AfterFX.exe", 0);
    }

    if (!pid)
        return false;

    //Every 525ms check is there any blocking popups
    //If detected - close them and unlock PS main window
    if (GetTimeMS() - lastMonitored[app] >= 525)
    {
        if (TryToKillPopup(pid))
            UnlockApplication(pid, app == L"Adobe Premiere Pro.exe");
        lastMonitored[app] = GetTimeMS();
    }

    return true;
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int) 
{
    //Optional feature as PS launcher
    if (FileExists("Photoshop.exe"))
        RunExecutable(L"Photoshop.exe");

    if (FileExists("Adobe Premiere Pro.exe"))
        RunExecutable(L"Adobe Premiere Pro.exe");

    if (FileExists("AfterFX.exe"))
        RunExecutable(L"AfterFX.exe");

    //Prevents running multiple instances of psunlock
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
    bool psFound = false;
    bool psFounStatus = false;
    while (true)
    {
        psFound = UnlockChecksForApp(L"Photoshop.exe") | UnlockChecksForApp(L"Adobe Premiere Pro.exe") | UnlockChecksForApp(L"AfterFX.exe");
        if (psFound && !psFounStatus)
        {
            lstrcpy(nid.szTip, TEXT("PS Unlock (Monitoring for popups...)"));
            nid.uFlags = NIF_TIP;
            Shell_NotifyIcon(NIM_MODIFY, &nid);
            psFounStatus = true;
        }

        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
                break;
        }

        if (psFounStatus && !psFound)
            break;
    }

    Shell_NotifyIcon(NIM_DELETE, &nid);
    CloseHandle(hUniqueMutex);
    return 0;
}