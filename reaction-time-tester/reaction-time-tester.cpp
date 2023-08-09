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
#define DEFAULT_VIRTUAL_DEBOUNCE 50
#define DEFAULT_NUM_TRIALS 5
#define DEFAULT_RAWKEYBOARDENABLE 1
#define DEFAULT_RAWMOUSEENABLE 1
#define MAX_KEYS 256

COLORREF ReadyColor[3], ReactColor[3], EarlyColor[3], ResultColor[3], EarlyTextColor[3], ResultsTextColor[3];
int MinDelay, MaxDelay, NumberOfTrials, EarlyResetDelay, VirtualDebounce, RawKeyboardEnable, RawMouseEnable, RawInputDebug;
double* reactionTimes = NULL; // Array to store the last 5 reaction times.
int currentAttempt = 0;
int TotalTrialNumber = 0;
int keyStates[MAX_KEYS] = { 0 }; // 0: not pressed, 1: pressed

// Global variables to maintain the program's state.
BOOL isReact = FALSE;
BOOL isEarly = FALSE;
BOOL isResult = FALSE;
BOOL isReadyForReact = FALSE;

LARGE_INTEGER startTime, endTime, freq, lastInputTime; // For high-resolution timing.

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
bool InitializeConfigFileAndPath(wchar_t* cfgPath, size_t maxLength);
void LoadColorConfiguration(const wchar_t* cfgPath, const wchar_t* colorName, COLORREF* targetColorArray);
void CheckColorValidity(COLORREF color[]);
void BrushCleanup();
void ValidateDelays();
void LoadTrialConfiguration(const wchar_t* cfgPath);
void LoadTextColorConfiguration(const wchar_t* cfgPath);
void GetColorFromConfig(const wchar_t* cfgPath, const wchar_t* colorName, COLORREF* targetColorArray, wchar_t* buffer);
void AllocateMemoryForReactionTimes();
void HandleError(const wchar_t* errorMessage);
void RegisterForRawKeyboardInput(HWND hwnd);
void RegisterForRawMouseInput(HWND hwnd);
void HandleInput(HWND hwnd);
void HandleRawKeyboardInput(RAWINPUT* raw, HWND hwnd);
void HandleGenericKeyboardInput(HWND hwnd);
void HandleRawMouseInput(RAWINPUT* raw, HWND hwnd);
void HandleGenericMouseInput(HWND hwnd);


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
    if (RawKeyboardEnable==1) RegisterForRawKeyboardInput(hwnd);
    if (RawMouseEnable == 1) RegisterForRawMouseInput(hwnd);

    if (RawInputDebug == 1) {
        wchar_t message[256];
        swprintf(message, sizeof(message) / sizeof(wchar_t), L"RawKeyboardEnable: %d\nRawMouseEnable: %d\nRegisterForRawKeyboardInput: %s\nRegisterForRawMouseInput: %s", RawKeyboardEnable, RawMouseEnable, RegisterForRawKeyboardInput ? L"True" : L"False", RegisterForRawMouseInput ? L"True" : L"False");
        MessageBox(NULL, message, L"Raw Input Variables", MB_OK);
    }

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
        UINT dwSize = 0;
        GetRawInputData((HRAWINPUT)lParam, RID_INPUT, NULL, &dwSize, sizeof(RAWINPUTHEADER));
        LPBYTE lpb = new BYTE[dwSize];

        if (lpb == NULL) {
            return 0;
        }

        if (GetRawInputData((HRAWINPUT)lParam, RID_INPUT, lpb, &dwSize, sizeof(RAWINPUTHEADER)) != dwSize) {
            MessageBox(NULL, L"GetRawInputData did not return correct size!", L"Error", MB_OK);
            delete[] lpb;
            return 0;
        }

        RAWINPUT* raw = (RAWINPUT*)lpb;

        if (raw->header.dwType == RIM_TYPEKEYBOARD && RawKeyboardEnable == 1) {
            HandleRawKeyboardInput(raw, hwnd);
        }
        else if (raw->header.dwType == RIM_TYPEMOUSE && RawMouseEnable == 1) {
            HandleRawMouseInput(raw, hwnd);
        }

        delete[] lpb;
    }
    break;

    // Handle non-raw keyboard input
    case WM_KEYDOWN:
    case WM_KEYUP:
        if (RawKeyboardEnable == 0) {
            HandleGenericKeyboardInput(hwnd);
        }
        break;

        // Handle non-raw mouse input
    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP:
        if (RawMouseEnable == 0) {
            HandleGenericMouseInput(hwnd);
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

bool InitializeConfigFileAndPath(wchar_t* cfgPath, size_t maxLength){
    wchar_t exePath[MAX_PATH];
    wchar_t defaultCfgPath[MAX_PATH];

    if (!GetModuleFileName(NULL, exePath, MAX_PATH)) {
        MessageBox(NULL, L"Failed to get module file name", L"Error", MB_OK);
        return false;  // Failed to get the module file name
    }

    wchar_t* lastSlash = wcsrchr(exePath, '\\');
    if (lastSlash) {
        *(lastSlash + 1) = L'\0';
    }

    swprintf_s(cfgPath, maxLength, L"%s%s", exePath, L"reaction.cfg");
    swprintf_s(defaultCfgPath, MAX_PATH, L"%s%s", exePath, L"default.cfg");

    // Check if reaction.cfg exists
    if (GetFileAttributes(cfgPath) == INVALID_FILE_ATTRIBUTES) {
        // If reaction.cfg does not exist, copy from default.cfg

        FILE* defaultFile;
        errno_t err1 = _wfopen_s(&defaultFile, defaultCfgPath, L"rb"); // open in binary mode for reading
        if (err1 != 0 || !defaultFile) {
            MessageBox(NULL, L"Failed to open default.cfg", L"Error", MB_OK);
            return false;
        }

        FILE* newFile;
        errno_t err2 = _wfopen_s(&newFile, cfgPath, L"wb");  // open in binary mode for writing
        if (err2 != 0 || !newFile) {
            fclose(defaultFile);
            MessageBox(NULL, L"Failed to create reaction.cfg", L"Error", MB_OK);
            return false;
        }

        char buffer[1024];
        size_t bytesRead;
        while ((bytesRead = fread(buffer, 1, sizeof(buffer), defaultFile)) > 0) {
            fwrite(buffer, 1, bytesRead, newFile);
        }

        fclose(newFile);
        fclose(defaultFile);
    }

    return true;  // Successfully obtained the config path
}


void LoadConfig() {
    wchar_t cfgPath[MAX_PATH];

    if (!InitializeConfigFileAndPath(cfgPath, MAX_PATH)) {
        exit(1); // Exiting as the path acquisition failed
    }

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
    RawMouseEnable = GetPrivateProfileInt(L"Toggles", L"RawMouseEnable", DEFAULT_RAWMOUSEENABLE, cfgPath);
    RawInputDebug = GetPrivateProfileInt(L"Toggles", L"RawInputDebug", 0, cfgPath); // Debug => No macro wanted

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

void RegisterForRawKeyboardInput(HWND hwnd) {
    RAWINPUTDEVICE rid{};

    // Register for keyboard raw input
    rid.usUsagePage = 0x01;
    rid.usUsage = 0x06;
    rid.dwFlags = 0;
    rid.hwndTarget = hwnd;

    if (!RegisterRawInputDevices(&rid, 1, sizeof(rid))) {
        MessageBox(NULL, L"Failed to register raw keyboard input device", L"Error", MB_OK);
    }
}

void RegisterForRawMouseInput(HWND hwnd) {
    RAWINPUTDEVICE rid{};

    // Register for mouse raw input
    rid.usUsagePage = 0x01;
    rid.usUsage = 0x02;
    rid.dwFlags = 0;
    rid.hwndTarget = hwnd;

    if (!RegisterRawInputDevices(&rid, 1, sizeof(rid))) {
        MessageBox(NULL, L"Failed to register raw mouse input device", L"Error", MB_OK);
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
    Sleep(VirtualDebounce); // Rudimentary "debounce." Seems to work okay for this application
}

void HandleRawKeyboardInput(RAWINPUT* raw, HWND hwnd) {
    int vkey = raw->data.keyboard.VKey;

    if (raw->data.keyboard.Flags == RI_KEY_MAKE) {
        if (!keyStates[vkey]) {
            if ((vkey >= 0x30 && vkey <= 0x39) || // '0' to '9'
                (vkey >= 0x41 && vkey <= 0x5A))   // 'A' to 'Z'
            {
                HandleInput(hwnd);
                keyStates[vkey] = 1; // Latch the key state
            }
        }
    }
    else if (raw->data.keyboard.Flags == RI_KEY_BREAK) {
        keyStates[vkey] = 0; // Unlatch on key release
    }
}

void HandleGenericKeyboardInput(HWND hwnd) {
    for (int vk = 0x30; vk <= 0x5A; vk++) {
        if (vk <= 0x39 || (vk >= 0x41 && vk <= 0x5A)) {
            bool isKeyPressed = GetAsyncKeyState(vk) & 0x8000;
            if (isKeyPressed && !keyStates[vk]) {
                HandleInput(hwnd);
                keyStates[vk] = 1; // Latch the key state
            }
            else if (!isKeyPressed && keyStates[vk]) {
                keyStates[vk] = 0; // Unlatch on key release
            }
        }
    }
}

void HandleRawMouseInput(RAWINPUT* raw, HWND hwnd) {
    static int wasButtonPressed = 0; // 0: not pressed, 1: pressed

    if (raw->data.mouse.usButtonFlags & RI_MOUSE_LEFT_BUTTON_DOWN && !wasButtonPressed) {
        HandleInput(hwnd);
        wasButtonPressed = 1; // Latch the button state
    }
    else if (raw->data.mouse.usButtonFlags & RI_MOUSE_LEFT_BUTTON_UP && wasButtonPressed) {
        wasButtonPressed = 0; // Unlatch immediately on button release
    }
}

void HandleGenericMouseInput(HWND hwnd) {
    static int wasButtonPressed = 0; // 0: not pressed, 1: pressed
    int isButtonPressed = GetAsyncKeyState(VK_LBUTTON) & 0x8000;

    if (isButtonPressed && !wasButtonPressed) { // Check for transition from up to down
        HandleInput(hwnd);
        wasButtonPressed = 1; // Latch the button state
    }
    else if (!isButtonPressed && wasButtonPressed) { // Check for transition from down to up
        wasButtonPressed = 0; // Unlatch immediately on button release
    }
}
