#pragma once
#include <windows.h>
#include <string>
#include <tlhelp32.h>
#include <vector>
#include <Utils/Utils.h>
#include <MainWindow.xaml.h>

using namespace winrt;
using namespace winrt::StarlightGUI::implementation;

static MainWindow* instance = nullptr;

static bool EnablePrivilege0(LPCTSTR privilege) {
    HANDLE hToken;
    TOKEN_PRIVILEGES tkp{};

    if (!OpenProcessToken(GetCurrentProcess(),
        TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken)) {
        return false;
    }

    LookupPrivilegeValueW(NULL, privilege, &tkp.Privileges[0].Luid);

    tkp.PrivilegeCount = 1;
    tkp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

    BOOL result = AdjustTokenPrivileges(hToken, FALSE, &tkp, 0, NULL, 0);
    CloseHandle(hToken);

    return result != FALSE;
}

static DWORD FindProcessId(const wchar_t* processName) {
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) return 0;

    PROCESSENTRY32W pe = { sizeof(pe) };

    if (Process32FirstW(hSnapshot, &pe)) {
        do {
            if (_wcsicmp(pe.szExeFile, processName) == 0) {
                CloseHandle(hSnapshot);
                return pe.th32ProcessID;
            }
        } while (Process32NextW(hSnapshot, &pe));
    }

    CloseHandle(hSnapshot);
    return 0;
}

static bool EnableAllPrivileges(HANDLE hToken) {
    DWORD dwSize;
    if (!GetTokenInformation(hToken, TokenPrivileges, nullptr, 0, &dwSize)) {
        if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
            return false;
        }
    }

    std::vector<BYTE> buffer(dwSize);
    PTOKEN_PRIVILEGES pTokenPrivileges = reinterpret_cast<PTOKEN_PRIVILEGES>(buffer.data());

    if (!GetTokenInformation(hToken, TokenPrivileges, pTokenPrivileges, dwSize, &dwSize)) {
        return false;
    }

    for (DWORD i = 0; i < pTokenPrivileges->PrivilegeCount; i++) {
        pTokenPrivileges->Privileges[i].Attributes |= SE_PRIVILEGE_ENABLED;
    }

    if (!AdjustTokenPrivileges(hToken, FALSE, pTokenPrivileges, dwSize, nullptr, nullptr)) {
        return false;
    }

    return GetLastError() == ERROR_SUCCESS;
}

