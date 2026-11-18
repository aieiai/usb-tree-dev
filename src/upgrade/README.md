# 升级模块

`src/upgrade/` 是运行态双槽升级实现，逻辑与量产工程 `kd-v75-lite` 的
`fw_upgrade.c` 一致，只把线上交互换成 Rot-CC 的 START/DATA/FINISH 命令。

## 分区

地址全部来自 `src/db/flash_layout.h`：

| 分区 | 镜像起点 | 容量 |
| --- | --- | --- |
| APP1 | `0x80000000` | 256 KB |
| APP2 | `0x80040000` | 256 KB |

每个槽的完整布局（与官方例程一致）：

- `+0x0000`：EXIP 信息
- `+0x0400`：NOR 配置
- `+0x1000`：HPM 启动头
- `+0x3000`：用户程序

镜像在 Flash 中的起点由 OTP EXIP 熔丝决定：

| 设备 | 写入目标槽的起始地址 | 镜像内 Boot Header |
| --- | --- | --- |
| 明文（EXIP 未启用） | 槽 +0x400（NOR 配置区） | 镜像 +0xC00 |
| EXIP 加密 | 槽 +0x000（EXIP 信息） | 镜像 +0x1000 |

明文镜像不包含 EXIP 区，只从 NOR 配置区开始传输。升级会擦除整个目标槽，
避免旧 EXIP 头残留导致明文镜像无法启动。启动头随数据流接收并缓存，只有
整镜像校验通过后才写回 Flash，因此中断的传输不会留下可启动的目标槽。

当前启动分区由 ROM remap 状态判断，升级始终写入另一个槽。

## OTP 布局检查

开始升级前会读取 OTP word 25：

- `[15:0]`：`SEC_IMG_OFFSET`，单位 16 KB，必须等于 APP2 的槽偏移（16）；
- `[31:16]`：`MAX_IMG_LEN`，为 0 时使用 `SEC_IMG_OFFSET`，否则必须等于 16。

不匹配时返回 `rotsta_upgrade_boot_layout_mismatch`，不允许升级。
EXIP 状态取自 OTP word 24 的 bit12。

## 调用方式

```c
upgrade_init();
upgrade_get_info(&info);      /* Rot-CC 的升级信息命令 */
upgrade_start(image_size, image_crc32);
while (has_data) {
    upgrade_write(data, size);
}
upgrade_finish();
upgrade_restart();
```

Rot-CC 的 START 请求携带完整镜像的 `fw_size` 与标准 CRC-32（poly
`0xEDB88320`，初值/结果取反）；DATA 直接发送原始固件分片，没有序号，
逐帧状态响应即流控，主机不要重发同一帧。

## 校验与提交

`upgrade_finish()` 会：

1. 将不足 4 字节的尾部补 `0xFF`，满足 NOR 写入对齐要求。
2. 从 Flash 回读整镜像，用接收时缓存的启动头替换仍为擦除态的启动头区域，
   再计算 CRC-32，与 START 参数和接收流 CRC 双重比对。
3. 校验启动头：`tag=0xBF`、`version=0x10`、`length∈[16,0x2000]`、
   `fw_count∈[1,2]`、`SW_VERSION≠0`。
4. 写回启动头并回读比对。
5. 若目标版本低于当前运行版本，把当前槽启动头的 `SW_VERSION` 编程为 `0`，
   复位后 BootROM 会启动目标槽。NOR 只支持 `1 -> 0`，因此退役当前槽不需要
   擦除正在运行的镜像。

目标槽不写 IMGINFO 页，与量产工程一致。

## 失败处理

- START 会擦除整个目标槽。
- 任意写入、校验、提交失败或主机发送 STOP 时，立即擦除目标槽。
- 失败路径不会改动当前运行槽；降级退役是唯一会修改当前槽的操作。

## 降级策略

BootROM 选择镜像时只比较两份 Boot Header 的 `SW_VERSION`，不比较应用的
`ROTVER`。只有目标版本严格小于当前版本时才退役当前槽；同版本重装不会
切换启动槽。

例如 APP2 的 `SW_VERSION=301`，要降级到 APP1 上的 290：目标保存为
290，当前 APP2 的版本被清零，复位后启动 290。后续正式版本继续使用自身
真实版本号。
