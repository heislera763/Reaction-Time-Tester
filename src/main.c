#define UNICODE
#define _UNICODE
#include <windows.h>
#include <shlwapi.h>
#include <stdlib.h>
#include <time.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <uchar.h>
#include "main_definitions.h"

// Configuration
typedef struct {
    // Appearance
    COLORREF early_font[3];
    COLORREF results_font[3];
    wchar_t font_name[MAX_PATH];
    wchar_t font_style[MAX_PATH];
    int font_size;
    int resolution_width;
    int resolution_height;

    // Toggles
    bool raw_keyboard;
    bool raw_mouse;
    bool raw_input_debug;
    bool trial_logging;
    bool debug_logging;

    // Game Options
    int averaging_trials;
    int total_trials;
    int min_delay;
    int max_delay;
    int early_reset_delay;
    int virtual_debounce;
} Configuration;

// Program State and Data
typedef struct { // ##REVIEW## Should I split this up a bit? Have a ProgramState and ProgramData?
    // Game State
    enum {
        STATE_INITIAL,
        STATE_READY,
        STATE_REACT,
        STATE_EARLY,
        STATE_RESULT
    } game_state;
    int current_attempt;
    int trial_iteration;

    // Input State
    bool mouse_active;
    bool debounce_active;
    int key_states[256];
} ProgramState;

typedef struct {
    // Logging and Data
    double reaction_time_value;
    double reaction_time_array[1024]; // ##REVIEW## Hardcoded size for now. Make user configurable later? Maybe rolling array would be a better
    wchar_t trial_log_path[MAX_PATH];
    wchar_t debug_log_path[MAX_PATH];

    // Timing
    LARGE_INTEGER start_time;
    LARGE_INTEGER end_time;
    LARGE_INTEGER frequency;
} ProgramData;


// UI and Rendering
typedef struct {
    HBRUSH ready_brush;
    HBRUSH react_brush;
    HBRUSH early_brush;
    HBRUSH result_brush;
    HFONT font;
    int font_weight;
    BOOL italics_enabled;
} UI;

// Declare global structs
Configuration config = {.virtual_debounce = DEFAULT_VIRTUAL_DEBOUNCE};
ProgramState state = {.game_state = STATE_INITIAL};
ProgramData data = {.reaction_time_array = {0}};
UI ui;

