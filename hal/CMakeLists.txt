
# 每个 USB 功能一个子目录，通过 hal_usb 的注册表关联：
#   usb_core    复合设备装配 + 功能注册与事件分发
#   usb_desc    USB 描述符（设备/配置/字符串/MS OS/WebUSB）
#   dev_kbd     键盘接口（interface 0）
#   dev_control_center 上位机接口（interface 1）
#   dev_lighting 灯效接口（interface 2）
#   dev_diagnostic_tracking  诊断追踪接口（interface 3，WinUSB/WebUSB）
#   dev_gamepad 游戏手柄接口（interface 4，HID / Xbox 两种模式，由 g_gamepad_mode 选择）
sdk_inc(./)

sdk_app_src(
    hal_usb.c
    hal_flash.c
    
    
    usb/descriptor/hid_descriptor.c
    usb/keyboard/hal_kbd.c
    usb/control_center/hal_cc.c
    usb/lighting/hal_lighting.c
    usb/diagnostic_tracking/hal_dt.c
    usb/gamepad/hal_gamepad.c
)