static int _CreateProcess(std::wstring processName, bool fullPrivileges) {

    if (!EnablePrivilege0(SE_DEBUG_NAME)) {
        slg::CreateInfoBarAndDisplay(L"Elevator", L"无法获取SE_DEBUG_PRIVILEGE权限", InfoBarSeverity::Error, instance);
        return 1;
    }

    if (!EnablePrivilege0(SE_TCB_NAME)) {
        slg::CreateInfoBarAndDisplay(L"Elevator", L"无法获取SE_TCB_PRIVILEGE权限", InfoBarSeverity::Error, instance);
        return 1;
    }

    HANDLE hSystemToken = nullptr;
    HANDLE hImpersonationToken = nullptr;
    HANDLE hTrustedInstallerProcessToken = nullptr;
    HANDLE hTrustedInstallerToken = nullptr;

    DWORD winlogonPid = FindProcessId(L"winlogon.exe");
    if (winlogonPid == 0) {
        slg::CreateInfoBarAndDisplay(L"Elevator", L"无法找到Winlogon进程", InfoBarSeverity::Error, instance);
        return 1;
    }

    HANDLE hWinlogon = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, winlogonPid);
    if (!hWinlogon) {
        slg::CreateInfoBarAndDisplay(L"Elevator", L"无法打开Winlogon进程", InfoBarSeverity::Error, instance);
        return 1;
    }

    HANDLE hWinlogonToken = nullptr;
    if (!OpenProcessToken(hWinlogon, TOKEN_DUPLICATE | TOKEN_QUERY, &hWinlogonToken)) {
        CloseHandle(hWinlogon);
        slg::CreateInfoBarAndDisplay(L"Elevator", L"无法获取Winlogon进程令牌", InfoBarSeverity::Error, instance);
        return 1;
    }
    CloseHandle(hWinlogon);

    if (!DuplicateTokenEx(hWinlogonToken, MAXIMUM_ALLOWED, nullptr,
        SecurityImpersonation, TokenPrimary, &hSystemToken)) {
        CloseHandle(hWinlogonToken);
        slg::CreateInfoBarAndDisplay(L"Elevator", L"无法复制Winlogon进程令牌 [1]", InfoBarSeverity::Error, instance);
        return 1;
    }

    if (!DuplicateTokenEx(hWinlogonToken, MAXIMUM_ALLOWED, nullptr,
        SecurityImpersonation, TokenImpersonation, &hImpersonationToken)) {
        CloseHandle(hWinlogonToken);
        CloseHandle(hSystemToken);
        slg::CreateInfoBarAndDisplay(L"Elevator", L"无法复制Winlogon进程令牌 [2]", InfoBarSeverity::Error, instance);
        return 1;
    }
    CloseHandle(hWinlogonToken);

    if (!ImpersonateLoggedOnUser(hImpersonationToken)) {
        CloseHandle(hSystemToken);
        CloseHandle(hImpersonationToken);
        slg::CreateInfoBarAndDisplay(L"Elevator", L"无法模拟以SYSTEM身份登录", InfoBarSeverity::Error, instance);
        return 1;
    }

    if (!SetThreadToken(NULL, hImpersonationToken)) {
        CloseHandle(hSystemToken);
        CloseHandle(hImpersonationToken);
        slg::CreateInfoBarAndDisplay(L"Elevator", L"无法设置线程令牌", InfoBarSeverity::Error, instance);
        return 1;
    }

    SC_HANDLE scManager = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_ALL_ACCESS);
    if (!scManager) {
        RevertToSelf();
        CloseHandle(hSystemToken);
        CloseHandle(hImpersonationToken);
        slg::CreateInfoBarAndDisplay(L"Elevator", L"无法打开SCManager", InfoBarSeverity::Error, instance);
        return 1;
    }

    SC_HANDLE service = OpenServiceW(scManager, L"TrustedInstaller", SERVICE_ALL_ACCESS);
    bool serviceStarted = false;

    if (service) {
        if (!StartServiceW(service, 0, nullptr)) {
            if (GetLastError() != ERROR_SERVICE_ALREADY_RUNNING) {
                slg::CreateInfoBarAndDisplay(L"Elevator", L"无法启动服务，尝试直接创建进程...", InfoBarSeverity::Warning, instance);
                std::wstring trustedInstallerPath = L"C:\\Windows\\servicing\\TrustedInstaller.exe";

                STARTUPINFOW si = { sizeof(si) };
                PROCESS_INFORMATION pi = { 0 };

                if (CreateProcessAsUserW(hSystemToken, trustedInstallerPath.c_str(),
                    nullptr, nullptr, nullptr, FALSE, 0,
                    nullptr, nullptr, &si, &pi)) {
                    CloseHandle(pi.hThread);
                    CloseHandle(pi.hProcess);
                    serviceStarted = true;
                }
                else {
                    CloseHandle(pi.hThread);
                    CloseHandle(pi.hProcess);
                    serviceStarted = false;
                }
            }
            else {
                serviceStarted = true;
            }
        }
        else {
            serviceStarted = true;
        }
        CloseServiceHandle(service);
    }
    CloseServiceHandle(scManager);

    if (!serviceStarted) {
        RevertToSelf();
        CloseHandle(hSystemToken);
        CloseHandle(hImpersonationToken);
        slg::CreateInfoBarAndDisplay(L"Elevator", L"所有启动尝试都失败了！", InfoBarSeverity::Error, instance);
        return 1;
    }


    DWORD tiPid = 0;
    for (int i = 0; i < 10; i++) {
        tiPid = FindProcessId(L"TrustedInstaller.exe");
        if (tiPid != 0) break;
        Sleep(500);
    }

    if (tiPid == 0) {
        RevertToSelf();
        CloseHandle(hSystemToken);
        CloseHandle(hImpersonationToken);
        slg::CreateInfoBarAndDisplay(L"Elevator", L"无法找到TrustedInstaller进程", InfoBarSeverity::Error, instance);
        return 1;
    }

    HANDLE hTiProcess = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, tiPid);
    if (!hTiProcess) {
        RevertToSelf();
        CloseHandle(hSystemToken);
        CloseHandle(hImpersonationToken);
        slg::CreateInfoBarAndDisplay(L"Elevator", L"无法打开TrustedInstaller进程", InfoBarSeverity::Error, instance);
        return 1;
    }

    if (!OpenProcessToken(hTiProcess, TOKEN_DUPLICATE, &hTrustedInstallerProcessToken)) {
        CloseHandle(hTiProcess);
        RevertToSelf();
        CloseHandle(hSystemToken);
        CloseHandle(hImpersonationToken);
        slg::CreateInfoBarAndDisplay(L"Elevator", L"无法获取TrustedInstaller进程令牌", InfoBarSeverity::Error, instance);
        return 1;
    }

    SECURITY_ATTRIBUTES sa = { sizeof(sa) };
    if (!DuplicateTokenEx(hTrustedInstallerProcessToken, TOKEN_ALL_ACCESS, &sa, SecurityImpersonation, TokenPrimary, &hTrustedInstallerToken)) {
        CloseHandle(hTiProcess);
        RevertToSelf();
        CloseHandle(hSystemToken);
        CloseHandle(hImpersonationToken);
        slg::CreateInfoBarAndDisplay(L"Elevator", L"无法复制TrustInstaller进程令牌", InfoBarSeverity::Error, instance);
        return 1;
    }
    CloseHandle(hTiProcess);

    if (fullPrivileges) {
        if (!EnableAllPrivileges(hTrustedInstallerToken)) {
            CloseHandle(hTrustedInstallerToken);
            RevertToSelf();
            CloseHandle(hSystemToken);
            CloseHandle(hImpersonationToken);
            slg::CreateInfoBarAndDisplay(L"Elevator", L"无法设置完整权限", InfoBarSeverity::Error, instance);
            return 1;
        }
    }

    DWORD currentSessionId = WTSGetActiveConsoleSessionId();
    ProcessIdToSessionId(GetCurrentProcessId(), &currentSessionId);

    if (!SetTokenInformation(hTrustedInstallerToken, TokenSessionId, &currentSessionId, sizeof(currentSessionId))) {
        CloseHandle(hTrustedInstallerToken);
        RevertToSelf();
        CloseHandle(hSystemToken);
        CloseHandle(hImpersonationToken);
        slg::CreateInfoBarAndDisplay(L"Elevator", L"无法设置令牌信息", InfoBarSeverity::Error, instance);
        return 1;
    }

    RevertToSelf();

    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi = { 0 };

    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_SHOW;

    if (!CreateProcessWithTokenW(hTrustedInstallerToken,
        LOGON_WITH_PROFILE,
        processName.c_str(),
        nullptr,
        0,
        nullptr,
        nullptr,
        &si,
        &pi)) {
        slg::CreateInfoBarAndDisplay(L"Elevator", L"使用令牌创建进程失败，尝试其他方法...", InfoBarSeverity::Warning, instance);

        if (!CreateProcessAsUserW(hTrustedInstallerToken,
            processName.c_str(),
            nullptr,
            nullptr,
            nullptr,
            FALSE,
            0,
            nullptr,
            nullptr,
            &si,
            &pi)) {
            CloseHandle(pi.hThread);
            CloseHandle(pi.hProcess);
            CloseHandle(hTrustedInstallerToken);
            CloseHandle(hSystemToken);
            CloseHandle(hImpersonationToken);
            slg::CreateInfoBarAndDisplay(L"Elevator", L"所有启动尝试都失败了！", InfoBarSeverity::Error, instance);
            return 1;
        }
    }

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    CloseHandle(hTrustedInstallerToken);
    CloseHandle(hSystemToken);
    CloseHandle(hImpersonationToken);

    return pi.dwProcessId;
}

static int CreateProcessElevated(std::wstring name, bool full, MainWindow* mainWindow) {
    instance = mainWindow;
    return _CreateProcess(name, full);
}