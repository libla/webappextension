#include <Windows.h>

namespace
{
	typedef int(*StartFunc)(void *wnd, void *ptr, const char *args);
	typedef void(*CloseFunc)();
	typedef void(*ResizeFunc)();
	typedef const char *(*MessageFunc)(const char *msg);
}

static HMODULE startdll;

LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_LBUTTONDOWN://左键单击时拖动窗体
		SendMessage(hWnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
		return 0;
	case WM_SIZE:
		if (startdll != NULL)
		{
			ResizeFunc fn = (ResizeFunc)GetProcAddress(startdll, "Resize");
			if (fn != NULL)
				fn();
		}
		return 0;
	case WM_DESTROY:
		if (startdll != NULL)
		{
			CloseFunc fn = (CloseFunc)GetProcAddress(startdll, "Close");
			if (fn != NULL)
				fn();
			FreeLibrary(startdll);
			startdll = NULL;
		}
		PostQuitMessage(wParam);
		return 0;
	}
	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
	WNDCLASSEX wce;
	memset(&wce, 0, sizeof(wce));
	wce.cbSize = sizeof(wce);
	wce.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
	wce.hCursor = LoadCursor(NULL, IDC_ARROW);
	wce.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	wce.hInstance = hInstance;
	wce.lpfnWndProc = WndProc;
	wce.lpszClassName = "Host";
	wce.style = CS_HREDRAW | CS_VREDRAW;
	if (!RegisterClassEx(&wce))
		return GetLastError();

	char* title = "Win32SDK GDI+ 图像显示示例程序(拖动图片文件到窗口以显示)";
	HWND hWnd = CreateWindowEx(0, "Host", title, WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
		NULL, NULL, hInstance, NULL);
	if (hWnd == NULL)
		return GetLastError();

	ShowWindow(hWnd, nShowCmd);
	UpdateWindow(hWnd);

	startdll = LoadLibraryA("start.dll");
	StartFunc fn = (StartFunc)GetProcAddress(startdll, "Start");
	if (fn == NULL)
	{
		SendMessage(hWnd, WM_DESTROY, -1, 0);
	}
	else
	{
		int result = fn(hWnd, NULL, NULL);
		if (result != 0)
			SendMessage(hWnd, WM_DESTROY, result, 0);
	}

	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	return msg.wParam;
}