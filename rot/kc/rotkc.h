#ifndef ROTKC_H
#define ROTKC_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>


/**
 * @cite:   HID Usage Tables, https://www.usb.org/sites/default/files/hut1_7.pdf#page=90
 * @brief:  Keyboard/KeypadPage(0x07)
 */
typedef enum {
    KC_NONE             = 0x00u,   // 无按键（保留值，不产生事件） 
    KC_ROLL_OVER        = 0x01u,   // Roll Over：按键溢出
    KC_POST_FAIL        = 0x02u,   // POST 失败（上电自检失败）
    KC_UNDEFINED        = 0x03u,   // 未定义 
    KC_A                = 0x04u,
    KC_B                = 0x05u,
    KC_C                = 0x06u,
    KC_D                = 0x07u,
    KC_E                = 0x08u,
    KC_F                = 0x09u,
    KC_G                = 0x0Au,
    KC_H                = 0x0Bu,
    KC_I                = 0x0Cu,
    KC_J                = 0x0Du,
    KC_K                = 0x0Eu,
    KC_L                = 0x0Fu,
    KC_M                = 0x10u,
    KC_N                = 0x11u,
    KC_O                = 0x12u,
    KC_P                = 0x13u,
    KC_Q                = 0x14u,
    KC_R                = 0x15u,
    KC_S                = 0x16u,
    KC_T                = 0x17u,
    KC_U                = 0x18u,
    KC_V                = 0x19u,
    KC_W                = 0x1Au,
    KC_X                = 0x1Bu,
    KC_Y                = 0x1Cu,
    KC_Z                = 0x1Du,
    KC_1                = 0x1Eu,   // 1 !
    KC_2                = 0x1Fu,   // 2 @
    KC_3                = 0x20u,   // 3 #
    KC_4                = 0x21u,   // 4 $
    KC_5                = 0x22u,   // 5 %
    KC_6                = 0x23u,   // 6 ^
    KC_7                = 0x24u,   // 7 &
    KC_8                = 0x25u,   // 8 *
    KC_9                = 0x26u,   // 9 (
    KC_0                = 0x27u,   // 0 )
    KC_ENTER            = 0x28u,
    KC_ESC              = 0x29u,
    KC_BACKSPACE        = 0x2Au,
    KC_TAB              = 0x2Bu,
    KC_SPACE            = 0x2Cu,
    KC_MINUS            = 0x2Du,   // 减号 - _ 
    KC_EQUAL            = 0x2Eu,   // 等号 = + 
    KC_LBRACKET         = 0x2Fu,   // 左方括号 [ { 
    KC_RBRACKET         = 0x30u,   // 右方括号 ] } 
    KC_BACKSLASH        = 0x31u,   // 反斜杠 \ | 
    KC_NONUS_HASH       = 0x32u,   // 非美式 # ~（ISO 键盘） 
    KC_SEMICOLON        = 0x33u,   // 分号 ; : 
    KC_APOSTROPHE       = 0x34u,   // 单引号 ' " 
    KC_GRAVE            = 0x35u,   // 重音符 ` ~ 
    KC_COMMA            = 0x36u,   // 逗号 , < 
    KC_DOT              = 0x37u,   // 句点 . > 
    KC_SLASH            = 0x38u,   // 斜杠 / ? 
    KC_CAPSLOCK         = 0x39u,
    KC_F1               = 0x3Au,
    KC_F2               = 0x3Bu,
    KC_F3               = 0x3Cu,
    KC_F4               = 0x3Du,
    KC_F5               = 0x3Eu,
    KC_F6               = 0x3Fu,
    KC_F7               = 0x40u,
    KC_F8               = 0x41u,
    KC_F9               = 0x42u,
    KC_F10              = 0x43u,
    KC_F11              = 0x44u,
    KC_F12              = 0x45u,
    KC_PRINTSCREEN      = 0x46u,   // 打印屏幕 PrintScreen 
    KC_SCROLLLOCK       = 0x47u,   // 滚动锁定 ScrollLock 
    KC_PAUSE            = 0x48u,   // 暂停 Pause 
    KC_INSERT           = 0x49u,   // 插入 Insert
    KC_HOME             = 0x4Au,   // Home
    KC_PAGEUP           = 0x4Bu,   // Page Up
    KC_DELETE           = 0x4Cu,   // Delete
    KC_END              = 0x4Du,   // End
    KC_PAGEDOWN         = 0x4Eu,   // Page Down
    KC_RIGHT            = 0x4Fu,
    KC_LEFT             = 0x50u,
    KC_DOWN             = 0x51u,
    KC_UP               = 0x52u,
    KC_NUMLOCK          = 0x53u,
    KC_KP_SLASH         = 0x54u,   // 小键盘 / 
    KC_KP_ASTERISK      = 0x55u,   // 小键盘 * 
    KC_KP_MINUS         = 0x56u,   // 小键盘 - 
    KC_KP_PLUS          = 0x57u,   // 小键盘 + 
    KC_KP_ENTER         = 0x58u,   // 小键盘回车 
    KC_KP_1             = 0x59u,   // 小键盘 1（End） 
    KC_KP_2             = 0x5Au,   // 小键盘 2（下） 
    KC_KP_3             = 0x5Bu,   // 小键盘 3（PageDown） 
    KC_KP_4             = 0x5Cu,   // 小键盘 4（左） 
    KC_KP_5             = 0x5Du,   // 小键盘 5 
    KC_KP_6             = 0x5Eu,   // 小键盘 6（右） 
    KC_KP_7             = 0x5Fu,   // 小键盘 7（Home） 
    KC_KP_8             = 0x60u,   // 小键盘 8（上） 
    KC_KP_9             = 0x61u,   // 小键盘 9（PageUp） 
    KC_KP_0             = 0x62u,   // 小键盘 0（Insert） 
    KC_KP_DOT           = 0x63u,   // 小键盘 . （Delete） 
    KC_APPLICATION      = 0x65u,   // Application 菜单键（等同右键/上下文菜单） 
    KC_POWER            = 0x66u,   // 电源键
    KC_F13              = 0x68u,
    KC_F14              = 0x69u,
    KC_F15              = 0x6Au,
    KC_F16              = 0x6Bu,
    KC_F17              = 0x6Cu,
    KC_F18              = 0x6Du,
    KC_F19              = 0x6Eu,
    KC_F20              = 0x6Fu,
    KC_F21              = 0x70u,
    KC_F22              = 0x71u,
    KC_F23              = 0x72u,
    KC_F24              = 0x73u,
    KC_EXECUTE          = 0x74u,   // Execute（执行） 
    KC_HELP             = 0x75u,   // Help（帮助） 
    KC_MENU             = 0x76u,   // Menu（菜单） 
    KC_SELECT           = 0x77u,   // Select（选择） 
    KC_STOP             = 0x78u,   // Stop（停止） 
    KC_AGAIN            = 0x79u,   // Again（重做上次操作） 
    KC_UNDO             = 0x7Au,   // 撤销（对应应用里的 Ctrl+Z） 
    KC_CUT              = 0x7Bu,   // 剪切 
    KC_COPY             = 0x7Cu,   // 复制 
    KC_PASTE            = 0x7Du,   // 粘贴 
    KC_FIND             = 0x7Eu,   // 查找 
    KC_MUTE             = 0x7Fu,   // 静音
    KC_VOLUME_UP        = 0x80u,   // 音量加 */
    KC_VOLUME_DOWN      = 0x81u,   // 音量减 */
    KC_LOCKING_CAPS     = 0x82u,   // 大写锁定（locked 型，非切换型；一般不用） 
    KC_LOCKING_NUM      = 0x83u,   // 数字锁定（locked 型） 
    KC_LOCKING_SCROLL   = 0x84u,   // 滚动锁定（locked 型） 
    KC_KP_COMMA         = 0x85u,   // 小键盘逗号（部分布局） 
    KC_KP_EQUAL_AS400   = 0x86u,   // AS/400 键盘上的等号 
    KC_INT1             = 0x87u,   // 国际化保留键 1（日文/韩文等键盘） 
    KC_INT2             = 0x88u,
    KC_INT3             = 0x89u,
    KC_INT4             = 0x8au,
    KC_INT5             = 0x8bu,
    KC_INT6             = 0x8cu,
    KC_INT7             = 0x8du,
    KC_INT8             = 0x8eu,
    KC_INT9             = 0x8fu,
    KC_LANG1            = 0x90u,   // 语言/输入法键 1（日文 かな 等） 
    KC_LANG2            = 0x91u,   // 语言/输入法键 2 
    KC_LANG3            = 0x92u,   // 语言/输入法键 3 
    KC_LANG4            = 0x93u,   // 语言/输入法键 4 
    KC_LANG5            = 0x94u,   // 语言/输入法键 5 
    KC_LANG6            = 0x95u,   // 语言/输入法键 6 
    KC_LANG7            = 0x96u,   // 语言/输入法键 7 
    KC_LANG8            = 0x97u,   // 语言/输入法键 8 
    KC_LANG9            = 0x98u,   // 语言/输入法键 9 
    KC_ALT_ERASE        = 0x99u,   // Alt Erase 
    KC_SYSREQ           = 0x9au,   // 系统请求 SysRq 
    KC_CANCEL           = 0x9bu,   // 取消 Cancel 
    KC_CLEAR            = 0x9cu,   // 清除 Clear 
    KC_PRIOR            = 0x9du,   // Prior（同 PageUp 语义） 
    KC_RETURN           = 0x9eu,   // Return 
    KC_SEPARATOR        = 0x9fu,   // 分隔符 Separator 
    KC_OUT              = 0xA0u,   // Out 
    KC_OPER             = 0xa1u,   // Oper 
    KC_CLEAR_AGAIN      = 0xa2u,   // Clear / Again 
    KC_CRSEL            = 0xa3u,   // CrSel / Props 
    KC_EXSEL            = 0xA4u,   // ExSel 

    // modifiers
    KC_LCTL = 0xE0u,
    KC_LSFT = 0xE1u,
    KC_LALT = 0xE2u,
    KC_LGUI = 0xE3u,
    KC_RCTL = 0xE4u,
    KC_RSFT = 0xE5u,
    KC_RALT = 0xE6u,
    KC_RGUI = 0xE7u,

    // function customization keys
    KC_FN0 = 0xF0u,
    KC_FN1 = 0xF1u,
    KC_FN2 = 0xF2u,
    KC_FN3 = 0xF3u,
    KC_FN4 = 0xF4u,
    KC_FN5 = 0xF5u,
    KC_FN6 = 0xF6u,
    KC_FN7 = 0xF7u,
    KC_FNX = 0xF8u,
} usage_kc_ty;

