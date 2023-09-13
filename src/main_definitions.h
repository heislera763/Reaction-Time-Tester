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



// WIP Functions ##REVIEW## Need some cleanup here later
void SetBrush(HBRUSH* brush);
void SetMouseInputState(bool state);
void DisplayLogic(HDC* hdc, HWND* hwnd, HBRUSH* brush);
void GameStateLogic(WPARAM* wParam, HWND* hwnd);
void GameInput(HWND* hwnd, LPARAM* lParam);

// Utility functions
void InitializeRTT(HWND* hwnd);
void HandleError(const wchar_t* error_message);
void ValidateColors(const COLORREF color[]);
void RemoveComment(wchar_t* str);
int  GenerateRandomDelay(int min, int max);
void BrushCleanup();

// Configuration and setup functions
bool InitializeConfigFileAndPath(wchar_t* cfg_path, size_t max_length);
void LoadColorConfiguration(const wchar_t* cfg_path, const wchar_t* section_name, const wchar_t* color_name, const COLORREF* target_color_array);
void LoadFontConfiguration(const wchar_t* cfg_path, wchar_t* target_font_name, size_t max_length, unsigned int* font_size, wchar_t* font_style, size_t font_style_length);
void LoadTrialConfiguration(const wchar_t* cfg_path);
void LoadTextColorConfiguration(const wchar_t* cfg_path);
void AllocateMemoryForReactionTimes();
void LoadConfig();
void InitializeLogFileName(int log_type);
bool AppendToLog(double value, int iteration, wchar_t* log_file, const wchar_t* external_error_message);
void LoadAndSetIcon(HWND hwnd);

// Input handling functions
bool RegisterForRawInput(HWND hwnd, USHORT usage);
void HandleInput(HWND hwnd, bool is_mouse_input);
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