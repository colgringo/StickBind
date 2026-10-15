#define UNICODE
#define _UNICODE
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>
#include <wchar.h>
#include <stdio.h>

#define PATH_CAP 32768

/* DLL name next to this launcher. Overridable via -DMAPPER_DLL_NAME=L"..." */
#ifndef MAPPER_DLL_NAME
#  ifdef _WIN64
#    define MAPPER_DLL_NAME L"SB64.dll"
#  else
#    define MAPPER_DLL_NAME L"SB32.dll"
#  endif
#endif

static void message_error(const wchar_t *title, const wchar_t *message) {
    MessageBoxW(NULL, message, title, MB_OK | MB_ICONERROR);
}

static void get_app_dir(wchar_t *out, DWORD cap) {
    DWORD n = GetModuleFileNameW(NULL, out, cap);
    if (!n || n >= cap) { out[0] = L'\0'; return; }
    wchar_t *slash = wcsrchr(out, L'\\');
    if (!slash) slash = wcsrchr(out, L'/');
    if (slash) *(slash + 1) = L'\0'; else out[0] = L'\0';
}

static BOOL file_exists(const wchar_t *path) {
    DWORD a = GetFileAttributesW(path);
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

/* Resolve LoadLibraryW inside the target via RVA, with retries while modules load. */
static LPTHREAD_START_ROUTINE find_remote_load_library(DWORD pid, int max_attempts) {
    HMODULE local_owner = NULL;
    FARPROC local_load = GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "LoadLibraryW");
    if (!local_load || !GetModuleHandleExW(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            (LPCWSTR)local_load, &local_owner)) {
        return NULL;
    }

    wchar_t owner_path[PATH_CAP];
    DWORD owner_n = GetModuleFileNameW(local_owner, owner_path, PATH_CAP);
    if (!owner_n || owner_n >= PATH_CAP) return NULL;

    const wchar_t *owner_name = wcsrchr(owner_path, L'\\');
    owner_name = owner_name ? owner_name + 1 : owner_path;
    SIZE_T rva = (SIZE_T)((BYTE *)local_load - (BYTE *)local_owner);

    for (int attempt = 0; attempt < max_attempts; attempt++) {
        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
        if (snap != INVALID_HANDLE_VALUE) {
            MODULEENTRY32W me;
            me.dwSize = sizeof(me);
            if (Module32FirstW(snap, &me)) {
                do {
                    if (_wcsicmp(me.szModule, owner_name) == 0) {
                        LPTHREAD_START_ROUTINE remote =
                            (LPTHREAD_START_ROUTINE)((BYTE *)me.modBaseAddr + rva);
                        CloseHandle(snap);
                        return remote;
                    }
                } while (Module32NextW(snap, &me));
            }
            CloseHandle(snap);
        }
        Sleep(25);
    }
    return NULL;
}

