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
| 0x00 | 系统信息 | 0 版本、1 握手、2 设备信息、3 重启、4 重置 |
| 0x01 | 键码 | 0 获取、1 设置 |
| 0x02 | 升级 | 0 QUERY、1 START、2 DATA、3 END、4 ABORT |

键码命令用 `reserve` 指定位置：低字节为按键索引，高字节为层号。获取时
payload 为空、响应为 4 字节 `rotkc_t`；设置时 payload 为 4 字节 `rotkc_t`。

升级命令：

- QUERY：payload 空，响应为设备槽位/版本信息（28 字节，首字节为信息版本 4）。
- START：payload 为 `file_size(4) + session_id(4)`，开始擦写目标槽。
- DATA：payload 为 `seq(2) + 镜像数据`，seq 从 0 递增；重发同一 seq 直接回
  成功，用于响应丢失后的重试。
- END：payload 空，校验并提交镜像，成功响应后复位。
- ABORT：payload 空，取消会话并复位。

设备端不再有独立的升级任务：CC 线程解析到升级命令后直接调用
`upgrade_*`，`START/DATA` 每帧等待主机读取响应，相当于天然的流控。
单帧镜像数据上限为 `端点 MPS - 12(Rot-Gen) - 4(Rot-CC) - 2(seq)`
字节，1024 端点即 1006 字节。

## 上位机

浏览器实现位于 `tests/cc/web/webusb_dt.html`，已按本协议重写：可查询设备信息
与槽位信息、读写键码、执行固件升级。
