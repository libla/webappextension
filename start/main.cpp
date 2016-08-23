#include <Windows.h>
#include <gdiplus.h>
#include <stdio.h>

#pragma comment(lib, "gdiplus")

using namespace Gdiplus;

static HWND parentwnd = NULL;
static HWND selfwnd = NULL;
static ULONG_PTR gdiplustoken = NULL;

extern "C"
{
	struct Interface
	{
		void(*close)(Interface *ptr, int i);
		void(*notify)(Interface *ptr, const char *type, const char *msg);
	};

	int Start(const void *data, int len, void *wnd, Interface *ptr, const char *args);
	void Close();
	void Resize();
	const char *Message(const char *msg);
};

Image *loadimage(const void *data, size_t len)
{
	HGLOBAL page = GlobalAlloc(GMEM_FIXED, len);
	{
		BYTE* ptr = (BYTE*)GlobalLock(page);
		memcpy(ptr, data, len);
	}
	IStream* stream;
	CreateStreamOnHGlobal(page, FALSE, &stream);
	Image *image = new Image(stream);
	GlobalUnlock(page);
	stream->Release();
	return image;
}

void draw_image(HWND hWnd,wchar_t* file)
{
    HDC hdc;
    int width,height;

    //加载图像
	FILE *f = _wfopen(file, L"rb");
	fseek(f, 0, SEEK_END);
	size_t len = ftell(f);
	fseek(f, 0, SEEK_SET);
	void *data = malloc(len);
	fread(data, len, 1, f);
	fclose(f);
	Image *pimg = loadimage(data, len);
	free(data);
    Image &image = *pimg;
    if(image.GetLastStatus() != Ok){
        MessageBox(hWnd,"图片无效!",NULL,MB_OK);
        return;
    }
    
    //取得宽度和高度
    width = image.GetWidth();
    height = image.GetHeight();

    //更新窗口大小
	RECT rect;
	GetClientRect(hWnd, &rect);


    hdc = GetDC(hWnd);

    //绘图
    Graphics graphics(hdc);
	graphics.SetInterpolationMode(Gdiplus::InterpolationModeDefault);
    //graphics.DrawImage(&image,0,0,width,height);
	graphics.DrawImage(&image, RectF(0, 0, (REAL)(rect.right - rect.left), (REAL)(rect.bottom - rect.top)), 0, 0, (REAL)width, (REAL)height, Gdiplus::UnitPixel);

    ReleaseDC(hWnd,hdc);

	delete pimg;

    return;
}

static LRESULT CALLBACK WndProc(HWND hWnd,UINT uMsg,WPARAM wParam,LPARAM lParam)
{
    switch(uMsg)
    {
    case WM_DROPFILES://拖动图片文件时进行图像显示
        {
            wchar_t file[MAX_PATH];
            DragQueryFileW((HDROP)wParam,0,file,sizeof(file)/sizeof(*file));
            draw_image(hWnd,file);
            DragFinish((HDROP)wParam);
            return 0;
        }
    }
	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

int Start(const void *data, int len, void *wnd, Interface *ptr, const char *args)
{
	parentwnd = (HWND)wnd;
	if (parentwnd == NULL)
		return -1;
	HINSTANCE hInstance = GetModuleHandle(NULL);
	WNDCLASSEX wce;
	memset(&wce, 0, sizeof(wce));
	wce.cbSize = sizeof(wce);
	wce.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
	wce.hCursor = LoadCursor(NULL, IDC_ARROW);
	wce.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	wce.hInstance = hInstance;
	wce.lpfnWndProc = WndProc;
	wce.lpszClassName = "WebApp.Start";
	wce.style = CS_HREDRAW | CS_VREDRAW;
	if (!RegisterClassEx(&wce))
		return GetLastError();

	RECT rect;
	GetClientRect(parentwnd, &rect);
	selfwnd = CreateWindowEx(0, "WebApp.Start", "", WS_CHILD | WS_VISIBLE,
		0, 0, rect.right - rect.left, rect.bottom - rect.top,
		parentwnd, NULL, hInstance, NULL);
	if (selfwnd == NULL)
		return GetLastError();

	//窗口接收文件拖放
	DragAcceptFiles(selfwnd, TRUE);

	//GdiPlus初始化
	GdiplusStartupInput gdiplusInput;
	GdiplusStartup(&gdiplustoken, &gdiplusInput, NULL);

	ShowWindow(selfwnd, true);
	UpdateWindow(selfwnd);

	return 0;
}

void Close()
{
	//GdiPlus 取消初始化
	GdiplusShutdown(gdiplustoken);
}

void Resize()
{
	RECT rect;
	GetClientRect(parentwnd, &rect);
	SetWindowPos(selfwnd, 0, 0, 0, rect.right - rect.left, rect.bottom - rect.top, SWP_NOMOVE | SWP_NOZORDER);
}

const char *Message(const char *msg)
{
	return "";
}