/**
 * @cite:   HID Usage Tables, https://www.usb.org/sites/default/files/hut1_7.pdf#page=33
 * @brief:  GenericDesktopPage(0x01), Usage: SystemControl, 0x80...
 */
typedef enum {
    SC_POWER_DOWN   = 0x81u,    // 关机 
    SC_SLEEP        = 0x82u,    // 睡眠 
    SC_WAKE_UP      = 0x83u,    // 唤醒 
    SC_CONTEXT_MENU = 0x84u,    // 上下文菜单（右键菜单） 
    SC_MAIN_MENU    = 0x85u,    // 主菜单 
    SC_APP_MENU     = 0x86u,    // 应用菜单 
    SC_MENU_HELP    = 0x87u,    // 菜单：帮助 
    SC_MENU_EXIT    = 0x88u     // 菜单：退出 
} usage_sc_ty;

/**
 * @cite:   HID Usage Tables, https://www.usb.org/sites/default/files/hut1_7.pdf#page=127
 * @brief:  ConsumerPage(0x0C), commonly useful controls plus AL/AC ranges.
 */
typedef enum {
    CC_POWER                    = 0x0030u,   // 电源开关（主机总电源控制，键盘一般不用） 
    CC_RESET                    = 0x0031u,   // 复位 
    CC_SLEEP                    = 0x0032u,   // 睡眠（消费者页版；系统页另有 SC_SLEEP） 
    CC_ILLUMINATION             = 0x0035u,   // 照明开关（键盘背光 / 键盘灯） 
    
    CC_BRIGHTNESS_INC           = 0x006Fu,   // 亮度加
    CC_BRIGHTNESS_DEC           = 0x0070u,   // 亮度减 

    CC_MEDIA_SELECT_COMPUTER    = 0x0088u,   // 选择媒体源 = 计算机 
    CC_MEDIA_SELECT_TV          = 0x0089u,   // 选择媒体源 = 电视 
    CC_MEDIA_SELECT_WWW         = 0x008Au,   // 选择媒体源 = 网页 / 网络媒体 
    CC_MEDIA_SELECT_DVD         = 0x008Bu,   // 选择媒体源 = DVD 
    CC_MEDIA_SELECT_MESSAGES    = 0x0090u,   // 选择媒体源 = 消息 / 短信 
    CC_MEDIA_SELECT_CD          = 0x0091u,   // 选择媒体源 = CD 
    CC_MEDIA_SELECT_HOME        = 0x009Au,   // 选择媒体源 = 家庭网络 / 家庭影院 
    CC_CHANNEL_UP               = 0x009Cu,   // 频道加（电视 / 收音机） 
    CC_CHANNEL_DOWN             = 0x009Du,   // 频道减 
    
    // 媒体控制段 (消费者页 0x00B0 起)
    CC_PLAY                     = 0x00B0u,   // 播放 
    CC_PAUSE                    = 0x00B1u,   // 暂停（单发，区别于 0xCD 播放/暂停） 
    CC_RECORD                   = 0x00B2u,   // 录制 
    CC_FAST_FORWARD             = 0x00B3u,   // 快进 
    CC_REWIND                   = 0x00B4u,   // 快退 
    CC_NEXT_TRACK               = 0x00B5u,   // 下一曲
    CC_PREV_TRACK               = 0x00B6u,   // 上一曲
    CC_STOP                     = 0x00B7u,   // 停止 
    CC_EJECT                    = 0x00B8u,   // 弹出（光盘等介质） 
    CC_RANDOM_PLAY              = 0x00B9u,   // 随机播放 
    CC_REPEAT                   = 0x00BCu,   // 循环 / 重复播放 
    CC_PLAY_PAUSE               = 0x00CDu,   // 播放 / 暂停 
    CC_PLAY_SKIP                = 0x00CEu,   // 快进一段 / 跳到下一个章节 
    
    CC_VOLUME                   = 0x00E0u,   // 绝对音量（需带逻辑量数据；键盘用 0xE9/0xEA 相对量） 
    CC_MUTE                     = 0x00E2u,   // 静音（切换） 
    CC_VOLUME_INC               = 0x00E9u,   // 音量加
    CC_VOLUME_DEC               = 0x00EAu,   // 音量减

    /* Application Launch Buttons (0x0180..0x01CB)：启动各类应用 */
    CC_AL_LAUNCH_BUTTON_CONFIG       = 0x0181u,   // 启动按钮配置（配置用途，不是"启动某程序"） 
    CC_AL_PROGRAMMABLE_BUTTON_CONFIG = 0x0182u,   // 可编程按钮配置 
    CC_AL_CONSUMER_CONTROL_CONFIG    = 0x0183u,   // 消费者控制配置 
    CC_AL_WORD_PROCESSOR             = 0x0184u,   // 文字处理（Word 类） 
    CC_AL_TEXT_EDITOR                = 0x0185u,   // 文本编辑器（记事本类） 
    CC_AL_SPREADSHEET                = 0x0186u,   // 电子表格（Excel 类） 
    CC_AL_GRAPHICS_EDITOR            = 0x0187u,   // 图形 / 图像编辑器 
    CC_AL_PRESENTATION               = 0x0188u,   // 演示文稿（PowerPoint 类） 
    CC_AL_DATABASE                   = 0x0189u,   // 数据库程序 
    CC_AL_EMAIL                      = 0x018Au,   // 邮件客户端（Windows 有响应） 
    CC_AL_NEWSREADER                 = 0x018Bu,   // 新闻阅读器 
    CC_AL_VOICEMAIL                  = 0x018Cu,   // 语音邮件 
    CC_AL_CONTACTS                   = 0x018Du,   // 联系人 / 通讯录 
    CC_AL_CALENDAR                   = 0x018Eu,   // 日历 
    CC_AL_TASK_MANAGER               = 0x018Fu,   // 任务管理器（待办事务，不是进程管理器） 
    CC_AL_LOG_JOURNAL                = 0x0190u,   // 日志 / 日记 
    CC_AL_FINANCE                    = 0x0191u,   // 财务软件 
    CC_AL_CALCULATOR                 = 0x0192u,   // 计算器（Windows 有响应） 
    CC_AL_AV_CAPTURE_PLAYBACK        = 0x0193u,   // 音视频采集 / 播放 
    CC_AL_LOCAL_MACHINE_BROWSER      = 0x0194u,   // 本机浏览器 = 资源管理器；实测 Windows 可能无反应 
    CC_AL_LAN_WAN_BROWSER            = 0x0195u,   // 局域网 / 广域网浏览器 
    CC_AL_INTERNET_BROWSER           = 0x0196u,   // 互联网浏览器（Windows 有响应，打开默认浏览器） 
    CC_AL_REMOTE_NETWORK_CONNECT     = 0x0197u,   // 远程网络连接 
    CC_AL_NETWORK_CONFERENCE         = 0x0198u,   // 网络会议 
    CC_AL_NETWORK_CHAT               = 0x0199u,   // 网络聊天 
    CC_AL_TELEPHONY_DIALER           = 0x019Au,   // 电话拨号器 
    CC_AL_LOGON                      = 0x019Bu,   // 登录 
    CC_AL_LOGOFF                     = 0x019Cu,   // 注销 
    CC_AL_LOGON_LOGOFF               = 0x019Du,   // 登录 / 注销（切换） 
    CC_AL_LOCK_SCREENSAVER           = 0x019Eu,   // 锁定屏幕 / 启动屏保 
    CC_AL_CONTROL_PANEL              = 0x019Fu,   // 控制面板（Windows 有响应） 
    CC_AL_RUN                        = 0x01A0u,   // 运行对话框（Win+R 语义） 
    CC_AL_PROCESS_TASK_MANAGER       = 0x01A1u,   // 进程 / 任务管理器 
    CC_AL_SELECT_TASK                = 0x01A2u,   // 选择任务（任务切换器） 
    CC_AL_NEXT_TASK                  = 0x01A3u,   // 下一个任务（Alt+Tab 语义，Windows 通常无响应） 
    CC_AL_PREVIOUS_TASK              = 0x01A4u,   // 上一个任务（Alt+Tab 语义）；实测无反应，建议改用 _FB 组合键 
    CC_AL_HELP_CENTER                = 0x01A6u,   // 帮助中心 
    CC_AL_DOCUMENTS                  = 0x01A7u,   // 文档文件夹 
    CC_AL_THESAURUS                  = 0x01A8u,   // 同义词词典 
    CC_AL_DICTIONARY                 = 0x01A9u,   // 词典 
    CC_AL_DESKTOP                    = 0x01AAu,   // 桌面（显示桌面） 
    CC_AL_SPELL_CHECK                = 0x01ABu,   // 拼写检查 
    CC_AL_GRAMMAR_CHECK              = 0x01ACu,   // 语法检查 
    CC_AL_WIRELESS_STATUS            = 0x01ADu,   // 无线状态 
    CC_AL_KEYBOARD_LAYOUT            = 0x01AEu,   // 键盘布局 
    CC_AL_VIRUS_PROTECTION           = 0x01AFu,   // 病毒防护 
    CC_AL_ENCRYPTION                 = 0x01B0u,   // 加密 
    CC_AL_SCREEN_SAVER               = 0x01B1u,   // 屏幕保护 
    CC_AL_ALARMS                     = 0x01B2u,   // 闹钟 / 提醒 
    CC_AL_CLOCK                      = 0x01B3u,   // 时钟 
    CC_AL_FILE_BROWSER               = 0x01B4u,   // 文件浏览器（资源管理器） 
    CC_AL_POWER_STATUS               = 0x01B5u,   // 电源状态 
    CC_AL_IMAGE_BROWSER              = 0x01B6u,   // 图片浏览器 
    CC_AL_AUDIO_BROWSER              = 0x01B7u,   // 音频浏览器 
    CC_AL_MOVIE_BROWSER              = 0x01B8u,   // 视频浏览器 
    CC_AL_DIGITAL_RIGHTS_MANAGER     = 0x01B9u,   // 数字版权管理（DRM） 
    CC_AL_DIGITAL_WALLET             = 0x01BAu,   // 数字钱包 
    CC_AL_INSTANT_MESSAGING          = 0x01BCu,   // 即时通讯（IM） 
    CC_AL_OEM_HELP                   = 0x01BEu,   // OEM 帮助 
    CC_AL_ONLINE_COMMUNITY           = 0x01BFu,   // 在线社区 
    CC_AL_ENTERTAINMENT_BROWSER      = 0x01C0u,   // 娱乐浏览器 
    CC_AL_ONLINE_SHOPPING_BROWSER    = 0x01C1u,   // 在线购物浏览器 
    CC_AL_SMARTCARD_HELP             = 0x01C2u,   // 智能卡帮助 
    CC_AL_FINANCE_BROWSER            = 0x01C3u,   // 金融浏览器 
    CC_AL_CORPORATE_NEWS_BROWSER     = 0x01C4u,   // 企业新闻浏览器 
    CC_AL_ONLINE_ACTIVITY_BROWSER    = 0x01C5u,   // 在线活动浏览器 
    CC_AL_RESEARCH_SEARCH_BROWSER    = 0x01C6u,   // 调研 / 搜索浏览器 
    CC_AL_AUDIO_PLAYER               = 0x01C7u,   // 音频播放器 
    CC_AL_MESSAGE_STATUS             = 0x01C8u,   // 消息状态 
    CC_AL_CONTACT_SYNC               = 0x01C9u,   // 联系人同步 
    CC_AL_NAVIGATION                 = 0x01CAu,   // 导航 
    CC_AL_DESKTOP_ASSISTANT          = 0x01CBu,   // 桌面助手 

    /* Generic GUI Application Controls: 0x0201..0x02B0 */
    CC_AC_NEW                        = 0x0201u,   // 新建 
    CC_AC_OPEN                       = 0x0202u,   // 打开 
    CC_AC_CLOSE                      = 0x0203u,   // 关闭 
    CC_AC_EXIT                       = 0x0204u,   // 退出 
    CC_AC_MAXIMIZE                   = 0x0205u,   // 最大化 
    CC_AC_MINIMIZE                   = 0x0206u,   // 最小化 
    CC_AC_SAVE                       = 0x0207u,   // 保存 
    CC_AC_PRINT                      = 0x0208u,   // 打印 
    CC_AC_PROPERTIES                 = 0x0209u,   // 属性 
    CC_AC_UNDO                       = 0x021Au,   // 撤销 
    CC_AC_COPY                       = 0x021Bu,   // 复制 
    CC_AC_CUT                        = 0x021Cu,   // 剪切 
    CC_AC_PASTE                      = 0x021Du,   // 粘贴 
    CC_AC_SELECT_ALL                 = 0x021Eu,   // 全选 
    CC_AC_FIND                       = 0x021Fu,   // 查找 
    CC_AC_FIND_REPLACE               = 0x0220u,   // 查找并替换 
    CC_AC_SEARCH                     = 0x0221u,   // 搜索 
    CC_AC_GO_TO                      = 0x0222u,   // 转到 
    CC_AC_HOME                       = 0x0223u,   // 浏览器主页（浏览器打开时有效） 
    CC_AC_BACK                       = 0x0224u,   // 后退 
    CC_AC_FORWARD                    = 0x0225u,   // 前进 
    CC_AC_STOP                       = 0x0226u,   // 停止（网页 / 媒体加载） 
    CC_AC_REFRESH                    = 0x0227u,   // 刷新 
    CC_AC_PREVIOUS_LINK              = 0x0228u,   // 上一个链接 
    CC_AC_NEXT_LINK                  = 0x0229u,   // 下一个链接 
    CC_AC_BOOKMARKS                  = 0x022Au,   // 书签 / 收藏夹 
    CC_AC_HISTORY                    = 0x022Bu,   // 历史记录 
    CC_AC_SUBSCRIPTIONS              = 0x022Cu,   // 订阅 
    CC_AC_ZOOM_IN                    = 0x022Du,   // 放大 
    CC_AC_ZOOM_OUT                   = 0x022Eu,   // 缩小 
    CC_AC_ZOOM                       = 0x022Fu,   // 缩放（绝对量） 
    CC_AC_FULLSCREEN                 = 0x0230u,   // 全屏 
    CC_AC_NORMAL_VIEW                = 0x0231u,   // 普通视图 
    CC_AC_VIEW_TOGGLE                = 0x0232u,   // 视图切换 
    CC_AC_SCROLL_UP                  = 0x0233u,   // 向上滚动 
    CC_AC_SCROLL_DOWN                = 0x0234u,   // 向下滚动 
    CC_AC_SCROLL                     = 0x0235u,   // 滚动（绝对量） 
    CC_AC_PAN_LEFT                   = 0x0236u,   // 向左平移 
    CC_AC_PAN_RIGHT                  = 0x0237u,   // 向右平移 
    CC_AC_PAN                        = 0x0238u,   // 平移（绝对量） 
    CC_AC_NEW_WINDOW                 = 0x0239u,   // 新建窗口 
    CC_AC_TILE_HORIZONTALLY          = 0x023Au,   // 水平平铺 
    CC_AC_TILE_VERTICALLY            = 0x023Bu,   // 垂直平铺 
    CC_AC_FORMAT                     = 0x023Cu,   // 格式化 
    CC_AC_EDIT                       = 0x023Du,   // 编辑 
    CC_AC_BOLD                       = 0x023Eu,   // 加粗 
    CC_AC_ITALICS                    = 0x023Fu,   // 斜体 
    CC_AC_UNDERLINE                  = 0x0240u,   // 下划线 
    CC_AC_STRIKETHROUGH              = 0x0241u,   // 删除线 
    CC_AC_SUBSCRIPT                  = 0x0242u,   // 下标 
    CC_AC_SUPERSCRIPT                = 0x0243u,   // 上标 
    CC_AC_ALL_CAPS                   = 0x0244u,   // 全部大写 
    CC_AC_ROTATE                     = 0x0245u,   // 旋转 
    CC_AC_RESIZE                     = 0x0246u,   // 调整大小 
    CC_AC_FLIP_HORIZONTAL            = 0x0247u,   // 水平翻转 
    CC_AC_FLIP_VERTICAL              = 0x0248u,   // 垂直翻转 
    CC_AC_MIRROR_HORIZONTAL          = 0x0249u,   // 水平镜像 
    CC_AC_MIRROR_VERTICAL            = 0x024Au,   // 垂直镜像 
    CC_AC_FONT_SELECT                = 0x024Bu,   // 选择字体 
    CC_AC_FONT_COLOR                 = 0x024Cu,   // 字体颜色 
    CC_AC_FONT_SIZE                  = 0x024Du,   // 字号 
    CC_AC_JUSTIFY_LEFT               = 0x024Eu,   // 左对齐 
    CC_AC_JUSTIFY_CENTER_H           = 0x024Fu,   // 水平居中 
    CC_AC_JUSTIFY_RIGHT              = 0x0250u,   // 右对齐 
    CC_AC_JUSTIFY_BLOCK_H            = 0x0251u,   // 水平两端对齐 
    CC_AC_JUSTIFY_TOP                = 0x0252u,   // 顶端对齐 
    CC_AC_JUSTIFY_CENTER_V           = 0x0253u,   // 垂直居中 
    CC_AC_JUSTIFY_BOTTOM             = 0x0254u,   // 底端对齐 
    CC_AC_JUSTIFY_BLOCK_V            = 0x0255u,   // 垂直两端对齐 
    CC_AC_INDENT_DECREASE            = 0x0256u,   // 减少缩进 
    CC_AC_INDENT_INCREASE            = 0x0257u,   // 增加缩进 
    CC_AC_NUMBERED_LIST              = 0x0258u,   // 编号列表 
    CC_AC_RESTART_NUMBERING          = 0x0259u,   // 重新编号 
    CC_AC_BULLETED_LIST              = 0x025Au,   // 项目符号列表 
    CC_AC_PROMOTE                    = 0x025Bu,   // 提升层级（列表升级） 
    CC_AC_DEMOTE                     = 0x025Cu,   // 降低层级（列表降级） 
    CC_AC_YES                        = 0x025Du,   // 是 
    CC_AC_NO                         = 0x025Eu,   // 否 
    CC_AC_CANCEL                     = 0x025Fu,   // 取消 
    CC_AC_CATALOG                    = 0x0260u,   // 目录 / 分类 
    CC_AC_BUY_CHECKOUT               = 0x0261u,   // 购买 / 结账 
    CC_AC_ADD_TO_CART                = 0x0262u,   // 加入购物车 
    CC_AC_EXPAND                     = 0x0263u,   // 展开 
    CC_AC_EXPAND_ALL                 = 0x0264u,   // 全部展开 
    CC_AC_COLLAPSE                   = 0x0265u,   // 折叠 
    CC_AC_COLLAPSE_ALL               = 0x0266u,   // 全部折叠 
    CC_AC_PRINT_PREVIEW              = 0x0267u,   // 打印预览 
    CC_AC_PASTE_SPECIAL              = 0x0268u,   // 选择性粘贴 
    CC_AC_INSERT_MODE                = 0x0269u,   // 插入模式 
    CC_AC_DELETE                     = 0x026Au,   // 删除 
    CC_AC_LOCK                       = 0x026Bu,   // 锁定 
    CC_AC_UNLOCK                     = 0x026Cu,   // 解锁 
    CC_AC_PROTECT                    = 0x026Du,   // 保护 
    CC_AC_UNPROTECT                  = 0x026Eu,   // 取消保护 
    CC_AC_ATTACH_COMMENT             = 0x026Fu,   // 添加批注 
    CC_AC_DELETE_COMMENT             = 0x0270u,   // 删除批注 
    CC_AC_VIEW_COMMENT               = 0x0271u,   // 查看批注 
    CC_AC_SELECT_WORD                = 0x0272u,   // 选择单词 
    CC_AC_SELECT_SENTENCE            = 0x0273u,   // 选择句子 
    CC_AC_SELECT_PARAGRAPH           = 0x0274u,   // 选择段落 
    CC_AC_SELECT_COLUMN              = 0x0275u,   // 选择列 
    CC_AC_SELECT_ROW                 = 0x0276u,   // 选择行 
    CC_AC_SELECT_TABLE               = 0x0277u,   // 选择表格 
    CC_AC_SELECT_OBJECT              = 0x0278u,   // 选择对象 
    CC_AC_REDO                       = 0x0279u,   // 重做（恢复撤销） 
    CC_AC_SORT                       = 0x027Au,   // 排序 
    CC_AC_SORT_ASCENDING             = 0x027Bu,   // 升序排序 
    CC_AC_SORT_DESCENDING            = 0x027Cu,   // 降序排序 
    CC_AC_FILTER                     = 0x027Du,   // 筛选 
    CC_AC_SET_CLOCK                  = 0x027Eu,   // 设置时钟 
    CC_AC_VIEW_CLOCK                 = 0x027Fu,   // 查看时钟 
    CC_AC_SELECT_TIME_ZONE           = 0x0280u,   // 选择时区 
    CC_AC_EDIT_TIME_ZONES            = 0x0281u,   // 编辑时区 
    CC_AC_SET_ALARM                  = 0x0282u,   // 设置闹钟 
    CC_AC_CLEAR_ALARM                = 0x0283u,   // 清除闹钟 
    CC_AC_SNOOZE_ALARM               = 0x0284u,   // 稍后提醒（贪睡） 
    CC_AC_RESET_ALARM                = 0x0285u,   // 重置闹钟 
    CC_AC_SYNCHRONIZE                = 0x0286u,   // 同步 
    CC_AC_SEND_RECEIVE               = 0x0287u,   // 发送 / 接收 
    CC_AC_SEND_TO                    = 0x0288u,   // 发送到 
    CC_AC_REPLY                      = 0x0289u,   // 回复 
    CC_AC_REPLY_ALL                  = 0x028Au,   // 全部回复 
    CC_AC_FORWARD_MSG                = 0x028Bu,   // 转发邮件 
    CC_AC_SEND                       = 0x028Cu,   // 发送 
    CC_AC_ATTACH_FILE                = 0x028Du,   // 添加附件 
    CC_AC_UPLOAD                     = 0x028Eu,   // 上传 
    CC_AC_DOWNLOAD                   = 0x028Fu,   // 下载 
    CC_AC_SET_BORDERS                = 0x0290u,   // 设置边框 
    CC_AC_INSERT_ROW                 = 0x0291u,   // 插入行 
    CC_AC_INSERT_COLUMN              = 0x0292u,   // 插入列 
    CC_AC_INSERT_FILE                = 0x0293u,   // 插入文件 
    CC_AC_INSERT_PICTURE             = 0x0294u,   // 插入图片 
    CC_AC_INSERT_OBJECT              = 0x0295u,   // 插入对象 
    CC_AC_INSERT_SYMBOL              = 0x0296u,   // 插入符号 
    CC_AC_SAVE_AND_CLOSE             = 0x0297u,   // 保存并关闭 
    CC_AC_RENAME                     = 0x0298u,   // 重命名 
    CC_AC_MERGE                      = 0x0299u,   // 合并 
    CC_AC_SPLIT                      = 0x029Au,   // 拆分 
    CC_AC_DISTRIBUTE_H               = 0x029Bu,   // 水平分布 
    CC_AC_DISTRIBUTE_V               = 0x029Cu,   // 垂直分布 
    CC_AC_NEXT_KEYBOARD_LAYOUT       = 0x029Du,   // 切换到下一个键盘布局 
    CC_AC_NAVIGATION_GUIDANCE        = 0x029Eu,   // 导航指引 
    CC_AC_DESKTOP_SHOW_ALL_WINDOWS   = 0x029Fu,   // 显示所有窗口（部分 Windows 版本有响应） 
    CC_AC_DESKTOP_SHOW_ALL_APPS      = 0x02A2u,   // 显示所有应用（开始菜单 / 应用列表） 
    CC_AC_SOFT_KEY_LEFT              = 0x02A0u,   // 软键左 
    CC_AC_SOFT_KEY_RIGHT             = 0x02A1u,   // 软键右 
    CC_AC_IDLE_KEEP_ALIVE            = 0x02B0u,   // 空闲保活（周期性发送，防止主机休眠） 
} cc_ty;

