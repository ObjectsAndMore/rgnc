#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <shellapi.h>
#include <stdbool.h>
#include <pathcch.h>

#define TRANSPARENT_COLOR RGB(255, 0, 255)
static COLORREF transparentColor = TRANSPARENT_COLOR;
static unsigned short rgb_r, rgb_g, rgb_b;

static HBITMAP gBitmap;
static int gBitmapW;
static int gBitmapH;
static bool verboseFlag = FALSE;
static bool examineFlag = FALSE;

void PrintUsage(const LPWSTR exeName) {
    //wprintf(L"Usage: %s [options] <input_file>\n\n", exeName);
    printf("Usage: %ls [options] <input_file>\n\n", exeName);
    wprintf(L"Options:\n");
    wprintf(L"  -h, --help                    Show this help message and exit\n");
    wprintf(L"  -v, --verbose                 Enable verbose logging output\n");
    wprintf(L"  -e, --examine                 Examine .rgn or .bmp file for region/image dimensions (overrides -o, disables any file output)\n");
    wprintf(L"  -o <file>                     Specify the output file name - override the default name (\".rgn\") derived from input_file\n");
    wprintf(L"  -t <rgb_r> <rgb_g> <rgb_b>    Specify the transparent color as RGB value, default (Magenta): 255 0 255");
}

static int LoadShapeBitmap(const LPWSTR path) {
    BITMAP bm;
    gBitmap = (HBITMAP)LoadImageW(NULL, path, IMAGE_BITMAP, 0, 0,
        LR_LOADFROMFILE | LR_CREATEDIBSECTION);
    if (!gBitmap) {
        DWORD dw = GetLastError(); 
        printf("LoadImageW() failed with error code %lu\n", dw);
        return 0;
    }
    if (!GetObject(gBitmap, sizeof(bm), &bm)) return 0;
    gBitmapW = bm.bmWidth;
    gBitmapH = bm.bmHeight;
    return 1;
}

static HRGN BuildRegionFromBitmap(HBITMAP bitmap, COLORREF transparentColor) {
    BITMAPINFO bmi;
    uint32_t *pixels;
    HRGN result;
    int x;
    int y;
    ZeroMemory(&bmi, sizeof(bmi));
    bmi.bmiHeader.biSize = sizeof(bmi.bmiHeader);
    bmi.bmiHeader.biWidth = gBitmapW;
    bmi.bmiHeader.biHeight = -gBitmapH;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    pixels = (uint32_t *)malloc((size_t)gBitmapW * (size_t)gBitmapH * sizeof(uint32_t));
    if (!pixels) return CreateRectRgn(0, 0, gBitmapW, gBitmapH);

    {
        HDC hdc = GetDC(NULL);
        if (!GetDIBits(hdc, bitmap, 0, (UINT)gBitmapH, pixels, &bmi, DIB_RGB_COLORS)) {
            ReleaseDC(NULL, hdc);
            free(pixels);
            return CreateRectRgn(0, 0, gBitmapW, gBitmapH);
        }
        ReleaseDC(NULL, hdc);
    }

    result = CreateRectRgn(0, 0, 0, 0);
    for (y = 0; y < gBitmapH; ++y) {
        x = 0;
        while (x < gBitmapW) {
            HRGN run;
            COLORREF color;
            while (x < gBitmapW) {
                uint32_t p = pixels[(size_t)y * (size_t)gBitmapW + (size_t)x];
                color = RGB((p >> 16) & 0xff, (p >> 8) & 0xff, p & 0xff);
                if (color != transparentColor) break;
                ++x;
            }
            if (x >= gBitmapW) break;
            {
                int start = x;
                while (x < gBitmapW) {
                    uint32_t p = pixels[(size_t)y * (size_t)gBitmapW + (size_t)x];
                    color = RGB((p >> 16) & 0xff, (p >> 8) & 0xff, p & 0xff);
                    if (color == transparentColor) break;
                    ++x;
                }
                run = CreateRectRgn(start, y, x, y + 1);
                CombineRgn(result, result, run, RGN_OR);
                DeleteObject(run);
            }
        }
    }
    free(pixels);
    return result;
}


