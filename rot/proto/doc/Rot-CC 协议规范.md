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

### 获取 Rot\-CC 协议版本

|cmd|subcmd|reserve|payload|explain|
|---|---|---|---|---|
|0x00|0x00|0x0000|||
|0x00|0x00|0x0000|0\-3 byte||

### 设备握手

|cmd|subcmd|reserve|payload|explain|
|---|---|---|---|---|
|0x00|0x01|0x0000|||
|0x00|0x01|0x0000|||

### 获取设备信息

|cmd|subcmd|reserve|payload|explain|
|---|---|---|---|---|
|0x00|0x02|0x0000|||
|0x00|0x02|0x0000|0\-49 byte|<br>```C++<br>struct {<br>    uint16_t vid;<br>    uint16_t pid;<br>    uint8_t  pn[16];<br>    uint8_t  sn[16];<br>    uint64_t fw_ver;<br>    uint8_t  hw_ver;<br>};<br>```|

### 重启

|cmd|subcmd|reserve|payload|explain|
|---|---|---|---|---|
|0x00|0x03|0x0000|||
|0x00|0x03|0x0000|||

### 重置

|cmd|subcmd|reserve|payload|explain|
|---|---|---|---|---|
|0x00|0x04|0x0000||由4字节组成，每个位都可单独恢复相应模块；0xFFFFFFFF则全局恢复出厂：<br>```C++<br>union {<br>    uint32_t options;<br>    struct {<br>        uint32_t bit_kc : 1;<br>        uint32_t bit_rgb : 1;<br>        uint32_t bit_adv_mt : 1;<br>    };<br>};<br>```|
|0x00|0x04|0x0000|0\-3 byte|0x00000000: succeed|

### 获取按键数量

|cmd|subcmd|reserve|payload|explain|
|---|---|---|---|---|
|0x00|0x04|0x0000||由4字节组成，每个位都可单独恢复相应模块；0xFFFFFFFF则全局恢复出厂：<br>```C++<br>union {<br>    uint32_t options;<br>    struct {<br>        uint32_t bit_kc : 1;<br>        uint32_t bit_rgb : 1;<br>        uint32_t bit_adv_mt : 1;<br>    };<br>};<br>```|
|0x00|0x04|0x0000|0\-3 byte|0x00000000: succeed|



---

# 升级  



# 键码

### 获取键码

|cmd|subcmd|reserve|payload|explain|
|---|---|---|---|---|
|0x01|0x00|0x0000|||
|0x01|0x00|0x0000|`kc_t` byte|各类键码结构定义，由4字节组成：<br>```C++<br>// key code type<br>enum { KCT_KBD = 0x00, KCT_SYS, KCT_CSM, KCT_ADV, KCT_MS, KCT_GP, KCT_FN, KCT_MAX };<br>// function customization type<br>enum { FN_UT = 0x00, FN_RGB, FN_BT, FN_RADIO, FN_DISP, FN_SHCT, FN_MAX };<br>// key code<br>union __attribute__((aligned(4))) {<br>    uint32_t code;<br>    struct __attribute__((packed)) {<br>        uint32_t lk: 1; // Lock<br>        uint32_t ty: 6; // Easy to search/query<br>        uint32_t rv: 25;<br>    };<br>    struct __attribute__((packed)) { uint32_t lk: 1; uint32_t ty: 5; uint32_t rv: 18; uint32_t co: 8;  } kbd; // Keyboard<br>    struct __attribute__((packed)) { uint32_t lk: 1; uint32_t ty: 5; uint32_t rv: 18; uint32_t co: 8;  } sys; // System<br>    struct __attribute__((packed)) { uint32_t lk: 1; uint32_t ty: 5; uint32_t rv: 10; uint32_t co: 16; } csm; // Consumer<br>    struct __attribute__((packed)) { uint32_t lk: 1; uint32_t ty: 5; uint32_t sy: 5;  uint32_t rv: 13; uint32_t co: 8;  } adv; // Advanced<br>    struct __attribute__((packed)) { uint32_t lk: 1; uint32_t ty: 5; uint32_t sy: 5;  uint32_t rv: 5;  uint32_t co: 16; } ms;  // Mouse<br>    struct __attribute__((packed)) { uint32_t lk: 1; uint32_t ty: 5; uint32_t sy: 5;  uint32_t rv: 5;  uint32_t co: 16; } gp;  // Gamepad<br>    struct __attribute__((packed)) { uint32_t lk: 1; uint32_t ty: 5; uint32_t sy: 5;  uint32_t fy: 5;  uint32_t co: 16; } fn;  // Function customization<br>};<br>```<br>|

### 设置键码

|cmd|subcmd|reserve|payload|explain|
|---|---|---|---|---|
|0x01|0x01|0x0000|`kc_t` byte|格式见 `rotkc_t`|
|0x01|0x01|0x0000|0\-3 byte|0x00000000: succeed|



# 高级键

---

# 校准

---

# 灯光

---

# 工厂

---

# 调试

---



