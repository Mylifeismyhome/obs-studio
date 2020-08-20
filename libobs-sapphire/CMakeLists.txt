project(libobs-sapphire)

include_directories(SYSTEM "${CMAKE_SOURCE_DIR}/libobs")

add_definitions(-DLIBOBS_EXPORTS)

include_directories(${CMAKE_CURRENT_BINARY_DIR})

set(libobs-sapphire_SOURCES
	sapphire-capture.cpp
	)

set(libobs-sapphire_HEADERS
	sapphire-capture.h
	)

add_library(libobs-sapphire MODULE
	${libobs-sapphire_SOURCES}
	${libobs-sapphire_HEADERS})
set_target_properties(libobs-sapphire
	PROPERTIES
		FOLDER "core"
		OUTPUT_NAME libobs-sapphire
		PREFIX "")
target_precompile_headers(libobs-sapphire
	PRIVATE
		[["../libobs/util/windows/ComPtr.hpp"]]
		<obs-module.h>
		<d3d11.h>
		<DispatcherQueue.h>
		<dwmapi.h>
		<Windows.Graphics.Capture.Interop.h>
		<winrt/Windows.Foundation.Metadata.h>
		<winrt/Windows.Graphics.Capture.h>
		<winrt/Windows.System.h>)
target_link_libraries(libobs-sapphire
	libobs
	Dwmapi
	windowsapp)

install_obs_core(libobs-sapphire)
