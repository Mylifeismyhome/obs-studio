/*
 OPLeague.com - this wrapper is used to capture monitor

 Original based on winrt-capture by obs
*/

#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <time.h>
#include <windows.h>

#include <util/base.h>
#include <graphics/vec2.h>
#include <media-io/audio-resampler.h>
#include <obs.h>

#include <intrin.h>

#include <obs-module.h>

#include "sapphire-capture.h"

#include <vector>

struct MonitorInformation {
	HMONITOR hmon;
	RECT rect;
};

struct MonitorRects {
	std::vector<MonitorInformation> rcMonitors;
	RECT rcCombined;

	static BOOL CALLBACK MonitorEnum(HMONITOR hMon, HDC hdc,
					 LPRECT lprcMonitor, LPARAM pData)
	{
		MonitorRects *pThis = reinterpret_cast<MonitorRects *>(pData);
		pThis->rcMonitors.push_back({hMon, *lprcMonitor});
		UnionRect(&pThis->rcCombined, &pThis->rcCombined, lprcMonitor);
		return TRUE;
	}

	MonitorRects()
	{
		SetRectEmpty(&rcCombined);
		EnumDisplayMonitors(0, 0, MonitorEnum, (LPARAM)this);
	}
};

void GetDisplayResolution(RECT &rect, UINT32 monitorIDX)
{
	MonitorRects rects;

	// Get the logical width and height of the monitor
	MONITORINFOEX monitorInfoEx;
	monitorInfoEx.cbSize = sizeof(monitorInfoEx);
	GetMonitorInfo(rects.rcMonitors[monitorIDX].hmon, &monitorInfoEx);
	auto cxLogical =
		monitorInfoEx.rcMonitor.right - monitorInfoEx.rcMonitor.left;
	auto cyLogical =
		monitorInfoEx.rcMonitor.bottom - monitorInfoEx.rcMonitor.top;

	// Get the physical width and height of the monitor
	DEVMODE devMode;
	devMode.dmSize = sizeof(devMode);
	devMode.dmDriverExtra = 0;
	EnumDisplaySettings(monitorInfoEx.szDevice, ENUM_CURRENT_SETTINGS,
			    &devMode);
	auto cxPhysical = devMode.dmPelsWidth;
	auto cyPhysical = devMode.dmPelsHeight;

	auto dpiScaleX = ((double)cxPhysical / (double)cxLogical);
	auto dpiScaleY = ((double)cyPhysical / (double)cyLogical);

	rects.rcMonitors[monitorIDX].rect.right *= dpiScaleX;
	rects.rcMonitors[monitorIDX].rect.bottom *= dpiScaleY;

	rect = rects.rcMonitors[monitorIDX].rect;
}

void GetDisplayResolution(RECT &rect, LONG &x, LONG &y, UINT32 monitorIDX)
{
	MonitorRects rects;

	auto width = rects.rcMonitors[monitorIDX].rect.right -
		     rects.rcMonitors[monitorIDX].rect.left;
	auto height = rects.rcMonitors[monitorIDX].rect.bottom -
		      rects.rcMonitors[monitorIDX].rect.top;

	// Get the logical width and height of the monitor
	MONITORINFOEX monitorInfoEx;
	monitorInfoEx.cbSize = sizeof(monitorInfoEx);
	GetMonitorInfo(rects.rcMonitors[monitorIDX].hmon, &monitorInfoEx);
	auto cxLogical =
		monitorInfoEx.rcMonitor.right - monitorInfoEx.rcMonitor.left;
	auto cyLogical =
		monitorInfoEx.rcMonitor.bottom - monitorInfoEx.rcMonitor.top;

	// Get the physical width and height of the monitor
	DEVMODE devMode;
	devMode.dmSize = sizeof(devMode);
	devMode.dmDriverExtra = 0;
	EnumDisplaySettings(monitorInfoEx.szDevice, ENUM_CURRENT_SETTINGS,
			    &devMode);
	auto cxPhysical = devMode.dmPelsWidth;
	auto cyPhysical = devMode.dmPelsHeight;

	auto dpiScaleX = ((double)cxPhysical / (double)cxLogical);
	auto dpiScaleY = ((double)cyPhysical / (double)cyLogical);

	width *= dpiScaleX;
	height *= dpiScaleY;

	x = width;
	y = height;

	rects.rcMonitors[monitorIDX].rect.right *= dpiScaleX;
	rects.rcMonitors[monitorIDX].rect.bottom *= dpiScaleY;

	rect = rects.rcMonitors[monitorIDX].rect;
}

