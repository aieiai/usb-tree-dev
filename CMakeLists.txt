# 本文件由 script/localize_project.py 基于 HPM SDK 示例生成。
# 示例路径：samples/cherryusb/device/hid/keyboard_threadx
# 下面保留示例原有的 CONFIG_*、源文件列表和注释，便于对照 SDK 升级。

# Copyright (c) 2021-2023 HPMicro
# SPDX-License-Identifier: BSD-3-Clause

cmake_minimum_required(VERSION 3.13)

set(CONFIG_CHERRYUSB 1)
set(CONFIG_USB_DEVICE 1)
set(CONFIG_USB_DEVICE_HID 1)
set(CONFIG_ECLIPSE_THREADX 1)
set(CONFIG_SEGGER_RTT 1)

#set(CONFIG_ECLIPSE_THREADX_TIMER_RESOURCE_MTIMER 1) to use mchtmr as system's tick source
#set(CONFIG_ECLIPSE_THREADX_TIMER_RESOURCE_MTIMER 1)
# 由本地化脚本注入：APP_NAME 必须在 find_package 之前定义
set(APP_NAME ray)
# 引入工程内本地化 SDK、编译工具链和构建工具路径
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/localized_sdk.cmake)
    include(${CMAKE_CURRENT_SOURCE_DIR}/localized_sdk.cmake)
endif()


find_package(hpm-sdk REQUIRED HINTS $ENV{HPM_SDK_BASE})
project(ray)

add_subdirectory(tu)
add_subdirectory(hal)

sdk_inc(config)
sdk_app_src(src/main.c)
# sdk_app_src(src/webusb_hid_template.c)

sdk_compile_definitions(-DUSE_NONVECTOR_MODE=1)
sdk_compile_definitions(-DDISABLE_IRQ_PREEMPTIVE=1)
if(TARGET app)
    target_compile_options(app PRIVATE "-Wextra") # "-Wignored-qualifiers"
endif()

sdk_ses_opt_debug_connection(J-Link)
sdk_ses_opt_debug_jlink_speed(50000)

if (CONFIG_SEGGER_RTT)
  sdk_ses_opt_lib_io_type(RTT)
endif()

generate_ide_projects()



# ---- 以下由 script/localize_project.py 注入，重新本地化时保留 ----
# assert() 会把源码绝对路径（__FILE__）编译进 flash，本地化 SDK 目录名较长，
# 会让固件体积随工程路径变化。映射成相对路径后构建结果可复现。
if(DEFINED ENV{HPM_SDK_BASE})
    sdk_compile_options("-ffile-prefix-map=$ENV{HPM_SDK_BASE}=hpm_sdk")
endif()

# release 构建默认关闭 assert：断言字符串只占 flash，release 下没有调试价值。
# 需要在 release 中保留断言时，配置时加 -DAPP_KEEP_ASSERT_IN_RELEASE=ON。
option(APP_KEEP_ASSERT_IN_RELEASE "release 构建保留 assert" OFF)
string(TOLOWER "${CMAKE_BUILD_TYPE}" _APP_BUILD_TYPE)
if(_APP_BUILD_TYPE STREQUAL "release" AND NOT APP_KEEP_ASSERT_IN_RELEASE)
    sdk_compile_options("-DNDEBUG")
endif()
