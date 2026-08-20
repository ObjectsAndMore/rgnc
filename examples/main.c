#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdlib.h>

#define APP_CLASS_NAME "ImageShapeWindowClass"

static HBITMAP gBitmap;
static int gBitmapW;
static int gBitmapH;

static int LoadBitmapFromFile(const char *path) {
    BITMAP bm;
    gBitmap = (HBITMAP)LoadImageA(NULL, path, IMAGE_BITMAP, 0, 0,
        LR_LOADFROMFILE | LR_CREATEDIBSECTION);
    if (!gBitmap) return 0;
    if (!GetObject(gBitmap, sizeof(bm), &bm)) return 0;
    gBitmapW = bm.bmWidth;
    gBitmapH = bm.bmHeight;
    return 1;
}

static HRGN ReadRegionFromFile(HWND hwnd, const char *path) {
    HANDLE hFile = CreateFile(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        MessageBoxA(hwnd, "Error opening region file for read", "INVALID_HANDLE_VALUE", MB_ICONERROR);
        return (NULL);
    }

    LARGE_INTEGER filesize;
    if (!GetFileSizeEx(hFile, &filesize)) {
        CloseHandle(hFile);
        return (NULL); // error condition, could call GetLastError to find out more
    }

    // Safely cast file size to a size_t for allocation boundaries
    size_t totalBytesToRead = (size_t)filesize.QuadPart;

    // apparently VirtualAlloc is ideal here as it interacts directly with the Windows Virtual Memory Manager
    BYTE* pBuffer = (BYTE*)VirtualAlloc(NULL, totalBytesToRead, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (pBuffer == NULL) {
        MessageBoxA(hwnd, "Memory allocation for reading input file failed", "malloc error", MB_ICONERROR);
        CloseHandle(hFile);
        return (NULL); // error condition, could call GetLastError to find out more
    }

    // Read the file contents into the buffer
    size_t bytesRemaining = totalBytesToRead;
    BYTE* pCurrentBufferPointer = pBuffer;
    DWORD bytesReadThisChunk = 0;
    BOOL bReadSuccess = TRUE;

    // Read in a loop because ReadFile accepts a maximum chunk size of a 32-bit DWORD (4GB)
    while (bytesRemaining > 0) {
        // Clamp chunk read size to maximum DWORD value if processing massive files
        DWORD chunkToRead = (bytesRemaining > MAXDWORD) ? MAXDWORD : (DWORD)bytesRemaining;
        bReadSuccess = ReadFile(hFile, pCurrentBufferPointer, chunkToRead, &bytesReadThisChunk, NULL);
        if (!bReadSuccess) {
            MessageBoxA(hwnd, "Error reading region file", "ReadFile() error", MB_ICONERROR);
            return (NULL);
        }
        // Check for unexpected End Of File (EOF)
        if (bytesReadThisChunk == 0) {
            break; 
        }
        bytesRemaining -= bytesReadThisChunk;
        pCurrentBufferPointer += bytesReadThisChunk;
    }

    if (!(bReadSuccess && bytesRemaining == 0)) {
        MessageBoxA(hwnd, "Error reading region file", "read error", MB_ICONERROR);
        return (NULL);
    }

    CloseHandle(hFile);

    return ExtCreateRegion(NULL, totalBytesToRead, (RGNDATA*)pBuffer);
}

static void ApplyImageShape(HWND hwnd) {
    HRGN region;
    if (!gBitmap) return;
    region = ReadRegionFromFile(hwnd, "example.rgn");
    SetWindowRgn(hwnd, region, TRUE);
}

static void PaintBitmap(HWND hwnd) {
    PAINTSTRUCT ps;
    HDC hdc;
    HDC memdc;
    HGDIOBJ oldBitmap;
    hdc = BeginPaint(hwnd, &ps);
    memdc = CreateCompatibleDC(hdc);
    oldBitmap = SelectObject(memdc, gBitmap);
    BitBlt(hdc, 0, 0, gBitmapW, gBitmapH, memdc, 0, 0, SRCCOPY);
    SelectObject(memdc, oldBitmap);
    DeleteDC(memdc);
    EndPaint(hwnd, &ps);
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE:
        if (!LoadBitmapFromFile("example.bmp")) {
            MessageBoxA(hwnd, "Missing file example.bmp", "example.bmp not found", MB_ICONERROR);
            return -1;
        }

        SetWindowPos(hwnd, NULL, 0, 0, gBitmapW, gBitmapH, SWP_NOMOVE | SWP_NOZORDER);

        MoveWindow(hwnd,
                   GetSystemMetrics(SM_CXSCREEN) / 2 - gBitmapW / 2,
                   GetSystemMetrics(SM_CYSCREEN) / 2 - gBitmapH / 2,
                   gBitmapW,
                   gBitmapH,
                   TRUE);

        ApplyImageShape(hwnd);
        return 0;

    case WM_LBUTTONDOWN: {
        ReleaseCapture();
        SendMessage(hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
        return 0;
    }

    case WM_PAINT:
        PaintBitmap(hwnd);
        return 0;

    case WM_RBUTTONUP:
        PostMessage(hwnd, WM_CLOSE, 0, 0);
        return 0;

    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE) {
            PostMessage(hwnd, WM_CLOSE, 0, 0);
            return 0;
        }
        break;

    case WM_DESTROY:
        if (gBitmap) DeleteObject(gBitmap);
        gBitmap = NULL;
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE prev, LPSTR cmdLine, int showCmd) {
    WNDCLASSA wc;
    HWND hwnd;
    MSG msg;
    (void)prev;
    (void)cmdLine;
    ZeroMemory(&wc, sizeof(wc));
    wc.lpfnWndProc = WndProc;
    wc.hInstance = instance;
    wc.lpszClassName = APP_CLASS_NAME;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    if (!RegisterClassA(&wc)) return 1;

    hwnd = CreateWindowExA(0, APP_CLASS_NAME, "Region File Example", WS_POPUP | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, 320, 320, NULL, NULL, instance, NULL);
    if (!hwnd) return 1;

    ShowWindow(hwnd, showCmd);
    UpdateWindow(hwnd);
    while (GetMessage(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return (int)msg.wParam;
}
