#define UNICODE
#define _UNICODE
#include <windows.h>
#include <shlwapi.h>
#include <stdlib.h>
#include <time.h>
#include <stdio.h>
#include <stdbool.h>
#include "main_definitions.h"

// Configuration
typedef struct {
    // Appearance
    COLORREF early_font[3];
    COLORREF results_font[3];
    wchar_t font_name[MAX_PATH];
    wchar_t font_style[MAX_PATH];
    unsigned int font_size;
    unsigned int resolution_width;
    unsigned int resolution_height;

    // Toggles
    bool raw_keyboard;
    bool raw_mouse;
    bool raw_input_debug;
    bool trial_logging;
    bool debug_logging;

    // Game Options
    int averaging_trials;
    int total_trials;
    unsigned int min_delay;
    unsigned int max_delay;
    unsigned int early_reset_delay;
} Configuration;

// Program State and Data
typedef struct {
    // Game State ##REVEIEW## Is "game" clear terminology for what this does?
    enum {
        STATE_READY,
        STATE_REACT,
        STATE_EARLY,
        STATE_RESULT
    } game_state;
    int current_attempt;
    int trial_iteration;

    // Input State
    bool input_mouse;
    bool input_keyboard;
    bool virtual_debounce;
    bool debounce_active;
    int key_states[256];

    // Logging and Data
    double reaction_times[256]; // ##REVIEW## Hardcoded size for now, will be user configurable later.
    wchar_t trial_log_path[MAX_PATH];
    wchar_t debug_log_path[MAX_PATH];

    // High-resolution timing
    LARGE_INTEGER start_time;
    LARGE_INTEGER end_time;
    LARGE_INTEGER frequency;
} ProgramState;

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

//Declare structs, pointers, and variables ##REVIEW## I would prefer this to be in my main scope
Configuration config;
ProgramState program_state = {.game_state = STATE_READY, .virtual_debounce = DEFAULT_VIRTUAL_DEBOUNCE, .reaction_times = {0}};
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

    InitializeRTT(&hwnd);

    // Display the window.
    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    if (hwnd == NULL) {
        MessageBoxW(NULL, L"Failed to create window", L"Error", MB_OK);
        return 0;
    }

    // Seed RNG and set initial timer
    srand((unsigned)time(NULL));
    SetTimer(hwnd, TIMER_READY, GenerateRandomDelay(config.min_delay, config.max_delay), NULL);

    // Enter Windows message loop. ##REVIEW## This is important and I don't 100% get it
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
        switch (LOWORD(lParam)) { // ##REVIEW## End goal is to remove mouse state from this section entirely
        case HTCLIENT:
            SetMouseInputState(true);
            SetCursor(LoadCursor(NULL, IDC_HAND)); // Hand cursor
            break;
        case HTLEFT:
        case HTRIGHT:
            SetMouseInputState(false);
            SetCursor(LoadCursor(NULL, IDC_SIZEWE)); // Left or right border cursor
            break;
        case HTTOP:
        case HTBOTTOM:
            SetMouseInputState(false);
            SetCursor(LoadCursor(NULL, IDC_SIZENS)); // Top or bottom border cursor
            break;
        case HTTOPLEFT:
        case HTBOTTOMRIGHT:
            SetMouseInputState(false);
            SetCursor(LoadCursor(NULL, IDC_SIZENWSE)); // Top-left or bottom-right corner cursor
            break;
        case HTTOPRIGHT:
        case HTBOTTOMLEFT:
            SetMouseInputState(false);
            SetCursor(LoadCursor(NULL, IDC_SIZENESW)); // Top-right or bottom-left corner cursor
            break;
        default:
            SetMouseInputState(false);
            SetCursor(LoadCursor(NULL, IDC_ARROW)); // Default cursor
            break;
        }
        return TRUE;

    case WM_PAINT: 
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        HBRUSH hBrush;

        SetBrush(&hBrush);
        DisplayLogic(&hdc, &hwnd, &hBrush);
        EndPaint(hwnd, &ps);
        break;

    case WM_TIMER:
        GameStateLogic(&wParam, &hwnd); // ##REVIEW## Any better way?
        break;

    case WM_INPUT:
        GameInput(&hwnd, &lParam);
        break;

    // Handle generic keyboard input
    case WM_KEYDOWN:
    case WM_KEYUP:
        HandleGenericKeyboardInput(hwnd);
        break;

    // Handle generic mouse input
    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP:
        HandleGenericMouseInput(hwnd);
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    default:
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }

    return 0;
}



