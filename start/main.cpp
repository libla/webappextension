#include <Windows.h>
#include <gdiplus.h>
#include <stdio.h>
#include <string.h>
#include <map>
#include <minizip/unzip.h>

#pragma comment(lib, "gdiplus")

using namespace Gdiplus;

static HWND parentwnd = NULL;
static HWND selfwnd = NULL;
static ULONG_PTR gdiplustoken = NULL;
static bool running = false;
static float progress = 0;

static Image *sence = NULL;
static Image *progress1 = NULL;
static Image *progress2 = NULL;

static const float STD_WIDTH = 1024;
static const float STD_HEIGHT = 576;

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

namespace
{
	struct MemZip
	{
	public:
		const unsigned char *data;
		size_t length;
		size_t offset;
		int error;
	};

	struct FileData 
	{
		size_t uncompressed;
		size_t compressed;
		unz_file_pos pos;
	};

	enum AnchorType
	{
		ANCHOR_TOPLEFT = 0,
		ANCHOR_TOP,
		ANCHOR_TOPRIGHT,
		ANCHOR_LEFT,
		ANCHOR_CENTER,
		ANCHOR_RIGHT,
		ANCHOR_BOTTOMLEFT,
		ANCHOR_BOTTOM,
		ANCHOR_BOTTOMRIGHT,

		MAX_ANCHOR_TYPE
	};

	enum ScaleType
	{
		SCALE_FULL = 0,
		SCALE_MIN,
		SCALE_MAX,
		SCALE_X,
		SCALE_Y,

		MAX_SCALE_TYPE
	};
}

static voidpf zopen_file(voidpf opaque, const char* filename, int mode)
{
	MemZip *zip = (MemZip *)opaque;
	zip->error = 0;
	return opaque;
}

static uLong zread_file(voidpf opaque, voidpf stream, void* buf, uLong size)
{
	MemZip *zip = (MemZip *)opaque;
	zip->error = 0;
	size_t l = (size_t)size;
	if (l + zip->offset > zip->length)
		l = zip->length - zip->offset;
	memcpy(buf, zip->data + zip->offset, l);
	zip->offset += l;
	return (uLong)l;
}

static uLong zwrite_file(voidpf opaque, voidpf stream, const void* buf, uLong size)
{
	MemZip *zip = (MemZip *)opaque;
	zip->error = 0;
	return 0;
}

static long ztell_file(voidpf opaque, voidpf stream)
{
	MemZip *zip = (MemZip *)opaque;
	zip->error = 0;
	return (long)zip->offset;
}

static long zseek_file(voidpf opaque, voidpf stream, uLong offset, int origin)
{
	MemZip *zip = (MemZip *)opaque;
	zip->error = 0;
	size_t origin_offset;
	switch (origin)
	{
	case ZLIB_FILEFUNC_SEEK_CUR :
		origin_offset = zip->offset;
		break;
	case ZLIB_FILEFUNC_SEEK_END :
		origin_offset = zip->length;
		break;
	case ZLIB_FILEFUNC_SEEK_SET :
		origin_offset = 0;
		break;
	default:
		return -1;
	}
	size_t newoffset = origin_offset + (size_t)offset;
	if (zip->length < newoffset)
	{
		zip->error = -1;
		return -1;
	}
	zip->offset = newoffset;
	return 0;
}

static int zclose_file(voidpf opaque, voidpf stream)
{
	MemZip *zip = (MemZip *)opaque;
	zip->error = 0;
	return 0;
}

static int zerror_file(voidpf opaque, voidpf stream)
{
	MemZip *zip = (MemZip *)opaque;
	return zip->error;
}

static unsigned int FNV(const char *path)
{
	unsigned int num = 2166136261U;
	while (true)
	{
		unsigned char byte = *path++;
		if (byte == 0)
			break;
		if (byte == '\\')
			byte = '/';
		else
			byte = tolower(byte);
		num ^= byte;
		num *= 16777619U;
	}
	return num;
}

