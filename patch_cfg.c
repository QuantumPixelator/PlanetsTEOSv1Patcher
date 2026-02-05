#include <windows.h>
#include <commdlg.h>
#include <stdio.h>
#include <stdlib.h>

#define ID_PATCH_BUTTON 101
#define ID_FOOTER_LABEL 102

void ShowMessage(HWND hwnd, const char* title, const char* message, UINT type) {
    MessageBoxA(hwnd, message, title, type);
}

int PatchBuffer(unsigned char* data, size_t size) {
    int count = 0;
    
    // 1. Target 1: Scan for 9A 4E 09 75 0A
    // Patch: 31 C0 83 C4 04 (XOR EAX, EAX; ADD ESP, 4)
    unsigned char target1[] = { 0x9A, 0x4E, 0x09, 0x75, 0x0A };
    unsigned char patch1[] = { 0x31, 0xC0, 0x83, 0xC4, 0x04 };

    for (size_t i = 0; i <= size - 5; i++) {
        if (memcmp(&data[i], target1, 5) == 0) {
            memcpy(&data[i], patch1, 5);
            count++;
        }
    }

    // 2. Target 2: Registration flag setter
    unsigned char target2[] = { 0x3B, 0x16, 0xDE, 0x67, 0x75, 0x12, 0x3B, 0x06, 0xDC, 0x67, 0x75, 0x0C };
    for (size_t i = 0; i <= size - 12; i++) {
        if (memcmp(&data[i], target2, 12) == 0) {
            data[i + 4] = 0x90;  // NOP
            data[i + 5] = 0x90;  // NOP
            data[i + 10] = 0x90; // NOP
            data[i + 11] = 0x90; // NOP
            count++;
        }
    }

    return count;
}

void RunPatch(HWND hwnd) {
    OPENFILENAMEA ofn;
    char szFile[MAX_PATH] = { 0 };

    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = "Executable Files\0*.EXE\0All Files\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrTitle = "Select PLANCFG.EXE to Patch";
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

    if (GetOpenFileNameA(&ofn)) {
        char backupPath[MAX_PATH];
        snprintf(backupPath, MAX_PATH, "%s.bak", szFile);
        
        if (!CopyFileA(szFile, backupPath, TRUE)) {
            DWORD err = GetLastError();
            if (err != ERROR_FILE_EXISTS) {
                ShowMessage(hwnd, "Error", "Could not create backup file.", MB_ICONERROR);
                return;
            }
        }

        HANDLE hFile = CreateFileA(szFile, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile == INVALID_HANDLE_VALUE) {
            ShowMessage(hwnd, "Error", "Could not open file for writing.", MB_ICONERROR);
            return;
        }

        DWORD fileSize = GetFileSize(hFile, NULL);
        unsigned char* buffer = (unsigned char*)malloc(fileSize);
        if (!buffer) {
            CloseHandle(hFile);
            return;
        }

        DWORD bytesRead;
        ReadFile(hFile, buffer, fileSize, &bytesRead, NULL);

        int patchesApplied = PatchBuffer(buffer, fileSize);

        if (patchesApplied > 0) {
            SetFilePointer(hFile, 0, NULL, FILE_BEGIN);
            DWORD bytesWritten;
            WriteFile(hFile, buffer, fileSize, &bytesWritten, NULL);
            CloseHandle(hFile);
            char successMsg[256];
            snprintf(successMsg, 256, "Successfully applied %d patches.\nBackup: %s", patchesApplied, backupPath);
            ShowMessage(hwnd, "Success", successMsg, MB_ICONINFORMATION);
        } else {
            CloseHandle(hFile);
            ShowMessage(hwnd, "Info", "No patch locations found. File may already be patched.", MB_ICONINFORMATION);
        }

        free(buffer);
    }
}

BOOL CALLBACK SetFontCallback(HWND hwnd, LPARAM lParam) {
    SendMessage(hwnd, WM_SETFONT, (WPARAM)lParam, TRUE);
    return TRUE;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            // Main title label
            CreateWindowA("STATIC", "PLANETS:TEOS v1 Registration Patcher", 
                WS_VISIBLE | WS_CHILD | SS_CENTER, 
                0, 20, 420, 20, hwnd, NULL, NULL, NULL);

            // Description label
            CreateWindowA("STATIC", "This utility bypasses registration checks in PLANCFG.EXE.\nA backup (.bak) will be created automatically.\nOnce patched, any numbers/text will work as registration info.", 
                WS_VISIBLE | WS_CHILD | SS_CENTER, 
                10, 60, 400, 60, hwnd, NULL, NULL, NULL);

            // Patch Button
            CreateWindowA("BUTTON", "Select File and Patch", 
                WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON, 
                110, 140, 200, 50, hwnd, (HMENU)ID_PATCH_BUTTON, NULL, NULL);

            // Cracked by label at the bottom (Centered, Dark Maroon)
            CreateWindowA("STATIC", "Cracked by QuantumPixelator", 
                WS_VISIBLE | WS_CHILD | SS_CENTER, 
                0, 210, 420, 20, hwnd, (HMENU)ID_FOOTER_LABEL, NULL, NULL);

            HFONT hFont = CreateFontA(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI");
            
            // Set font for all children using a compatible callback
            EnumChildWindows(hwnd, SetFontCallback, (LPARAM)hFont);
            break;
        }

        case WM_CTLCOLORSTATIC: {
            HDC hdcStatic = (HDC)wParam;
            HWND hwndStatic = (HWND)lParam;
            if (GetDlgCtrlID(hwndStatic) == ID_FOOTER_LABEL) {
                SetTextColor(hdcStatic, RGB(128, 0, 0));
                SetBkMode(hdcStatic, TRANSPARENT);
                return (LRESULT)GetStockObject(NULL_BRUSH);
            }
            break;
        }
        case WM_COMMAND: {
            if (LOWORD(wParam) == ID_PATCH_BUTTON) {
                RunPatch(hwnd);
            }
            break;
        }
        case WM_DESTROY:
            PostQuitMessage(0);
            break;
        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    (void)hPrevInstance; (void)lpCmdLine;
    const char g_szClassName[] = "PatcherWindowClass";
    WNDCLASSEXA wc = {0};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = g_szClassName;

    if (!RegisterClassExA(&wc)) return 0;

    HWND hwnd = CreateWindowExA(WS_EX_CLIENTEDGE, g_szClassName, "TEOS Patcher", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, CW_USEDEFAULT, CW_USEDEFAULT, 440, 320, NULL, NULL, hInstance, NULL);
    if (!hwnd) return 0;

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG Msg;
    while (GetMessage(&Msg, NULL, 0, 0) > 0) {
        TranslateMessage(&Msg);
        DispatchMessage(&Msg);
    }
    return (int)Msg.wParam;
}
