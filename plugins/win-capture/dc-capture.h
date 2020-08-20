#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <obs-module.h>

extern void dc_capture_init(struct dc_capture *capture, int x, int y,
			    uint32_t width, uint32_t height, bool cursor,
			    bool compatibility);
extern void dc_capture_free(struct dc_capture *capture);

extern void dc_capture_capture(struct dc_capture *capture, HWND window);
extern void dc_capture_render(struct dc_capture *capture, gs_effect_t *effect);
