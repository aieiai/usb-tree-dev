# Rot 协议分层

正式规范见 `doc/Rot-Gen 协议规范.md` 与 `doc/Rot-CC 协议规范.md`。本目录实现三层：

- `rotgen_proto.c`：通用协议层。只负责 Rot-Gen 帧的组帧、CRC8/CRC16、
  流式拆包与重同步，收发都通过回调交给上层，不依赖 USB。
- `../cc/rotcc.c`：CC 传输与收发任务。OUT 中断只把字节写进环形缓冲并唤醒
  CC 线程；线程把字节喂给协议层。发送没有队列，直接交给 IN 端点。
- `rotcc_proto.c`：Rot-CC 设备协议解析。解析 `cmd/subcmd/reserve/payload`，
  分发到系统信息、键码和升级处理。

## Rot-Gen 帧

| 偏移 | 长度 | 内容 |
| --- | --- | --- |
| 0 | 1 | sof `'R'`（0x52） |
| 1 | 1 | 协议版本，当前 1 |
| 2 | 1 | options，bit3 为请求/响应 |
| 3 | 1 | 源地址 |
| 4 | 1 | 目的地址 |
| 5 | 2 | payload 长度（小端） |
| 7 | 2 | secret_key，保留 |
| 9 | 1 | header CRC8（覆盖 0..8，poly 0x07） |
| 10 | 2 | package CRC16（覆盖 0..9 与 payload，poly 0x1021） |
| 12 | N | payload |

地址：0 控制中心（上位机），1 键盘，2 鼠标，3 Hub，4 耳机。

## Rot-CC payload

`cmd(1) subcmd(1) reserve(2) payload(n)`。响应与请求使用相同的 cmd/subcmd，
并清除 options 的请求位；状态类响应的 payload 为 4 字节 `rotsta_t`，
`0` 表示成功。

| cmd | 名称 | subcmd |
| --- | --- | --- |
| 0x00 | 系统信息 | 1 握手（响应协议版本）、2 设备信息、3 重启、4 重置、5 按键数量 |
| 0x01 | 升级 | 0 升级信息、1 请求升级、2 传输数据 |
| 0x02 | 键码 | 0 获取、1 设置、2 映射层数、3 活动层、4 设置活动层、5/6 tick、7/8 触发模式、9/a 死区、b/c 常规触发、d/e 快速触发 |
| 0x03 | 轴体 | 0 支持列表、1 获取按键轴体、2 设置按键轴体 |
| 0x04 | 校准 | 0/1 校准状态、2/3 自校准、4 sensor 上报开关、5 sensor 上报频率 |
| 0x05 | 通知 | 0 异常状态、1 sensor 数据（设备主动上报，无需应答） |

按键参数命令（0x02 的 0~1、5~14 号子命令与 0x03/1、0x03/2）使用按键范围：

- 查询：payload 为 `{u8 start; u8 end; u8 rsv; u8 rsv}`，响应为
  `(end - start + 1)` 个条目顺序拼接。
- 设置：payload 为 `{u8 start; u8 end; 条目}`，同一个条目应用到整个范围。
- 按键索引必须满足 `start <= end < KBD_KEY_MAX`，长度不符返回参数错误。

| 子命令 | 条目长度 | 条目内容 |
| --- | --- | --- |
| 0x02/0x00 获取键码 | 4 | `rotkc_t`，层号取 `reserve` 高字节 |
| 0x02/0x01 设置键码 | 4 | `rotkc_t`，层号取 `reserve` 高字节 |
| 0x02/0x05 获取 tick | 8 | `{u16 sclt; u16 sclto; u16 dclt; u16 dclto}`（ms） |
| 0x02/0x06 设置 tick | 8 | `{u16 sclt; u16 sclto; u16 dclt; u16 dclto}`（ms） |
| 0x02/0x07 触发模式 | 4 | `u32 ktrg_ty` |
| 0x02/0x09 死区 | 4 | `{u16 dst_t; u16 dst_b}` |
| 0x02/0x0b 常规触发 | 4 | `{u16 dst_p; u16 dst_r}` |
| 0x02/0x0d 快速触发 | 4 | `{u16 dst_p; u16 dst_r}` |
| 0x03/0x01 获取按键轴体 | 4 | `u32 kaxis_ty` |
| 0x03/0x02 设置按键轴体 | 4 | `u32 kaxis_ty`，必须是支持列表中的值 |