/**
 * @cite:   HID Usage Tables, https://www.usb.org/sites/default/files/hut1_7.pdf#page=33
 * @brief:  GenericDesktopPage(0x01), Usage: Mouse, 0x02
 *          Mouse button bits for the supplied report format
 */
enum {
    MS_BTN1 = (1u << 0),
    MS_BTN2 = (1u << 1),
    MS_BTN3 = (1u << 2),
    MS_BTN4 = (1u << 3),
    MS_BTN5 = (1u << 4),
    MS_BTN6 = (1u << 5),
    MS_BTN7 = (1u << 6),
    MS_BTN8 = (1u << 7)
} ;

/* Modifier usages E0..E7 map directly to the 8-bit modifier field. */
enum {
    KC_MOD_LCTRL  = (1u << 0),
    KC_MOD_LSHIFT = (1u << 1),
    KC_MOD_LALT   = (1u << 2),
    KC_MOD_LGUI   = (1u << 3),
    KC_MOD_RCTRL  = (1u << 4),
    KC_MOD_RSHIFT = (1u << 5),
    KC_MOD_RALT   = (1u << 6),
    KC_MOD_RGUI   = (1u << 7),
};

// key code type
enum { 
    KCT_KBD = 0x00, 
    KCT_SYS, 
    KCT_CSM, 
    KCT_ADV, 
    KCT_MS, 
    KCT_GP, 
    KCT_FN, 
    KCT_MAX 
};
// function customization type
enum { 
    FN_KEY = 0x00, 
    FN_LIGHT, 
    FN_BT, 
    FN_RADIO, 
    FN_DISP, 
    FN_SHCT, 
    FN_MAX 
};


