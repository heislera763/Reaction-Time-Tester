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
#define DEFAULT_RESOLUTION_WIDTH 1280
#define DEFAULT_RESOLUTION_HEIGHT 720
#define TIMER_READY 1
#define TIMER_REACT 2
#define TIMER_EARLY 3
#define TIMER_DEBOUNCE 4


// Configuration and Settings
COLORREF ready_color[3], react_color[3], early_color[3], result_color[3], early_font_color[3], results_font_color[3];
int min_delay, max_delay, number_of_trials, early_reset_delay, virtual_debounce, raw_keyboard_enabled, raw_mouse_enabled, raw_input_debug, trial_logging_enabled, debug_logging_enabled;
wchar_t trial_log_file_name[MAX_PATH]; // Log file name global for ease
wchar_t debug_log_file_name[MAX_PATH]; // Log file name global for ease
wchar_t font_name[MAX_PATH];
wchar_t font_style[MAX_PATH];
int font_size;
int resolution_width;
int resolution_height;

// Program State and Data
typedef enum { // Define states
    STATE_READY,
    STATE_REACT,
    STATE_EARLY,
    STATE_RESULT
} ProgramState;
ProgramState Current_State = STATE_READY; // Initial state

typedef struct { // Struct for determining whether an input is allowable
    bool Mouse;
    bool Keyboard;
} InputState;
InputState Input_Enabled = { true, true }; // Input allowed or not

int current_attempt = 0;
int total_trial_number = 0;
int key_states[256] = { 0 }; // 0: not pressed, 1: pressed
double* reaction_times = NULL; // Array to store the last 5 reaction times.
LARGE_INTEGER start_time, end_time, frequency; // For high-resolution timing
bool debounce_active = false; // This is a global indicator for whether or not program is halting inputs for Virtual Debounce feature

// UI and Rendering
HBRUSH hReadyBrush, hReactBrush, hEarlyBrush, hResultBrush; // Brushes for painting the window
HFONT hFont;

// Forward declarations for window procedure and other functions.
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

// Utility functions
void HandleError(const wchar_t* errorMessage);
void ValidateColors(COLORREF color[]);
void ValidateConfigSetting();
void RemoveComment(wchar_t* str);
int GenerateRandomDelay(int min, int max);
void BrushCleanup();

// Configuration and setup functions
bool InitializeConfigFileAndPath(wchar_t* cfgPath, size_t maxLength);
void LoadColorConfiguration(const wchar_t* cfgPath, const wchar_t* sectionName, const wchar_t* colorName, const COLORREF* targetColorArray);
void LoadFontConfiguration(const wchar_t* cfgPath, wchar_t* targetFontName, size_t maxLength, int* fontSize, wchar_t* fontStyle, size_t fontStyleLength);
void LoadTrialConfiguration(const wchar_t* cfgPath);
void LoadTextColorConfiguration(const wchar_t* cfgPath);
void AllocateMemoryForReactionTimes();
void LoadConfig();
void InitializeLogFileName(int x);
bool AppendToLog(double x, int y, wchar_t* log_file);

// Input handling functions
bool RegisterForRawInput(HWND hwnd, USHORT usage);
void HandleInput(HWND hwnd, bool x);
void HandleGenericKeyboardInput(HWND hwnd);
void HandleRawKeyboardInput(RAWINPUT* raw, HWND hwnd);
void HandleGenericMouseInput(HWND hwnd);
void HandleRawMouseInput(RAWINPUT* raw, HWND hwnd);
bool IsAlphanumeric(int vkey);
void UpdateKeyState(int vkey, HWND hwnd);

// Main application logic functions
void ResetLogic(HWND hwnd);
void HandleReactClick(HWND hwnd);
void HandleEarlyClick(HWND hwnd);
void ResetAll(HWND hwnd);


