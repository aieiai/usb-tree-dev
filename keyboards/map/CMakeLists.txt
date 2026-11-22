# 键盘配列布局：每工程只编译一个, 切换时重新配置即可, cmake -B build -DKBD_MAP=40base
set(KBD_MAPS 40base 60base 68base)
set(KBD_MAP "68base" CACHE STRING "键盘配列布局（keyboards/map 下的目录名，每工程只选一个）")
set_property(CACHE KBD_MAP PROPERTY STRINGS ${KBD_MAPS})

if(NOT KBD_MAP IN_LIST KBD_MAPS)
    message(FATAL_ERROR "KBD_MAP=${KBD_MAP} 不存在，可选：${KBD_MAPS}")
endif()

sdk_inc(./ ${KBD_MAP})
sdk_app_src(keymap.c ${KBD_MAP}/map.c)
