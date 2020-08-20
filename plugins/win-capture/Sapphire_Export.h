#pragma once
#include <Windows.h>
#include <stdlib.h>
#include <util/dstr.h>

__declspec(dllexport) BOOL CALLBACK enum_monitor(HMONITOR handle, HDC hdc, LPRECT rect, LPARAM param);
_declspec(dllexport) void update_monitor(struct monitor_capture *capture,obs_data_t *settings);
_declspec(dllexport) const char *monitor_capture_getname(void *unused);
_declspec(dllexport) void monitor_capture_destroy(void *data);
_declspec(dllexport) void monitor_capture_defaults(obs_data_t *settings);
_declspec(dllexport) void monitor_capture_update(void *data, obs_data_t *settings);
_declspec(dllexport) void *monitor_capture_create(obs_data_t *settings, obs_source_t *source);
_declspec(dllexport) void monitor_capture_tick(void *data, float seconds);
_declspec(dllexport) void monitor_capture_render(void *data, gs_effect_t *effect);
_declspec(dllexport) uint32_t monitor_capture_width(void *data);
_declspec(dllexport) uint32_t monitor_capture_height(void *data);
_declspec(dllexport) BOOL CALLBACK enum_monitor_props(HMONITOR handle, HDC hdc, LPRECT rect, LPARAM param);
_declspec(dllexport) obs_properties_t *monitor_capture_properties(void *unused);