static Image *loadImage(const void *data, size_t len)
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
	if (image->GetLastStatus() != Ok)
	{
		delete image;
		return NULL;
	}
	return image;
}

static bool loadImages(unzFile zip, const std::map<unsigned int, FileData> &filelist, const char *files[], Image **images[])
{
	bool result = false;
	char *buffer = NULL;
	size_t length = 0;
	std::map<unsigned int, FileData>::const_iterator iter;
	for (int i = 0;; ++i)
	{
		const char *file = files[i];
		if (file == NULL)
		{
			result = true;
			break;
		}
		Image **image = images[i];
		if (image == NULL)
			continue;
		iter = filelist.find(FNV(file));
		if (iter == filelist.end())
			break;
		if (length < iter->second.uncompressed)
		{
			free(buffer);
			length = iter->second.uncompressed;
			buffer = (char *)malloc(length);
		}
		unzGoToFilePos(zip, &(iter->second.pos));
		unzOpenCurrentFile(zip);
		unzReadCurrentFile(zip, buffer, iter->second.uncompressed);
		*image = loadImage(buffer, iter->second.uncompressed);
	}
	return result;
}

static void selectanchor(AnchorType anchor, float w, float h, float &x, float &y)
{
	switch (anchor)
	{
	case ANCHOR_TOP:
		x = w / 2;
		y = 0;
		break;
	case ANCHOR_TOPRIGHT:
		x = w;
		y = 0;
		break;
	case ANCHOR_LEFT:
		x = 0;
		y = h / 2;
		break;
	case ANCHOR_CENTER:
		x = w / 2;
		y = h / 2;
		break;
	case ANCHOR_RIGHT:
		x = w;
		y = h / 2;
		break;
	case ANCHOR_BOTTOMLEFT:
		x = 0;
		y = h;
		break;
	case ANCHOR_BOTTOM:
		x = w / 2;
		y = h;
		break;
	case ANCHOR_BOTTOMRIGHT:
		x = w;
		y = h;
		break;
	case ANCHOR_TOPLEFT:
	default:
		x = 0;
		y = 0;
		break;
	}
}

static void selectscale(ScaleType scale, float &x, float &y)
{
	switch (scale)
	{
	case SCALE_X:
		y = x;
	case SCALE_Y:
		x = y;
	case SCALE_MIN:
		x = y = x < y ? x : y;
	case SCALE_MAX:
		x = y = x > y ? x : y;
	}
}

static void locating(float width, float height, AnchorType self, AnchorType parent, ScaleType scale, float &x, float &y, float &w, float &h)
{
	float scalex = width / STD_WIDTH;
	float scaley = height / STD_HEIGHT;
	selectscale(scale, scalex, scaley);
	w = w * scalex;
	h = h * scaley;
	float sx, sy;
	selectanchor(self, w, h, sx, sy);
	float tx, ty;
	selectanchor(parent, width, height, tx, ty);
	x = tx + x * scalex - sx;
	y = ty + y * scaley - sy;
}

void Draw(HWND hwnd, HDC hdc, float progress)
{
	float x, y, w, h;
	RECT rect;
	GetClientRect(hwnd, &rect);
	float rw = (float)(rect.right - rect.left);
	float rh = (float)(rect.bottom - rect.top);
	Graphics graphics(hdc);
	graphics.SetInterpolationMode(Gdiplus::InterpolationModeDefault);

	x = 0;
	y = 0;
	w = STD_WIDTH;
	h = STD_HEIGHT;
	locating(rw, rh, ANCHOR_CENTER, ANCHOR_CENTER, SCALE_MAX, x, y, w, h);
	graphics.DrawImage(sence, RectF((REAL)x, (REAL)y, (REAL)w, (REAL)h), 0, 0, (REAL)sence->GetWidth(), (REAL)sence->GetHeight(), Gdiplus::UnitPixel);

	x = -384;
	y = -50;
	w = 768;
	h = 25;
	locating(rw, rh, ANCHOR_LEFT, ANCHOR_BOTTOM, SCALE_MAX, x, y, w, h);
	graphics.DrawImage(progress2, RectF((REAL)x, (REAL)y, (REAL)w, (REAL)h), 0, 0, (REAL)progress2->GetWidth(), (REAL)progress2->GetHeight(), Gdiplus::UnitPixel);

	x = -384;
	y = -50;
	w = 768 * progress;
	h = 25;
	locating(rw, rh, ANCHOR_LEFT, ANCHOR_BOTTOM, SCALE_MAX, x, y, w, h);
	graphics.DrawImage(progress1, RectF((REAL)x, (REAL)y, (REAL)w, (REAL)h), 0, 0, (REAL)(progress1->GetWidth() * progress), (REAL)progress1->GetHeight(), Gdiplus::UnitPixel);
}

static LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch(uMsg)
    {
	case WM_PAINT:
		if (running)
			break;
		{
			PAINTSTRUCT ps;
			HDC hdc = BeginPaint(hWnd, &ps);
			RECT rect;
			GetClientRect(hWnd, &rect);
			HDC mdc = CreateCompatibleDC(hdc);
			HBITMAP background = CreateCompatibleBitmap(hdc, rect.right - rect.left, rect.bottom - rect.top);
			HBITMAP odl = (HBITMAP)SelectObject(mdc, background);
			Draw(hWnd, mdc, progress);
			BitBlt(hdc, 0, 0, rect.right - rect.left, rect.bottom - rect.top, mdc, 0, 0, SRCCOPY);
			DeleteDC(hdc);
			EndPaint(hWnd, &ps);
			SelectObject(mdc, odl);
			DeleteDC(mdc);
			DeleteObject(background);
		}
		return 0;
	case WM_TIMER:
		progress = progress + 0.02f;
		if (progress >= 1)
			KillTimer(hWnd, 1);
		InvalidateRect(hWnd, NULL, false);
		return 0;
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
	wce.hbrBackground = NULL;
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

	//GdiPlus初始化
	GdiplusStartupInput gdiplusInput;
	GdiplusStartup(&gdiplustoken, &gdiplusInput, NULL);

	MemZip zip;
	zip.data = (const unsigned char *)data;
	zip.length = len;
	zip.offset = 0;
	zlib_filefunc_def fn = {
		zopen_file,
		zread_file,
		zwrite_file,
		ztell_file,
		zseek_file,
		zclose_file,
		zerror_file,
		&zip,
	};
	std::map<unsigned int, FileData> filedatas;
	unzFile file = unzOpen2("", &fn);
	unz_global_info global_info;
	unzGetGlobalInfo(file, &global_info);
	{
		char *name = NULL;
		size_t length = 0;
		do 
		{
			unz_file_info info;
			unzGetCurrentFileInfo(file, &info, NULL, 0, NULL, 0, NULL, 0);
			if (length < info.size_filename + 1)
			{
				free(name);
				length = info.size_filename + 1;
				name = (char *)malloc(length);
			}
			unzGetCurrentFileInfo(file, NULL, name, info.size_filename, NULL, 0, NULL, 0);
			if (info.size_filename > 0 && name[info.size_filename - 1] != '/')
			{
				name[info.size_filename] = 0;
				FileData &filedata = filedatas[FNV(name)];
				filedata.compressed = info.compressed_size;
				filedata.uncompressed = info.uncompressed_size;
				unzGetFilePos(file, &(filedata.pos));
			}
		} while (UNZ_OK == unzGoToNextFile(file));
		free(name);
	}
	const char *files[] = {"sence.jpg", "progress1.png", "progress2.png", NULL};
	Image **images[] = {&sence, &progress1, &progress2};
	bool result = loadImages(file, filedatas, files, images);
	unzClose(file);

	ShowWindow(selfwnd, true);
	UpdateWindow(selfwnd);

	SetTimer(selfwnd, 1, 50, NULL);

	return result ? 0 : -1;
}

void Close()
{
	delete sence;
	delete progress1;
	delete progress2;
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
