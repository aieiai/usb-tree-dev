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
| 0x02 | 键码 | 0 获取、1 设置、2 映射层数、3 活动层、4 设置活动层 |

键码命令用 `reserve` 指定位置：低字节为按键索引，高字节为层号。获取时
payload 为空、响应为 4 字节 `rotkc_t`；设置时 payload 为 4 字节 `rotkc_t`。

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

浏览器实现位于 `tests/cc/rotcc-web/rotcc.html`：可查询设备信息与升级信息、
读写键码、切换活动层、执行固件升级。升级直接选择原始固件 bin，CRC-32 与
大小在 START 请求中下发。