int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nCmdShow) {
    QueryPerformanceFrequency(&frequency);

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

    if (trial_logging_enabled == 1) InitializeLogFileName(0);
    if (debug_logging_enabled == 1) InitializeLogFileName(1);

    // Get the dimensions of the main display
    int screen_width = GetSystemMetrics(SM_CXSCREEN);
    int screen_height = GetSystemMetrics(SM_CYSCREEN);

    // Calculate the position to center the window
    int window_width = resolution_width; // Your window width
    int window_height = resolution_height; // Your window height
    int position_x = (screen_width - window_width) / 2;
    int position_y = (screen_height - window_height) / 2;

    // Create the main window centered on the main display
    HWND hwnd = CreateWindowEx(
        0,
        CLASS_NAME,
        L"reaction-time-tester",
        WS_OVERLAPPEDWINDOW,
        position_x, position_y, window_width, window_height,
        NULL,
        NULL,
        hInstance,
        NULL
    );

    // Raw input
    if (raw_keyboard_enabled == 1) RegisterForRawInput(hwnd, 0x02); // Attempt to Register Keyboard
    if (raw_mouse_enabled == 1) RegisterForRawInput(hwnd, 0x02); // Attempt to Register Mouse

    if (raw_input_debug == 1) {
        wchar_t message[256];
        swprintf(message, sizeof(message) / sizeof(wchar_t), L"RawKeyboardEnable: %d\nRawMouseEnable: %d\nRegisterForRawKeyboardInput: %s\nRegisterForRawMouseInput: %s", raw_keyboard_enabled, raw_mouse_enabled, RegisterForRawInput(hwnd, 0x06) ? L"True" : L"False", RegisterForRawInput(hwnd, 0x02) ? L"True" : L"False");
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
    Current_State = STATE_READY;

    // Seed the random number generator.
    srand((unsigned)time(NULL));

    // Schedule the transition to react after a random delay.
    SetTimer(hwnd, TIMER_READY, GenerateRandomDelay(min_delay, max_delay), NULL);

    int font_weight = FW_REGULAR;
    BOOL italics_enabled = FALSE;

    if (wcscmp(font_style, L"Bold") == 0) {
        font_weight = FW_BOLD;
    }
    else if (wcscmp(font_style, L"Italic") == 0) {
        italics_enabled = TRUE;
    }

    // Prepare font using the loaded configurations
    hFont = CreateFont(font_size, 0, 0, 0, font_weight, italics_enabled, FALSE, FALSE, ANSI_CHARSET,
        OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE, font_name);

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

    free(reaction_times);

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
        case HTCLIENT: // In this section we use InputAllowed to change state of whether or not mouse can input commands
            if (!debounce_active) { Input_Enabled.Mouse = true; }
            SetCursor(LoadCursor(NULL, IDC_HAND)); // Set the cursor to a hand cursor
            break;
        case HTLEFT:
        case HTRIGHT:
            Input_Enabled.Mouse = false;
            SetCursor(LoadCursor(NULL, IDC_SIZEWE)); // Left or right border (resize cursor)
            break;
        case HTTOP:
        case HTBOTTOM:
            Input_Enabled.Mouse = false;
            SetCursor(LoadCursor(NULL, IDC_SIZENS)); // Top or bottom border (resize cursor)
            break;
        case HTTOPLEFT:
        case HTBOTTOMRIGHT:
            Input_Enabled.Mouse = false;
            SetCursor(LoadCursor(NULL, IDC_SIZENWSE)); // Top-left or bottom-right corner (diagonal resize cursor)
            break;
        case HTTOPRIGHT:
        case HTBOTTOMLEFT:
            Input_Enabled.Mouse = false;
            SetCursor(LoadCursor(NULL, IDC_SIZENESW)); // Top-right or bottom-left corner (diagonal resize cursor)
            break;
        default:
            Input_Enabled.Mouse = false;
            SetCursor(LoadCursor(NULL, IDC_ARROW)); // Use the default cursor
            break;
        }
        return TRUE;


    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        // Decide which brush to use based on the current state.
        HBRUSH hBrush;
        switch (Current_State) {
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

        wchar_t buffer[100] = { 0 }; // Initialize buffer

        if (Current_State == STATE_RESULT) {
            SetTextColor(hdc, RGB(results_font_color[0], results_font_color[1], results_font_color[2]));
            total_trial_number++;

            if (current_attempt < number_of_trials) {
                swprintf_s(buffer, 100, L"Last: %.2lfms\nComplete %d trials for average.\nTrials so far: %d", reaction_times[(current_attempt - 1 + number_of_trials) % number_of_trials], number_of_trials, total_trial_number);
            }
            else {
                double total = 0;
                for (int i = 0; i < number_of_trials; i++) {
                    total += reaction_times[i];
                }
                double average = total / number_of_trials;
                swprintf_s(buffer, 100, L"Last: %.2lfms\nAverage (last %d): %.2lfms\nTrials so far: %d", reaction_times[(current_attempt - 1) % number_of_trials], number_of_trials, average, total_trial_number);
            }
            if (trial_logging_enabled == 1) AppendToLog(reaction_times[(current_attempt - 1 + number_of_trials) % number_of_trials], total_trial_number, trial_log_file_name);
            if (debug_logging_enabled == 1) AppendToLog(0, 0, debug_log_file_name);
        }
        else if (Current_State == STATE_EARLY) {
            SetTextColor(hdc, RGB(early_font_color[0], early_font_color[1], early_font_color[2]));
            swprintf_s(buffer, 100, L"Too early!\nTrials so far: %d", total_trial_number);
        }

        // Calculate rectangle for the text and display it.
        RECT text_rectangle;
        SetRectEmpty(&text_rectangle);
        DrawText(hdc, buffer, -1, &text_rectangle, DT_CALCRECT | DT_WORDBREAK);
        RECT centered_rectangle = rect;
        centered_rectangle.top += (rect.bottom - rect.top - (text_rectangle.bottom - text_rectangle.top)) / 2; // Text doesn't center vertically (Possibly due to newline not being accounted for?)
        DrawText(hdc, buffer, -1, &centered_rectangle, DT_CENTER | DT_WORDBREAK);

        EndPaint(hwnd, &ps);
    } break;


    case WM_TIMER:
        switch (wParam) {
        case TIMER_READY:
            Current_State = STATE_READY;
            KillTimer(hwnd, TIMER_READY);
            SetTimer(hwnd, TIMER_REACT, GenerateRandomDelay(min_delay, max_delay), NULL);
            break;

        case TIMER_REACT:
            if (Current_State == STATE_READY) {
                Current_State = STATE_REACT;
                QueryPerformanceCounter(&start_time); // Start the timer for reaction time.
                InvalidateRect(hwnd, NULL, TRUE); // Force repaint.
            }
            break;

        case TIMER_EARLY:
            ResetLogic(hwnd); // Reset the game after showing the "too early" state.
            break;

        case TIMER_DEBOUNCE:  // Debounce implementation is okay at this point, I 
            Input_Enabled.Mouse = true;
            Input_Enabled.Keyboard = true;
            debounce_active = false;
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

        if (raw->header.dwType == RIM_TYPEKEYBOARD && raw_keyboard_enabled == 1) { // Handle raw keyboard inputs
            HandleRawKeyboardInput(raw, hwnd);
        }
        else if (raw->header.dwType == RIM_TYPEMOUSE && raw_mouse_enabled == 1) { // Handle raw mouse inputs
            HandleRawMouseInput(raw, hwnd);
        }

        free(lpb);
    }
    break;

    // Handle generic keyboard input:
    case WM_KEYDOWN:
    case WM_KEYUP:
        if (raw_keyboard_enabled == 0) {
            HandleGenericKeyboardInput(hwnd);
        }
        break;

    // Handle generic mouse input:
    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP:
        if (raw_mouse_enabled == 0) {
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

void ValidateColors(COLORREF color[]) { // This is usable in general applications
    for (int i = 0; i < 3; i++) {
        if (color[i] < 0 || color[i] > 255) {
            HandleError(L"Invalid color values in reaction.cfg");
            return;
        }
    }
}

void ValidateConfigSetting() { // Dumping ground for validating settings. There is probably a general purpose-ish way to do this but I can't be bothered right now
    if (resolution_width <= 0 || resolution_height <= 0) {
        HandleError(L"Invalid resolution values in reaction.cfg");
    }
    if (min_delay <= 0 || max_delay <= 0 || max_delay < min_delay || virtual_debounce < 0 || early_reset_delay <= 0) {
        HandleError(L"Invalid delay values in reaction.cfg");
    }
    if ((raw_keyboard_enabled != 0 && raw_keyboard_enabled != 1) || (raw_mouse_enabled != 0 && raw_mouse_enabled != 1) || raw_input_debug != 0 && raw_input_debug != 1) {
        HandleError(L"Invalid raw input settings in reaction.cfg");
    }
    if ((trial_logging_enabled != 0 && trial_logging_enabled != 1)|| (debug_logging_enabled != 0 && debug_logging_enabled != 1)) {
        HandleError(L"Invalid raw input settings in reaction.cfg");
    }
}

void RemoveComment(wchar_t* str) { // Removes comments and trailing spaces from strings read from .cfg files
    wchar_t* semicolon_position = wcschr(str, L';'); // Locate the first occurrence of a semicolon, which denotes the start of a comment.
    if (semicolon_position) {
        *semicolon_position = L'\0';  // Terminate the string at the start of the comment.
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
    wchar_t exe_path[MAX_PATH];
    wchar_t default_cfg_path[MAX_PATH];

    if (!GetModuleFileName(NULL, exe_path, MAX_PATH)) {  // Failed to get current module's file path.
        HandleError(L"Failed to get module file name");
        return false;
    }

    wchar_t* last_slash = wcsrchr(exe_path, '\\');  // Find the last directory separator.
    if (last_slash) *(last_slash + 1) = L'\0';  // Null-terminate to get directory path.

    if (swprintf_s(cfgPath, maxLength, L"%s%s", exe_path, L"reaction.cfg") < 0 ||
        swprintf_s(default_cfg_path, MAX_PATH, L"%s%s", exe_path, L"default.cfg") < 0) {  // Format paths for config files.
        HandleError(L"Failed to create config paths");
        return false;
    }

    if (GetFileAttributes(cfgPath) == INVALID_FILE_ATTRIBUTES) {  // If reaction.cfg doesn't exist, copy from default.cfg.
        FILE* default_file;
        errno_t err1 = _wfopen_s(&default_file, default_cfg_path, L"rb");
        if (err1 != 0 || !default_file) {
            HandleError(L"Failed to open default.cfg");
            return false;
        }

        FILE* new_file;
        errno_t err2 = _wfopen_s(&new_file, cfgPath, L"wb");
        if (err2 != 0 || !new_file) {
            fclose(default_file);
            HandleError(L"Failed to create reaction.cfg");
            return false;
        }

        char buffer[1024];
        size_t bytes_read;
        while ((bytes_read = fread(buffer, 1, sizeof(buffer), default_file)) > 0) {  // Copy contents from default.cfg to reaction.cfg in chunks.
            if (fwrite(buffer, 1, bytes_read, new_file) < bytes_read) { // Has write operation written the correct number of bytes?
                fclose(new_file); fclose(default_file);
                HandleError(L"Failed to write to reaction.cfg");
                return false;
            }
        }
        fclose(new_file); fclose(default_file);
    }
    return true;
}

void LoadColorConfiguration(const wchar_t* cfgPath, const wchar_t* sectionName, const wchar_t* colorName, const COLORREF* targetColorArray) { // Load RGB from config file
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
    number_of_trials = GetPrivateProfileInt(L"Trial", L"NumberOfTrials", DEFAULT_NUM_TRIALS, cfgPath);
    if (number_of_trials <= 0) {
        HandleError(L"Invalid number of trials in reaction.cfg");
    }
}

void LoadTextColorConfiguration(const wchar_t* cfgPath) {
    wchar_t buffer[MAX_PATH] = L"";
    LoadColorConfiguration(cfgPath, L"Fonts", L"EarlyFontColor", early_font_color);
    LoadColorConfiguration(cfgPath, L"Fonts", L"ResultsFontColor", results_font_color);
}

void AllocateMemoryForReactionTimes() {
    size_t total_size = sizeof(double) * number_of_trials;
    reaction_times = (double*)malloc(total_size);
    if (reaction_times == NULL) {
        HandleError(L"Failed to allocate memory for reaction times!");
        return; // Return to prevent further execution
    }
    memset(reaction_times, 0, total_size);
}

void LoadConfig() {
    wchar_t cfg_path[MAX_PATH];

    if (!InitializeConfigFileAndPath(cfg_path, MAX_PATH)) {
        exit(1); // Exiting as the path acquisition failed
    }

    resolution_width = GetPrivateProfileInt(L"Resolution", L"ResolutionWidth", DEFAULT_RESOLUTION_WIDTH, cfg_path);
    resolution_height = GetPrivateProfileInt(L"Resolution", L"ResolutionHeight", DEFAULT_RESOLUTION_HEIGHT, cfg_path);

    LoadColorConfiguration(cfg_path, L"Colors", L"ReadyColor", ready_color);
    LoadColorConfiguration(cfg_path, L"Colors", L"ReactColor", react_color);
    LoadColorConfiguration(cfg_path, L"Colors", L"EarlyColor", early_color);
    LoadColorConfiguration(cfg_path, L"Colors", L"ResultColor", result_color);

    hReadyBrush = CreateSolidBrush(RGB(ready_color[0], ready_color[1], ready_color[2]));
    hReactBrush = CreateSolidBrush(RGB(react_color[0], react_color[1], react_color[2]));
    hEarlyBrush = CreateSolidBrush(RGB(early_color[0], early_color[1], early_color[2]));
    hResultBrush = CreateSolidBrush(RGB(result_color[0], result_color[1], result_color[2]));

    min_delay = GetPrivateProfileInt(L"Delays", L"MinDelay", DEFAULT_MIN_DELAY, cfg_path);
    max_delay = GetPrivateProfileInt(L"Delays", L"MaxDelay", DEFAULT_MAX_DELAY, cfg_path);
    early_reset_delay = GetPrivateProfileInt(L"Delays", L"EarlyResetDelay", DEFAULT_EARLY_RESET_DELAY, cfg_path);
    virtual_debounce = GetPrivateProfileInt(L"Delays", L"VirtualDebounce", DEFAULT_VIRTUAL_DEBOUNCE, cfg_path);

    raw_keyboard_enabled = GetPrivateProfileInt(L"Toggles", L"RawKeyboardEnabled", DEFAULT_RAWKEYBOARDENABLE, cfg_path);
    raw_mouse_enabled = GetPrivateProfileInt(L"Toggles", L"RawMouseEnabled", DEFAULT_RAWMOUSEENABLE, cfg_path);
    raw_input_debug = GetPrivateProfileInt(L"Toggles", L"RawInputDebug", 0, cfg_path);
    trial_logging_enabled = GetPrivateProfileInt(L"Toggles", L"TrialLoggingEnabled", 0, cfg_path);
    debug_logging_enabled = GetPrivateProfileInt(L"Toggles", L"DebugLoggingEnabled", 0, cfg_path);

    ValidateConfigSetting(); // Reminder: This is hardcoded to check for invalid settings, you must add those checks to the function directly (for now)

    LoadTrialConfiguration(cfg_path);
    LoadTextColorConfiguration(cfg_path);
    LoadFontConfiguration(cfg_path, font_name, MAX_PATH, &font_size, font_style, MAX_PATH);

    AllocateMemoryForReactionTimes();
}

void InitializeLogFileName(int x) { // Initialize a log file name, 0 = trial log, 1 = debug log (Might make this better later)
    time_t t;
    struct tm* tmp;
    
    time(&t);
    struct tm local_time;
    localtime_s(&local_time, &t);
    tmp = &local_time;

    wchar_t timestamp[20];
    int timestamp_length = sizeof(timestamp)/sizeof(wchar_t);

    if (x) {
        wcsftime(timestamp, timestamp_length, L"%Y%m%d%H%M%S", tmp);  // Format YYYYMMDDHHMMSS
        swprintf_s(debug_log_file_name, MAX_PATH, L"log\\DEBUG_Log_%s.log", timestamp);
    }else{
        wcsftime(timestamp, timestamp_length, L"%Y%m%d%H%M%S", tmp);  // Format YYYYMMDDHHMMSS
        swprintf_s(trial_log_file_name, MAX_PATH, L"log\\Log_%s.log", timestamp);
    }

}

bool AppendToLog(double x, int y, wchar_t* logfile) {  // Handles log file operations
    wchar_t exe_path[MAX_PATH];
    wchar_t log_file_path[MAX_PATH];
    wchar_t log_dir_path[MAX_PATH];  // Added for the directory path

    // Get the directory where the executable is running
    if (!GetModuleFileName(NULL, exe_path, MAX_PATH)) {
        HandleError(L"Failed to get module file name");
    }
    else {
        wchar_t* last_slash = wcsrchr(exe_path, '\\');
        if (last_slash) {
            *(last_slash + 1) = L'\0';
        }

        // Create full path for the log directory
        swprintf_s(log_dir_path, MAX_PATH, L"%slog", exe_path);

        // Ensure log directory exists
        if (!CreateDirectory(log_dir_path, NULL) && GetLastError() != ERROR_ALREADY_EXISTS) {
            HandleError(L"Failed to create log directory");
        }
        else {
            // Create full path for the log file
            swprintf_s(log_file_path, MAX_PATH, L"%s%s", exe_path, logfile);

            // Append to the log file
            FILE* log_file;
            errno_t err = _wfopen_s(&log_file, log_file_path, L"a");
            if (err != 0 || !log_file) {
                wchar_t error_message[512];
                _wcserror_s(error_message, sizeof(error_message) / sizeof(wchar_t), err);
                HandleError(error_message);
            } else 
            if (x == 0 && y == 0) {
                fwprintf(log_file, L"Nothing to log right now... (This is an unfinished dev tool)\n"); // Print debug info here
                fclose(log_file);
                return true;
                } else if (trial_logging_enabled) {
                    fwprintf(log_file, L"Trial %d: %f\n", y, x);
                    fclose(log_file);
                    return true;
                    {
                }
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

void HandleInput(HWND hwnd, bool x) {   // Primary "game" logic is done here. x variable to indicate the type of input device
    if ((!Input_Enabled.Mouse && x) || (!Input_Enabled.Keyboard && !x)) {
        return;  // Don't process the input if device is currently blocked
    }
    if (Current_State == STATE_REACT) {
        HandleReactClick(hwnd);
    }
    else if ((Current_State == STATE_EARLY) || (Current_State == STATE_RESULT)) {
        if ((Current_State == STATE_RESULT) && current_attempt == number_of_trials) {
            current_attempt = 0;
            for (int i = 0; i < number_of_trials; i++) {
                reaction_times[i] = 0;
            }
        }
        ResetLogic(hwnd);
    }
    else {
        HandleEarlyClick(hwnd);
    }
    if ((virtual_debounce > 0) && (Input_Enabled.Mouse || Input_Enabled.Keyboard)) { // Block inputs if debounce is enabled and either input devices is active
        Input_Enabled.Mouse = false;
        Input_Enabled.Keyboard = false;
        debounce_active = true;
        SetTimer(hwnd, TIMER_DEBOUNCE, virtual_debounce, NULL);
    }
}

void HandleGenericKeyboardInput(HWND hwnd) {
    for (int vkey = 0; vkey <= 255; vkey++) { // loop through all possible VK values
        UpdateKeyState(vkey, hwnd);
    }
}

void HandleRawKeyboardInput(RAWINPUT* raw, HWND hwnd) {
    int vkey = raw->data.keyboard.VKey;

    if (raw->data.keyboard.Flags == RI_KEY_MAKE && IsAlphanumeric(vkey) && !key_states[vkey]) {
        HandleInput(hwnd, 0);
        key_states[vkey] = 1; // Latch the key state
    }
    else if (raw->data.keyboard.Flags == RI_KEY_BREAK) {
        key_states[vkey] = 0; // Unlatch on key release
    }
}

void HandleGenericMouseInput(HWND hwnd) {
    static int was_button_pressed = 0; // 0: not pressed, 1: pressed
    int is_button_pressed = GetAsyncKeyState(VK_LBUTTON) & 0x8000;

    if (is_button_pressed && !was_button_pressed) { // Check for transition from up to down
        HandleInput(hwnd, 1);
        was_button_pressed = 1; // Latch the button state
    }
    else if (!is_button_pressed && was_button_pressed) { // Check for transition from down to up
        was_button_pressed = 0; // Unlatch immediately on button release
    }
}

void HandleRawMouseInput(RAWINPUT* raw, HWND hwnd) {
    static int was_button_pressed = 0; // 0: not pressed, 1: pressed

    if (raw->data.mouse.usButtonFlags & RI_MOUSE_LEFT_BUTTON_DOWN && !was_button_pressed) {
        HandleInput(hwnd, 1);
        was_button_pressed = 1; // Latch the button state
    }
    else if (raw->data.mouse.usButtonFlags & RI_MOUSE_LEFT_BUTTON_UP && was_button_pressed) {
        was_button_pressed = 0; // Unlatch immediately on button release
    }
}

bool IsAlphanumeric(int vkey) {
    return (vkey >= '0' && vkey <= '9') || // '0' to '9'
        (vkey >= 'A' && vkey <= 'Z');   // 'A' to 'Z'
}

void UpdateKeyState(int vkey, HWND hwnd) {
    if (IsAlphanumeric(vkey)) {
        bool is_key_pressed = GetAsyncKeyState(vkey) & 0x8000;
        if (is_key_pressed && !key_states[vkey]) {
            HandleInput(hwnd, 0);
            key_states[vkey] = 1; // Latch the key state
        }
        else if (!is_key_pressed && key_states[vkey]) {
            key_states[vkey] = 0; // Unlatch on key release
        }
    }
}


// Main application logic functions
void ResetLogic(HWND hwnd) {
    // Kill all timers.
    KillTimer(hwnd, TIMER_READY);
    KillTimer(hwnd, TIMER_REACT);
    KillTimer(hwnd, TIMER_EARLY);
    KillTimer(hwnd, TIMER_DEBOUNCE); // Not clear if this is needed right now

    Current_State = STATE_READY;
   
    // Schedule the transition to green after a random delay.
    SetTimer(hwnd, TIMER_READY, GenerateRandomDelay(min_delay,max_delay), NULL);

    // Force repaint.
    InvalidateRect(hwnd, NULL, TRUE);
}

void HandleReactClick(HWND hwnd) {
    QueryPerformanceCounter(&end_time);
    double time_taken = ((double)(end_time.QuadPart - start_time.QuadPart) / frequency.QuadPart) * 1000;

    reaction_times[current_attempt % number_of_trials] = time_taken;
    current_attempt++;

    Current_State = STATE_RESULT;   // Change to results screen
    InvalidateRect(hwnd, NULL, TRUE);  // Force repaint.
}

void HandleEarlyClick(HWND hwnd) {
    Current_State = STATE_EARLY;  // Early state
    SetTimer(hwnd, TIMER_EARLY, early_reset_delay, NULL); // Early state eventually resets back to Ready state
    InvalidateRect(hwnd, NULL, TRUE);
}

void ResetAll(HWND hwnd) { // This isn't used and I don't recall what the purpose would have been anyways...
    current_attempt = 0;
    for (int i = 0; i < number_of_trials; i++) {
        reaction_times[i] = 0;
    }
    ResetLogic(hwnd);
}