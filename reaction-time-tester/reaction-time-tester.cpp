#include <windows.h>
#include <stdlib.h>
#include <time.h>
#include <stdio.h>

#pragma warning(disable: 28251)

#define TIMER_READY 1
#define TIMER_REACT 2
#define TIMER_EARLY 3

COLORREF ReadyColor[3], ReactColor[3], EarlyColor[3], ResultColor[3], EarlyTextColor[3], ResultsTextColor[3];
int MinDelay, MaxDelay, NumberOfTrials, InputRejectionDelay;

// Global variables to maintain the program's state.
BOOL isReact = FALSE;
BOOL isEarly = FALSE;
BOOL isResult = FALSE;
BOOL isReadyForReact = FALSE;
LARGE_INTEGER startTime, endTime, freq;    // For high-resolution timing.
double* reactionTimes = NULL; // Array to store the last 5 reaction times.
int currentAttempt = 0;

// Brushes for painting the window background.
HBRUSH hReadyBrush, hReactBrush, hEarlyBrush, hResultBrush;

// Forward declarations for window procedure and other functions.
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void ResetLogic(HWND hwnd);
void HandleReactClick(HWND hwnd);
void HandleEarlyClick(HWND hwnd);
void ResetAll(HWND hwnd);
void LoadConfig();

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    QueryPerformanceFrequency(&freq);

    const wchar_t CLASS_NAME[] = L"Sample Window Class";

    WNDCLASS wc = {};

    // Define properties for our window class.
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;

    // Register the window class. 
    if (!RegisterClass(&wc)) {
        MessageBox(NULL, L"Failed to register window class", L"Error", MB_OK);
        return 0;
    }

    // Initialize brushes for painting.
    LoadConfig(); // Load configuration at the start

    hReadyBrush = CreateSolidBrush(RGB(ReadyColor[0], ReadyColor[1], ReadyColor[2]));
    hReactBrush = CreateSolidBrush(RGB(ReactColor[0], ReactColor[1], ReactColor[2]));
    hEarlyBrush = CreateSolidBrush(RGB(EarlyColor[0], EarlyColor[1], EarlyColor[2]));
    hResultBrush = CreateSolidBrush(RGB(ResultColor[0], ResultColor[1], ResultColor[2]));

    // Create the main window.
    HWND hwnd = CreateWindowEx(
        0,
        CLASS_NAME,
        L"reaction-time-tester",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 1280, 720,   // Updated window size.
        NULL,
        NULL,
        hInstance,
        NULL
    );

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    // Set isReadyForGreen to TRUE right after displaying the window.
    isReadyForReact = TRUE;

    if (hwnd == NULL) {
        MessageBox(NULL, L"Failed to create window", L"Error", MB_OK);
        DeleteObject(hReadyBrush);
        DeleteObject(hReactBrush);
        DeleteObject(hEarlyBrush);
        DeleteObject(hResultBrush);
        return 0;
    }

    // Seed the random number generator.
    srand((unsigned)time(NULL));

    // Display the window.
    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    // Schedule the transition to green after a random delay.
    int delay = (rand() % (MaxDelay - MinDelay + 1)) + MinDelay;
    SetTimer(hwnd, TIMER_READY, delay, NULL);

    // Enter the standard Windows message loop.
    MSG msg = {};
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // Cleanup.
    DeleteObject(hReadyBrush);
    DeleteObject(hReactBrush);
    DeleteObject(hEarlyBrush);
    DeleteObject(hResultBrush);

    free(reactionTimes);

    return 0;
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_SETCURSOR:
        if (LOWORD(lParam) == HTCLIENT) {
            SetCursor(LoadCursor(NULL, IDC_HAND));
            return TRUE;
        }
        break;
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        // Decide which brush to use based on the current state.
        HBRUSH hBrush;
        if (isReact) {
            hBrush = hReactBrush;
        }
        else if (isEarly) {
            hBrush = hEarlyBrush;
        }
        else if (isResult) {
            hBrush = hResultBrush;
        }
        else {
            hBrush = hReadyBrush;
        }

        // Paint the entire window with the selected brush.
        RECT rect;
        GetClientRect(hwnd, &rect);
        FillRect(hdc, &rect, hBrush);

        // Display text based on the state.
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(255, 255, 255));  // White text.
        HFONT hFont = CreateFont(30, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, ANSI_CHARSET,
            OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
            DEFAULT_PITCH | FF_DONTCARE, L"Arial");
        SelectObject(hdc, hFont);


        if (isResult) {
            SetTextColor(hdc, RGB(ResultsTextColor[0], ResultsTextColor[1], ResultsTextColor[2]));
            wchar_t buf[100];
            if (currentAttempt < NumberOfTrials) {
                swprintf_s(buf, 100, L"Last: %.2lfms\nComplete %d trials for average.", reactionTimes[(currentAttempt - 1 + NumberOfTrials) % NumberOfTrials], NumberOfTrials);
            }
            else {
                double total = 0;
                for (int i = 0; i < NumberOfTrials; i++) {
                    total += reactionTimes[i];
                }
                double average = total / NumberOfTrials;
                swprintf_s(buf, 100, L"Last: %.2lfms\nAverage (last %d): %.2lfms", reactionTimes[(currentAttempt - 1) % NumberOfTrials], NumberOfTrials, average);
            }

            RECT textRect;
            SetRectEmpty(&textRect);
            DrawText(hdc, buf, -1, &textRect, DT_CALCRECT | DT_WORDBREAK); // Calculate the rectangle required for text

            RECT centeredRect = rect;
            centeredRect.top += (rect.bottom - textRect.bottom) / 2; // Adjust the top for vertical centering

            DrawText(hdc, buf, -1, &centeredRect, DT_CENTER | DT_WORDBREAK);
        }
        else if (isEarly) {
            SetTextColor(hdc, RGB(EarlyTextColor[0], EarlyTextColor[1], EarlyTextColor[2]));
            DrawText(hdc, L"Too early!", -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }

        DeleteObject(hFont);
        EndPaint(hwnd, &ps);
    }
    break;
    case WM_TIMER:
        switch (wParam) {
        case TIMER_READY:
        {
            isReadyForReact = TRUE;
            KillTimer(hwnd, TIMER_READY);
            int greenDelay = (rand() % (MaxDelay - MinDelay + 1)) + MinDelay;
            SetTimer(hwnd, TIMER_REACT, greenDelay, NULL);
            break;
        }
        case TIMER_REACT:
            if (isReadyForReact) {
                isReact = TRUE;  // Change the screen to green.
                isReadyForReact = FALSE;
                QueryPerformanceCounter(&startTime);  // Start the timer for reaction time.
                InvalidateRect(hwnd, NULL, TRUE);  // Force repaint.
            }
            break;
        case TIMER_EARLY:
            ResetLogic(hwnd);  // Reset the game after showing the "too early" state.
            break;
        }
        break;

    case WM_LBUTTONDOWN:
    case WM_KEYDOWN:
        // If it's an alphabetical key or a mouse click, proceed.
        if ((uMsg == WM_KEYDOWN && wParam >= 0x41 && wParam <= 0x5A) || uMsg == WM_LBUTTONDOWN) {
            if (isReact) {
                HandleReactClick(hwnd);
            }
            else if (isEarly) {
                ResetLogic(hwnd);
            }
            else if (isResult) {
                if (currentAttempt == NumberOfTrials) {
                    currentAttempt = 0;
                    for (int i = 0; i < NumberOfTrials; i++) {
                        reactionTimes[i] = 0;
                    }
                    ResetLogic(hwnd);
                }
                else {
                    ResetLogic(hwnd);
                }
            }
            else {
                HandleEarlyClick(hwnd);
            }
        }
        break;


    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }


    return 0;
}

