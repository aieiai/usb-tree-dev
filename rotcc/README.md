# CC 固件升级协议

固件传输只使用 CC Interface 1：

- 主机到设备：OUT `0x02`
- 设备到主机：IN `0x82`
- 高速 USB 下默认使用 1024 字节中断端点
- 可用编译定义 `CONFIG_CC_EP_SIZE=64/128/512/1024` 覆盖高速端点大小
- 上位机从端点描述符读取 IN/OUT 的 `packetSize`，按两者较小值自适应
- 支持 64/128/512/1024 等 2 的幂端点；DATA 载荷为 `MPS - 10`
- DT Interface 3 不参与升级协议，只承载日志

## 模块分层

- `rotcc.c`：CC USB 队列、收发事件和 HAL 适配，不处理升级业务。
- `rotcc_protocol.c`：协议帧解析、ACK/NAK 编码和 CRC 校验。
- `rotcc_upgrade.c`：只把合法协议帧转换成升级任务命令。
- `upgrade_task.c`：独立高优先级 `thread_upgrade`，负责会话状态、Flash
  擦写、镜像校验和最终响应。
- `upgrade.c`：纯升级领域操作，不依赖 USB 或 ThreadX。

升级命令体最大约 1 KiB，超过 ThreadX `TX_QUEUE` 单消息 16 个 `ULONG`
的限制。因此命令体由 `TX_BLOCK_POOL` 管理，`TX_QUEUE` 只传递块指针。

升级操作以重启结束：成功、写入失败、校验失败或主机取消时，先排队发送
最终 ACK/NAK 并等待发送空闲，然后复位设备。因此不需要挂起或恢复其他任务。

## 设备信息查询

上位机发送 `QUERY(0x20)`，设备由升级任务返回 `INFO(0x82)`：

- 当前 BootROM remap 槽位（APP1 / APP2）
- 编译时固件版本 `ROT_VER`
- 当前槽 Boot Header 中的 `SW_VERSION`
- APP1 / APP2 是否存在、应用记录版本、Boot 选择版本及是否已退役
- OTP word 25 的 `SEC_IMG_OFFSET` 和 `MAX_IMG_LEN`
- 当前工程配置的 256 KiB 槽数量，以及是否与 OTP 布局匹配

退役槽的 Boot 选择版本为 `0`，但应用记录和镜像仍可能存在。查询协议
版本 4 会分别返回应用记录版本与退役状态，上位机不会再把退役槽误报为
“无效”。

BootROM OTA 的 START 会检查 OTP 布局。双槽未启用或槽地址与
`SEC_IMG_OFFSET * 256 KiB` 不一致时，设备返回
`rot_state_upgrade_boot_layout_mismatch`，不会擦写目标槽。

### 降级规则

BootROM 只比较 Boot Header 的 `SW_VERSION`，不比较应用编译版本 `ROT_VER`。
该规则由设备固件执行，上位机始终发送未经修改的官方升级文件。设备在
正常升级到更高 Boot 版本时保留现有回退槽。仅当目标 Boot 版本小于或
等于当前槽时，目标镜像校验完成后才把当前运行槽 Boot Header 的
`SW_VERSION` 编程为 `0`；重启后 BootROM 会选择保留真实版本号的目标
镜像。目标镜像的 `SW_VERSION`、应用 `ROT_VER` 和 FOTA checksum 都不修改。

退役顺序是硬约束：完整镜像回读校验 → 提交目标槽记录 → 重新读取记录并
执行整槽验证 → 当前槽 `SW_VERSION=0` → 发送最终响应并复位。任一步失败
都不会退役当前槽。

降级语义是放弃当前槽：旧槽的 `SW_VERSION=0`，不再参与正常版本选择；
下一次升级会覆盖该槽。该操作只适用于允许放弃旧槽的回滚场景，不能作为
安全启动下的常规双向切换。

## 包格式

所有字段使用小端序：

| 偏移 | 长度 | 内容 |
| --- | --- | --- |
| 0 | 2 | magic `52 43`（`RC`） |
| 2 | 1 | 协议版本，当前为 1 |
| 3 | 1 | 包类型 |
| 4 | 2 | 序号 |
| 6 | 2 | 载荷长度，最大 `端点 MPS - 10` |
| 8 | N | 载荷 |
| 8+N | 2 | CRC16-CCITT，覆盖头和载荷 |

包类型：

| 值 | 名称 | 说明 |
| --- | --- | --- |
| `0x10` | START | 载荷为文件长度和 32 位会话 ID |
| `0x11` | DATA | 文件数据，序号从 0 开始 |
| `0x12` | END | 请求设备校验并提交镜像 |
| `0x13` | ABORT | 取消当前传输 |
| `0x80` | ACK | 载荷为 32 位 `rot_state_t` |
| `0x81` | NAK | 载荷为 32 位 `rot_state_t` |

## 流程

1. 主机发送 START，载荷依次为 `file_size` 和 `session_id`。
2. 设备复位旧会话并确认 START；镜像仍由 `upgrade_feed()` 从文件头开始解析。
   `file_size` 是包含 16 字节官方升级头的完整文件长度。
3. 主机按端点 MPS 动态计算载荷并切分文件，使用递增 DATA 序号发送；
   每包必须收到 ACK 后才继续。64/128/512/1024 端点对应载荷上限为
   54/118/502/1014 字节。
4. ACK 丢失时主机重发同一序号，设备对已接收的上一包直接再次 ACK。
5. 全部字节确认后发送 END；设备调用 `upgrade_finish()`，成功后返回 ACK。
6. 失败或取消时主机发送 ABORT，设备调用 `upgrade_abort()`。

浏览器实现位于 `tests/cc/web/webusb_dt.html`，C 侧实现位于
`rotcc_upgrade.c`。