static inline void init_textures(struct dc_capture *capture)
{
	if (capture->compatibility)
		capture->texture = gs_texture_create(capture->width,
						     capture->height, GS_BGRA,
						     1, NULL, GS_DYNAMIC);
	else
		capture->texture =
			gs_texture_create_gdi(capture->width, capture->height);

	if (!capture->texture) {
		blog(LOG_WARNING, "[dc_capture_init] Failed to "
				  "create textures");
		return;
	}

	capture->valid = true;
}

static void dc_capture_init(struct dc_capture *capture, int x, int y,
			    uint32_t width, uint32_t height, bool cursor,
			    bool compatibility)
{
	memset(capture, 0, sizeof(struct dc_capture));

	capture->x = x;
	capture->y = y;
	capture->width = width;
	capture->height = height;
	capture->capture_cursor = cursor;

	obs_enter_graphics();

	if (!gs_gdi_texture_available())
		compatibility = true;

	capture->compatibility = compatibility;

	init_textures(capture);

	obs_leave_graphics();

	if (!capture->valid)
		return;

	if (compatibility) {
		BITMAPINFO bi = {0};
		BITMAPINFOHEADER *bih = &bi.bmiHeader;
		bih->biSize = sizeof(BITMAPINFOHEADER);
		bih->biBitCount = 32;
		bih->biWidth = width;
		bih->biHeight = height;
		bih->biPlanes = 1;

		capture->hdc = CreateCompatibleDC(NULL);
		capture->bmp =
			CreateDIBSection(capture->hdc, &bi, DIB_RGB_COLORS,
					 (void **)&capture->bits, NULL, 0);
		capture->old_bmp =
			(HBITMAP)SelectObject(capture->hdc, capture->bmp);
	}
}

static inline HDC dc_capture_get_dc(struct dc_capture *capture)
{
	if (!capture->valid)
		return NULL;

	if (capture->compatibility)
		return capture->hdc;
	else
		return (HDC)gs_texture_get_dc(capture->texture);
}

static inline void dc_capture_release_dc(struct dc_capture *capture)
{
	if (capture->compatibility) {
		gs_texture_set_image(capture->texture, capture->bits,
				     capture->width * 4, false);
	} else {
		gs_texture_release_dc(capture->texture);
	}
}

static void draw_cursor(struct dc_capture *capture, HDC hdc, HWND window)
{
	HICON icon;
	ICONINFO ii;
	CURSORINFO *ci = &capture->ci;
	POINT win_pos = {capture->x, capture->y};

	if (!(capture->ci.flags & CURSOR_SHOWING))
		return;

	icon = CopyIcon(capture->ci.hCursor);
	if (!icon)
		return;

	if (GetIconInfo(icon, &ii)) {
		POINT pos;

		if (window)
			ClientToScreen(window, &win_pos);

		pos.x = ci->ptScreenPos.x - (int)ii.xHotspot - win_pos.x;
		pos.y = ci->ptScreenPos.y - (int)ii.yHotspot - win_pos.y;

		DrawIconEx(hdc, pos.x, pos.y, icon, 0, 0, 0, NULL, DI_NORMAL);

		DeleteObject(ii.hbmColor);
		DeleteObject(ii.hbmMask);
	}

	DestroyIcon(icon);
}

static void dc_capture_capture(struct dc_capture *capture, HWND window)
{
	HDC hdc_target;
	HDC hdc;

	if (capture->capture_cursor) {
		memset(&capture->ci, 0, sizeof(CURSORINFO));
		capture->ci.cbSize = sizeof(CURSORINFO);
		capture->cursor_captured = GetCursorInfo(&capture->ci);
	}

	hdc = dc_capture_get_dc(capture);
	if (!hdc) {
		blog(LOG_WARNING, "[capture_screen] Failed to get "
				  "texture DC");
		return;
	}

	hdc_target = GetDC(window);

	BitBlt(hdc, 0, 0, capture->width, capture->height, hdc_target,
	       capture->x, capture->y, SRCCOPY);

	ReleaseDC(NULL, hdc_target);

	if (capture->cursor_captured && !capture->cursor_hidden)
		draw_cursor(capture, hdc, window);

	dc_capture_release_dc(capture);

	capture->texture_written = true;
}