void ResetLogic(HWND hwnd) {
    // Kill all timers.
    KillTimer(hwnd, TIMER_READY);
    KillTimer(hwnd, TIMER_REACT);
    KillTimer(hwnd, TIMER_EARLY);

    // Reset state flags.
    isReact = FALSE;
    isEarly = FALSE;
    isResult = FALSE;
    isReadyForReact = FALSE;

    // Schedule the transition to green after a random delay.
    int delay = (rand() % (MaxDelay - MinDelay + 1)) + MinDelay;
    SetTimer(hwnd, TIMER_READY, delay, NULL);

    // Force repaint.
    InvalidateRect(hwnd, NULL, TRUE);
}

void HandleReactClick(HWND hwnd) {
    QueryPerformanceCounter(&endTime);
    double timeTaken = ((double)(endTime.QuadPart - startTime.QuadPart) / freq.QuadPart) * 1000;

    reactionTimes[currentAttempt % NumberOfTrials] = timeTaken;
    currentAttempt++;

    isReact = FALSE; // No longer green once clicked.
    isResult = TRUE;   // Change to grey to show the result.
    InvalidateRect(hwnd, NULL, TRUE);  // Force repaint.
}

void HandleEarlyClick(HWND hwnd) {
    isEarly = TRUE;
    isReadyForReact = FALSE;
    SetTimer(hwnd, TIMER_EARLY, 1500, NULL);  // Show the red screen for 1.5 seconds.
    InvalidateRect(hwnd, NULL, TRUE);
}

