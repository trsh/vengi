project(thumbnailer)
set(SRCS
	ImageGenerator.h ImageGenerator.cpp
	Thumbnailer.h Thumbnailer.cpp
)

engine_add_executable(TARGET ${PROJECT_NAME} SRCS ${SRCS})
engine_target_link_libraries(TARGET ${PROJECT_NAME} DEPENDENCIES app voxelrender)

if (WIN32)
	set(WIN_SRCS
		windows/ThumbnailerDLL.cpp
		windows/ThumbnailerDLL.def
		windows/ThumbnailerProvider.cpp windows/ThumbnailerProvider.h
	)
	add_library(voxelthumb SHARED ${WIN_SRCS})
	set_target_properties(voxelthumb PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS OFF)
	target_link_libraries(voxelthumb voxelrender shlwapi)
endif()
