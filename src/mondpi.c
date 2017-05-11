// Monitor DPI Enumeration
// Dan Jackson

#include <windows.h>
#include <stdio.h>

// GetDpiForMonitor
#include <ShellScalingApi.h>
#pragma comment(lib, "shcore.lib")

// Common Controls (affects text box background)
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='x86' publicKeyToken='6595b64144ccf1df' language='*'\"")


static HMONITOR hMainMonitor = NULL;
static HWND hWndMain = NULL;
static HWND hWndEdit = NULL;
static int scaleFactor = 100;
static int gnCmdShow = 0;


// Append a wide string to the log control
static void LogAppend(WCHAR *message)
{
	DWORD textLength = GetWindowTextLength(hWndEdit);
	SendMessage(hWndEdit, EM_SETSEL, textLength, textLength);
	SendMessage(hWndEdit, EM_REPLACESEL, FALSE, (LPARAM)message);
	textLength = GetWindowTextLength(hWndEdit);
	SendMessage(hWndEdit, EM_SETSEL, textLength, textLength);
	SendMessage(hWndEdit, EM_SCROLLCARET, 0, 0);
	return;
}


// Append a formatted ASCII string to the log control
static void Log(const char *format, ...)
{
	int length;
	char *buffer;
	va_list args;

	va_start(args, format);
	length = _vscprintf(format, args);
	va_end(args);

	buffer = (char *)malloc((length + 1) * sizeof(char));

	va_start(args, format);
	_vsnprintf_s(buffer, length + 1, length, format, args);
	va_end(args);

	wchar_t *wbuffer = (wchar_t *)malloc((length + 1) * sizeof(wchar_t));
	mbstowcs_s(NULL, wbuffer, length + 1, buffer, length); //MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, buffer, -1, NULL, wbuffer);

	LogAppend(wbuffer);

	free(buffer);
	free(wbuffer);
}


static int SetScaleFactor(int dpi)
{
	scaleFactor = 100 * dpi / 96;
	Log("Currently: DPI=%d, scaling: %d%%\r\n", dpi, scaleFactor);
	return scaleFactor;
}


void UpdateFonts(void)
{
	static HFONT hFont = NULL;
	if (hFont != NULL)
	{
		DeleteObject(hFont);
	}
	int size = 17 * scaleFactor / 100;
	//Log("New font size: %d\r\n", size);
	hFont = CreateFont(-size, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, FIXED_PITCH, TEXT("Consolas"));
	SendMessage(hWndEdit, WM_SETFONT, (WPARAM)hFont, TRUE);
}



// Dump monitor data
void DumpMonitorInfo(HMONITOR hMonitor)
{
	MONITORINFOEX monitorInfo = { 0 };
	monitorInfo.cbSize = sizeof(monitorInfo);
	GetMonitorInfo(hMonitor, (LPMONITORINFO)&monitorInfo);

	Log("Device="); LogAppend(monitorInfo.szDevice); Log("\r\n");
	Log("Monitor=(%d,%d)-(%d,%d)\r\n", monitorInfo.rcMonitor.left, monitorInfo.rcMonitor.top, monitorInfo.rcMonitor.right, monitorInfo.rcMonitor.bottom);
	Log("Work=(%d,%d)-(%d,%d)\r\n", monitorInfo.rcWork.left, monitorInfo.rcWork.top, monitorInfo.rcWork.right, monitorInfo.rcWork.bottom);
	Log("Flags=%d=%s\r\n", monitorInfo.dwFlags, (monitorInfo.dwFlags & MONITORINFOF_PRIMARY) ? "primary" : "non-primary");

	UINT dpiX = 0, dpiY = 0;
	HRESULT hr = GetDpiForMonitor(hMonitor, MDT_EFFECTIVE_DPI, &dpiX, &dpiY);
	Log("DPI=%d,%d (scaling %d%%)\r\n", dpiX, dpiY, 100 * dpiX / 96);

	return;
}


// Per-monitor information callback
int monitorIndex = 0;
BOOL CALLBACK MonitorEnum(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData)
{
	Log("--- MONITOR #%d ---\r\n", monitorIndex++);
	DumpMonitorInfo(hMonitor);
	monitorIndex++;
	Log("\r\n");
	return TRUE;
}


// Run code
BOOL Run(void)
{
	Log("--- DPI Awareness ---\r\n");

	// Verify that this process is DPI-aware
	PROCESS_DPI_AWARENESS dpiAwareness = -1;
	HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, GetCurrentProcessId());
	HRESULT hr = GetProcessDpiAwareness(hProcess, &dpiAwareness);
	if (hr != S_OK)
	{
		MessageBox(NULL, TEXT("GetProcessDpiAwareness failed"), TEXT("Notification"), MB_OK);
		return FALSE;
	}
	Log("ProcessDpiAwareness=%d (0=PROCESS_DPI_UNAWARE, 1=PROCESS_SYSTEM_DPI_AWARE, 2=PROCESS_PER_MONITOR_DPI_AWARE)\r\n", dpiAwareness);
	int numMonitors = GetSystemMetrics(SM_CMONITORS);
	Log("Number of monitors = %d\r\n", numMonitors);

	Log("\r\n");

	monitorIndex = 0;
	EnumDisplayMonitors(NULL, NULL, MonitorEnum, (LPARAM)NULL);

	Log("Done\r\n");

	return TRUE;
}


