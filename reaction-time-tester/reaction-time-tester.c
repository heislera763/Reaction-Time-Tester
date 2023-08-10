#include <windows.h>
#include <stdlib.h>
#include <time.h>
#include <stdio.h>
#include <stdbool.h>
#include "Resource.h"

#define DEFAULT_MIN_DELAY 1000
#define DEFAULT_MAX_DELAY 3000
#define DEFAULT_EARLY_RESET_DELAY 3000
#define DEFAULT_VIRTUAL_DEBOUNCE 50
#define DEFAULT_NUM_TRIALS 5
#define DEFAULT_RAWKEYBOARDENABLE 1
#define DEFAULT_RAWMOUSEENABLE 1
#define DEFAULT_FONT_SIZE 32
#define DEFAULT_FONT_NAME L"Arial"
#define DEFAULT_FONT_STYLE L"Regular"
#define TIME_BUFFER_SIZE 20
#define MAX_KEYS 256
#define TIMER_READY 1
#define TIMER_REACT 2
#define TIMER_EARLY 3

// Configuration and Settings
COLORREF ReadyColor[3], ReactColor[3], EarlyColor[3], ResultColor[3], EarlyFontColor[3], ResultsFontColor[3];
int MinDelay, MaxDelay, NumberOfTrials, EarlyResetDelay, VirtualDebounce, RawKeyboardEnable, RawMouseEnable, RawInputDebug, LogEnable;
wchar_t logFileName[MAX_PATH]; // Log file name global for ease
wchar_t fontName[MAX_PATH];
wchar_t fontStyle[MAX_PATH];
int fontSize;

// Program State and Data
typedef enum { // Define states
    STATE_READY,
    STATE_REACT,
    STATE_EARLY,
    STATE_RESULT
} ProgramState;
ProgramState currentState = STATE_READY; // Initial state
double* reactionTimes = NULL; // Array to store the last 5 reaction times.
int currentAttempt = 0;
int TotalTrialNumber = 0;
int keyStates[MAX_KEYS] = { 0 }; // 0: not pressed, 1: pressed
LARGE_INTEGER startTime, endTime, freq; // For high-resolution timing
bool isCursorOverClientArea = false; // Window content hover check

// UI and Rendering
HBRUSH hReadyBrush, hReactBrush, hEarlyBrush, hResultBrush; // Brushes for painting the window
HFONT hFont;

// Forward declarations for window procedure and other functions.
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

// Utility functions
void HandleError(const wchar_t* errorMessage);
void ValidateColors(COLORREF color[]);
void ValidateDelays();
void RemoveComment(wchar_t* str);
int GenerateRandomDelay(int min, int max);
void BrushCleanup();

// Configuration and setup functions
bool InitializeConfigFileAndPath(wchar_t* cfgPath, size_t maxLength);
void LoadColorConfiguration(const wchar_t* cfgPath, const wchar_t* sectionName, const wchar_t* colorName, COLORREF* targetColorArray);
void LoadFontConfiguration(const wchar_t* cfgPath, wchar_t* targetFontName, size_t maxLength, int* fontSize, wchar_t* fontStyle, size_t fontStyleLength);
void LoadTrialConfiguration(const wchar_t* cfgPath);
void LoadTextColorConfiguration(const wchar_t* cfgPath);
void AllocateMemoryForReactionTimes();
void LoadConfig();
void InitializeLogFileName();
bool AppendToLog(double x, int y);

// Input handling functions
bool RegisterForRawInput(HWND hwnd, USHORT usage);
void HandleInput(HWND hwnd);
void HandleGenericKeyboardInput(HWND hwnd);
void HandleRawKeyboardInput(RAWINPUT* raw, HWND hwnd);
void HandleGenericMouseInput(HWND hwnd);
void HandleRawMouseInput(RAWINPUT* raw, HWND hwnd);
bool IsAlphanumeric(int vkey);
void UpdateKeyState(int vk, HWND hwnd);

// Main application logic functions
void ResetLogic(HWND hwnd);
void HandleReactClick(HWND hwnd);
void HandleEarlyClick(HWND hwnd);
void ResetAll(HWND hwnd);


