#define UNICODE
#define _UNICODE

#include <windows.h>
#include <shlwapi.h>
#include <stdlib.h>
#include <time.h>
#include <stdio.h>
#include <stdbool.h>
#include "main_definitions.h"

// Putting everything relevant to LoadConfig() in this struct:
/*
    COLORREF ready_color[3], react_color[3], early_color[3], result_color[3];
    wchar_t cfg_path[MAX_PATH];

    hReadyBrush
    hReactBrush
    hEarlyBrush
    hResultBrush

    min_delay = GetPrivateProfileIntW(L"Delays", L"MinDelay", DEFAULT_MIN_DELAY, cfg_path);
    max_delay = GetPrivateProfileIntW(L"Delays", L"MaxDelay", DEFAULT_MAX_DELAY, cfg_path);
    early_reset_delay = GetPrivateProfileIntW(L"Delays", L"EarlyResetDelay", DEFAULT_EARLY_RESET_DELAY, cfg_path);
    input_status.virtual_debounce = GetPrivateProfileIntW(L"Delays", L"VirtualDebounce", DEFAULT_VIRTUAL_DEBOUNCE, cfg_path);

    raw_keyboard_enabled = GetPrivateProfileIntW(L"Toggles", L"RawKeyboardEnabled", DEFAULT_RAWKEYBOARDENABLE, cfg_path);
    raw_mouse_enabled = GetPrivateProfileIntW(L"Toggles", L"RawMouseEnabled", DEFAULT_RAWMOUSEENABLE, cfg_path);
    raw_input_debug_enabled = GetPrivateProfileIntW(L"Toggles", L"RawInputDebug", 0, cfg_path);
    trial_logging_enabled = GetPrivateProfileIntW(L"Toggles", L"TrialLoggingEnabled", 0, cfg_path);
    debug_logging_enabled = GetPrivateProfileIntW(L"Toggles", L"DebugLoggingEnabled", 0, cfg_path);

    ValidateConfigSetting(); // Reminder: This is hardcoded to check for invalid settings, you must add those checks to this function directly

    LoadTrialConfiguration(cfg_path);
    LoadTextColorConfiguration(cfg_path);
    LoadFontConfiguration(cfg_path, font_name, MAX_PATH, &font_size, font_style, MAX_PATH);
*/





// Configuration and Settings
COLORREF early_font_color[3], results_font_color[3];

int averaging_trials, total_trials;
bool raw_keyboard_enabled, raw_mouse_enabled, raw_input_debug_enabled, trial_logging_enabled, debug_logging_enabled;

wchar_t font_name[MAX_PATH], font_style[MAX_PATH];
unsigned int font_size, resolution_width, resolution_height, min_delay, max_delay, early_reset_delay;


double reaction_times[256] = {0}; // Hardcoded size for now, will be user configurable later

// Program State and Data
enum {
    STATE_READY,
    STATE_REACT,
    STATE_EARLY,
    STATE_RESULT
} Program_State = STATE_READY;

struct { // Stores the active/inactive state of input devices
    bool mouse, keyboard, virtual_debounce;
} input_status = {true, true, DEFAULT_VIRTUAL_DEBOUNCE};

struct {
    wchar_t trial_log[MAX_PATH];
    wchar_t debug_log[MAX_PATH];
} log_paths;

int current_attempt = 0;
int trial_iteration = 0;
int key_states[256] = {0};
LARGE_INTEGER start_time, end_time, frequency; // For high-resolution timing
bool debounce_active = false; // Global indicator for whether or not program is halting inputs for virtual debounce feature

// UI and Rendering
HBRUSH hReadyBrush, hReactBrush, hEarlyBrush, hResultBrush;
HFONT hFont;