static void dc_capture_free(struct dc_capture *capture)
{
	if (capture->hdc) {
		SelectObject(capture->hdc, capture->old_bmp);
		DeleteDC(capture->hdc);
		DeleteObject(capture->bmp);
	}

	obs_enter_graphics();
	gs_texture_destroy(capture->texture);
	obs_leave_graphics();

	memset(capture, 0, sizeof(struct dc_capture));
}

static void monitor_capture_device_loss_release(void *data) {}

static void monitor_capture_device_loss_rebuild(void *device_void, void *data)
{
}

static BYTE *HDCToPixels(HDC Context, RECT Area, uint16_t BitsPerPixel = 24)
{
	int x = GetSystemMetrics(SM_XVIRTUALSCREEN);
	int y = GetSystemMetrics(SM_YVIRTUALSCREEN);
	int w = GetSystemMetrics(SM_CXVIRTUALSCREEN);
	int h = GetSystemMetrics(SM_CYVIRTUALSCREEN);

	LONG Width = Area.right - Area.left;
	LONG Height = Area.bottom - Area.top;
	BITMAPINFO Info;
	memset(&Info, 0, sizeof(Info));
	Info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	Info.bmiHeader.biWidth = Width;
	Info.bmiHeader.biHeight = Height;
	Info.bmiHeader.biPlanes = 1;
	Info.bmiHeader.biBitCount = BitsPerPixel;
	Info.bmiHeader.biCompression = BI_RGB;
	Info.bmiHeader.biSizeImage =
		Width * Height * (BitsPerPixel > 24 ? 4 : 3);

	byte *rawData = nullptr;
	HDC MemDC = CreateCompatibleDC(Context);
	HBITMAP Section = CreateDIBSection(Context, &Info, DIB_RGB_COLORS,
					   (void **)&rawData, 0, 0);
	DeleteObject(SelectObject(MemDC, Section));
	BitBlt(MemDC, 0, 0, Width, Height, Context, Area.left, Area.top,
	       SRCCOPY);
	StretchBlt(MemDC, 0, 0, w, h, Context, x, y, w, h, SRCCOPY);
	StretchBlt(MemDC, 0, Height, Width, -Height, Context, 0, 0, Width,
		   Height, SRCCOPY);
	DeleteDC(MemDC);

	const auto size = Info.bmiHeader.biSizeImage;
	const auto data = new byte[size + 1];
	memcpy(data, rawData, size);
	data[size] = '\0';
	DeleteObject(Section);

	return data;
}

extern "C" EXPORT monitor_capture_t *CreateDisplay(UINT32 idx, UINT32 w,
						   UINT32 h)
{
	monitor_capture_t *capture = new monitor_capture_t();
	capture->idx = idx;
	capture->capture_cursor = true;
	capture->compatibility = true;
	capture->w = w;
	capture->h = h;

	obs_enter_graphics();
	gs_device_loss callbacks;
	callbacks.device_loss_release = monitor_capture_device_loss_release;
	callbacks.device_loss_rebuild = monitor_capture_device_loss_rebuild;
	callbacks.data = capture;
	gs_register_loss_callbacks(&callbacks);
	obs_leave_graphics();

	RECT rect;
	LONG width, height;
	GetDisplayResolution(rect, width, height, idx);

	dc_capture_init(&capture->data, rect.left, rect.top, width, height,
			capture->capture_cursor, capture->compatibility);

	return capture;
}

extern "C" EXPORT void DestroyDisplay(monitor_capture_t *data)
{
	dc_capture_free(&data->data);
	delete data;
}

extern "C" EXPORT CaptureRef CaptureScreen(monitor_capture_t *data)
{
	obs_enter_graphics();
	dc_capture_capture(&data->data, 0);
	if (data->data.valid && data->data.texture_written) {
		RECT rect;
		LONG width, height;
		GetDisplayResolution(rect, width, height, data->idx);

		const auto gdi_tex = gs_texture_create_gdi(width, height);
		if (gdi_tex) {
			gs_copy_texture(gdi_tex, data->data.texture);

			HDC context = (HDC)gs_texture_get_dc(gdi_tex);

			const auto byteArray =
				HDCToPixels(context, {0, 0, width, height}, 32);

			gs_texture_release_dc(gdi_tex);

			obs_leave_graphics();

			return CaptureRef(dc_capture_get_dc(&data->data),
					  byteArray, width, height, true);
		}
	}

	obs_leave_graphics();
	return CaptureRef(NULL, nullptr, NULL, NULL, false);
};

extern "C" EXPORT void CaptureFreeByteArray(BYTE *pointer)
{
	delete[] pointer;
	pointer = nullptr;
}