// key code
typedef union {
    uint32_t code;
    struct __attribute__((packed)) {
        uint32_t lk: 1; // Lock
        uint32_t ty: 6; // Easy to search/query
        uint32_t rv: 25;
    };
    struct __attribute__((packed)) { uint32_t lk: 1; uint32_t ty: 5; uint32_t rv: 18; uint32_t co: 8;  } kbd; // Keyboard
    struct __attribute__((packed)) { uint32_t lk: 1; uint32_t ty: 5; uint32_t rv: 18; uint32_t co: 8;  } sys; // System
    struct __attribute__((packed)) { uint32_t lk: 1; uint32_t ty: 5; uint32_t rv: 10; uint32_t co: 16; } csm; // Consumer
    struct __attribute__((packed)) { uint32_t lk: 1; uint32_t ty: 5; uint32_t sy: 5;  uint32_t rv: 13; uint32_t co: 8;  } adv; // Advanced
    struct __attribute__((packed)) { uint32_t lk: 1; uint32_t ty: 5; uint32_t sy: 5;  uint32_t rv: 5;  uint32_t co: 16; } ms;  // Mouse
    struct __attribute__((packed)) { uint32_t lk: 1; uint32_t ty: 5; uint32_t sy: 5;  uint32_t rv: 5;  uint32_t co: 16; } gp;  // Gamepad
    struct __attribute__((packed)) { uint32_t lk: 1; uint32_t ty: 5; uint32_t sy: 5;  uint32_t fy: 5;  uint32_t co: 16; } fn;  // Function customization
} __attribute__((aligned(4))) rotkc_t;
_Static_assert(4 == sizeof(rotkc_t), "rotkc_t must be 4 bytes");