int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nCmdShow) {
    // Voiding unused parameters
    (void)hPrevInstance;
    (void)lpCmdLine;

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
    LoadConfig();

    if (trial_logging_enabled == true) InitializeLogFileName(0);
    if (debug_logging_enabled == true) InitializeLogFileName(1);

    // Get the dimensions of the main display, then calculate the position to center the window
    int screen_width = GetSystemMetrics(SM_CXSCREEN);
    int screen_height = GetSystemMetrics(SM_CYSCREEN);
    int window_width = resolution_width;
    int window_height = resolution_height;
    int position_x = (screen_width - window_width) / 2;
    int position_y = (screen_height - window_height) / 2;

    // Create main window centered on the main display
    HWND hwnd = CreateWindowExW(0, CLASS_NAME, L"Reaction Time Tester", WS_OVERLAPPEDWINDOW, position_x, position_y, window_width, window_height, NULL, NULL, hInstance, NULL);

    // Raw input
    if (raw_keyboard_enabled == true) RegisterForRawInput(hwnd, 0x06);
    if (raw_mouse_enabled == true) RegisterForRawInput(hwnd, 0x02);
    if (raw_input_debug_enabled == true) {
        wchar_t message[256];
        swprintf(message, sizeof(message) / sizeof(wchar_t), L"RawKeyboardEnable: %d\nRawMouseEnable: %d\nRegisterForRawKeyboardInput: %s\nRegisterForRawMouseInput: %s", 
            raw_keyboard_enabled, raw_mouse_enabled, RegisterForRawInput(hwnd, 0x06) ? L"True" : L"False", RegisterForRawInput(hwnd, 0x02) ? L"True" : L"False");
        MessageBoxW(NULL, message, L"Raw Input Variables", MB_OK);
    }

    // Display the window.
    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    if (hwnd == NULL) {
        MessageBoxW(NULL, L"Failed to create window", L"Error", MB_OK);
        BrushCleanup();
        return 0;
    }

    // Switch to ready state
    Program_State = STATE_READY;

    // Seed RNG and set initial timer
    srand((unsigned)time(NULL));
    SetTimer(hwnd, TIMER_READY, GenerateRandomDelay(min_delay, max_delay), NULL);

    // Prepare font
    int font_weight = FW_REGULAR;
    BOOL italics_enabled = FALSE;
    if (wcscmp(font_style, L"Bold") == 0) {
        font_weight = FW_BOLD;
    }
    else if (wcscmp(font_style, L"Italic") == 0) {
        italics_enabled = TRUE;
    }
    hFont = CreateFontW(font_size, 0, 0, 0, font_weight, italics_enabled, FALSE, FALSE, ANSI_CHARSET,
        OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE, font_name);
    if (!hFont) {
        HandleError(L"Failed to create font.");
    }

    // Enter Windows message loop.
    MSG msg = {0};
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    BrushCleanup();

    return 0;
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_CREATE:
    {
        LoadAndSetIcon(hwnd);
    }
    break;

    case WM_SETCURSOR:
        switch (LOWORD(lParam)) {
        case HTCLIENT: // In this section we use InputAllowed to change state of whether or not mouse can input commands
            if (!debounce_active) { input_status.mouse = true; }
            SetCursor(LoadCursor(NULL, IDC_HAND)); // Switch to a hand cursor
            break;
        case HTLEFT:
        case HTRIGHT:
            input_status.mouse = false;
            SetCursor(LoadCursor(NULL, IDC_SIZEWE)); // Left or right border (resize cursor)
            break;
        case HTTOP:
        case HTBOTTOM:
            input_status.mouse = false;
            SetCursor(LoadCursor(NULL, IDC_SIZENS)); // Top or bottom border (resize cursor)
            break;
        case HTTOPLEFT:
        case HTBOTTOMRIGHT:
            input_status.mouse = false;
            SetCursor(LoadCursor(NULL, IDC_SIZENWSE)); // Top-left or bottom-right corner (diagonal resize cursor)
            break;
        case HTTOPRIGHT:
        case HTBOTTOMLEFT:
            input_status.mouse = false;
            SetCursor(LoadCursor(NULL, IDC_SIZENESW)); // Top-right or bottom-left corner (diagonal resize cursor)
            break;
        default:
            input_status.mouse = false;
            SetCursor(LoadCursor(NULL, IDC_ARROW)); // Use the default cursor
            break;
        }
        return TRUE;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        HBRUSH hBrush;
        switch (Program_State) {
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
            hBrush = hReadyBrush; // Default to a known brush to avoid undefined behavior
            HandleError(L"Invalid or undefined program state!");
            break;
        }

        // Paint the entire window
        RECT rect;
        GetClientRect(hwnd, &rect);
        FillRect(hdc, &rect, hBrush);

        // Display text
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(255, 255, 255));
        SelectObject(hdc, hFont);

        wchar_t buffer[100] = { 0 }; // Buffer for any/all text

        if (Program_State == STATE_RESULT) {
            SetTextColor(hdc, RGB(results_font_color[0], results_font_color[1], results_font_color[2]));
            trial_iteration++;

            if (current_attempt < averaging_trials) {
                swprintf_s(buffer, 100, L"Last: %.2lfms\nComplete %d trials for average.\nTrials so far: %d",
                    reaction_times[(current_attempt - 1 + averaging_trials) % averaging_trials], averaging_trials, trial_iteration);
            }
            else {
                double total = 0;
                for (int i = 0; i < averaging_trials; i++) {
                    total += reaction_times[i];
                }
                double average = total / averaging_trials;
                swprintf_s(buffer, 100, L"Last: %.2lfms\nAverage (last %d): %.2lfms\nTrials so far: %d",
                    reaction_times[(current_attempt - 1) % averaging_trials], averaging_trials, average, trial_iteration);
            }
            if (trial_logging_enabled == 1){
                AppendToLog(reaction_times[(current_attempt - 1 + averaging_trials) % averaging_trials], trial_iteration, log_paths.trial_log, NULL);
            }
        }
        else if (Program_State == STATE_EARLY) {
            SetTextColor(hdc, RGB(early_font_color[0], early_font_color[1], early_font_color[2]));
            swprintf_s(buffer, 100, L"Too early!\nTrials so far: %d", trial_iteration);
        }

        // Calculate rectangle for the text and display it.
        RECT text_rectangle;
        SetRectEmpty(&text_rectangle);
        DrawTextW(hdc, buffer, -1, &text_rectangle, DT_CALCRECT | DT_WORDBREAK);
        RECT centered_rectangle = rect;
        centered_rectangle.top += (rect.bottom - rect.top - (text_rectangle.bottom - text_rectangle.top)) / 2; // Text doesn't center vertically (Newline not being accounted for?)
        DrawTextW(hdc, buffer, -1, &centered_rectangle, DT_CENTER | DT_WORDBREAK);

        EndPaint(hwnd, &ps);
    } break;

    case WM_TIMER: // Thank you windows for basically forcing a nested switch here
        switch (wParam) {
        case TIMER_READY:
            Program_State = STATE_READY;
            KillTimer(hwnd, TIMER_READY);
            SetTimer(hwnd, TIMER_REACT, GenerateRandomDelay(min_delay, max_delay), NULL);
            break;

        case TIMER_REACT:
            if (Program_State == STATE_READY) {
                Program_State = STATE_REACT;
                QueryPerformanceCounter(&start_time); // Start reaction timer
                InvalidateRect(hwnd, NULL, TRUE); // Force repaint
            }
            break;

        case TIMER_EARLY:
            ResetLogic(hwnd); // Reset the game after showing the "too early" screen
            break;

        case TIMER_DEBOUNCE:  // Debounce reset
            input_status.mouse = true;
            input_status.keyboard = true;
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
            return 0;
        }

        RAWINPUT* raw = (RAWINPUT*)lpb;

        if (raw->header.dwType == RIM_TYPEKEYBOARD && raw_keyboard_enabled == true) {
            HandleRawKeyboardInput(raw, hwnd);
        }
        else if (raw->header.dwType == RIM_TYPEMOUSE && raw_mouse_enabled == true) {
            HandleRawMouseInput(raw, hwnd);
        }

        free(lpb);
    }
    break;

    // Handle generic keyboard input
    case WM_KEYDOWN:
    case WM_KEYUP:
        if (raw_keyboard_enabled == false) {
            HandleGenericKeyboardInput(hwnd);
        }
        break;

    // Handle generic mouse input
    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP:
        if (raw_mouse_enabled == false) {
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


// Utility Functions
void HandleError(const wchar_t* error_message) { // Generic error handler
    MessageBoxW(NULL, error_message, L"Error", MB_OK);
    if (debug_logging_enabled == 1){
        AppendToLog(0, 0, log_paths.debug_log, error_message);
    }
    exit(1);
}

void ValidateColors(const COLORREF color[]) {
    for (int i = 0; i < 3; i++) {
        if (color[i] > 255) {
            HandleError(L"Invalid color values in user.cfg");
        }
    }
}

void ValidateConfigSetting() { // Dumping ground for validating settings
    if (max_delay < min_delay) {
        HandleError(L"MaxDelay cannot be less than MinDelay in user.cfg");
    }
}

void RemoveComment(wchar_t* str) { // Removes comments and trailing spaces from strings read from .cfg files
    wchar_t* semicolon_position = wcschr(str, L';');
    if (semicolon_position) {
        *semicolon_position = L'\0';
    }

    size_t len = wcslen(str);
    while (len > 0 && iswspace(str[len - 1])) { // Loop to remove any trailing whitespace after excluding the comment
        str[len - 1] = L'\0';
        len--;
    }
}

int GenerateRandomDelay(int min, int max) { // RNG stuff
    int range = max - min + 1;
    int buckets = RAND_MAX / range;
    int limit = buckets * range;

    int r;
    do {
        r = rand();
    } while (r >= limit);

    return min + (r / buckets);
}

void BrushCleanup() { // Hardcoded to delete only these brushes, should probably replace with a more modular approach. Also, are these conditionals needed?
    if (hReadyBrush) DeleteObject(hReadyBrush);
    if (hReactBrush) DeleteObject(hReactBrush);
    if (hEarlyBrush) DeleteObject(hEarlyBrush);
    if (hResultBrush) DeleteObject(hResultBrush);
}


// Configuration and setup functions
bool InitializeConfigFileAndPath(wchar_t* cfgPath, size_t maxLength) { // Initializes paths and attempts to copy default.cfg to user.cfg
    wchar_t exe_path[MAX_PATH];
    wchar_t default_cfg_path[MAX_PATH];

    if (!GetModuleFileNameW(NULL, exe_path, MAX_PATH)) {
        HandleError(L"Failed to get module file name");
    }

    wchar_t* last_slash = wcsrchr(exe_path, '\\');  // Find the last directory separator
    if (last_slash) *(last_slash + 1) = L'\0';  // Null-terminate to get directory path

    if (swprintf_s(cfgPath, maxLength, L"%s/config/%s", exe_path, L"user.cfg") < 0 ||
        swprintf_s(default_cfg_path, MAX_PATH, L"%s/config/%s", exe_path, L"default.cfg") < 0) {
        HandleError(L"Failed to create config paths");
    }

    if (GetFileAttributesW(cfgPath) == INVALID_FILE_ATTRIBUTES) {  // If user.cfg doesn't exist, copy from default.cfg
        FILE* default_file;
        errno_t err1 = _wfopen_s(&default_file, default_cfg_path, L"rb");
        if (err1 != 0 || !default_file) {
            HandleError(L"Failed to open default.cfg");
        }

        FILE* new_file;
        errno_t err2 = _wfopen_s(&new_file, cfgPath, L"wb");
        if (err2 != 0 || !new_file) {
            fclose(default_file);
            HandleError(L"Failed to create user.cfg");
        }

        char buffer[1024];
        size_t bytes_read;
        while ((bytes_read = fread(buffer, 1, sizeof(buffer), default_file)) > 0) {  // Copy contents from default.cfg to user.cfg in chunks
            if (fwrite(buffer, 1, bytes_read, new_file) < bytes_read) { // Has write operation written the correct number of bytes?
                fclose(new_file); fclose(default_file);
                HandleError(L"Failed to write to user.cfg");
            }
        }
        fclose(new_file); fclose(default_file);
    }
    return true;
}

void LoadColorConfiguration(const wchar_t* cfgPath, const wchar_t* sectionName, const wchar_t* colorName, const COLORREF* targetColorArray) { // Load RGB
    wchar_t buffer[255];
    GetPrivateProfileStringW(sectionName, colorName, L"", buffer, sizeof(buffer) / sizeof(wchar_t), cfgPath);  // Retrieve color configuration as comma-separated RGB values

    if (wcslen(buffer) == 0) {
        HandleError(L"Failed to read color configuration");
    }

    swscanf_s(buffer, L"%d,%d,%d", &targetColorArray[0], &targetColorArray[1], &targetColorArray[2]);  // Extract individual RGB values
    ValidateColors(targetColorArray);
}

void LoadFontConfiguration(const wchar_t* cfgPath, wchar_t* targetFontName, size_t maxLength, unsigned int* fontSize, wchar_t* fontStyle, size_t fontStyleLength) {
    GetPrivateProfileStringW(L"Fonts", L"FontName", DEFAULT_FONT_NAME, targetFontName, (DWORD)maxLength, cfgPath);  // Retrieve font name
    RemoveComment(targetFontName);
    if (wcslen(targetFontName) == 0) {
        HandleError(L"Failed to read font configuration");
    }

    *fontSize = GetPrivateProfileIntW(L"Fonts", L"FontSize", DEFAULT_FONT_SIZE, cfgPath);

    GetPrivateProfileStringW(L"Fonts", L"FontStyle", DEFAULT_FONT_STYLE, fontStyle, (DWORD)fontStyleLength, cfgPath);
    RemoveComment(fontStyle);
}

void LoadTrialConfiguration(const wchar_t* cfgPath) {
    averaging_trials = GetPrivateProfileIntW(L"Trial", L"AveragingTrials", DEFAULT_AVG_TRIALS, cfgPath);
    if (averaging_trials <= 0) {
        HandleError(L"Invalid number of averaging trials in user.cfg");
    }
    total_trials = GetPrivateProfileIntW(L"Trial", L"TotalTrials", DEFAULT_TOTAL_TRIALS, cfgPath);
    if (total_trials <= 0) {
        HandleError(L"Invalid number of total trials in user.cfg");
    }
}

void LoadTextColorConfiguration(const wchar_t* cfgPath) {
    LoadColorConfiguration(cfgPath, L"Fonts", L"EarlyFontColor", early_font_color);
    LoadColorConfiguration(cfgPath, L"Fonts", L"ResultsFontColor", results_font_color);
}

void LoadConfig() {
    COLORREF ready_color[3], react_color[3], early_color[3], result_color[3];
    wchar_t cfg_path[MAX_PATH];

    if (!InitializeConfigFileAndPath(cfg_path, MAX_PATH)) {
        exit(1);
    }

    resolution_width = GetPrivateProfileIntW(L"Resolution", L"ResolutionWidth", DEFAULT_RESOLUTION_WIDTH, cfg_path);
    resolution_height = GetPrivateProfileIntW(L"Resolution", L"ResolutionHeight", DEFAULT_RESOLUTION_HEIGHT, cfg_path);

    LoadColorConfiguration(cfg_path, L"Colors", L"ReadyColor", ready_color);
    LoadColorConfiguration(cfg_path, L"Colors", L"ReactColor", react_color);
    LoadColorConfiguration(cfg_path, L"Colors", L"EarlyColor", early_color);
    LoadColorConfiguration(cfg_path, L"Colors", L"ResultColor", result_color);

    hReadyBrush = CreateSolidBrush(RGB(ready_color[0], ready_color[1], ready_color[2]));
    hReactBrush = CreateSolidBrush(RGB(react_color[0], react_color[1], react_color[2]));
    hEarlyBrush = CreateSolidBrush(RGB(early_color[0], early_color[1], early_color[2]));
    hResultBrush = CreateSolidBrush(RGB(result_color[0], result_color[1], result_color[2]));

    min_delay = GetPrivateProfileIntW(L"Delays", L"MinDelay", DEFAULT_MIN_DELAY, cfg_path);
    max_delay = GetPrivateProfileIntW(L"Delays", L"MaxDelay", DEFAULT_MAX_DELAY, cfg_path);
    early_reset_delay = GetPrivateProfileIntW(L"Delays", L"EarlyResetDelay", DEFAULT_EARLY_RESET_DELAY, cfg_path);
    input_status.virtual_debounce = GetPrivateProfileIntW(L"Delays", L"VirtualDebounce", DEFAULT_VIRTUAL_DEBOUNCE, cfg_path);

    raw_keyboard_enabled = GetPrivateProfileIntW(L"Toggles", L"RawKeyboardEnabled", DEFAULT_RAWKEYBOARDENABLE, cfg_path);
    raw_mouse_enabled = GetPrivateProfileIntW(L"Toggles", L"RawMouseEnabled", DEFAULT_RAWMOUSEENABLE, cfg_path);
    raw_input_debug_enabled = GetPrivateProfileIntW(L"Toggles", L"RawInputDebug", 0, cfg_path);
    trial_logging_enabled = GetPrivateProfileIntW(L"Toggles", L"TrialLoggingEnabled", 0, cfg_path);
    debug_logging_enabled = GetPrivateProfileIntW(L"Toggles", L"DebugLoggingEnabled", 0, cfg_path);

    ValidateConfigSetting(); // Reminder: This is hardcoded to check for invalid settings, you must add those checks to this function directly

    LoadTrialConfiguration(cfg_path);
    LoadTextColorConfiguration(cfg_path);
    LoadFontConfiguration(cfg_path, font_name, MAX_PATH, &font_size, font_style, MAX_PATH);

}

void InitializeLogFileName(int log_type) { // log_type = 0 = trial log, log_type = 1 = debug log
    time_t t;
    struct tm* tmp;
    
    time(&t);
    struct tm local_time;
    localtime_s(&local_time, &t);
    tmp = &local_time;

    wchar_t timestamp[20];
    int timestamp_length = sizeof(timestamp)/sizeof(wchar_t);

    if (log_type) {
        wcsftime(timestamp, timestamp_length, L"%Y%m%d%H%M%S", tmp);  // Format YYYYMMDDHHMMSS
        swprintf_s(log_paths.debug_log, MAX_PATH, L"log\\DEBUG_Log_%s.log", timestamp);
    } else {
        wcsftime(timestamp, timestamp_length, L"%Y%m%d%H%M%S", tmp);  // Format YYYYMMDDHHMMSS
        swprintf_s(log_paths.trial_log, MAX_PATH, L"log\\Log_%s.log", timestamp);
    }

}

bool AppendToLog(double value, int iteration, wchar_t* logfile, const wchar_t* external_error_message) {  // Handles log file operations
    wchar_t exe_path[MAX_PATH];
    wchar_t log_file_path[MAX_PATH];
    wchar_t log_dir_path[MAX_PATH];

    // Get the directory where the executable is running
    if (!GetModuleFileName(NULL, exe_path, MAX_PATH)) {
        HandleError(L"Failed to get module file name");
    } else {
        wchar_t* last_slash = wcsrchr(exe_path, '\\');
        if (last_slash) {
            *(last_slash + 1) = L'\0';
        }

        // Create full path for the log directory
        swprintf_s(log_dir_path, MAX_PATH, L"%slog", exe_path);

        // Ensure log directory exists
        if (!CreateDirectory(log_dir_path, NULL) && GetLastError() != ERROR_ALREADY_EXISTS) {
            HandleError(L"Failed to create log directory");
        } else {
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
            if (value == 0 && iteration == 0) { // Hypothetically value == 0 && iteration == 0 shouldn't be possible unless the values are forced
                fwprintf(log_file, L"ERROR: %s\n", external_error_message); // Note: This only logs errors after we have already loaded the config
                fclose(log_file);
                return true;
            } else if (trial_logging_enabled) {
                fwprintf(log_file, L"Trial %d: %f\n", iteration, value);
                fclose(log_file);
                return true;
                    {
                }
            }
        }
    }
    return false;
}

void LoadAndSetIcon(HWND hwnd) {
    TCHAR szIconPath[MAX_PATH];

    // Get the current module's directory
    GetModuleFileName(NULL, szIconPath, MAX_PATH);
    PathRemoveFileSpec(szIconPath);

    // Append the relative path to the icon
    PathAppend(szIconPath, TEXT("\\resources\\icon.ico"));

    // Load the small icon from the file
    HICON hIcon = (HICON)LoadImage(NULL, szIconPath, IMAGE_ICON, 32, 32, LR_LOADFROMFILE);
    if (hIcon)
    {
        // Set the icon for the window
        SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
    }

    // Load the large icon for the Alt-Tab dialog
    HICON hIconLarge = (HICON)LoadImage(NULL, szIconPath, IMAGE_ICON, 64, 64, LR_LOADFROMFILE);
    if (hIconLarge)
    {
        SendMessage(hwnd, WM_SETICON, ICON_BIG, (LPARAM)hIconLarge);
    }
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

void HandleInput(HWND hwnd, bool is_mouse_input) {   // Primary input logic is done here
    if ((!input_status.mouse && is_mouse_input) || (!input_status.keyboard && !is_mouse_input)) {
        return;  // Ignore input if device is currently blocked
    }
    if (Program_State == STATE_REACT) {
        HandleReactClick(hwnd);
    }
    else if ((Program_State == STATE_EARLY) || (Program_State == STATE_RESULT)) {
        if ((Program_State == STATE_RESULT) && current_attempt == averaging_trials) { // This seems very odd, really not sure why it was done this way
            current_attempt = 0;
            for (int i = 0; i < averaging_trials; i++) {
                reaction_times[i] = 0;
            }
        }
        ResetLogic(hwnd);
    }
    else {
        HandleEarlyClick(hwnd);
    }
    if ((input_status.virtual_debounce) && (input_status.mouse || input_status.keyboard)) { // Block inputs if debounce is enabled and either input devices is active
        input_status.mouse = false;
        input_status.keyboard = false;
        debounce_active = true;
        SetTimer(hwnd, TIMER_DEBOUNCE, input_status.virtual_debounce, NULL);
    }
}

void HandleGenericKeyboardInput(HWND hwnd) {
    for (int vkey = 0; vkey <= 255; vkey++) {
        UpdateKeyState(vkey, hwnd);
    }
}

void HandleRawKeyboardInput(RAWINPUT* raw, HWND hwnd) {
    int vkey = raw->data.keyboard.VKey;

    if (raw->data.keyboard.Flags == RI_KEY_MAKE && IsAlphanumeric(vkey) && !key_states[vkey]) {
        HandleInput(hwnd, false);
        key_states[vkey] = true;
    }
    else if (raw->data.keyboard.Flags == RI_KEY_BREAK) {
        key_states[vkey] = false;
    }
}

void HandleGenericMouseInput(HWND hwnd) {
    static bool was_button_pressed = false;
    bool is_button_pressed = GetAsyncKeyState(VK_LBUTTON) & 0x8000;

    if (is_button_pressed && !was_button_pressed) {
        HandleInput(hwnd, true);
        was_button_pressed = true;
    }
    else if (!is_button_pressed && was_button_pressed) {
        was_button_pressed = false;
    }
}

void HandleRawMouseInput(RAWINPUT* raw, HWND hwnd) {
    static bool was_button_pressed = false;

    if (raw->data.mouse.usButtonFlags & RI_MOUSE_LEFT_BUTTON_DOWN && !was_button_pressed) {
        HandleInput(hwnd, true);
        was_button_pressed = true;
    }
    else if (raw->data.mouse.usButtonFlags & RI_MOUSE_LEFT_BUTTON_UP && was_button_pressed) {
        was_button_pressed = false;
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
            HandleInput(hwnd, false);
            key_states[vkey] = 1;
        }
        else if (!is_key_pressed && key_states[vkey]) {
            key_states[vkey] = 0;
        }
    }
}


// Main application logic functions
void ResetLogic(HWND hwnd) {
    KillTimer(hwnd, TIMER_READY);
    KillTimer(hwnd, TIMER_REACT);
    KillTimer(hwnd, TIMER_EARLY);

    Program_State = STATE_READY;
   
    SetTimer(hwnd, TIMER_READY, GenerateRandomDelay(min_delay,max_delay), NULL);
    InvalidateRect(hwnd, NULL, TRUE);
}

void HandleReactClick(HWND hwnd) {
    QueryPerformanceCounter(&end_time);
    double time_taken = ((double)(end_time.QuadPart - start_time.QuadPart) / frequency.QuadPart) * 1000;

    reaction_times[current_attempt % averaging_trials] = time_taken;
    current_attempt++;

    Program_State = STATE_RESULT;
    InvalidateRect(hwnd, NULL, TRUE);
}

void HandleEarlyClick(HWND hwnd) {
    Program_State = STATE_EARLY;
    SetTimer(hwnd, TIMER_EARLY, early_reset_delay, NULL); // Early state eventually resets back to Ready state regardless of user input
    InvalidateRect(hwnd, NULL, TRUE);
}

void ResetAll(HWND hwnd) { // This isn't used but I guess it could be used to manually reset attempts, might break logging though
    current_attempt = 0;
    for (int i = 0; i < averaging_trials; i++) {
        reaction_times[i] = 0;
    }
    ResetLogic(hwnd);
}