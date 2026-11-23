# Rot\-CC 协议规范

# 背景

---

# 修订说明

|撰写人|协议版本|更新时间|修改内容|说明|
|---|---|---|---|---|
|rot|0x01|2026/11/15|初始定义|基于 [Rot\-Gen 协议规范](https://icnoerdzc2ph.feishu.cn/docx/E0GldSw7KowwbzxeGvrc3QP6nzc) 定义与每个产品的通信协议|
||||||

---

# 数据结构定义

|cmd|subcmd|reserve|payload|explain|
|---|---|---|---|---|
|1 byte|1 byte|2 byte|||

---

# 系统信息

### 设备握手

|cmd|subcmd|reserve|payload|explain|
|---|---|---|---|---|
|0x00|0x01|0x0000||0\-3 byte: Rot\-CC 协议版本|
|0x00|0x01|0x0000|0\-3 byte|0x00: succeed|

### 获取设备信息

|cmd|subcmd|reserve|payload|explain|
|---|---|---|---|---|
|0x00|0x02|0x0000|||
|0x00|0x02|0x0000|0\-60 byte|<br>```C++<br>struct {<br>    uint16_t vid;<br>    uint16_t pid;<br>    uint8_t  pn[16];<br>    uint8_t  sn[16];<br>    uint64_t fw_ver;<br>    uint8_t  hw_ver;<br>    uint8_t  cd[16]; // 构建时间 YYYYMMDDHHMMSS<br>};<br>```|

### 重启

|cmd|subcmd|reserve|payload|explain|
|---|---|---|---|---|
|0x00|0x03|0x0000|||
|0x00|0x03|0x0000|0\-3 byte|0x00: succeed|

### 重置

|cmd|subcmd|reserve|payload|explain|
|---|---|---|---|---|
|0x00|0x04|0x0000|0\-3 byte|注：每个位都可单独恢复相应模块；0xFFFFFFFF则全局恢复出厂：<br>```C++<br>union {<br>    uint32_t options;<br>    struct {<br>        uint32_t bit_kc : 1;<br>        uint32_t bit_rgb : 1;<br>        uint32_t bit_adv_mt : 1;<br>    };<br>};<br>```|
|0x00|0x04|0x0000|0\-3 byte|0x00: succeed|

### 获取按键数量

|cmd|subcmd|reserve|payload|explain|
|---|---|---|---|---|
|0x00|0x05|0x0000|||
|0x00|0x05|0x0000|0\-3 byte|返回实际键盘的按键数量|

### 获取是否支持驱动弹窗

|cmd|subcmd|reserve|payload|explain|
|---|---|---|---|---|
|0x00|0x06|0x0000|||
|0x00|0x06|0x0000|0\-3 byte|返回是否支持驱动弹窗|

### 获取驱动弹窗

|cmd|subcmd|reserve|payload|explain|
|---|---|---|---|---|
|0x00|0x07|0x0000|||
|0x00|0x07|0x0000|0\-3 byte|获取驱动弹窗状态|

### 设置驱动弹窗

|cmd|subcmd|reserve|payload|explain|
|---|---|---|---|---|
|0x00|0x08|0x0000|0\-3 byte|设置驱动弹窗|
|0x00|0x08|0x0000|0\-3 byte|0x00: succeed|



---

# 升级  

### 升级信息

|cmd|subcmd|reserve|payload|explain|
|---|---|---|---|---|
|0x01|0x00|0x0000|||
|0x01|0x00|0x0000||```C++<br>struct {<br>    uint8_t  slot_act;          // 运行槽<br>    uint8_t  slot_tag;          // 下次升级目标槽<br>    uint8_t  slot_cnt;          // 槽数量<br>    uint8_t  encrypted;         // OTP EXIP<br>    uint8_t  remap_enabled;     // ROM remap<br>    uint8_t  boot_ota_enabled;  // OTP SEC_IMG_OFFSET 已烧录<br>    uint8_t  otp_address_match; // OTP 地址匹配 APP2<br>    uint8_t  otp_length_match;  // OTP 长度覆盖一个槽<br>    uint8_t  slot_valid[2];     // 槽内 Boot Header 有效<br>    uint8_t  slot_retired[2];   // SW_VERSION 已清 0<br>    uint16_t slot_version[2];   // 每槽 Boot SW_VERSION<br>    uint32_t slot_size;         // 槽大小<br>    uint32_t image_base_offset; // 镜像在槽内的起点<br>    uint32_t file_boot_offset;  // Boot Header 在镜像内的偏移<br>    uint32_t otp_offset;        // OTP 地址，字节<br>    uint32_t otp_size;          // OTP 长度，字节<br>    uint16_t otp_sec_img_offset;// OTP word25[15:0]<br>    uint16_t otp_max_img_len;   // OTP word25[31:16]<br>    uint64_t firmware_version;  // 当前运行固件版本<br>};<br>```|

### 请求升级

|cmd|subcmd|reserve|payload|explain|
|---|---|---|---|---|
|0x01<br>|0x01<br>|0x0000||```C++<br>struct {<br>    uint32_t req_ty;   // 升级请求类型，1：开始，2：停止，3：完成<br>    uint32_t fw_size;  // 固件大小<br>    uint32_t fw_crc32; // 固件包crc32校验<br>    uint8_t  src_addr; // 源地址<br>    uint8_t  dst_addr; // 目的地址<br>};<br>```|
|0x01|0x01|0x0000|0\-3 byte|0x00: succeed|

### 传输数据

|cmd|subcmd|reserve|payload|explain|
|---|---|---|---|---|
|0x01|0x02|0x0000|||
|0x01|0x02|0x0000||固件数据传输|

---

# 键码

### 获取键码

|cmd|subcmd|reserve|payload|explain|
|---|---|---|---|---|
|0x02|0x00|0x0000|0\-3 byte|byte0: start key id<br>byte1: end key id<br>byte2: rsv<br>byte3: rsv|
|0x02|0x00|0x0000|`kc_t` byte|各类键码结构定义，由4字节组成：<br>```C++<br>// key code type<br>enum { KCT_KBD = 0x00, KCT_SYS, KCT_CSM, KCT_ADV, KCT_MS, KCT_GP, KCT_FN, KCT_MAX };<br>// function customization type<br>enum { FN_UT = 0x00, FN_RGB, FN_BT, FN_RADIO, FN_DISP, FN_SHCT, FN_MAX };<br>// key code<br>union __attribute__((aligned(4))) {<br>    uint32_t code;<br>    struct __attribute__((packed)) {<br>        uint32_t lk: 1; // Lock<br>        uint32_t ty: 6; // Easy to search/query<br>        uint32_t rv: 25;<br>    };<br>    struct __attribute__((packed)) { uint32_t lk: 1; uint32_t ty: 5; uint32_t rv: 18; uint32_t co: 8;  } kbd; // Keyboard<br>    struct __attribute__((packed)) { uint32_t lk: 1; uint32_t ty: 5; uint32_t rv: 18; uint32_t co: 8;  } sys; // System<br>    struct __attribute__((packed)) { uint32_t lk: 1; uint32_t ty: 5; uint32_t rv: 10; uint32_t co: 16; } csm; // Consumer<br>    struct __attribute__((packed)) { uint32_t lk: 1; uint32_t ty: 5; uint32_t sy: 5;  uint32_t rv: 13; uint32_t co: 8;  } adv; // Advanced<br>    struct __attribute__((packed)) { uint32_t lk: 1; uint32_t ty: 5; uint32_t sy: 5;  uint32_t rv: 5;  uint32_t co: 16; } ms;  // Mouse<br>    struct __attribute__((packed)) { uint32_t lk: 1; uint32_t ty: 5; uint32_t sy: 5;  uint32_t rv: 5;  uint32_t co: 16; } gp;  // Gamepad<br>    struct __attribute__((packed)) { uint32_t lk: 1; uint32_t ty: 5; uint32_t sy: 5;  uint32_t fy: 5;  uint32_t co: 16; } fn;  // Function customization<br>};<br>```<br>|

### 设置键码

|cmd|subcmd|reserve|payload|explain|
|---|---|---|---|---|
|0x02|0x01|0x0000||byte0: start key id<br>byte1: end key id<br>byte2: 按键参数 `kc_t` |
|0x02|0x01|0x0000|0\-3 byte|0x00: succeed|

### 获取映射层数

|cmd|subcmd|reserve|payload|explain|
|---|---|---|---|---|
|0x02|0x02|0x0000|||
|0x02|0x02|0x0000|0\-3 byte|返回最大支持的映射层数|

### 获取活动映射层

|cmd|subcmd|reserve|payload|explain|
|---|---|---|---|---|
|0x02|0x03|0x0000|||
|0x02|0x03|0x0000|0\-3 byte|返回当前活动层|

### 设置活动映射层

|cmd|subcmd|reserve|payload|explain|
|---|---|---|---|---|
|0x02|0x04|0x0000|0\-3 byte|当前活动层|
|0x02|0x04|0x0000|0\-3 byte|0x00: succeed|

### 获取按键tick参数

|cmd|subcmd|reserve|payload|explain|
|---|---|---|---|---|
|0x02|0x05|0x0000|0\-3 byte|byte0: start key id<br>byte1: end key id<br>byte2: rsv<br>byte3: rsv|
|0x02|0x05|0x0000|0\-7 byte|```C++<br>struct {<br>    uint16_t sclt;<br>    uint16_t sclto;<br>    uint16_t dclt;<br>    uint16_t dclto;<br>} act_t; // unit: ms<br>```|

### 设置按键tick参数

|cmd|subcmd|reserve|payload|explain|
|---|---|---|---|---|
|0x02|0x06|0x0000||byte0: start key id<br>byte1: end key id<br>byte2: 按键参数 `act_t`|
|0x02|0x06|0x0000|0\-7 byte||

### 获取触发模式

|cmd|subcmd|reserve|payload|explain|
|---|---|---|---|---|
|0x02|0x07|0x0000|0\-3 byte|byte0: start key id<br>byte1: end key id<br>byte2: rsv<br>byte3: rsv|
|0x02|0x07|0x0000|0\-3 byte|```C++<br>typedef enum {<br>    KTRG_TY_NOM = 0,<br>    KTRG_TY_RT,<br>    KTRG_TY_RT_CTN,<br>} ktrg_ty;<br>```|

### 设置触发模式

|cmd|subcmd|reserve|payload|explain|
|---|---|---|---|---|
|0x02|0x08|0x0000||byte0: start key id<br>byte1: end key id<br>byte2: 按键参数 `ktrg_ty`|
|0x02|0x08|0x0000|0\-3 byte|0x00: succeed|

### 获取死区参数

|cmd|subcmd|reserve|payload|explain|
|---|---|---|---|---|
|0x02|0x09|0x0000|0\-3 byte|byte0: start key id<br>byte1: end key id<br>byte2: rsv<br>byte3: rsv|
|0x02|0x09|0x0000|0\-3 byte|```C++<br>struct {<br>    uint16_t dst_t;     /* top dead zero dist */<br>    uint16_t dst_b;     /* bot dead zero dist */<br>} dz_t;<br>```|

### 设置死区参数

|cmd|subcmd|reserve|payload|explain|
|---|---|---|---|---|
|0x02|0x0a|0x0000||byte0: start key id<br>byte1: end key id<br>byte2: 按键参数 `dz_t`|
|0x02|0x0a|0x0000|0\-3 byte|0x00: succeed|

### 获取常规触发参数

|cmd|subcmd|reserve|payload|explain|
|---|---|---|---|---|
|0x02|0x0b|0x0000|0\-3 byte|byte0: start key id<br>byte1: end key id<br>byte2: rsv<br>byte3: rsv|
|0x02|0x0b|0x0000|0\-3 byte|```C++<br>struct {<br>    uint16_t dst_p;      /* normal press dist */<br>    uint16_t dst_r;      /* normal raise dist */<br>} nom_t;<br>```|

### 设置常规触发参数

|cmd|subcmd|reserve|payload|explain|
|---|---|---|---|---|
|0x02|0x0c|0x0000||byte0: start key id<br>byte1: end key id<br>byte2: 按键参数 `nom_t`|
|0x02|0x0c|0x0000|0\-3 byte|0x00: succeed|

### 获取快速触发参数

|cmd|subcmd|reserve|payload|explain|
|---|---|---|---|---|
|0x02|0x0d|0x0000|0\-3 byte|byte0: start key id<br>byte1: end key id<br>byte2: rsv<br>byte3: rsv|
|0x02|0x0d|0x0000|0\-5 byte|```C++<br>struct __attribute__((packed)) {<br>    uint16_t dst_p;     /* rapid trigger sensitivity press dist */<br>    uint16_t dst_r;     /* rapid trigger sensitivity raise dist */<br>} rt_t;<br>```|

### 设置快速触发参数

|cmd|subcmd|reserve|payload|explain|
|---|---|---|---|---|
|0x02|0x0e|0x0000||byte0: start key id<br>byte1: end key id<br>byte2: 按键参数 `rt_t`|
|0x02|0x0e|0x0000|0\-3 byte|0x00: succeed|



---

# 轴体

### 获取键盘支持的轴体

|cmd|subcmd|reserve|payload|explain|
|---|---|---|---|---|
|0x03|0x00|0x0000|||
|0x03|0x00|0x0000|n byte|返回键盘所支持的轴体（每个轴体占1byte）|

### 获取按键轴体类型

|cmd|subcmd|reserve|payload|explain|
|---|---|---|---|---|
|0x03|0x01|0x0000|0\-3 byte|byte0: start key id<br>byte1: end key id<br>byte2: rsv<br>byte3: rsv|
|0x03|0x01|0x0000|0\-3 byte|```C++<br>typedef enum {<br>    KAXIS_BB = 0,<br>    KAXIS_GDL,<br>    KAXIS_TTC,<br>    KAXIS_MAX,<br>} axis_ty;<br>```|

### 设置按键轴体类型

|cmd|subcmd|reserve|payload|explain|
|---|---|---|---|---|
|0x03|0x02|0x0000||byte0: start key id<br>byte1: end key id<br>byte2: 按键参数 `axis_ty`|
|0x03|0x02|0x0000|0\-3 byte|0x00: succeed|



---

# 校准

### 获取校准状态

|cmd|subcmd|reserve|payload|explain|
|---|---|---|---|---|
|0x04|0x00|0x0000|||
|0x04|0x00|0x0000||```C++<br>typedef struct {<br>    /** 0x80：停止校准<br>     *  0x81：开始校准<br>     *  0x82：校准完成 <br>     */<br>    uint8_t cali_sta;<br>    uint8_t rsv;<br>} calib_t;<br>```|

### 设置校准状态

|cmd|subcmd|reserve|payload|explain|
|---|---|---|---|---|
|0x04|0x01|0x0000||设置校准状态 `calib_t`|
|0x04|0x01|0x0000|0\-3 byte|0x00: succeed|

### 获取自校准

|cmd|subcmd|reserve|payload|explain|
|---|---|---|---|---|
|0x04|0x02|0x0000|||
|0x04|0x02|0x0000||```C++<br>typedef struct {<br>    // 0：关闭自校准； 1：打开自校准<br>    uint8_t auto_calib;<br>    uint8_t rsv;<br>} auto_calib_t;<br>```|

### 设置自校准

|cmd|subcmd|reserve|payload|explain|
|---|---|---|---|---|
|0x04|0x03|0x0000||设置自校准 `auto_calib_t`|
|0x04|0x03|0x0000|0\-3 byte|0x00: succeed|



---

# 高级键

---

# 灯光

---

# 工厂

---

# 调试

---



