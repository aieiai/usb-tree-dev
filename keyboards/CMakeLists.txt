# 键盘配列布局：每工程只编译一个，切换时重新配置即可
#   cmake -B build -DKBD_LAYOUT=60base
set(KBD_LAYOUTS 40base 60base)
set(KBD_LAYOUT "40base" CACHE STRING "键盘配列布局（keyboards 下的目录名，每工程只选一个）")
set_property(CACHE KBD_LAYOUT PROPERTY STRINGS ${KBD_LAYOUTS})

if(NOT KBD_LAYOUT IN_LIST KBD_LAYOUTS)
    message(FATAL_ERROR "KBD_LAYOUT=${KBD_LAYOUT} 不存在，可选：${KBD_LAYOUTS}")
endif()

sdk_inc(${KBD_LAYOUT})
sdk_app_src(${KBD_LAYOUT}/keymap.c)
