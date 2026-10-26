
# 每个 USB 功能一个子目录，通过 hal_usb 的注册表关联：
#   usb_core    复合设备装配 + 功能注册与事件分发
#   usb_desc    USB 描述符（设备/配置/字符串/MS OS/WebUSB）
#   dev_kbd     键盘接口（interface 0）
#   dev_control_center 上位机接口（interface 1）
#   dev_lighting 灯效接口（interface 2）
#   dev_diagnostic_tracking  诊断追踪接口（interface 3，WinUSB/WebUSB）
#   dev_gamepad 游戏手柄接口（interface 4，HID / Xbox 两种模式，由 g_gamepad_mode 选择）
sdk_inc(usb_core)
sdk_inc(usb_desc)
sdk_inc(dev_kbd)
sdk_inc(dev_control_center)
sdk_inc(dev_lighting)
sdk_inc(dev_diagnostic_tracking)
sdk_inc(dev_gamepad)

sdk_app_src(
    usb_core/hal_usb.c
    usb_desc/hid_descriptor.c
    dev_kbd/hal_kbd.c
    dev_control_center/hal_cc.c
    dev_lighting/hal_lighting.c
    dev_diagnostic_tracking/hal_dt.c
    dev_gamepad/hal_gamepad.c
)