#define _K(c)       (((uint32_t)(c) << 24) | ((uint32_t)KCT_KBD << 1))  /* Keyboard */
#define _S(c)       (((uint32_t)(c) << 24) | ((uint32_t)KCT_SYS << 1))  /* System */
#define _C(c)       (((uint32_t)(c) << 16) | ((uint32_t)KCT_CSM << 1))  /* Consumer */
#define _A(c)       (((uint32_t)(c) << 24) | ((uint32_t)KCT_ADV << 1))  /* Advanced */
#define _M(c)       (((uint32_t)(c) << 16) | ((uint32_t)KCT_MS  << 1))  /* Mouse */
#define _G(c)       (((uint32_t)(c) << 16) | ((uint32_t)KCT_GP  << 1))  /* Gamepad */
#define _L(t, c)    (((uint32_t)(c) << 24) | ((uint32_t)(t) << 1) | 1u) /* lock */
#define ___         (_K(KC_UNDEFINED))                                  /* Keyboard Unvarnished transmission */

#define _FN(c)      (((uint32_t)(c) << 16) | ((uint32_t)FN_KEY   << 6) | ((uint32_t)KCT_FN << 1)) /* fn Key */
#define _FL(c)      (((uint32_t)(c) << 16) | ((uint32_t)FN_LIGHT << 6) | ((uint32_t)KCT_FN << 1)) /* fn Lights */
#define _FB(c)      (((uint32_t)(c) << 16) | ((uint32_t)FN_BT    << 6) | ((uint32_t)KCT_FN << 1)) /* fn Bluetooth */
#define _FR(c)      (((uint32_t)(c) << 16) | ((uint32_t)FN_RADIO << 6) | ((uint32_t)KCT_FN << 1)) /* fn Radio */
#define _FD(c)      (((uint32_t)(c) << 16) | ((uint32_t)FN_DISP  << 6) | ((uint32_t)KCT_FN << 1)) /* fn Display */
#define _FS(c)      (((uint32_t)(c) << 16) | ((uint32_t)FN_SHCT  << 6) | ((uint32_t)KCT_FN << 1)) /* fn Shortcut */



#ifdef __cplusplus
}
#endif

#endif
