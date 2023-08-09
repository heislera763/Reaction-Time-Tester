#include <windows.h>
#include <stdlib.h>
#include <time.h>
#include <stdio.h>

#define TIMER_READY 1
#define TIMER_REACT 2
#define TIMER_EARLY 3
#define CHECK_RGB_VALUE(v) ((v) >= 0 && (v) <= 255)
#define MAXMINDELAY (rand() % (MaxDelay - MinDelay + 1)) + MinDelay
#define DEFAULT_MIN_DELAY 1000
#define DEFAULT_MAX_DELAY 3000
#define DEFAULT_EARLY_RESET_DELAY 3000
#define DEFAULT_VIRTUAL_DEBOUNCE 0
#define DEFAULT_NUM_TRIALS 5
#define DEFAULT_RAWKEYBOARDENABLE 0

COLORREF ReadyColor[3], ReactColor[3], EarlyColor[3], ResultColor[3], EarlyTextColor[3], ResultsTextColor[3];
int MinDelay, MaxDelay, NumberOfTrials, EarlyResetDelay, VirtualDebounce, RawKeyboardEnable;

int TotalTrialNumber = 0;

// Global variables to maintain the program's state.
BOOL isReact = FALSE;
BOOL isEarly = FALSE;
BOOL isResult = FALSE;
BOOL isReadyForReact = FALSE;
BOOL isRawInputEnabled = FALSE;

LARGE_INTEGER startTime, endTime, freq, lastInputTime; // For high-resolution timing.
BOOL isFirstInput = TRUE;

double* reactionTimes = NULL; // Array to store the last 5 reaction times.
int currentAttempt = 0;

// Font for text.
HFONT hFont;

// Brushes for painting the window background.
HBRUSH hReadyBrush, hReactBrush, hEarlyBrush, hResultBrush;

// Forward declarations for window procedure and other functions.
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void ResetLogic(HWND hwnd);
void HandleReactClick(HWND hwnd);
void HandleEarlyClick(HWND hwnd);
void ResetAll(HWND hwnd);
void LoadConfig();
void LoadColorConfiguration(const wchar_t* cfgPath, const wchar_t* colorName, COLORREF* targetColorArray);
void CheckColorValidity(COLORREF color[]);
void BrushCleanup();
void ValidateDelays();
void LoadTrialConfiguration(const wchar_t* cfgPath);
void LoadTextColorConfiguration(const wchar_t* cfgPath);
void GetColorFromConfig(const wchar_t* cfgPath, const wchar_t* colorName, COLORREF* targetColorArray, wchar_t* buffer);
void AllocateMemoryForReactionTimes();
void HandleError(const wchar_t* errorMessage);
void RegisterForRawInput(HWND hwnd);
void HandleInput(HWND hwnd);