void ResetAll(HWND hwnd) {
    currentAttempt = 0;
    for (int i = 0; i < NumberOfTrials; i++) {
        reactionTimes[i] = 0;
    }
    ResetLogic(hwnd);
}

void LoadConfig() {
    wchar_t exePath[MAX_PATH];
    wchar_t cfgPath[MAX_PATH];

    // Get the full path of the executable.
    GetModuleFileName(NULL, exePath, MAX_PATH);

    // Extract the directory from the full path.
    wchar_t* lastSlash = wcsrchr(exePath, '\\');
    if (lastSlash) {
        *(lastSlash + 1) = L'\0'; // Terminate the string after the last slash.
    }

    // Construct the path for reaction.cfg.
    swprintf_s(cfgPath, MAX_PATH, L"%s%s", exePath, L"reaction.cfg");

    wchar_t buffer[255];

    GetPrivateProfileString(L"Colors", L"ReadyColor", L"", buffer, 255, cfgPath);
    swscanf_s(buffer, L"%d,%d,%d", &ReadyColor[0], &ReadyColor[1], &ReadyColor[2]);

    GetPrivateProfileString(L"Colors", L"ReactColor", L"", buffer, 255, cfgPath);
    swscanf_s(buffer, L"%d,%d,%d", &ReactColor[0], &ReactColor[1], &ReactColor[2]);

    GetPrivateProfileString(L"Colors", L"EarlyColor", L"", buffer, 255, cfgPath);
    swscanf_s(buffer, L"%d,%d,%d", &EarlyColor[0], &EarlyColor[1], &EarlyColor[2]);

    GetPrivateProfileString(L"Colors", L"ResultColor", L"", buffer, 255, cfgPath);
    swscanf_s(buffer, L"%d,%d,%d", &ResultColor[0], &ResultColor[1], &ResultColor[2]);

    MinDelay = GetPrivateProfileInt(L"Delays", L"MinDelay", 1000, cfgPath);
    MaxDelay = GetPrivateProfileInt(L"Delays", L"MaxDelay", 3000, cfgPath);
    NumberOfTrials = GetPrivateProfileInt(L"Trial", L"NumberOfTrials", 5, cfgPath);

    InputRejectionDelay = GetPrivateProfileInt(L"Delays", L"InputRejectionDelay", 150, cfgPath);

    GetPrivateProfileString(L"Colors", L"EarlyTextColor", L"", buffer, 255, cfgPath);
    swscanf_s(buffer, L"%d,%d,%d", &EarlyTextColor[0], &EarlyTextColor[1], &EarlyTextColor[2]);

    GetPrivateProfileString(L"Colors", L"ResultsTextColor", L"", buffer, 255, cfgPath);
    swscanf_s(buffer, L"%d,%d,%d", &ResultsTextColor[0], &ResultsTextColor[1], &ResultsTextColor[2]);


    // If previously allocated, free the memory
    if (reactionTimes) {
        free(reactionTimes);
    }

    // Allocate memory based on NumberOfTrials
    reactionTimes = (double*)malloc(sizeof(double) * NumberOfTrials);
    if (!reactionTimes) {
        MessageBox(NULL, L"Memory allocation failed!", L"Error", MB_OK);
        exit(1);
    }

    // Initialize the memory
    for (int i = 0; i < NumberOfTrials; i++) {
        reactionTimes[i] = 0.0;
    }
}