static int SerializeRegionToFile(const LPWSTR ofn) {
    HRGN region;
    if (!gBitmap) return(0);
    region = BuildRegionFromBitmap(gBitmap, transparentColor);
    DWORD rgndatasize = GetRegionData(region, 0, NULL);
    if (verboseFlag) {
        printf("RGNDATA size (in bytes) as reported by GetRegionData(region, 0, NULL): %lu\n", rgndatasize);
    }
    RGNDATA* pRgnData = (RGNDATA*)malloc(rgndatasize);
    if (GetRegionData(region, rgndatasize, pRgnData) == rgndatasize) {
        // access rectangles via pRgnData->Buffer
        if (verboseFlag) {
            printf("RGNDATA obtained via GetRegionData(region, rgndatasize, pRgnData<)\n");
        }
    } else {
        return(0);
    }


    HANDLE hFile = CreateFileW(ofn, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        printf("Error creating/opening file. Error code: %lu\n", GetLastError());
        return 0; // Failure
    }

    DWORD bytes_written = 0;
    // Write the buffer to disk
    BOOL result = WriteFile(hFile, pRgnData, rgndatasize, &bytes_written, NULL);
    if (!result) {
        printf("Error writing file. Error code: %lu\n", GetLastError());
        CloseHandle(hFile);
        return 0; // Failure
    }

    // Close the handle to prevent leaks
    CloseHandle(hFile);

    // Check if all bytes were saved
    if (bytes_written != rgndatasize) {
        printf("Error writing file (size mismatch). bytes_written: %lu rgndatasize: %lu\n", bytes_written, rgndatasize);
        return 0; // Failure
    };

    if (verboseFlag) {
        printf("Output file %ls successfully written (%lu bytes)\n", ofn, bytes_written);
    }

    return(1);
}


