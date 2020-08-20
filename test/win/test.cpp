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

#include <vector>

#include <ShellScalingApi.h>

static UINT32 monitorIDX = NULL;

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

static void GetDisplayResolution(RECT &rect)
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

static void GetDisplayResolution(RECT &rect, LONG &x, LONG &y)
{
	MonitorRects rects;

	auto width =
		rects.rcMonitors[monitorIDX].rect.right - rects.rcMonitors[monitorIDX].rect.left;
	auto height =
		rects.rcMonitors[monitorIDX].rect.bottom - rects.rcMonitors[monitorIDX].rect.top;

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

/* --------------------------------------------------- */

class SourceContext {
	obs_source_t *source;

public:
	inline SourceContext(obs_source_t *source) : source(source) {}
	inline ~SourceContext() { obs_source_release(source); }
	inline operator obs_source_t *() { return source; }
};

/* --------------------------------------------------- */

class SceneContext {
	obs_scene_t *scene;

public:
	inline SceneContext(obs_scene_t *scene) : scene(scene) {}
	inline ~SceneContext() { obs_scene_release(scene); }
	inline operator obs_scene_t *() { return scene; }
};

/* --------------------------------------------------- */

class DisplayContext {
	obs_display_t *display;

public:
	inline DisplayContext(obs_display_t *display) : display(display) {}
	inline ~DisplayContext() { obs_display_destroy(display); }
	inline operator obs_display_t *() { return display; }
};

/* --------------------------------------------------- */

static LRESULT CALLBACK sceneProc(HWND hwnd, UINT message, WPARAM wParam,
				  LPARAM lParam)
{
	switch (message) {

	case WM_CLOSE:
		PostQuitMessage(0);
		break;

	default:
		return DefWindowProc(hwnd, message, wParam, lParam);
	}

	return 0;
}

static void do_log(int log_level, const char *msg, va_list args, void *param)
{
	char bla[4096];
	vsnprintf(bla, 4095, msg, args);

	printf(bla);
	printf("\n");

	UNUSED_PARAMETER(param);
}

static void CreateOBS(HWND hwnd)
{
	RECT rc;
	GetClientRect(hwnd, &rc);

	if (!obs_startup("en-US", nullptr, nullptr))
		throw "Couldn't create OBS";

	RECT rect;
	LONG width, height;
	GetDisplayResolution(rect, width, height);

	struct obs_video_info ovi;
	ovi.adapter = 0;
	ovi.base_width = width;
	ovi.base_height = height;
	ovi.fps_num = 600000;
	ovi.fps_den = 1001;
	ovi.graphics_module = "libobs-d3d11.dll";
	ovi.output_format = VIDEO_FORMAT_RGBA;
	ovi.output_width = width;
	ovi.output_height = height;

	if (obs_reset_video(&ovi) != 0)
		throw "Couldn't initialize video";
}

static DisplayContext CreateDisplay(HWND hwnd)
{
	RECT rc;
	GetClientRect(hwnd, &rc);

	gs_init_data info = {};
	info.cx = rc.right;
	info.cy = rc.bottom;
	info.format = GS_RGBA;
	info.zsformat = GS_ZS_NONE;
	info.window.hwnd = hwnd;
	info.num_backbuffers = 2;

	return obs_display_create(&info, 0);
}

static HWND CreateTestWindow(HINSTANCE instance)
{
	WNDCLASS wc;

	memset(&wc, 0, sizeof(wc));
	wc.lpszClassName = TEXT("bla");
	wc.hbrBackground = (HBRUSH)COLOR_WINDOW;
	wc.hInstance = instance;
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.lpfnWndProc = (WNDPROC)sceneProc;

	if (!RegisterClass(&wc))
		return 0;

	RECT rect;
	GetDisplayResolution(rect);

	const auto width = rect.right - rect.left;
	const auto height = rect.bottom - rect.top;

	return CreateWindow(TEXT("bla"), TEXT("bla"),
			    WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, 0, width,
			    height, NULL, NULL, instance, NULL);
}

/* --------------------------------------------------- */

/* --------------------------------------------------- */

struct dc_capture {
	gs_texture_t *texture;
	bool texture_written;
	int x, y;
	uint32_t width;
	uint32_t height;

	bool compatibility;
	HDC hdc;
	HBITMAP bmp, old_bmp;
	BYTE *bits;

	bool capture_cursor;
	bool cursor_captured;
	bool cursor_hidden;
	CURSORINFO ci;

	bool valid;
};

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

static void draw_texture(struct dc_capture *capture, gs_effect_t *effect)
{
	gs_texture_t *texture = capture->texture;
	gs_technique_t *tech = gs_effect_get_technique(effect, "Draw");
	gs_eparam_t *image = gs_effect_get_param_by_name(effect, "image");
	size_t passes;

	gs_effect_set_texture(image, texture);

	passes = gs_technique_begin(tech);
	for (size_t i = 0; i < passes; i++) {
		if (gs_technique_begin_pass(tech, i)) {
			if (capture->compatibility)
				gs_draw_sprite(texture, GS_FLIP_V, 0, 0);
			else
				gs_draw_sprite(texture, 0, 0, 0);

			gs_technique_end_pass(tech);
		}
	}
	gs_technique_end(tech);
}

void dc_capture_render(struct dc_capture *capture, gs_effect_t *effect)
{
	if (capture->valid && capture->texture_written)
		draw_texture(capture, effect);
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

struct monitor_capture_t {
	UINT idx;
	bool capture_cursor;
	bool compatibility;

	struct dc_capture data;
};

static void monitor_capture_device_loss_release(void *data) {}

static void monitor_capture_device_loss_rebuild(void *device_void, void *data)
{
}

static UINT GetDPI(HWND hWnd)
{
	if (hWnd != NULL) {
		if (GetDpiForWindow)
			return GetDpiForWindow(hWnd);
	} else {
		if (GetDpiForSystem)
			return GetDpiForSystem();
	}
	if (HDC hDC = GetDC(hWnd)) {
		auto dpi = GetDeviceCaps(hDC, LOGPIXELSX);
		ReleaseDC(hWnd, hDC);
		return dpi;
	} else
		return USER_DEFAULT_SCREEN_DPI;
}

static monitor_capture_t *CreateDisplay()
{
	monitor_capture_t *capture = new monitor_capture_t();
	capture->idx = 1;
	capture->capture_cursor = true;
	capture->compatibility = true;

	obs_enter_graphics();
	gs_device_loss callbacks;
	callbacks.device_loss_release = monitor_capture_device_loss_release;
	callbacks.device_loss_rebuild = monitor_capture_device_loss_rebuild;
	callbacks.data = capture;
	gs_register_loss_callbacks(&callbacks);
	obs_leave_graphics();

	RECT rect;
	LONG width, height;
	GetDisplayResolution(rect, width, height);

	dc_capture_init(&capture->data, rect.left, rect.top, width, height,
			capture->capture_cursor, capture->compatibility);

	return capture;
}

static void DestroyDisplay(monitor_capture_t *data)
{
	dc_capture_free(&data->data);
	delete data;
}

/* --------------------------------------------------- */

#include <fstream>

static bool Sapphire_OBS_ScreenShot_Save(const char *fpath, byte *data,
					 const uint16_t BitsPerPixel)
{
	RECT rect;
	LONG width, height;
	GetDisplayResolution(rect, width, height);

	BITMAPINFO Info;
	BITMAPFILEHEADER Header;
	memset(&Info, 0, sizeof(Info));
	memset(&Header, 0, sizeof(Header));
	Info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	Info.bmiHeader.biWidth = width;
	Info.bmiHeader.biHeight = height;
	Info.bmiHeader.biPlanes = 1;
	Info.bmiHeader.biBitCount = BitsPerPixel;
	Info.bmiHeader.biCompression = BI_RGB;
	Info.bmiHeader.biSizeImage =
		width * height * (BitsPerPixel > 24 ? 4 : 3);
	Header.bfType = 0x4D42;
	Header.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);

	//NET_CREATEDIRTREE(fpath);

	std::fstream hFile(fpath, std::ios::out | std::ios::binary);
	if (hFile.is_open()) {
		hFile.write((char *)&Header, sizeof(Header));
		hFile.write((char *)&Info.bmiHeader, sizeof(Info.bmiHeader));
		hFile.write((char *)data,
			    (((BitsPerPixel * width + 31) & ~31) / 8) * height);
		hFile.close();
		return true;
	}
	return false;
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

static void RenderWindow(void *data, uint32_t cx, uint32_t cy)
{
	obs_render_main_texture();

	monitor_capture_t *monitor = (monitor_capture_t *)data;
	dc_capture_capture(&monitor->data, 0);
	static bool bwritten = false;
	if (monitor->data.valid && monitor->data.texture_written) {
		if (!bwritten) {
			RECT rect;
			LONG width, height;
			GetDisplayResolution(rect, width, height);

			const auto gdi_tex =
				gs_texture_create_gdi(width, height);
			if (gdi_tex) {
				gs_copy_texture(gdi_tex, monitor->data.texture);

				HDC context = (HDC)gs_texture_get_dc(gdi_tex);

				const auto data = HDCToPixels(
					context,
					{0, 0, static_cast<long>(width),
					 static_cast<long>(height)},
					32);

				gs_texture_release_dc(gdi_tex);

				Sapphire_OBS_ScreenShot_Save("test.bmp", data,
							     32);

				delete[] data;
				bwritten = true;
			}
		}
	}
	dc_capture_render(&monitor->data,
			  obs_get_base_effect(OBS_EFFECT_OPAQUE));

	UNUSED_PARAMETER(data);
	UNUSED_PARAMETER(cx);
	UNUSED_PARAMETER(cy);
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE prevInstance, LPSTR cmdLine,
		   int numCmd)
{
	AllocConsole();
	freopen("CON", "w", stdout);

	HWND hwnd = NULL;
	base_set_log_handler(do_log, nullptr);

	try {
		hwnd = CreateTestWindow(instance);
		if (!hwnd)
			throw "Couldn't create main window";

		/* ------------------------------------------------------ */
		/* create OBS */
		CreateOBS(hwnd);

		obs_load_all_modules();

		const auto monitor = CreateDisplay();

		/* ------------------------------------------------------ */
		/* create display for output and set the output render callback */
		DisplayContext display = CreateDisplay(hwnd);
		obs_display_add_draw_callback(display, RenderWindow, monitor);

		MSG msg;
		while (GetMessage(&msg, NULL, 0, 0)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

	} catch (char *error) {
		MessageBoxA(NULL, error, NULL, 0);
	}

	obs_shutdown();

	blog(LOG_INFO, "Number of memory leaks: %ld", bnum_allocs());
	DestroyWindow(hwnd);

	UNUSED_PARAMETER(prevInstance);
	UNUSED_PARAMETER(cmdLine);
	UNUSED_PARAMETER(numCmd);
	return 0;
}