// WIP Functions ##REVIEW## Need some cleanup here later
void SetMouseInputState(bool state) { // ##REVIEW## Questionable conditional
    if (!program_state.debounce_active) {
        program_state.input_mouse = state;
    }
}

void SetBrush(HBRUSH* brush) {
    switch (program_state.game_state) {
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

        if (program_state.game_state == STATE_RESULT) { // ##REVIEW## This is probably the hardest to follow code in the script
            SetTextColor(*hdc, RGB(config.results_font[0], config.results_font[1], config.results_font[2]));
            program_state.trial_iteration++;

            if (program_state.current_attempt < config.averaging_trials) {
                swprintf_s(buffer, 100, L"Last: %.2lfms\nComplete %d trials for average.\nTrials so far: %d",
                    program_state.reaction_times[(program_state.current_attempt - 1 + config.averaging_trials) % config.averaging_trials],
                         config.averaging_trials, program_state.trial_iteration);
            }
            else {
                double total = 0;
                for (int i = 0; i < config.averaging_trials; i++) {
                    total += program_state.reaction_times[i];
                }
                double average = total / config.averaging_trials;
                swprintf_s(buffer, 100, L"Last: %.2lfms\nAverage (last %d): %.2lfms\nTrials so far: %d",
                    program_state.reaction_times[(program_state.current_attempt - 1) % config.averaging_trials], config.averaging_trials, average, program_state.trial_iteration);
            }
            if (config.trial_logging == 1) {
                AppendToLog(program_state.reaction_times[(program_state.current_attempt - 1 + config.averaging_trials) % config.averaging_trials],
                    program_state.trial_iteration, program_state.trial_log_path, NULL);
            }
        }
        else if (program_state.game_state == STATE_EARLY) {
            SetTextColor(*hdc, RGB(config.early_font[0], config.early_font[1], config.early_font[2]));
            swprintf_s(buffer, 100, L"Too early!\nTrials so far: %d", program_state.trial_iteration);
        }

        // Calculate rectangle for the text and display it.
        RECT text_rectangle;
        SetRectEmpty(&text_rectangle);
        DrawTextW(*hdc, buffer, -1, &text_rectangle, DT_CALCRECT | DT_WORDBREAK);

        // ##REVIEW## Text doesn't center vertically (Pretty sure GPT mangled this at some point)
        RECT centered_rectangle = rect;
        centered_rectangle.top += (rect.bottom - rect.top - (text_rectangle.bottom - text_rectangle.top)) / 2; 
        DrawTextW(*hdc, buffer, -1, &centered_rectangle, DT_CENTER | DT_WORDBREAK);
};

void GameStateLogic(WPARAM* wParam, HWND* hwnd) {
        switch (*wParam) {
        case TIMER_READY:
            program_state.game_state = STATE_READY;
            KillTimer(*hwnd, TIMER_READY);
            SetTimer(*hwnd, TIMER_REACT, GenerateRandomDelay(config.min_delay, config.max_delay), NULL);
            break;

        case TIMER_REACT:
            if (program_state.game_state == STATE_READY) {
                program_state.game_state = STATE_REACT;
                QueryPerformanceCounter(&program_state.start_time); // Start reaction timer
                InvalidateRect(*hwnd, NULL, TRUE); // Force repaint
            }
            break;

        case TIMER_EARLY:
            ResetLogic(*hwnd); // Reset the game after showing the "too early" screen
            break;

        case TIMER_DEBOUNCE:  // Debounce reset
            program_state.input_mouse = true;
            program_state.input_keyboard = true;
            program_state.debounce_active = false;
            break;

        }
}

void GameInput(HWND* hwnd, LPARAM* lParam) {
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

    if (raw->header.dwType == RIM_TYPEKEYBOARD && config.raw_keyboard == true) {
        HandleRawKeyboardInput(raw, *hwnd);
    }
    else if (raw->header.dwType == RIM_TYPEMOUSE && config.raw_mouse == true) {
        HandleRawMouseInput(raw, *hwnd);
    }

    free(lpb);
}

// Utility Functions
void InitializeRTT(HWND* hwnd) { // ##REVIEW## Needs to be renamed
    QueryPerformanceFrequency(&program_state.frequency);

    if (config.trial_logging == true) InitializeLogFileName(0);
    if (config.debug_logging == true) InitializeLogFileName(1);

    // Switch to ready state
    program_state.game_state = STATE_READY;

    // Prepare font
    ui.font_weight = FW_REGULAR;
    ui.italics_enabled = FALSE;
    if (wcscmp(config.font_style, L"Bold") == 0) {
        ui.font_weight = FW_BOLD;
    }
    else if (wcscmp(config.font_style, L"Italic") == 0) {
        ui.italics_enabled = TRUE;
    }
    ui.font = CreateFontW(config.font_size, 0, 0, 0, ui.font_weight, ui.italics_enabled, FALSE, FALSE, ANSI_CHARSET,
        OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE, config.font_name);
    if (!ui.font) {
        HandleError(L"Failed to create font.");
    }

    // Raw input
    if (config.raw_keyboard == true) RegisterForRawInput(*hwnd, 0x06);
    if (config.raw_mouse == true) RegisterForRawInput(*hwnd, 0x02);
    if (config.raw_input_debug == true) {
        wchar_t message[256];
        swprintf(message, sizeof(message) / sizeof(wchar_t), L"RawKeyboardEnable: %d\nRawMouseEnable: %d\nRegisterForRawKeyboardInput: %s\nRegisterForRawMouseInput: %s", 
            config.raw_keyboard, config.raw_mouse, RegisterForRawInput(*hwnd, 0x06) ? L"True" : L"False", RegisterForRawInput(*hwnd, 0x02) ? L"True" : L"False");
        MessageBoxW(NULL, message, L"Raw Input Variables", MB_OK);
    }

}

void HandleError(const wchar_t* error_message) { // Generic error handler
    MessageBoxW(NULL, error_message, L"Error", MB_OK);
    if (config.debug_logging == 1){
        AppendToLog(0, 0, program_state.debug_log_path, error_message);
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
bool InitializeConfigFileAndPath(wchar_t* cfg_path, size_t max_length) { // Initializes paths and attempts to copy default.cfg to user.cfg
    wchar_t exe_path[MAX_PATH];
    wchar_t default_cfg_path[MAX_PATH];

    if (!GetModuleFileNameW(NULL, exe_path, MAX_PATH)) {
        HandleError(L"Failed to get module file name");
    }

    wchar_t* last_slash = wcsrchr(exe_path, '\\');  // Find the last directory separator
    if (last_slash) *(last_slash + 1) = L'\0';  // Null-terminate to get directory path

    if (swprintf_s(cfg_path, max_length, L"%s/config/%s", exe_path, L"user.cfg") < 0 ||
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

void LoadColorConfiguration(const wchar_t* cfg_path, const wchar_t* section_name, const wchar_t* colorName, const COLORREF* target_color_array) { // Load RGB
    wchar_t buffer[255];
    GetPrivateProfileStringW(section_name, colorName, L"", buffer, sizeof(buffer) / sizeof(wchar_t), cfg_path);  // Retrieve color configuration as comma-separated RGB values

    if (wcslen(buffer) == 0) {
        HandleError(L"Failed to read color configuration");
    }

    swscanf_s(buffer, L"%d,%d,%d", &target_color_array[0], &target_color_array[1], &target_color_array[2]);  // Extract individual RGB values
    ValidateColors(target_color_array);
}

void LoadFontConfiguration(const wchar_t* cfg_path, wchar_t* target_font_name, size_t max_length, unsigned int* font_size, wchar_t* font_style, size_t font_style_length) {
    GetPrivateProfileStringW(L"Fonts", L"FontName", DEFAULT_FONT_NAME, target_font_name, (DWORD)max_length, cfg_path);  // Retrieve font name
    RemoveComment(target_font_name);
    if (wcslen(target_font_name) == 0) {
        HandleError(L"Failed to read font configuration");
    }

    *font_size = GetPrivateProfileIntW(L"Fonts", L"FontSize", DEFAULT_FONT_SIZE, cfg_path);

    GetPrivateProfileStringW(L"Fonts", L"FontStyle", DEFAULT_FONT_STYLE, font_style, (DWORD)font_style_length, cfg_path);
    RemoveComment(font_style);
}

void LoadTrialConfiguration(const wchar_t* cfg_path) {
    config.averaging_trials = GetPrivateProfileIntW(L"Trial", L"AveragingTrials", DEFAULT_AVG_TRIALS, cfg_path);
    if (config.averaging_trials <= 0) {
        HandleError(L"Invalid number of averaging trials in user.cfg");
    }
    config.total_trials = GetPrivateProfileIntW(L"Trial", L"TotalTrials", DEFAULT_TOTAL_TRIALS, cfg_path); // ##REVIEW## Unused variable
    if (config.total_trials <= 0) {
        HandleError(L"Invalid number of total trials in user.cfg");
    }
}

void LoadTextColorConfiguration(const wchar_t* cfg_path) {
    LoadColorConfiguration(cfg_path, L"Fonts", L"EarlyFontColor", config.early_font);
    LoadColorConfiguration(cfg_path, L"Fonts", L"ResultsFontColor", config.results_font);
}

void LoadConfig() {
    COLORREF ready_color[3], react_color[3], early_color[3], result_color[3];  // ##REVIEW## I would like to eliminate these variables
    wchar_t cfg_path[MAX_PATH];

    if (!InitializeConfigFileAndPath(cfg_path, MAX_PATH)) {
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

    program_state.virtual_debounce = GetPrivateProfileIntW(L"Delays", L"VirtualDebounce", DEFAULT_VIRTUAL_DEBOUNCE, cfg_path);

    config.raw_keyboard = GetPrivateProfileIntW(L"Toggles", L"RawKeyboardEnabled", DEFAULT_RAWKEYBOARDENABLE, cfg_path);
    config.raw_mouse = GetPrivateProfileIntW(L"Toggles", L"RawMouseEnabled", DEFAULT_RAWMOUSEENABLE, cfg_path);
    config.raw_input_debug = GetPrivateProfileIntW(L"Toggles", L"RawInputDebug", 0, cfg_path);
    config.trial_logging = GetPrivateProfileIntW(L"Toggles", L"TrialLoggingEnabled", 0, cfg_path);
    config.debug_logging = GetPrivateProfileIntW(L"Toggles", L"DebugLoggingEnabled", 0, cfg_path);
    

    // ##REVIEW## This is broken apart from LoadConfig. I would like to be able to reduce this and combine logic
    LoadTrialConfiguration(cfg_path);
    LoadTextColorConfiguration(cfg_path);
    LoadFontConfiguration(cfg_path, config.font_name, MAX_PATH, &config.font_size, config.font_style, MAX_PATH);

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
        swprintf_s(program_state.debug_log_path, MAX_PATH, L"log\\DEBUG_Log_%s.log", timestamp);
    } else {
        wcsftime(timestamp, timestamp_length, L"%Y%m%d%H%M%S", tmp);  // Format YYYYMMDDHHMMSS
        swprintf_s(program_state.trial_log_path, MAX_PATH, L"log\\Log_%s.log", timestamp);
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
            } else if (value == 0 && iteration == 0) { // Hypothetically value == 0 && iteration == 0 shouldn't be possible unless the values are forced
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
    if ((!program_state.input_mouse && is_mouse_input) || (!program_state.input_keyboard && !is_mouse_input)) {
        return;  // Ignore input if device is currently blocked
    }
    if (program_state.game_state == STATE_REACT) {
        HandleReactClick(hwnd);
    }
    else if ((program_state.game_state == STATE_EARLY) || (program_state.game_state == STATE_RESULT)) {
        if ((program_state.game_state == STATE_RESULT) && program_state.current_attempt == config.averaging_trials) { // ##REVIEW## Double check this
            program_state.current_attempt = 0;
            for (int i = 0; i < config.averaging_trials; i++) {
                program_state.reaction_times[i] = 0;
            }
        }
        ResetLogic(hwnd);
    }
    else {
        HandleEarlyClick(hwnd);
    }
    if ((program_state.virtual_debounce) && (program_state.input_mouse || program_state.input_keyboard)) { // Activate debounce if enabled
        program_state.input_mouse = false;
        program_state.input_keyboard = false;
        program_state.debounce_active = true;
        SetTimer(hwnd, TIMER_DEBOUNCE, program_state.virtual_debounce, NULL);
    }
}

void HandleGenericKeyboardInput(HWND hwnd) { // ##REVIEW## Seems like we have an in-between function here
    if (config.raw_keyboard == false){
        for (int vkey = 0; vkey <= 255; vkey++) {
            UpdateKeyState(vkey, hwnd);
        }
    }
}

void HandleRawKeyboardInput(RAWINPUT* raw, HWND hwnd) { // ##REVIEW## Thrown together, double check
    int vkey = raw->data.keyboard.VKey;

    if (raw->data.keyboard.Flags == RI_KEY_MAKE && IsAlphanumeric(vkey) && !program_state.key_states[vkey]) {
        HandleInput(hwnd, false);
        program_state.key_states[vkey] = true;
    }
    else if (raw->data.keyboard.Flags == RI_KEY_BREAK) {
        program_state.key_states[vkey] = false;
    }
}

void HandleGenericMouseInput(HWND hwnd) { // ##REVIEW## Some quick and dirty conditional checks here
    if (config.raw_mouse == false) {
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
        if (is_key_pressed && !program_state.key_states[vkey]) {
            HandleInput(hwnd, false);
            program_state.key_states[vkey] = 1;
        }
        else if (!is_key_pressed && program_state.key_states[vkey]) {
            program_state.key_states[vkey] = 0;
        }
    }
}


// Main application logic functions
void ResetLogic(HWND hwnd) {
    KillTimer(hwnd, TIMER_READY);
    KillTimer(hwnd, TIMER_REACT);
    KillTimer(hwnd, TIMER_EARLY);

    program_state.game_state = STATE_READY;
   
    SetTimer(hwnd, TIMER_READY, GenerateRandomDelay(config.min_delay, config.max_delay), NULL);
    InvalidateRect(hwnd, NULL, TRUE);
}

void HandleReactClick(HWND hwnd) {
    QueryPerformanceCounter(&program_state.end_time);
    double time_taken = ((double)(program_state.end_time.QuadPart - program_state.start_time.QuadPart) / program_state.frequency.QuadPart) * 1000;

    program_state.reaction_times[program_state.current_attempt % config.averaging_trials] = time_taken;
    program_state.current_attempt++;

    program_state.game_state = STATE_RESULT;
    InvalidateRect(hwnd, NULL, TRUE);
}

void HandleEarlyClick(HWND hwnd) {
    program_state.game_state = STATE_EARLY;
    SetTimer(hwnd, TIMER_EARLY, config.early_reset_delay, NULL); // Early state eventually resets back to Ready state regardless of user input
    InvalidateRect(hwnd, NULL, TRUE);
}