int __cdecl main() {
    LPWSTR *szArglist;
    int nArgs;
    int i;

    LPWSTR ifn = NULL; // input file name
    LPWSTR ofn = NULL; // output file name

    szArglist = CommandLineToArgvW(GetCommandLineW(), &nArgs);
    if (NULL == szArglist) {
        wprintf(L"Fatal error: CommandLineToArgvW() failed\n");
        return(-1);  // return and exit
    }

    if (nArgs < 2) {
        wprintf(L"Fatal error: check arguments\n");
        PrintUsage(szArglist[0]);
        return(-1);  // return and exit
    }

    for (i=1; i < nArgs; i++) {
        if (szArglist[i][0] == L'-') {
            if (wcscmp(szArglist[i], L"-h") == 0 || wcscmp(szArglist[i], L"--help") == 0) {
                PrintUsage(szArglist[0]);
                return(0);            
            } else {
                if (wcscmp(szArglist[i], L"-v") == 0 || wcscmp(szArglist[i], L"--verbose") == 0) {
                    verboseFlag = TRUE;
                    continue;
                } else {
                    if (wcscmp(szArglist[i], L"-e") == 0 || wcscmp(szArglist[i], L"--examine") == 0) {
                        // incompatible with -o option (which will be overridden)
                        examineFlag = TRUE;
                    } else {
                        if (wcscmp(szArglist[i], L"-o") == 0) {
                            i++;
                            if (i < nArgs) {
                                ofn = szArglist[i];
                            } else {
                                wprintf(L"-o option specified but no output file name given\n");
                                PrintUsage(szArglist[0]);
                                return(-2);            
                            }
                        } else {
                            if (wcscmp(szArglist[i], L"-t") == 0) {
                                i++;
                                if (i < nArgs) {
                                    if (swscanf(szArglist[i], L"%hu", &rgb_r) != 1) {
                                        wprintf(L"-t option specified but no proper RGB arguments given\n");
                                        PrintUsage(szArglist[0]);
                                        return(-10);                                                
                                    }   
                                } else {
                                    wprintf(L"-t option specified but no proper RGB arguments given\n");
                                    PrintUsage(szArglist[0]);
                                    return(-10);                                                
                                }
                                i++;
                                if (i < nArgs) {
                                    if (swscanf(szArglist[i], L"%hu", &rgb_g) != 1) {
                                        wprintf(L"-t option specified but no proper RGB arguments given\n");
                                        PrintUsage(szArglist[0]);
                                        return(-10);                                                
                                    }   
                                } else {
                                    wprintf(L"-t option specified but no proper RGB arguments given\n");
                                    PrintUsage(szArglist[0]);
                                    return(-10);                                                
                                }
                                i++;
                                if (i < nArgs) {
                                    if (swscanf(szArglist[i], L"%hu", &rgb_b) != 1) {
                                        wprintf(L"-t option specified but no proper RGB arguments given\n");
                                        PrintUsage(szArglist[0]);
                                        return(-10);                                                
                                    }   
                                } else {
                                    wprintf(L"-t option specified but no proper RGB arguments given\n");
                                    PrintUsage(szArglist[0]);
                                    return(-10);                                                
                                }
                                transparentColor = RGB(rgb_r, rgb_g, rgb_b);
                            } else {
                                wprintf(L"invalid option specified\n");
                                PrintUsage(szArglist[0]);
                                return(-3);
                            }
                        }
                    }
                }
            }
        } else {
            // if not an option (no leading "-"), then it must be the input file argument
            ifn = szArglist[i];
        }
    }
    
    if (ifn == NULL) {
        wprintf(L"missing input file name\n");
        PrintUsage(szArglist[0]);
        return(-1);            
    }

    if (verboseFlag) {
        printf("input file %ls specified\n", ifn);
    }

    if (examineFlag) {
        // examine only (determine x and y dimensions of region or image), do not output any file

        PCWSTR extension = NULL;
        // wcslen + 1 includes the null terminator character length
        HRESULT hr = PathCchFindExtension(ifn, wcslen(ifn) + 1, &extension);
        if (SUCCEEDED(hr)) {
            /*
            if (*extension != L'\0') {
                printf("Extension found: %ls\n", extension);
            } else {
                printf("No extension found\n");
            }
            */
        } else {
            printf("PathCchFindExtension() failed with error code\n");
        }

        if (wcscmp(extension, L".rgn") == 0) {
            // examine region file
            HANDLE hFile = CreateFileW(ifn, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
            if (hFile == INVALID_HANDLE_VALUE) {
                printf("Error opening file %ls (read-only). Error code: %lu\n", ifn, GetLastError());
                return (-1);
            }

            LARGE_INTEGER filesize;
            if (!GetFileSizeEx(hFile, &filesize)) {
                CloseHandle(hFile);
                return -1; // error condition, could call GetLastError to find out more
            }

            // Safely cast file size to a size_t for allocation boundaries
            size_t totalBytesToRead = (size_t)filesize.QuadPart;

            if (verboseFlag) {
                printf("reading file %ls (%llu bytes)\n", ifn, totalBytesToRead);
            }

            // apparently VirtualAlloc is ideal here as it interacts directly with the Windows Virtual Memory Manager
            BYTE* pBuffer = (BYTE*)VirtualAlloc(NULL, totalBytesToRead, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
            if (pBuffer == NULL) {
                printf("Memory allocation for reading input file failed\n");
                CloseHandle(hFile);
                return -1; // error condition, could call GetLastError to find out more
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
                    printf("Error reading file. Error code: %lu\n", GetLastError());
                    return -1;
                }
                // Check for unexpected End Of File (EOF)
                if (bytesReadThisChunk == 0) {
                    break; 
                }
                bytesRemaining -= bytesReadThisChunk;
                pCurrentBufferPointer += bytesReadThisChunk;
            }

            if (bReadSuccess && bytesRemaining == 0) {
                if (verboseFlag) {
                    printf("Successfully read %zu bytes into the buffer\n", totalBytesToRead);
                }
            } else {
                printf("Error reading input file\n");
                return -1;
            }

            RGNDATA* pRgnData = (RGNDATA*)pBuffer;
            printf("Bounding rectangle for the region in logical (x,y) units: (%ld,%ld)..(%ld,%ld)\n",
                                                                              pRgnData->rdh.rcBound.left,
                                                                              pRgnData->rdh.rcBound.top,
                                                                              pRgnData->rdh.rcBound.right,
                                                                              pRgnData->rdh.rcBound.bottom);
            // Free the virtual memory layout and close the kernel handle
            VirtualFree(pBuffer, 0, MEM_RELEASE);
            CloseHandle(hFile);
        } else {
            if (wcscmp(extension, L".bmp") == 0) {
                // examine image file
                BITMAP bm;
                HBITMAP gBitmap = (HBITMAP)LoadImageW(NULL, ifn, IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
                if ((!gBitmap) || (!GetObject(gBitmap, sizeof(bm), &bm))) {
                    printf("LoadImage rror. Error code: %lu\n", GetLastError());
                    return -20;
                }
                printf("Bitmap size in logical (x,y) units: (%ld,%ld)\n", bm.bmWidth, bm.bmHeight);
            } else {
                printf("Unsupported file name extension; only .rgn and .bmp are supported in examine mode\n");
                PrintUsage(szArglist[0]);
                return(-7);                   
            }
        }
    } else {
        // regular operation: read input image file and create output region file 
        if (ofn == NULL) {
            // no output file name given, construct output file name from input file name
            LPWSTR path = (LPWSTR)malloc(MAX_PATH * sizeof(wchar_t));
            wcscpy_s(path, MAX_PATH, ifn);
            //HRESULT hr = PathCchRemoveExtension(path, ARRAYSIZE(path));
            HRESULT hr = PathCchRemoveExtension(path, MAX_PATH);

            if (SUCCEEDED(hr)) {
                if ((hr == S_FALSE) && (verboseFlag)) {
                    wprintf(L"No input file name extension found to remove, adding \".rgn\" extension to output file name\n");
                }
            } else {
                printf("Error: failed to process path %ls\n", path);
                PrintUsage(szArglist[0]);
                return(-4);            
            }
            wcscpy_s(path + wcslen(path), MAX_PATH, L".rgn");

            ofn = path;
        }
        
        if (verboseFlag) {
            printf("using output file name %ls\n", ofn);
        }

        if (!LoadShapeBitmap(ifn)) {
            printf("LoadShapeBitmap() failure\n");
            PrintUsage(szArglist[0]);
            return(-5);
        };

        if (!SerializeRegionToFile(ofn)) {
            printf("SerializeRegionToFile() failure\n");
            PrintUsage(szArglist[0]);
            return(-6);
        }
    }

    // Free memory allocated for CommandLineToArgvW arguments.
    LocalFree(szArglist);
    
    return(0);
}