int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nCmdShow) {
    QueryPerformanceFrequency(&freq);

    const wchar_t CLASS_NAME[] = L"Sample Window Class";

    WNDCLASS wc = {0};

    // Define properties for our window structs
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;

    // Register the window class.
    if (!RegisterClass(&wc)) {
        HandleError(L"Failed to register window class");
    }

    // Initialize brushes for painting.
    LoadConfig(); // Load configuration at the start

    if (LogEnable==1) InitializeLogFileName();

    // Get the dimensions of the main display
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);

    // Calculate the position to center the window
    int windowWidth = 1280; // Your window width
    int windowHeight = 720; // Your window height
    int posX = (screenWidth - windowWidth) / 2;
    int posY = (screenHeight - windowHeight) / 2;

    // Create the main window centered on the main display
    HWND hwnd = CreateWindowEx(
        0,
        CLASS_NAME,
        L"reaction-time-tester",
        WS_OVERLAPPEDWINDOW,
        posX, posY, windowWidth, windowHeight,
        NULL,
        NULL,
        hInstance,
        NULL
    );

    // Raw input
    bool keyboardRegistered = false;
    bool mouseRegistered = false;

    if (RawKeyboardEnable == 1) keyboardRegistered = RegisterForRawInput(hwnd, 0x06); // Attempt to Register Keyboard
    if (RawMouseEnable == 1) mouseRegistered = RegisterForRawInput(hwnd, 0x02); // Attempt to Register Mouse

    if (RawInputDebug == 1) {
        wchar_t message[256];
        swprintf(message, sizeof(message) / sizeof(wchar_t), L"RawKeyboardEnable: %d\nRawMouseEnable: %d\nRegisterForRawKeyboardInput: %s\nRegisterForRawMouseInput: %s", RawKeyboardEnable, RawMouseEnable, keyboardRegistered ? L"True" : L"False", mouseRegistered ? L"True" : L"False");
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

    // Switch to ready state
    currentState = STATE_READY;

    // Seed the random number generator.
    srand((unsigned)time(NULL));

    // Schedule the transition to react after a random delay.
    SetTimer(hwnd, TIMER_READY, GenerateRandomDelay(MinDelay, MaxDelay), NULL);

    int fontWeight = FW_REGULAR;
    BOOL isItalic = FALSE;

    if (wcscmp(fontStyle, L"Bold") == 0) {
        fontWeight = FW_BOLD;
    }
    else if (wcscmp(fontStyle, L"Italic") == 0) {
        isItalic = TRUE;
    }

    // Prepare font using the loaded configurations
    hFont = CreateFont(fontSize, 0, 0, 0, fontWeight, isItalic, FALSE, FALSE, ANSI_CHARSET,
        OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE, fontName);

    if (!hFont) {
        HandleError(L"Failed to create font.");
    }

    // Enter the standard Windows message loop.
    MSG msg = {0};
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
    case WM_CREATE:
    {
        // Load the icon from the resource
        HICON hIcon = (HICON)LoadImage(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_ICON1), IMAGE_ICON, 32, 32, 0);

        // Set the icon for the window
        SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);

        // For the Alt-Tab dialog
        HICON hIconLarge = (HICON)LoadImage(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_ICON1), IMAGE_ICON, 64, 64, 0);
        SendMessage(hwnd, WM_SETICON, ICON_BIG, (LPARAM)hIconLarge);
    }
    break;

    case WM_SETCURSOR:
        switch (LOWORD(lParam)) {
        case HTCLIENT: // In this section we use isCursorOverClientArea to signify hovering over the window contents, I wonder if we can check cursor state directly to get rid of this var?
            isCursorOverClientArea = true;
            SetCursor(LoadCursor(NULL, IDC_HAND)); // Set the cursor to a hand cursor
            break;
        case HTLEFT:
        case HTRIGHT:
            isCursorOverClientArea = false;
            SetCursor(LoadCursor(NULL, IDC_SIZEWE)); // Left or right border (resize cursor)
            break;
        case HTTOP:
        case HTBOTTOM:
            isCursorOverClientArea = false;
            SetCursor(LoadCursor(NULL, IDC_SIZENS)); // Top or bottom border (resize cursor)
            break;
        case HTTOPLEFT:
        case HTBOTTOMRIGHT:
            isCursorOverClientArea = false;
            SetCursor(LoadCursor(NULL, IDC_SIZENWSE)); // Top-left or bottom-right corner (diagonal resize cursor)
            break;
        case HTTOPRIGHT:
        case HTBOTTOMLEFT:
            isCursorOverClientArea = false;
            SetCursor(LoadCursor(NULL, IDC_SIZENESW)); // Top-right or bottom-left corner (diagonal resize cursor)
            break;
        default:
            isCursorOverClientArea = false;
            SetCursor(LoadCursor(NULL, IDC_ARROW)); // Use the default cursor
            break;
        }
        return TRUE;


    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        // Decide which brush to use based on the current state.
        HBRUSH hBrush;
        switch (currentState) {
        case STATE_REACT:
            hBrush = hReactBrush;
            break;

        case STATE_EARLY:
            hBrush = hEarlyBrush;
            break;

        case STATE_RESULT:
            hBrush = hResultBrush;
            break;

        case STATE_READY:
            hBrush = hReadyBrush;
            break;

        default:
            hBrush = hReadyBrush; // Default to a known brush to avoid undefined behavior.
            HandleError(L"Invalid or undefined program state!");
            break;
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

        if (currentState == STATE_RESULT) {
            SetTextColor(hdc, RGB(ResultsFontColor[0], ResultsFontColor[1], ResultsFontColor[2]));
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
            if (LogEnable==1) AppendToLog(reactionTimes[(currentAttempt - 1 + NumberOfTrials) % NumberOfTrials], TotalTrialNumber);
        }
        else if (currentState == STATE_EARLY) {
            SetTextColor(hdc, RGB(EarlyFontColor[0], EarlyFontColor[1], EarlyFontColor[2]));
            swprintf_s(buf, 100, L"Too early!\nTrials so far: %d", TotalTrialNumber);
        }

        // Calculate rectangle for the text and display it.
        RECT textRect;
        SetRectEmpty(&textRect);
        DrawText(hdc, buf, -1, &textRect, DT_CALCRECT | DT_WORDBREAK);
        RECT centeredRect = rect;
        centeredRect.top += (rect.bottom - rect.top - (textRect.bottom - textRect.top)) / 2; // Text doesn't center vertically (Possibly due to newline not being accounted for?)
        DrawText(hdc, buf, -1, &centeredRect, DT_CENTER | DT_WORDBREAK);

        EndPaint(hwnd, &ps);
    } break;


    case WM_TIMER:
        switch (wParam) {
        case TIMER_READY:
            currentState = STATE_READY;
            KillTimer(hwnd, TIMER_READY);
            SetTimer(hwnd, TIMER_REACT, GenerateRandomDelay(MinDelay, MaxDelay), NULL);
            break;

        case TIMER_REACT:
            if (currentState == STATE_READY) {
                currentState = STATE_REACT;
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
        LPBYTE lpb = (LPBYTE)malloc(dwSize * sizeof(BYTE));

        if (lpb == NULL) {
            return 0;
        }

        if (GetRawInputData((HRAWINPUT)lParam, RID_INPUT, lpb, &dwSize, sizeof(RAWINPUTHEADER)) != dwSize) {
            HandleError(L"GetRawInputData did not return correct size!");
            free(lpb);
            return 0; // added to ensure function exits after an error
        }

        RAWINPUT* raw = (RAWINPUT*)lpb;

        if (raw->header.dwType == RIM_TYPEKEYBOARD && RawKeyboardEnable == 1) { // Handle raw keyboard inputs
            HandleRawKeyboardInput(raw, hwnd);
        }
        else if (raw->header.dwType == RIM_TYPEMOUSE && RawMouseEnable == 1) { // Handle raw mouse inputs
            HandleRawMouseInput(raw, hwnd);
        }

        free(lpb);
    }
    break;

    // Handle generic keyboard input:
    case WM_KEYDOWN:
    case WM_KEYUP:
        if (RawKeyboardEnable == 0) {
            HandleGenericKeyboardInput(hwnd);
        }
        break;

    // Handle generic mouse input:
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


// Utility Functions:
void HandleError(const wchar_t* errorMessage) { // Generic error handler
    MessageBox(NULL, errorMessage, L"Error", MB_OK);
    exit(1);
}

void ValidateColors(COLORREF color[]) {
    for (int i = 0; i < 3; i++) {
        if (color[i] < 0 || color[i] > 255) {
            HandleError(L"Invalid color values in the configuration!");
            return;
        }
    }
}

void ValidateDelays() { // Check for validity of delay. Very single purpose, may want to rewrite for modularity/flexibility
    if (MinDelay <= 0 || MaxDelay <= 0 || MaxDelay < MinDelay || VirtualDebounce < 0 || EarlyResetDelay <= 0) {
        HandleError(L"Invalid delay values in the configuration!");
    }
}

void RemoveComment(wchar_t* str) { // Removes comments and trailing spaces from strings read from .cfg files
    wchar_t* semicolonPos = wcschr(str, L';'); // Locate the first occurrence of a semicolon, which denotes the start of a comment.
    if (semicolonPos) {
        *semicolonPos = L'\0';  // Terminate the string at the start of the comment.
    }

    size_t len = wcslen(str);
    while (len > 0 && iswspace(str[len - 1])) { // Loop to remove any trailing whitespace after excluding the comment.
        str[len - 1] = L'\0';
        len--;
    }
}


int GenerateRandomDelay(int min, int max) { // Generate a random delay given a min and max delay value
    return rand() % (max - min + 1) + min;
}

void BrushCleanup() { // Hardcoded to delete only these brushes, should probably replace with a more modular approach...
    if (hReadyBrush) DeleteObject(hReadyBrush);
    if (hReactBrush) DeleteObject(hReactBrush);
    if (hEarlyBrush) DeleteObject(hEarlyBrush);
    if (hResultBrush) DeleteObject(hResultBrush);
}


// Configuration and setup functions
bool InitializeConfigFileAndPath(wchar_t* cfgPath, size_t maxLength) { // Initializes paths and possibly copies default.cfg to reaction.cfg
    wchar_t exePath[MAX_PATH];
    wchar_t defaultCfgPath[MAX_PATH];

    if (!GetModuleFileName(NULL, exePath, MAX_PATH)) {  // Failed to get current module's file path.
        HandleError(L"Failed to get module file name");
        return false;
    }

    wchar_t* lastSlash = wcsrchr(exePath, '\\');  // Find the last directory separator.
    if (lastSlash) *(lastSlash + 1) = L'\0';  // Null-terminate to get directory path.

    if (swprintf_s(cfgPath, maxLength, L"%s%s", exePath, L"reaction.cfg") < 0 ||
        swprintf_s(defaultCfgPath, MAX_PATH, L"%s%s", exePath, L"default.cfg") < 0) {  // Format paths for config files.
        HandleError(L"Failed to create config paths");
        return false;
    }

    if (GetFileAttributes(cfgPath) == INVALID_FILE_ATTRIBUTES) {  // If reaction.cfg doesn't exist, copy from default.cfg.
        FILE* defaultFile;
        errno_t err1 = _wfopen_s(&defaultFile, defaultCfgPath, L"rb");
        if (err1 != 0 || !defaultFile) {
            HandleError(L"Failed to open default.cfg");
            return false;
        }

        FILE* newFile;
        errno_t err2 = _wfopen_s(&newFile, cfgPath, L"wb");
        if (err2 != 0 || !newFile) {
            fclose(defaultFile);
            HandleError(L"Failed to create reaction.cfg");
            return false;
        }

        char buffer[1024];
        size_t bytesRead;
        while ((bytesRead = fread(buffer, 1, sizeof(buffer), defaultFile)) > 0) {  // Copy contents from default.cfg to reaction.cfg in chunks.
            if (fwrite(buffer, 1, bytesRead, newFile) < bytesRead) { // Has write operation written the correct number of bytes?
                fclose(newFile); fclose(defaultFile);
                HandleError(L"Failed to write to reaction.cfg");
                return false;
            }
        }
        fclose(newFile); fclose(defaultFile);
    }
    return true;
}

void LoadColorConfiguration(const wchar_t* cfgPath, const wchar_t* sectionName, const wchar_t* colorName, COLORREF* targetColorArray) {
    wchar_t buffer[255];
    GetPrivateProfileString(sectionName, colorName, L"", buffer, sizeof(buffer) / sizeof(wchar_t), cfgPath);  // Retrieve color configuration as comma-separated RGB values

    if (wcslen(buffer) == 0) {  // Check color data retrieval
        HandleError(L"Failed to read color configuration");
    }

    swscanf_s(buffer, L"%d,%d,%d", &targetColorArray[0], &targetColorArray[1], &targetColorArray[2]);  // Extract individual RGB values.
    ValidateColors(targetColorArray);  // Validate RGB values
}

void LoadFontConfiguration(const wchar_t* cfgPath, wchar_t* targetFontName, size_t maxLength, int* fontSize, wchar_t* fontStyle, size_t fontStyleLength) {
    GetPrivateProfileString(L"Fonts", L"FontName", DEFAULT_FONT_NAME, targetFontName, (DWORD)maxLength, cfgPath);  // Retrieve font name
    RemoveComment(targetFontName);  // Cleanup any comments from the retrieved font name
    if (wcslen(targetFontName) == 0) {  // Check font name retrieval
        HandleError(L"Failed to read font configuration");
    }

    *fontSize = GetPrivateProfileInt(L"Fonts", L"FontSize", DEFAULT_FONT_SIZE, cfgPath);  // Retrieve font size

    GetPrivateProfileString(L"Fonts", L"FontStyle", DEFAULT_FONT_STYLE, fontStyle, (DWORD)fontStyleLength, cfgPath);  // Retrieve font style
    RemoveComment(fontStyle);  // Cleanup any comments from the retrieved font style.
}


void LoadTrialConfiguration(const wchar_t* cfgPath) {
    NumberOfTrials = GetPrivateProfileInt(L"Trial", L"NumberOfTrials", DEFAULT_NUM_TRIALS, cfgPath);
    if (NumberOfTrials <= 0) {
        HandleError(L"Invalid number of trials in the configuration!");
    }
}

void LoadTextColorConfiguration(const wchar_t* cfgPath) {
    wchar_t buffer[MAX_PATH] = L"";
    LoadColorConfiguration(cfgPath, L"Fonts", L"EarlyFontColor", EarlyFontColor);
    LoadColorConfiguration(cfgPath, L"Fonts", L"ResultsFontColor", ResultsFontColor);
}

void AllocateMemoryForReactionTimes() {
    size_t totalSize = sizeof(double) * NumberOfTrials;
    reactionTimes = (double*)malloc(totalSize);
    if (reactionTimes == NULL) {
        HandleError(L"Failed to allocate memory for reaction times!");
        return; // Return to prevent further execution
    }
    memset(reactionTimes, 0, totalSize);
}

void LoadConfig() {
    wchar_t cfgPath[MAX_PATH];

    if (!InitializeConfigFileAndPath(cfgPath, MAX_PATH)) {
        exit(1); // Exiting as the path acquisition failed
    }

    LoadColorConfiguration(cfgPath, L"Colors", L"ReadyColor", ReadyColor);
    LoadColorConfiguration(cfgPath, L"Colors", L"ReactColor", ReactColor);
    LoadColorConfiguration(cfgPath, L"Colors", L"EarlyColor", EarlyColor);
    LoadColorConfiguration(cfgPath, L"Colors", L"ResultColor", ResultColor);

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

    LogEnable = GetPrivateProfileInt(L"Toggles", L"LogEnable", 0, cfgPath);

    LoadTrialConfiguration(cfgPath);
    LoadTextColorConfiguration(cfgPath);
    LoadFontConfiguration(cfgPath, fontName, MAX_PATH, &fontSize, fontStyle, MAX_PATH);

    AllocateMemoryForReactionTimes();
}

void InitializeLogFileName() {
    time_t t;
    struct tm* tmp;

    time(&t);
    struct tm localTime;
    localtime_s(&localTime, &t);
    tmp = &localTime;

    wchar_t timestamp[TIME_BUFFER_SIZE];
    wcsftime(timestamp, TIME_BUFFER_SIZE, L"%Y%m%d%H%M%S", tmp);  // Format YYYYMMDDHHMMSS

    swprintf_s(logFileName, MAX_PATH, L"log\\Log_%s.log", timestamp);
}

bool AppendToLog(double x, int y) {  // Handles log file operations
    wchar_t exePath[MAX_PATH];
    wchar_t logFilePath[MAX_PATH];
    wchar_t logDirPath[MAX_PATH];  // Added for the directory path

    // Get the directory where the executable is running
    if (!GetModuleFileName(NULL, exePath, MAX_PATH)) {
        HandleError(L"Failed to get module file name");
    }
    else {
        wchar_t* lastSlash = wcsrchr(exePath, '\\');
        if (lastSlash) {
            *(lastSlash + 1) = L'\0';
        }

        // Create full path for the log directory
        swprintf_s(logDirPath, MAX_PATH, L"%slog", exePath);

        // Ensure log directory exists
        if (!CreateDirectory(logDirPath, NULL) && GetLastError() != ERROR_ALREADY_EXISTS) {
            HandleError(L"Failed to create log directory");
        }
        else {
            // Create full path for the log file
            swprintf_s(logFilePath, MAX_PATH, L"%s%s", exePath, logFileName);

            // Append to the log file
            FILE* logFile;
            errno_t err = _wfopen_s(&logFile, logFilePath, L"a");
            if (err != 0 || !logFile) {
                wchar_t errorMessage[512];
                _wcserror_s(errorMessage, sizeof(errorMessage) / sizeof(wchar_t), err);
                HandleError(errorMessage);
            }
            else {
                fwprintf(logFile, L"Trial %d: %f\n", y, x);
                fclose(logFile);
                return true;
            }
        }
    }
    return false;
}


// Input handling functions
bool RegisterForRawInput(HWND hwnd, USHORT usage) {
    RAWINPUTDEVICE rid = {0};
    rid.usUsagePage = 0x01;
    rid.usUsage = usage;
    rid.dwFlags = 0;
    rid.hwndTarget = hwnd;

    if (!RegisterRawInputDevices(&rid, 1, sizeof(rid))) {
        HandleError(L"Failed to register raw input device");
    }
    return true;
}

void HandleInput(HWND hwnd) {   // Primary "game" logic is done here
    if (!isCursorOverClientArea) {
        return;  // Don't process the input if the cursor is not over the client area
    }
    if (currentState == STATE_REACT) {
        HandleReactClick(hwnd);
    }
    else if ((currentState == STATE_EARLY) || (currentState == STATE_RESULT)) {
        if ((currentState == STATE_RESULT) && currentAttempt == NumberOfTrials) {
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

void HandleGenericKeyboardInput(HWND hwnd) {
    for (int vk = 0; vk <= 255; vk++) { // loop through all possible VK values
        UpdateKeyState(vk, hwnd);
    }
}

void HandleRawKeyboardInput(RAWINPUT* raw, HWND hwnd) {
    int vkey = raw->data.keyboard.VKey;

    if (raw->data.keyboard.Flags == RI_KEY_MAKE && IsAlphanumeric(vkey) && !keyStates[vkey]) {
        HandleInput(hwnd);
        keyStates[vkey] = 1; // Latch the key state
    }
    else if (raw->data.keyboard.Flags == RI_KEY_BREAK) {
        keyStates[vkey] = 0; // Unlatch on key release
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

bool IsAlphanumeric(int vkey) {
    return (vkey >= '0' && vkey <= '9') || // '0' to '9'
        (vkey >= 'A' && vkey <= 'Z');   // 'A' to 'Z'
}

void UpdateKeyState(int vk, HWND hwnd) {
    if (IsAlphanumeric(vk)) {
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



// Main application logic functions
void ResetLogic(HWND hwnd) {
    // Kill all timers.
    KillTimer(hwnd, TIMER_READY);
    KillTimer(hwnd, TIMER_REACT);
    KillTimer(hwnd, TIMER_EARLY);

    currentState = STATE_READY;
   
    // Schedule the transition to green after a random delay.
    SetTimer(hwnd, TIMER_READY, GenerateRandomDelay(MinDelay,MaxDelay), NULL);

    // Force repaint.
    InvalidateRect(hwnd, NULL, TRUE);
}

void HandleReactClick(HWND hwnd) {
    QueryPerformanceCounter(&endTime);
    double timeTaken = ((double)(endTime.QuadPart - startTime.QuadPart) / freq.QuadPart) * 1000;

    reactionTimes[currentAttempt % NumberOfTrials] = timeTaken;
    currentAttempt++;

    currentState = STATE_RESULT;   // Change to results screen
    InvalidateRect(hwnd, NULL, TRUE);  // Force repaint.
}

void HandleEarlyClick(HWND hwnd) {
    currentState = STATE_EARLY;  // Early state
    SetTimer(hwnd, TIMER_EARLY, EarlyResetDelay, NULL); // Early state eventually resets back to Ready state
    InvalidateRect(hwnd, NULL, TRUE);
}

void ResetAll(HWND hwnd) { // This isn't used and I don't recall what the purpose would have been anyways...
    currentAttempt = 0;
    for (int i = 0; i < NumberOfTrials; i++) {
        reactionTimes[i] = 0;
    }
    ResetLogic(hwnd);
}