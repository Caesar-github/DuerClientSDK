cmake_minimum_required(VERSION 3.1 FATAL_ERROR)

# Set project information
project(DCSDeviceApp VERSION 1.0.3 LANGUAGES CXX)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D__LOG_FILENAME__='\"$(notdir $<)\"' -D__SAMPLEAPP_VERSION__='\"${__SAMPLEAPP_VERSION__}\"' -D__DCSSDK_VERSION__='\"${__DCSSDK_VERSION__}\"' -D__DUER_LINK_VERSION__='\"${__DUER_LINK_VERSION__}\"'")

include_directories(
    sdk/include
    sdk/include/duer_link
    sdk/include/duer_link/network_observer
    sdk/include/duer_link/device_ctrl_observer
    third/include
    Framework/include)

if(CPU_ARCH STREQUAL "arm")
	message("CPU_ARCH is ${CPU_ARCH}")
	set(DUEROS_SDK_LIB_DIR sdk/lib32)
	set(DUEROS_THIRD_LIB_DIR third/lib32)
endif()
if(CPU_ARCH STREQUAL "aarch64")
	message("CPU_ARCH is ${CPU_ARCH}")
	set(DUEROS_SDK_LIB_DIR sdk/lib64)
	set(DUEROS_THIRD_LIB_DIR third/lib64)
endif()

link_directories(
    ${DUEROS_SDK_LIB_DIR}
    ${DUEROS_THIRD_LIB_DIR})

add_definitions(-DLinux)
add_definitions(-D${Platform})

if(KITTAI_KEY_WORD_DETECTOR)
    add_definitions("-DKITTAI_KEY_WORD_DETECTOR")
endif()

if(BUILD_TTS_SDK)
    add_definitions("-DBUILD_TTS_SDK")
endif()

if(DEBUG_FLAG)
	add_definitions("-DDEBUG_FLAG")
endif()

if(BUILD_CRAB_SDK)
    add_definitions(-DBuild_CrabSdk)
endif()

if(DUERLINK_V2)
    add_definitions("-DDUERLINK_V2")
endif()

add_subdirectory("DCSApp")
add_subdirectory("SocketApp")
add_subdirectory("Framework")
add_subdirectory("appresources")