// Handle window message
LRESULT CALLBACK WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
		// Close window
		case WM_DESTROY:
			PostQuitMessage(0);
			return 0;

		case WM_NCCREATE:
			// Allow the non-client area to resize
			EnableNonClientDpiScaling(hWnd);
			break;

		// Resize window
		case WM_SIZE:
			// Resize edit window to client area
			if (hWndEdit != NULL)
			{
				WORD width = LOWORD(lParam);
				WORD height = HIWORD(lParam);
				MoveWindow(hWndEdit, 0, 0, width, height, 1);
			}
			break;	// pass through

		// DPI changed
		case WM_DPICHANGED:
			{
				WORD dpi = LOWORD(wParam);
				LPRECT lprcNewScale = (LPRECT)lParam;
				SetScaleFactor(dpi);
				SetWindowPos(hWnd, HWND_TOP, lprcNewScale->left, lprcNewScale->top, lprcNewScale->right - lprcNewScale->left, lprcNewScale->bottom - lprcNewScale->top, SWP_NOZORDER | SWP_NOACTIVATE);
				UpdateFonts();

				// Send to textbox?
				SendMessage(hWndEdit, uMsg, wParam, lParam);

				RedrawWindow(hWnd, NULL, NULL, RDW_ERASE | RDW_INVALIDATE);
			}
			break;	// pass through

		// Read-only edit control (not WM_CTLCOLOREDIT) has window background color
		case WM_CTLCOLORSTATIC:
			return (LRESULT)(HBRUSH)(COLOR_WINDOW + 1);
	}
	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}


// Main entry point
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow)
{
	PROCESS_DPI_AWARENESS dpiAwareness = PROCESS_PER_MONITOR_DPI_AWARE;

	// Process command-line arguments
	int argc = 0;
	LPWSTR *argv = CommandLineToArgvW(GetCommandLineW(), &argc);
	int paramErr = 0;
	for (int i = 1; i < argc; i++)
	{
		if (_wcsicmp(L"-dpiaware", argv[i]) == 0)
		{
			dpiAwareness = (PROCESS_DPI_AWARENESS)_wtoi(argv[++i]);
		}
		else
		{
			paramErr++;
		}
	}
	if (paramErr > 0)
	{
		MessageBox(NULL, TEXT("Some command-line parameters were not recognized."), TEXT("Warning"), MB_OK | MB_ICONWARNING);
	}

	// Register DPI awareness before any API call that causes the DWM to begin virtualization
	SetProcessDpiAwareness(dpiAwareness);

	// Register window class
	WNDCLASS wc = { 0 };
	wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wc.lpfnWndProc = WindowProc;
	wc.hInstance = hInstance;
	wc.lpszClassName = TEXT("mondpi");
	RegisterClass(&wc);

	// Get main monitor DPI
	POINT point;
	point.x = 0;
	point.y = 0;
	hMainMonitor = MonitorFromPoint(point, MONITOR_DEFAULTTONEAREST);
	UINT dpiX = 0, dpiY = 0;
	GetDpiForMonitor(hMainMonitor, MDT_EFFECTIVE_DPI, &dpiX, &dpiY);
	SetScaleFactor(dpiX);

	// Create window
	gnCmdShow = nCmdShow;
	int width = 640, height = 480;
	hWndMain = CreateWindowEx(0, wc.lpszClassName, TEXT("Monitor DPI"), WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, width * scaleFactor / 100, height * scaleFactor / 100, NULL, NULL, hInstance, NULL);
	if (hWndMain == NULL) { return 0; }

	// Create read-only multi-line edit control with mono-space font
	hWndEdit = CreateWindowEx(WS_EX_CLIENTEDGE, TEXT("Edit"), TEXT(""), WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_LEFT | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY, 0, 0, 0, 0, hWndMain, NULL, NULL, NULL);
	PostMessage(hWndEdit, WM_SETFOCUS, 0, 0);

	// Scale fonts
	UpdateFonts();

	// Initial log
	Log("Starting...\r\n");
	Log("\r\n");

	Log("--- MAIN MONITOR ---\r\n");
	DumpMonitorInfo(hMainMonitor);
	Log("\r\n");

	// Show main window
	ShowWindow(hWndMain, gnCmdShow);

	// Run process
	Run();

	// Message loop
	MSG msg = { 0 };
	while (GetMessage(&msg, NULL, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	LocalFree(argv);
	return 0;
}
