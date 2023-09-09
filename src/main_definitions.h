// Various default settings
#pragma once
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


// Utility functions
void HandleError(const wchar_t* errorMessage);
void ValidateColors(const COLORREF color[]);
void ValidateConfigSetting();
void RemoveComment(wchar_t* str);
int  GenerateRandomDelay(int min, int max);
void BrushCleanup();

// Configuration and setup functions
bool InitializeConfigFileAndPath(wchar_t* cfgPath, size_t maxLength);
void LoadColorConfiguration(const wchar_t* cfgPath, const wchar_t* sectionName, const wchar_t* colorName, const COLORREF* targetColorArray);
void LoadFontConfiguration(const wchar_t* cfgPath, wchar_t* targetFontName, size_t maxLength, int* fontSize, wchar_t* fontStyle, size_t fontStyleLength);
void LoadTrialConfiguration(const wchar_t* cfgPath);
void LoadTextColorConfiguration(const wchar_t* cfgPath);
void AllocateMemoryForReactionTimes();
void LoadConfig();
void InitializeLogFileName(int log_type);
bool AppendToLog(double reaction_time_value, int trial_number, wchar_t* log_file, const wchar_t* external_error_message);
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