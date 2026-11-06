# 升级模块

`upgrade/` 是运行态升级实现，`upgrade/eg/` 只保留官方示例，不参与工程编译。

## 分区

地址全部来自 `fdb/fdb_map.h`：

| 分区 | IMGINFO 页 | 镜像起点 | 容量 |
| --- | --- | --- | --- |
| APP1 | `0x80080000` | `0x80000000` | 256 KB |
| APP2 | `0x80081000` | `0x80040000` | 256 KB |

APP2 IMGINFO 页之后保留 8 KB，原有 FDB 公共参数区起点仍为 `0x80084000`。

每个镜像都采用例程中的完整布局：

- `+0x0000`：EXIP 信息
- `+0x0400`：NOR 配置
- `+0x1000`：HPM 启动头
- `+0x3000`：用户程序

当前启动分区由 ROM remap 状态判断，升级始终写入另一个 256 KB 分区。镜像校验会检查 `+0x1000` 处的 `0xBF` 启动头。

## 调用方式

通用接口：

```c
upgrade_init();
upgrade_begin(&header);
while (has_data) {
    upgrade_write(data, size);
}
upgrade_finish();
upgrade_reboot();
```

兼容官方 16 字节文件头的接口为 `upgrade_feed()`。第一次调用必须包含完整文件头，后续数据可任意分包，XMODEM 末尾填充会自动截断。原有 `ota_board_*` 函数也已适配到新实现。

## 提交与校验

`upgrade_finish()` 会：

1. 将不足 4 字节的尾部补 `0xFF`，满足 NOR 写入对齐要求。
2. 从 Flash 回读镜像，同时校验官方 32 位字节和与内部 CRC32。
3. 校验 HPM 启动头、软件版本和头部长度。
4. 在目标槽 IMGINFO 页写入官方 16 字节文件头，随后写入 40 字节扩展记录；扩展记录自身带 CRC32。

IMGINFO 页采用读回、修改、擦除、回写流程。官方 16 字节头位于页首，可直接由例程对应的启动侧读取。

记录提交完成后，启动侧应先调用 `upgrade_validate_slot()` 再次校验目标镜像，再切换到该槽。`upgrade_reboot()` 只负责复位，不直接修改 ROM remap。