static BOOL remote_load_library(HANDLE process, DWORD pid, const wchar_t *dll_path) {
    SIZE_T bytes = (wcslen(dll_path) + 1) * sizeof(wchar_t);
    LPVOID remote_str = VirtualAllocEx(process, NULL, bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!remote_str) return FALSE;

    if (!WriteProcessMemory(process, remote_str, dll_path, bytes, NULL)) {
        VirtualFreeEx(process, remote_str, 0, MEM_RELEASE);
        return FALSE;
    }

    LPTHREAD_START_ROUTINE remote_load = find_remote_load_library(pid, 40);
    if (!remote_load) {
        VirtualFreeEx(process, remote_str, 0, MEM_RELEASE);
        return FALSE;
    }

    HANDLE thread = CreateRemoteThread(process, NULL, 0, remote_load, remote_str, 0, NULL);
    if (!thread) {
        VirtualFreeEx(process, remote_str, 0, MEM_RELEASE);
        return FALSE;
    }

    WaitForSingleObject(thread, 10000);
    DWORD exit_code = 0;
    BOOL got_code = GetExitCodeThread(thread, &exit_code);
    CloseHandle(thread);
    VirtualFreeEx(process, remote_str, 0, MEM_RELEASE);

    return got_code && exit_code != 0;
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE previous, PWSTR cmdline, int show) {
    (void)instance; (void)previous; (void)cmdline; (void)show;
    wchar_t dir[PATH_CAP], ini[PATH_CAP], game_value[PATH_CAP], args[PATH_CAP];
    wchar_t game_path[PATH_CAP], dll_path[PATH_CAP], workdir[PATH_CAP];
    get_app_dir(dir, PATH_CAP);
    if (!dir[0]) { message_error(L"StickBindLauncher", L"Could not determine the launcher directory."); return 1; }
    _snwprintf(ini, PATH_CAP, L"%sStickBind.ini", dir);
    game_value[0] = args[0] = L'\0';
    GetPrivateProfileStringW(L"Launcher", L"GAME", L"", game_value, PATH_CAP, ini);
    GetPrivateProfileStringW(L"Launcher", L"ARGS", L"", args, PATH_CAP, ini);
    if (!game_value[0]) { message_error(L"StickBindLauncher", L"StickBind.ini is missing [Launcher] GAME."); return 1; }

    if (game_value[1] == L':' || (game_value[0] == L'\\' && game_value[1] == L'\\'))
        _snwprintf(game_path, PATH_CAP, L"%s", game_value);
    else
        _snwprintf(game_path, PATH_CAP, L"%s%s", dir, game_value);
    if (!file_exists(game_path)) {
        message_error(L"StickBindLauncher", L"The GAME executable specified in StickBind.ini was not found.");
        return 1;
    }

    _snwprintf(dll_path, PATH_CAP, L"%s%s", dir, MAPPER_DLL_NAME);
    if (!file_exists(dll_path)) {
        wchar_t msg[512];
        _snwprintf(msg, 512, L"%s was not found beside the launcher.", MAPPER_DLL_NAME);
        message_error(L"StickBindLauncher", msg);
        return 1;
    }

    _snwprintf(workdir, PATH_CAP, L"%s", game_path);
    wchar_t *last = wcsrchr(workdir, L'\\');
    if (last) *(last + 1) = L'\0'; else _snwprintf(workdir, PATH_CAP, L"%s", dir);

    SIZE_T cmd_chars = wcslen(game_path) + wcslen(args) + 8;
    wchar_t *command = (wchar_t *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, cmd_chars * sizeof(wchar_t));
    if (!command) { message_error(L"StickBindLauncher", L"Out of memory."); return 1; }
    if (args[0]) _snwprintf(command, cmd_chars, L"\"%s\" %s", game_path, args);
    else _snwprintf(command, cmd_chars, L"\"%s\"", game_path);

    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    ZeroMemory(&pi, sizeof(pi));
    si.cb = sizeof(si);

    if (!CreateProcessW(game_path, command, NULL, NULL, FALSE, CREATE_SUSPENDED, NULL, workdir, &si, &pi)) {
        DWORD err = GetLastError();
        wchar_t msg[256];
        _snwprintf(msg, 256, L"Could not start the game (Windows error %lu).", (unsigned long)err);
        HeapFree(GetProcessHeap(), 0, command);
        message_error(L"StickBindLauncher", msg);
        return 1;
    }
    HeapFree(GetProcessHeap(), 0, command);

    /* Resume so the loader maps kernel32 before we inject. */
    ResumeThread(pi.hThread);

    if (!remote_load_library(pi.hProcess, pi.dwProcessId, dll_path)) {
        TerminateProcess(pi.hProcess, 1);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        message_error(L"StickBindLauncher",
            L"Could not load the mapper DLL into the game. "
            L"Check that the launcher, DLL and game have matching "
            L"32-bit/64-bit architectures, and that the DLL is beside the launcher.");
        return 1;
    }

    CloseHandle(pi.hThread);
    WaitForSingleObject(pi.hProcess, INFINITE);
    CloseHandle(pi.hProcess);
    return 0;
}