int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nCmdShow) {
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

    // Create the main window.
    HWND hwnd = CreateWindowEx(
        0,
        CLASS_NAME,
        L"reaction-time-tester",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 1280, 720, // Updated window size.
        NULL,
        NULL,
        hInstance,
        NULL
    );

    // Raw input
    RegisterForRawInput(hwnd);

    // Display the window.
    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    if (hwnd == NULL) {
        MessageBox(NULL, L"Failed to create window", L"Error", MB_OK);
        BrushCleanup();
        return 0;
    }

    // Set isReadyForGreen to TRUE right after displaying the window.
    isReadyForReact = TRUE;

    // Seed the random number generator.
    srand((unsigned)time(NULL));

    // Schedule the transition to green after a random delay.
    SetTimer(hwnd, TIMER_READY, MAXMINDELAY, NULL);

    // Prepare font before message loop
    hFont = CreateFont(30, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, ANSI_CHARSET,
        OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE, L"Arial");

    // Enter the standard Windows message loop.
    MSG msg = {};
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // Cleanup.
    BrushCleanup();

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

    case WM_PAINT: {
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
        SetTextColor(hdc, RGB(255, 255, 255)); // Default to white text.
        SelectObject(hdc, hFont);

        wchar_t buf[100] = { 0 }; // Initialize buffer

        if (isResult) {
            SetTextColor(hdc, RGB(ResultsTextColor[0], ResultsTextColor[1], ResultsTextColor[2]));
            TotalTrialNumber++;

            if (currentAttempt < NumberOfTrials) {
                swprintf_s(buf, 100, L"Last: %.2lfms\nComplete %d trials for average.\nTrials so far: %d", reactionTimes[(currentAttempt - 1 + NumberOfTrials) % NumberOfTrials], NumberOfTrials, TotalTrialNumber);
            }
            else {
                double total = 0;
                for (int i = 0; i < NumberOfTrials; i++) {
                    total += reactionTimes[i];
                }
                double average = total / NumberOfTrials;
                swprintf_s(buf, 100, L"Last: %.2lfms\nAverage (last %d): %.2lfms\nTrials so far: %d", reactionTimes[(currentAttempt - 1) % NumberOfTrials], NumberOfTrials, average, TotalTrialNumber);
            }

        }
        else if (isEarly) {
            SetTextColor(hdc, RGB(EarlyTextColor[0], EarlyTextColor[1], EarlyTextColor[2]));
            swprintf_s(buf, 100, L"Too early!\nTrials so far: %d", TotalTrialNumber);
        }

        // Calculate rectangle for the text and display it.
        RECT textRect;
        SetRectEmpty(&textRect);
        DrawText(hdc, buf, -1, &textRect, DT_CALCRECT | DT_WORDBREAK);
        RECT centeredRect = rect;
        centeredRect.top += (rect.bottom - textRect.bottom) / 2;
        DrawText(hdc, buf, -1, &centeredRect, DT_CENTER | DT_WORDBREAK);

        EndPaint(hwnd, &ps);
    } break;


    case WM_TIMER:
        switch (wParam) {
        case TIMER_READY:
            isReadyForReact = TRUE;
            KillTimer(hwnd, TIMER_READY);
            SetTimer(hwnd, TIMER_REACT, MAXMINDELAY, NULL);
            break;

        case TIMER_REACT:
            if (isReadyForReact) {
                isReact = TRUE;
                isReadyForReact = FALSE;
                QueryPerformanceCounter(&startTime); // Start the timer for reaction time.
                InvalidateRect(hwnd, NULL, TRUE); // Force repaint.
            }
            break;

        case TIMER_EARLY:
            ResetLogic(hwnd); // Reset the game after showing the "too early" state.
            break;

        }
        break;

    case WM_INPUT:
    {
        if (RawKeyboardEnable == 1) {  // Raw input is enabled
            UINT dwSize = 0;
            GetRawInputData((HRAWINPUT)lParam, RID_INPUT, NULL, &dwSize, sizeof(RAWINPUTHEADER));
            LPBYTE lpb = new BYTE[dwSize];

            if (lpb == NULL) {
                return 0;
            }

            if (GetRawInputData((HRAWINPUT)lParam, RID_INPUT, lpb, &dwSize, sizeof(RAWINPUTHEADER)) != dwSize) {
                MessageBox(NULL, L"GetRawInputData did not return correct size!", L"Error", MB_OK);
            }

            RAWINPUT* raw = (RAWINPUT*)lpb;

            if (raw->header.dwType == RIM_TYPEKEYBOARD && (raw->data.keyboard.Flags & RI_KEY_MAKE)) {
                if ((raw->data.keyboard.VKey >= 0x30 && raw->data.keyboard.VKey <= 0x39) || // '0' to '9'
                    (raw->data.keyboard.VKey >= 0x41 && raw->data.keyboard.VKey <= 0x5A))   // 'A' to 'Z'
                {
                    HandleInput(hwnd);
                }
            }
            else if (raw->header.dwType == RIM_TYPEMOUSE && (raw->data.mouse.usButtonFlags & RI_MOUSE_LEFT_BUTTON_DOWN)) {
                HandleInput(hwnd);
            }
            delete[] lpb;
        }
        else {  // Use generic Windows input
            bool keyFound = false;
            for (int vk = 0x30; vk <= 0x5A; vk++) {  // Check all alphanumeric keys
                if (vk <= 0x39 || (vk >= 0x41 && vk <= 0x5A)) {
                    if (GetAsyncKeyState(vk) & 0x8000) {
                        HandleInput(hwnd);
                        keyFound = true;
                        break;
                    }
                }
            }
            if (!keyFound && (GetAsyncKeyState(VK_LBUTTON) & 0x8000)) {
                // Handle the left mouse click logic
                HandleInput(hwnd);
            }
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
    SetTimer(hwnd, TIMER_READY, MAXMINDELAY, NULL);

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
    SetTimer(hwnd, TIMER_EARLY, EarlyResetDelay, NULL);  // Show the red screen for 1.5 seconds.
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

    if (!GetModuleFileName(NULL, exePath, MAX_PATH)) {
        MessageBox(NULL, L"Failed to get module file name", L"Error", MB_OK);
        exit(1);
    }

    wchar_t* lastSlash = wcsrchr(exePath, '\\');
    if (lastSlash) {
        *(lastSlash + 1) = L'\0';
    }

    swprintf_s(cfgPath, MAX_PATH, L"%s%s", exePath, L"reaction.cfg");

    LoadColorConfiguration(cfgPath, L"ReadyColor", ReadyColor);
    LoadColorConfiguration(cfgPath, L"ReactColor", ReactColor);
    LoadColorConfiguration(cfgPath, L"EarlyColor", EarlyColor);
    LoadColorConfiguration(cfgPath, L"ResultColor", ResultColor);
    hReadyBrush = CreateSolidBrush(RGB(ReadyColor[0], ReadyColor[1], ReadyColor[2]));
    hReactBrush = CreateSolidBrush(RGB(ReactColor[0], ReactColor[1], ReactColor[2]));
    hEarlyBrush = CreateSolidBrush(RGB(EarlyColor[0], EarlyColor[1], EarlyColor[2]));
    hResultBrush = CreateSolidBrush(RGB(ResultColor[0], ResultColor[1], ResultColor[2]));

    MinDelay = GetPrivateProfileInt(L"Delays", L"MinDelay", DEFAULT_MIN_DELAY, cfgPath);
    MaxDelay = GetPrivateProfileInt(L"Delays", L"MaxDelay", DEFAULT_MAX_DELAY, cfgPath);
    EarlyResetDelay = GetPrivateProfileInt(L"Delays", L"EarlyResetDelay", DEFAULT_EARLY_RESET_DELAY, cfgPath);
    VirtualDebounce = GetPrivateProfileInt(L"Delays", L"VirtualDebounce", DEFAULT_VIRTUAL_DEBOUNCE, cfgPath);
    ValidateDelays(); 

    RawKeyboardEnable = GetPrivateProfileInt(L"Toggles", L"RawKeyboardEnable", DEFAULT_RAWKEYBOARDENABLE, cfgPath);

    LoadTrialConfiguration(cfgPath);
    LoadTextColorConfiguration(cfgPath);

    AllocateMemoryForReactionTimes();
}

void ValidateDelays() {
    if (MinDelay <= 0 || MaxDelay <= 0 || MaxDelay < MinDelay || VirtualDebounce < 0 || EarlyResetDelay <=0) {
        MessageBox(NULL, L"Invalid delay values in the configuration!", L"Error", MB_OK);
        exit(1);
    }
}

void LoadTrialConfiguration(const wchar_t* cfgPath) {
    NumberOfTrials = GetPrivateProfileInt(L"Trial", L"NumberOfTrials", DEFAULT_NUM_TRIALS, cfgPath);
    if (NumberOfTrials <= 0) {
        MessageBox(NULL, L"Invalid number of trials in the configuration!", L"Error", MB_OK);
        exit(1);
    }
}

void GetColorFromConfig(const wchar_t* cfgPath, const wchar_t* colorName, COLORREF* targetColorArray, wchar_t* buffer) {
    GetPrivateProfileString(L"Colors", colorName, L"", buffer, MAX_PATH, cfgPath);
    if (wcslen(buffer) == 0) {
        HandleError(L"Failed to read color configuration");
    }
    swscanf_s(buffer, L"%d,%d,%d", &targetColorArray[0], &targetColorArray[1], &targetColorArray[2]);
}

void LoadTextColorConfiguration(const wchar_t* cfgPath) {
    wchar_t buffer[MAX_PATH];
    GetColorFromConfig(cfgPath, L"EarlyTextColor", EarlyTextColor, buffer);
    GetColorFromConfig(cfgPath, L"ResultsTextColor", ResultsTextColor, buffer);
}

void AllocateMemoryForReactionTimes() {
    reactionTimes = (double*)malloc(sizeof(double) * NumberOfTrials);
    if (reactionTimes == NULL) {
        MessageBox(NULL, L"Failed to allocate memory for reaction times!", L"Error", MB_OK);
        exit(1);
    }
    memset(reactionTimes, 0, sizeof(double) * NumberOfTrials);
}

void LoadColorConfiguration(const wchar_t* cfgPath, const wchar_t* colorName, COLORREF* targetColorArray) {
    wchar_t buffer[255];
    GetPrivateProfileString(L"Colors", colorName, L"", buffer, sizeof(buffer) / sizeof(wchar_t), cfgPath);
    swscanf_s(buffer, L"%d,%d,%d", &targetColorArray[0], &targetColorArray[1], &targetColorArray[2]);
    CheckColorValidity(targetColorArray);
}

void CheckColorValidity(COLORREF color[]) {
    if (!CHECK_RGB_VALUE(color[0]) ||
        !CHECK_RGB_VALUE(color[1]) ||
        !CHECK_RGB_VALUE(color[2])) {
        MessageBox(NULL, L"Invalid color values in the configuration!", L"Error", MB_OK);
        exit(1);
    }
}

void BrushCleanup() {
    if (hReadyBrush) DeleteObject(hReadyBrush);
    if (hReactBrush) DeleteObject(hReactBrush);
    if (hEarlyBrush) DeleteObject(hEarlyBrush);
    if (hResultBrush) DeleteObject(hResultBrush);
}


void HandleError(const wchar_t* errorMessage) {
    MessageBox(NULL, errorMessage, L"Error", MB_OK);
    exit(1);
}

void RegisterForRawInput(HWND hwnd) {
    RAWINPUTDEVICE rid[2]{};

    // Register for keyboard raw input
    rid[0].usUsagePage = 0x01;
    rid[0].usUsage = 0x06;
    rid[0].dwFlags = 0;
    rid[0].hwndTarget = hwnd;

    // Register for mouse raw input
    rid[1].usUsagePage = 0x01;
    rid[1].usUsage = 0x02;
    rid[1].dwFlags = 0;
    rid[1].hwndTarget = hwnd;

    if (RegisterRawInputDevices(rid, 2, sizeof(rid[0]))) {
        isRawInputEnabled = true;  // set the flag if raw input registration is successful
    }
    else {
        MessageBox(NULL, L"Failed to register raw input devices", L"Error", MB_OK);
    }
}


void HandleInput(HWND hwnd) {
    if (isReact) {
        HandleReactClick(hwnd);
    }
    else if (isEarly || isResult) {
        if (isResult && currentAttempt == NumberOfTrials) {
            currentAttempt = 0;
            for (int i = 0; i < NumberOfTrials; i++) {
                reactionTimes[i] = 0;
            }
        }
        ResetLogic(hwnd);
    }
    else {
        HandleEarlyClick(hwnd);
    }
}