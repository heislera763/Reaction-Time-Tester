// Various default settings and timer names
#pragma once
#define TIMER_READY 1
#define TIMER_REACT 2
#define TIMER_EARLY 3
#define TIMER_DEBOUNCE 4 
#define DEFAULT_MIN_DELAY 1000
#define DEFAULT_MAX_DELAY 3000
#define DEFAULT_EARLY_RESET_DELAY 3000
#define DEFAULT_VIRTUAL_DEBOUNCE 50
#define DEFAULT_AVG_TRIALS 5
#define DEFAULT_TOTAL_TRIALS 1000
#define DEFAULT_RAWKEYBOARDENABLE 1
#define DEFAULT_RAWMOUSEENABLE 1
#define DEFAULT_FONT_SIZE 32
#define DEFAULT_FONT_NAME L"Arial"
#define DEFAULT_FONT_STYLE L"Regular"
#define DEFAULT_RESOLUTION_WIDTH 1280
#define DEFAULT_RESOLUTION_HEIGHT 720

// Forward declarations for window procedure and other functions.
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParamg);

// Game Logic Functions
void DisplayLogic(HDC* hdc, HWND* hwnd, HBRUSH* brush);
void TimerStateLogic(WPARAM* wParam, HWND* hwnd);
void ResetLogic(HWND hwnd);

// Utility Functions
void InitializeSettings(HWND* hwnd);
void HandleError(const wchar_t* error_message);
void SetBrush(HBRUSH* brush);
void ValidateColors(const COLORREF color[]);
void RemoveCommentFromString(wchar_t* str);
int  GenerateRandomDelay(int min, int max);

// Configuration and Setup Functions
bool InitializeConfigFileAndPath(wchar_t* cfg_path);
void LoadColorConfiguration(const wchar_t* cfg_path, const wchar_t* section_name, const wchar_t* color_name, const COLORREF* target_color_array);
void LoadConfig();
void InitializeLogFileName(int log_type);
bool AppendToLog(double value, int iteration, wchar_t* log_file, const wchar_t* external_error_message);
void LoadAndSetIcon(HWND hwnd);

// Input Functions
bool RegisterForRawInput(HWND hwnd, USHORT usage);
void HandleInput(HWND hwnd, bool is_mouse_input);
void HandleRawInput(HWND* hwnd, LPARAM* lParam);
void HandleRawKeyboardInput(RAWINPUT* raw, HWND hwnd);
void HandleRawMouseInput(RAWINPUT* raw, HWND hwnd);
bool IsAlphanumeric(int vkey);
void UpdateKeyState(int vkey, HWND hwnd);