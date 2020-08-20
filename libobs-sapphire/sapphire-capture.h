#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <dxgi.h>

#include <obs-module.h>

void GetDisplayResolution(RECT &rect, UINT32 monitorIDX);
void GetDisplayResolution(RECT &rect, LONG &x, LONG &y, UINT32 monitorIDX);

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

struct monitor_capture_t {
	UINT idx;
	bool capture_cursor;
	bool compatibility;

	LONG w, h;

	struct dc_capture data;
};

struct CaptureRef {
	HDC target;
	LONG width, height;
	BYTE *data;
	bool valid;

	CaptureRef(const HDC target, BYTE *data, const LONG width,
		   const LONG height, const bool valid)
	{
		this->target = target;
		this->data = data;
		this->valid = valid;
		this->width = width;
		this->height = height;
	}
};

#ifdef __cplusplus
extern "C" {
#endif
EXPORT monitor_capture_t *CreateDisplay(UINT32 idx, UINT32 w, UINT32 h);
EXPORT void DestroyDisplay(monitor_capture_t *data);
EXPORT CaptureRef CaptureScreen(monitor_capture_t *data);
EXPORT void CaptureFreeByteArray(BYTE *pointer);
#ifdef __cplusplus
}
#endif