int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nCmdShow) {
    
    // Voiding unused parameters
    (void)hPrevInstance;
    (void)lpCmdLine;

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

    LoadConfig();

    // Get the dimensions of the main display, then calculate the position to center the window
    int position_x = (GetSystemMetrics(SM_CXSCREEN) - config.resolution_width) / 2;
    int position_y = (GetSystemMetrics(SM_CYSCREEN) - config.resolution_height) / 2;

    // Create main window centered on the main display
    HWND hwnd = CreateWindowExW(0, CLASS_NAME, L"Reaction Time Tester", WS_OVERLAPPEDWINDOW, position_x, position_y, config.resolution_width, config.resolution_height, NULL, NULL, hInstance, NULL);

    InitializeSettings(&hwnd);

    // Display the window.
    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    if (hwnd == NULL) {
        MessageBoxW(NULL, L"Failed to create window", L"Error", MB_OK);
        return 0;
    }

    // Seed RNG and set initial timer
    srand((unsigned)time(NULL));
    // SetTimer(hwnd, TIMER_READY, GenerateRandomDelay(config.min_delay, config.max_delay), NULL);

    // Enter Windows message loop.
    MSG msg = {0};
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_CREATE:
        LoadAndSetIcon(hwnd);
        break;

    case WM_SETCURSOR:
        switch (LOWORD(lParam)) {
        case HTCAPTION:
            state.mouse_active = false;
            SetCursor(LoadCursor(NULL, IDC_ARROW)); // Hand cursor
            break;
        case HTCLIENT:
            state.mouse_active = true;
            SetCursor(LoadCursor(NULL, IDC_HAND)); // Hand cursor
            break;
        case HTLEFT:
        case HTRIGHT:
            state.mouse_active = false;
            SetCursor(LoadCursor(NULL, IDC_SIZEWE)); // Left or right border cursor
            break;
        case HTTOP:
        case HTBOTTOM:
            state.mouse_active = false;
            SetCursor(LoadCursor(NULL, IDC_SIZENS)); // Top or bottom border cursor
            break;
        case HTTOPLEFT:
        case HTBOTTOMRIGHT:
            state.mouse_active = false;
            SetCursor(LoadCursor(NULL, IDC_SIZENWSE)); // Top-left or bottom-right corner cursor
            break;
        case HTTOPRIGHT:
        case HTBOTTOMLEFT:
            state.mouse_active = false;
            SetCursor(LoadCursor(NULL, IDC_SIZENESW)); // Top-right or bottom-left corner cursor
            break;
        default:
            state.mouse_active = false;
            SetCursor(LoadCursor(NULL, IDC_ARROW)); // Default cursor
            break;
        }
        return TRUE;

    case WM_SIZE:
        InvalidateRect(hwnd,NULL,TRUE);
        break;  

    case WM_PAINT: 
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        HBRUSH hBrush;

        SetBrush(&hBrush);
        DisplayLogic(&hdc, &hwnd, &hBrush);
        EndPaint(hwnd, &ps);
        break;

    case WM_TIMER:
        TimerStateLogic(&wParam, &hwnd);
        break;

    case WM_INPUT:
        HandleRawInput(&hwnd, &lParam);
        break;

    // Handle generic keyboard input
    case WM_KEYDOWN:
    case WM_KEYUP:
        if (config.raw_keyboard) {
            break;
        }

        for (int vkey = 0; vkey <= 255; vkey++) {
            if (IsAlphanumeric(vkey)) {
                bool is_key_pressed = GetAsyncKeyState(vkey) & 0x8000;
                if (is_key_pressed && !state.key_states[vkey]) {
                    HandleInput(hwnd, false);
                    state.key_states[vkey] = 1;
                }
                else if (!is_key_pressed && state.key_states[vkey]) {
                    state.key_states[vkey] = 0;
                }
            }
        }
        break;

    // Handle generic mouse input
    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP:
        if (config.raw_mouse) { 
            break;
        }
        if (GetAsyncKeyState(VK_LBUTTON) & 0x8000) {
            HandleInput(hwnd, true);
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

// Game Logic Functions
void DisplayLogic(HDC* hdc, HWND* hwnd, HBRUSH* brush) {
    // Paint the entire window
    RECT rect;
    GetClientRect(*hwnd, &rect);
    FillRect(*hdc, &rect, *brush);

    // Display text
    SetBkMode(*hdc, TRANSPARENT);
    SetTextColor(*hdc, RGB(255, 255, 255));
    SelectObject(*hdc, ui.font);

    wchar_t buffer[100] = {0};

    switch (state.game_state){
    case STATE_INITIAL:
        SetTextColor(*hdc, RGB(config.results_font[0], config.results_font[1], config.results_font[2]));
        swprintf_s(buffer, 100, L"Click to Begin");
        break;

    case STATE_RESULT:
        SetTextColor(*hdc, RGB(config.results_font[0], config.results_font[1], config.results_font[2]));
        GameResultLogic(buffer);
        break;
        
    case STATE_EARLY:
        SetTextColor(*hdc, RGB(config.early_font[0], config.early_font[1], config.early_font[2]));
        swprintf_s(buffer, 100, L"Too early!\nTrials so far: %d", state.trial_iteration);
        break;

    default:
        break;
    }
    

    // Calculate rectangle for the text and display it.
    RECT text_rectangle;
    SetRectEmpty(&text_rectangle);
    DrawTextW(*hdc, buffer, -1, &text_rectangle, DT_CALCRECT | DT_WORDBREAK);

    // ##REVIEW##LOW## Text doesn't center vertically (Pretty sure GPT mangled this at some point)
    RECT centered_rectangle = rect;
    centered_rectangle.top += (rect.bottom - rect.top - (text_rectangle.bottom - text_rectangle.top)) / 2; 
    DrawTextW(*hdc, buffer, -1, &centered_rectangle, DT_CENTER | DT_WORDBREAK);
};

void TimerStateLogic(WPARAM* wParam, HWND* hwnd) {
    switch (*wParam) {
    case TIMER_READY:
        state.game_state = STATE_READY;
        KillTimer(*hwnd, TIMER_READY);
        SetTimer(*hwnd, TIMER_REACT, GenerateRandomDelay(config.min_delay, config.max_delay), NULL);
        break;

    case TIMER_REACT:
        if (state.game_state == STATE_READY) {
            state.game_state = STATE_REACT;
            QueryPerformanceCounter(&data.start_time); // Start reaction timer
            InvalidateRect(*hwnd, NULL, TRUE); // Force repaint
        }
        break;

    case TIMER_EARLY:
        ResetLogic(*hwnd); // Reset the game after showing the "too early" screen
        break;

    case TIMER_DEBOUNCE:  // Debounce reset
        state.debounce_active = false;
        break;

    }
}

void ResetLogic(HWND hwnd) {
    KillTimer(hwnd, TIMER_READY);
    KillTimer(hwnd, TIMER_REACT);
    KillTimer(hwnd, TIMER_EARLY);

    state.game_state = STATE_READY;
   
    SetTimer(hwnd, TIMER_READY, GenerateRandomDelay(config.min_delay, config.max_delay), NULL);
    InvalidateRect(hwnd, NULL, TRUE);
}

void GameResultLogic(wchar_t* buffer) { // ##REVIEW## Hard to follow and combines visual data with game logic code. Needs clean up?
    if (state.current_attempt < config.averaging_trials) {
        swprintf_s(buffer, 100, L"Last: %.2lfms\nComplete %d trials for average.\nTrials so far: %d",
            data.reaction_time_value, config.averaging_trials, state.trial_iteration);
        } else {
            double total = 0;
            for (int i = 0; i < config.averaging_trials; i++) {
                total += data.reaction_time_array[i];
            }
            double average = total / config.averaging_trials;
            swprintf_s(buffer, 100, L"Last: %.2lfms\nAverage (last %d): %.2lfms\nTrials so far: %d",
                data.reaction_time_value, config.averaging_trials, average, state.trial_iteration);
        }
        if (config.trial_logging) {
            AppendToLog(data.reaction_time_value,
                state.trial_iteration, data.trial_log_path, NULL);
        }
}

// Utility Functions
void InitializeSettings(HWND* hwnd) {
    QueryPerformanceFrequency(&data.frequency);

    if (config.trial_logging) InitializeLogFileName(0);
    if (config.debug_logging) InitializeLogFileName(1);

    // Ensure Initial State
    state.game_state = STATE_INITIAL;

    // Prepare font
    ui.font_weight = FW_REGULAR;
    ui.italics_enabled = FALSE;
    if (!wcscmp(config.font_style, L"Bold")) {
        ui.font_weight = FW_BOLD;
    }
    if (!wcscmp(config.font_style, L"Italic")) {
        ui.italics_enabled = TRUE;
    }
    if (!wcscmp(config.font_style, L"Bold/Italic")) {
        ui.font_weight = FW_BOLD;
        ui.italics_enabled = TRUE;
    }
    ui.font = CreateFontW(config.font_size, 0, 0, 0, ui.font_weight, ui.italics_enabled, FALSE, FALSE, ANSI_CHARSET,
        OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE, config.font_name);
    if (!ui.font) {
        HandleError(L"Failed to create font.");
    }

    // Raw input
    if (config.raw_keyboard) RegisterForRawInput(*hwnd, 0x06);
    if (config.raw_mouse) RegisterForRawInput(*hwnd, 0x02);
    if (config.raw_input_debug) {
        wchar_t message[256];
        swprintf(message, sizeof(message) / sizeof(wchar_t), L"RawKeyboardEnable: %d\nRawMouseEnable: %d\nRegisterForRawKeyboardInput: %s\nRegisterForRawMouseInput: %s", 
            config.raw_keyboard, config.raw_mouse, RegisterForRawInput(*hwnd, 0x06) ? L"True" : L"False", RegisterForRawInput(*hwnd, 0x02) ? L"True" : L"False");
        MessageBoxW(NULL, message, L"Raw Input Variables", MB_OK);
    }
}

void HandleError(const wchar_t* error_message) {
    MessageBoxW(NULL, error_message, L"Error", MB_OK);
    if (config.debug_logging){
        AppendToLog(0, 0, data.debug_log_path, error_message);
    }
    exit(1);
}

void SetBrush(HBRUSH* brush) {
    switch (state.game_state) {
    case STATE_INITIAL:
        *brush = ui.result_brush;
        break;

    case STATE_REACT:
        *brush = ui.react_brush;
        break;

    case STATE_EARLY:
        *brush = ui.early_brush;
        break;

    case STATE_RESULT:
        *brush = ui.result_brush;
        break;

    case STATE_READY:
        *brush = ui.ready_brush;
        break;

    default:
        *brush = ui.ready_brush;
        HandleError(L"Invalid or undefined program state!");
        break;
    }
}

void ValidateColors(const COLORREF color[]) {
    for (int i = 0; i < 3; i++) {
        if (color[i] > 255) {
            HandleError(L"Invalid color values in user.cfg");
        }
    }
}

void RemoveCommentFromString(wchar_t* str) { // Removes comments and trailing spaces from strings read from .cfg files
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

int GenerateRandomDelay(int min, int max) { // Rejection Sampling RNG
    int range = max - min + 1;
    int buckets = RAND_MAX / range;
    int limit = buckets * range;

    int r;
    do {
        r = rand();
    } while (r >= limit);

    return min + (r / buckets);
}

// Configuration and setup functions
bool InitializeConfigFileAndPath(wchar_t* cfg_path) { // Initializes paths and attempts to copy default.cfg to user.cfg
    wchar_t exe_path[MAX_PATH];
    wchar_t default_cfg_path[MAX_PATH];

    if (!GetModuleFileNameW(NULL, exe_path, MAX_PATH)) {
        HandleError(L"Failed to get module file name");
    }

    wchar_t* last_slash = wcsrchr(exe_path, '\\');  // Find the last directory separator
    if (last_slash) *(last_slash + 1) = L'\0';  // Null-terminate to get directory path

    if (swprintf_s(cfg_path, MAX_PATH, L"%s/config/%s", exe_path, L"user.cfg") < 0 ||
        swprintf_s(default_cfg_path, MAX_PATH, L"%s/config/%s", exe_path, L"default.cfg") < 0) {
        HandleError(L"Failed to create config paths");
    }

    if (GetFileAttributesW(cfg_path) == INVALID_FILE_ATTRIBUTES) {  // If user.cfg doesn't exist, copy from default.cfg
        FILE* default_file;
        errno_t err1 = _wfopen_s(&default_file, default_cfg_path, L"rb");
        if (err1 != 0 || !default_file) {
            HandleError(L"Failed to open default.cfg");
        }

        FILE* new_file;
        errno_t err2 = _wfopen_s(&new_file, cfg_path, L"wb");
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

void LoadColorConfiguration(const wchar_t* cfg_path, const wchar_t* section_name, const wchar_t* colorName, const COLORREF* color) { // Load RGB
    wchar_t buffer[255];
    GetPrivateProfileStringW(section_name, colorName, L"", buffer, sizeof(buffer) / sizeof(wchar_t), cfg_path);  // Retrieve color configuration as comma-separated RGB values

    if (!wcslen(buffer)) {
        HandleError(L"Failed to read color configuration");
    }

    swscanf_s(buffer, L"%d,%d,%d", &color[0], &color[1], &color[2]);  // Extract individual RGB values
    ValidateColors(color);
}

void LoadConfig() { // ##REVIEW## Probably need more error handling for bad values in config?
    COLORREF ready_color[3], react_color[3], early_color[3], result_color[3];
    wchar_t cfg_path[MAX_PATH];

    if (!InitializeConfigFileAndPath(cfg_path)) {
        exit(1);
    }

    config.resolution_width = GetPrivateProfileIntW(L"Resolution", L"ResolutionWidth", DEFAULT_RESOLUTION_WIDTH, cfg_path);
    config.resolution_height = GetPrivateProfileIntW(L"Resolution", L"ResolutionHeight", DEFAULT_RESOLUTION_HEIGHT, cfg_path);

    LoadColorConfiguration(cfg_path, L"Colors", L"ReadyColor", ready_color);
    LoadColorConfiguration(cfg_path, L"Colors", L"ReactColor", react_color);
    LoadColorConfiguration(cfg_path, L"Colors", L"EarlyColor", early_color);
    LoadColorConfiguration(cfg_path, L"Colors", L"ResultColor", result_color);

    ui.ready_brush = CreateSolidBrush(RGB(ready_color[0], ready_color[1], ready_color[2]));
    ui.react_brush = CreateSolidBrush(RGB(react_color[0], react_color[1], react_color[2]));
    ui.early_brush = CreateSolidBrush(RGB(early_color[0], early_color[1], early_color[2]));
    ui.result_brush = CreateSolidBrush(RGB(result_color[0], result_color[1], result_color[2]));

    config.min_delay = GetPrivateProfileIntW(L"Delays", L"MinDelay", DEFAULT_MIN_DELAY, cfg_path);
    config.max_delay = GetPrivateProfileIntW(L"Delays", L"MaxDelay", DEFAULT_MAX_DELAY, cfg_path);
    config.early_reset_delay = GetPrivateProfileIntW(L"Delays", L"EarlyResetDelay", DEFAULT_EARLY_RESET_DELAY, cfg_path);

    if (config.max_delay < config.min_delay) {
        HandleError(L"MaxDelay cannot be less than MinDelay in user.cfg");
    }

    config.virtual_debounce = GetPrivateProfileIntW(L"Delays", L"VirtualDebounce", DEFAULT_VIRTUAL_DEBOUNCE, cfg_path);

    config.raw_keyboard = GetPrivateProfileIntW(L"Toggles", L"RawKeyboardEnabled", DEFAULT_RAWKEYBOARDENABLE, cfg_path);
    config.raw_mouse = GetPrivateProfileIntW(L"Toggles", L"RawMouseEnabled", DEFAULT_RAWMOUSEENABLE, cfg_path);
    config.raw_input_debug = GetPrivateProfileIntW(L"Toggles", L"RawInputDebug", 0, cfg_path);
    config.trial_logging = GetPrivateProfileIntW(L"Toggles", L"TrialLoggingEnabled", 0, cfg_path);
    config.debug_logging = GetPrivateProfileIntW(L"Toggles", L"DebugLoggingEnabled", 0, cfg_path);

    config.averaging_trials = GetPrivateProfileIntW(L"Trial", L"AveragingTrials", DEFAULT_AVG_TRIALS, cfg_path);
    if (config.averaging_trials <= 0) {
        HandleError(L"Invalid number of averaging trials in user.cfg");
    }
    config.total_trials = GetPrivateProfileIntW(L"Trial", L"TotalTrials", DEFAULT_TOTAL_TRIALS, cfg_path); // ##REVIEW##LOW## total_trials is not yet utilized for anything 
    if (config.total_trials <= 0) {
        HandleError(L"Invalid number of total trials in user.cfg");
    }

    LoadColorConfiguration(cfg_path, L"Fonts", L"EarlyFontColor", config.early_font);
    LoadColorConfiguration(cfg_path, L"Fonts", L"ResultsFontColor", config.results_font);

    GetPrivateProfileStringW(L"Fonts", L"FontName", DEFAULT_FONT_NAME, config.font_name, (DWORD)MAX_PATH, cfg_path);
    RemoveCommentFromString(config.font_name);
    if (!wcslen(config.font_name)) {
        HandleError(L"Failed to read font configuration");
    }

    config.font_size = GetPrivateProfileIntW(L"Fonts", L"FontSize", DEFAULT_FONT_SIZE, cfg_path);

    GetPrivateProfileStringW(L"Fonts", L"FontStyle", DEFAULT_FONT_STYLE, config.font_style, (DWORD)MAX_PATH, cfg_path);
    RemoveCommentFromString(config.font_style);
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
        swprintf_s(data.debug_log_path, MAX_PATH, L"log\\DEBUG_Log_%s.log", timestamp);
    } else {
        wcsftime(timestamp, timestamp_length, L"%Y%m%d%H%M%S", tmp);  // Format YYYYMMDDHHMMSS
        swprintf_s(data.trial_log_path, MAX_PATH, L"log\\Log_%s.log", timestamp);
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
            } else if (!value && !iteration) { // Hypothetically value == 0 && iteration == 0 shouldn't be possible unless the values are forced
                fwprintf(log_file, L"ERROR: %s\n", external_error_message); // Note: This only logs errors after we have already loaded the config
                fclose(log_file);
                return true;
            } else if (config.trial_logging) {
                fwprintf(log_file, L"Trial %d: %f\n", iteration, value);
                fclose(log_file);
                return true;
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
    if (hIcon) {
        // Set the icon for the window
        SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
    }

    // Load the large icon for the Alt-Tab dialog
    HICON hIconLarge = (HICON)LoadImage(NULL, szIconPath, IMAGE_ICON, 64, 64, LR_LOADFROMFILE);
    if (hIconLarge) {
        SendMessage(hwnd, WM_SETICON, ICON_BIG, (LPARAM)hIconLarge);
    }
}

// Input Functions
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
    if ((!state.mouse_active && is_mouse_input) || (state.debounce_active)) {
        return;  // Ignore mouse clicks outside of active area
    }

    switch(state.game_state) {
    case STATE_INITIAL:
        state.game_state = STATE_READY;
        SetTimer(hwnd, TIMER_READY, GenerateRandomDelay(config.min_delay, config.max_delay), NULL);
        InvalidateRect(hwnd, NULL, TRUE);
        break;

    case STATE_REACT:
        state.trial_iteration++;
        QueryPerformanceCounter(&data.end_time);
        data.reaction_time_value = ((double)(data.end_time.QuadPart - data.start_time.QuadPart) / data.frequency.QuadPart) * 1000;

        // ##REVIEW##HIGH## This is a rolling array of values
        data.reaction_time_array[state.current_attempt % config.averaging_trials] = data.reaction_time_value; 
        state.current_attempt++;

        state.game_state = STATE_RESULT;
        InvalidateRect(hwnd, NULL, TRUE);
        break;

    case STATE_EARLY:
    case STATE_RESULT:
        if ((state.game_state == STATE_RESULT) && state.current_attempt == config.averaging_trials) { // ##REVIEW## This is messy
            state.current_attempt = 0;
            for (int i = 0; i < config.averaging_trials; i++) {
                data.reaction_time_array[i] = 0;
            }
        }
        ResetLogic(hwnd);
        break;

    case STATE_READY:
        state.game_state = STATE_EARLY;
        KillTimer(hwnd, TIMER_READY);
        if (config.early_reset_delay > 0) {
            SetTimer(hwnd, TIMER_EARLY, config.early_reset_delay, NULL); // Early state eventually resets back to Ready state automatically
        }
        InvalidateRect(hwnd, NULL, TRUE);
        break;
    }

    if ((config.virtual_debounce > 0) && (!state.debounce_active)) { // Activate debounce if enabled
        state.debounce_active = true;
        SetTimer(hwnd, TIMER_DEBOUNCE, config.virtual_debounce, NULL);
    }
}

void HandleRawInput(HWND* hwnd, LPARAM* lParam) { // ##REVIEW## Keyboard and Mouse functions should be simplified and then moved into this function if possible
    UINT dwSize = 0;
    GetRawInputData((HRAWINPUT)*lParam, RID_INPUT, NULL, &dwSize, sizeof(RAWINPUTHEADER));
    LPBYTE lpb = (LPBYTE)malloc(dwSize * sizeof(BYTE));

    if (lpb == NULL) {
        return;
    }

    if (GetRawInputData((HRAWINPUT)*lParam, RID_INPUT, lpb, &dwSize, sizeof(RAWINPUTHEADER)) != dwSize) {
        HandleError(L"GetRawInputData did not return correct size!");
        free(lpb);
    }

    RAWINPUT* raw = (RAWINPUT*)lpb;

    if (raw->header.dwType == RIM_TYPEKEYBOARD && config.raw_keyboard) {
        HandleRawKeyboardInput(raw, *hwnd);
    }
    else if (raw->header.dwType == RIM_TYPEMOUSE && config.raw_mouse) {
        HandleRawMouseInput(raw, *hwnd);
    }

    free(lpb);
}

void HandleRawKeyboardInput(RAWINPUT* raw, HWND hwnd) {
    int vkey = raw->data.keyboard.VKey;

    if (raw->data.keyboard.Flags == RI_KEY_MAKE && IsAlphanumeric(vkey) && !state.key_states[vkey]) {
        HandleInput(hwnd, false);
        state.key_states[vkey] = true;
    }
    else if (raw->data.keyboard.Flags == RI_KEY_BREAK) {
        state.key_states[vkey] = false;
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
    return (vkey >= '0' && vkey <= '9') || (vkey >= 'A' && vkey <= 'Z');
}