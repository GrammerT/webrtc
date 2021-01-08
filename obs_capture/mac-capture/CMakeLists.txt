project(mac-capture)

find_library(COREAUDIO CoreAudio)
find_library(AUDIOUNIT AudioUnit)
find_library(COREFOUNDATION CoreFoundation)
find_library(IOSURF IOSurface)
find_library(COCOA Cocoa)
find_library(NS NoiseSuppression ${CMAKE_SOURCE_DIR}/CCLive/deps)

find_package(FFmpeg REQUIRED
	COMPONENTS avcodec avfilter avdevice avutil swscale avformat swresample)
include_directories(${FFMPEG_INCLUDE_DIRS})

include_directories(${COREAUDIO}
                    ${AUDIOUNIT}
                    ${COREFOUNDATION}
		    ${IOSURF}
		    ${COCOA})

set(mac-capture_HEADERS
	audio-device-enum.h
	mac-helpers.h
	window-utils.h
	NoiseSuppression.h)

set(mac-capture_SOURCES
	plugin-main.c
	audio-device-enum.c
	mac-audio.c
	mac-display-capture.m
	mac-window-capture.m
	mac-display-area-capture.m
	window-utils.m)
	
set_source_files_properties(mac-display-capture.m
			    mac-window-capture.m
			    window-utils.m
	PROPERTIES LANGUAGE C)

add_library(mac-capture MODULE
	${mac-capture_SOURCES}
	${mac-capture_HEADERS})
target_link_libraries(mac-capture
	libobs
	${COREAUDIO}
	${AUDIOUNIT}
	${COREFOUNDATION}
	${IOSURF}
	${COCOA}
	${FFMPEG_LIBRARIES}
	${NS})

install_obs_plugin_with_data(mac-capture data)