轴体支持列表（0x03/0x00）响应为每个支持轴体 1 字节。

校准命令的 payload 都是 2 字节 `{u8 值; u8 rsv}`。设置状态时 `0x81` 开始采样，
`0x82` 完成并保存量程，`0x80` 停止并放弃本次采样；查询返回当前状态
（`0x80` 停止、`0x81` 校准中、`0x82` 完成）。自校准值为 `0`/`1`。
完成校准时只有量程（`top-bot`）达到 ADC 满量程 1/8 的按键才会写入，
没按过的按键只有噪声、保留原校准值。

sensor 上报由校准线程负责：`0x04/0x04` 的 `{u8 en; u8 rsv}` 打开或关闭，
`0x04/0x05` 的 `{u16 freq; u16 rsv}` 设置频率（1~1000 Hz，默认 50 Hz）。
打开后每个周期把所有按键按 key id 顺序紧凑打包进 `0x05/0x01` 通知，
payload 为连续的 `{u8 ki; u8 trg_sta; u16 val; u16 dist}` 条目（`trg_sta` 1 按下、
0 释放，`val` 为 ADC 值，`dist` 为行程 um）。一帧最多容纳
`(CC 端点 MPS - 12 - 4) / 6` 条，装不下时
拆成多帧连续发送，最后一帧携带剩余条目。上报周期按毫秒计时，与手动校准
是否开启无关；校准线程在两者都不需要时才挂起。

升级命令：

- INFO：payload 空，响应 48 字节 `upgrade_info_t`（packed、小端）：
  ```c
  struct {
      uint8_t  slot_act;          /* 运行槽 */
      uint8_t  slot_tag;          /* 下次升级目标槽 */
      uint8_t  slot_count;
      uint8_t  encrypted;         /* OTP EXIP */
      uint8_t  remap_enabled;     /* ROM remap */
      uint8_t  boot_ota_enabled;  /* OTP SEC_IMG_OFFSET 已烧录 */
      uint8_t  otp_address_match; /* OTP 地址匹配 APP2 */
      uint8_t  otp_length_match;  /* OTP 长度覆盖一个槽 */
      uint8_t  slot_valid[2];
      uint8_t  slot_retired[2];
      uint16_t slot_version[2];   /* 每个槽的 Boot SW_VERSION */
      uint32_t slot_size;
      uint32_t image_base_offset;
      uint32_t file_boot_offset;
      uint32_t otp_offset;        /* OTP 地址，字节 */
      uint32_t otp_size;          /* OTP 长度，字节 */
      uint16_t otp_sec_img_offset;/* OTP word25[15:0] 原始值 */
      uint16_t otp_max_img_len;   /* OTP word25[31:16] 原始值 */
      uint64_t firmware_version;  /* 当前运行固件版本 */
  };
  ```
- REQUEST：payload 为 `{u32 req_ty; u32 fw_size; u32 fw_crc32; u8 src_addr; u8 dst_addr}`
  （14 字节），`req_ty` 1 开始、2 停止、3 完成。
- DATA：payload 为固件数据切片。没有序号，逐帧状态响应即流控；DATA 帧
  不要重发，否则会重复写入。

设备端不再有独立的升级任务：CC 线程解析到升级命令后直接调用
`upgrade_*`，`DATA` 每帧等待主机读取响应。单帧镜像数据上限为
`端点 MPS - 12(Rot-Gen) - 4(Rot-CC)` 字节，1024 端点即 1008 字节。

## 上位机

浏览器实现位于 `tests/cc/rotcc-web/index.html`：连接后按设备上报的按键数量
自动切换 42/68/80 键配列；按键映射页读写键码，按键参数页在键盘上选择键位后
自动读取 tick、触发模式、死区、常规触发与快速触发，修改表单即自动写入所选
键位；轴体页可选择键位读取/设置轴体；校准页控制校准状态、
自校准以及 sensor 上报开关与频率，键帽实时显示各键行程，选中按键后显示最近
5 秒的 dist 曲线（纵轴固定 0~4 mm，曲线按下蓝色、释放粉红色，
状态切换处按下绿点、释放红点）；
另有升级与系统页。升级直接选择原始固件 bin，CRC-32 与大小在 START 请求中下发。

协议自测：`node tests/cc/rotcc-web/tests/protocol_test.js`。
