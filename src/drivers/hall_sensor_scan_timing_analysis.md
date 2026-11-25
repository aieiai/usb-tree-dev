# HPM5100 8 kHz 霍尔传感器硬件扫描微观时序分析文档

## 一、 系统基础参数与基准设置

* **主控制器时钟基准**：
  * **GPTMR1 驱动时钟**：$100\text{ MHz}$（$1\text{ tick} = 10\text{ ns} = 0.01\ \mu\text{s}$）
  * **ADC 工作时钟**：$50\text{ MHz}$（`SENSOR_MATRIX_ADC_DIV = 4`，$1\text{ ADC cycle} = 20\text{ ns} = 0.02\ \mu\text{s}$）
* **扫描帧与通道规格**：
  * **帧刷新率**：$8\text{ kHz}$（总帧周期 $125.00\ \mu\text{s} = 12,500\text{ ticks}$）
  * **MUX 选通数**：$8$ 路（`SEL 0` $\sim$ `SEL 7`，地址线 `C2/C3/C4`）
  * **物理 ADC 通道配置**：$4$ 路 ADC 共 $10$ 个模拟输入通道，按 `{3, 2, 3, 2}` 分配（ADC0/ADC2 各 $3$ 通道，ADC1/ADC3 各 $2$ 通道）
  * **采样轮数与数据量**：每通道连续采样 $2$ 轮（`SENSOR_MATRIX_ADC_SAMPLES = 2`），单路 ADC 最长序列为 $3 \times 2 = 6$ 次转换；每个 SEL 产出 $10 \times 2 = 20\text{ words}$ ($80\text{ bytes}$)，整帧共 $8 \times 20 = 160\text{ words}$ ($640\text{ bytes}$)
* **当前优化后时序基准**：
  * **单路 SEL 周期 (`reload`)**：$457\text{ ticks} = 4.57\ \mu\text{s}$（自动推导 `SENSOR_MATRIX_TMR_SEL_FREQ = 218818 Hz`）
  * **周期尾部余量**：$0.10\ \mu\text{s}$ ($10\text{ ticks}$)
  * **DMA 流水线拍数**：精简为 **$9$ 次握手（`Slot 0` $\sim$ `Slot 8`）**，消除首拍 `dummy` 与末拍清零冗余
  * **整帧实测耗时（SOF 沿 $\rightarrow$ `isr_hdma` 入口）**：$60.38 \sim 60.54\ \mu\text{s}$（$6038 \sim 6054\text{ ticks}$），帧空闲余量约 **$64.5\ \mu\text{s}$ ($51.6\%$)**

---

## 二、 `SENSOR_MATRIX_TMR_SEL_FREQ` 自动计算与配置规范

### 1. 频率约束边界

`SENSOR_MATRIX_TMR_SEL_FREQ` 本质上是单路 SEL 周期的倒数，调参时必须同时满足以下两个硬性边界：

* **上限约束（防止 `TMR_ADC` 溢出卡死）**：单周期 `reload` 必须严格大于 `config_adc.cmp[1]`，保证输出高电平能够正常拉低以产生下一周期的触发上升沿：
  $$\text{reload} \ge \text{TMR\_ST} + \text{SEL\_HT1} + \text{ADC\_ST} + \text{ADC\_CONV} + \text{MARGIN}$$
  其中单路最长 ADC 序列转换时间（换算为 GPTMR ticks）为：
  $$\text{ADC\_CONV} = \left\lceil \frac{f_{\text{TMR}} \times (\text{SAMPLE\_CYCLE} + \text{CONVERT\_CYCLE}) \times \text{MAX\_CH} \times \text{ROUNDS}}{f_{\text{ADC}}} \right\rceil$$
  代入当前参数：$\frac{100\text{ MHz} \times (2 + 21) \times 3 \times 2}{50\text{ MHz}} = 276\text{ ticks}$ ($2.76\ \mu\text{s}$)。
* **下限约束（防止超出 $8\text{ kHz}$ 帧周期）**：SOF 延时、$8$ 路扫描段耗时与 CPU 业务处理总和必须小于 $125.00\ \mu\text{s}$ ($12,500\text{ ticks}$)。

### 2. 自动计算宏（`hall_scan_5100cfg.h` 末尾）

将以下自动计算宏放置在 `hall_scan_5100cfg.h` 底部（必须位于 `SENSOR_MATRIX_ADC_CHANNELS` 和 `SENSOR_MATRIX_ROUND_SELS_MAX` 定义之后），并删除手写的 `SENSOR_MATRIX_TMR_SEL_FREQ` 常量定义：

```c
/* ==================== 自动计算 SEL 扫描频率 ==================== */
#define SENSOR_MATRIX_TMR_CLK_HZ        100000000ULL /* GPTMR1 时钟: 100MHz */
#define SENSOR_MATRIX_ADC_CLK_HZ        50000000ULL  /* ADC 工作时钟: 50MHz */
#define SENSOR_MATRIX_SEL_MARGIN_TICKS  10ULL        /* 单周期尾部余量: 10 ticks = 0.10us @100MHz */

/* 1. 单路最长 ADC 序列转换所需 GPTMR ticks（使用 ULL 防止 32 位平台乘法溢出） */
#define SENSOR_MATRIX_ADC_CONV_TICKS                                                                             \
    CEILING((SENSOR_MATRIX_TMR_CLK_HZ) * ((SENSOR_MATRIX_ADC_SAMPLE_CYCLE) + (SENSOR_MATRIX_ADC_CONVERT_CYCLE))  \
                * (SENSOR_MATRIX_ADC_CHANNELS) * (SENSOR_MATRIX_ADC_SAMPLE_ROUNDS),                              \
            (SENSOR_MATRIX_ADC_CLK_HZ))

/* 2. 单路 SEL 所需最小 reload ticks = 70 + 1 + ADC_ST + ADC_CONV + MARGIN */
#define SENSOR_MATRIX_SEL_RELOAD_TICKS                                                                           \
    ((uint64_t)(SENSOR_MATRIX_TMR_ST_TIME) + (SENSOR_MATRIX_SEL_HT_TIME1) + (SENSOR_MATRIX_ADC_ST_TIME)          \
     + (SENSOR_MATRIX_ADC_CONV_TICKS) + (SENSOR_MATRIX_SEL_MARGIN_TICKS))

/* 3. 自动推导 SENSOR_MATRIX_TMR_SEL_FREQ（向下取整，保证反算回 reload >= 目标值） */
#define SENSOR_MATRIX_TMR_SEL_FREQ      ((SENSOR_MATRIX_TMR_CLK_HZ) / (SENSOR_MATRIX_SEL_RELOAD_TICKS))

/* 4. 编译期防错：确保整帧（含 SOF 延时 + 8 路扫描 + 至少 20us CPU 余量）不超 8kHz 预算 */
#if ((10ULL + (SENSOR_MATRIX_SOF_ST_TIME) + (SENSOR_MATRIX_ROUND_SELS_MAX) * (SENSOR_MATRIX_SEL_RELOAD_TICKS)    \
      + (SENSOR_MATRIX_TMR_ST_TIME) + 2000ULL) >= ((SENSOR_MATRIX_TMR_CLK_HZ) / (SENSOR_MATRIX_TMR_SHOT_FREQ)))
#    error "Hall scan total time exceeds 8kHz frame budget! Reduce ADC_ST_TIME / SOF_ST_TIME / MARGIN."
#endif
```

### 3. 配套辅助修正

* **`drv_hall.h` 换算宏修正**：
  ```c
  /* GPTMR 100MHz 换算：1 tick = 10 ns */
  #define SENSOR_MATRIX_TMR_TICKS_TO_NS(t)    ((uint32_t)(((uint64_t)(t)) * 10ULL))
  ```
* **`drv_hall.c` 运行期防溢出兜底（`sensor_matrix_init_timer()` 中）**：
  ```c
  /* 防止实际外设时钟树与配置宏不一致导致 reload <= cmp[1] 溢出 */
  uint32_t min_reload = config_adc.cmp[1] + (uint32_t)SENSOR_MATRIX_SEL_MARGIN_TICKS;
  if (config_dma.reload < min_reload) {
      config_dma.reload = min_reload;
      gptmr_channel_config_update_reload(SENSOR_MATRIX_TMR, SENSOR_MATRIX_TMR_DMA_CHANNEL, config_dma.reload);
      scan_freq = (tmr_clock / (float)config_dma.reload);
  }
  config_adc.reload = config_dma.reload; /* 强制锁死双通道周期完全一致 */
  ```

### 4. 常见调参组合速查表（$100\text{ MHz}$ GPTMR1 / $50\text{ MHz}$ ADC）

| 调参场景 | `ADC_SAMPLES` | `ADC_ST_TIME` | `MARGIN` | 自动 `reload` | 自动 `SEL_FREQ` | $8$ 路扫描段耗时 | 预计 `sof_tick` |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **当前配置（2 次采样）** | `2` | `100` ($1.0\ \mu\text{s}$) | `10` ($0.1\ \mu\text{s}$) | **$457\text{ ticks}$** ($4.57\ \mu\text{s}$) | **$218,818\text{ Hz}$** | $37.26\ \mu\text{s}$ | $\approx 60.4\ \mu\text{s}$ |
| **提速（1 次采样）** | `1` | `100` ($1.0\ \mu\text{s}$) | `10` ($0.1\ \mu\text{s}$) | **$319\text{ ticks}$** ($3.19\ \mu\text{s}$) | **$313,479\text{ Hz}$** | $26.22\ \mu\text{s}$ | $\approx 49.4\ \mu\text{s}$ |
| **加长建立（$1.5\ \mu\text{s}$）** | `2` | `150` ($1.5\ \mu\text{s}$) | `10` ($0.1\ \mu\text{s}$) | **$507\text{ ticks}$** ($5.07\ \mu\text{s}$) | **$197,238\text{ Hz}$** | $41.26\ \mu\text{s}$ | $\approx 64.4\ \mu\text{s}$ |
| **加长建立（$2.0\ \mu\text{s}$）** | `2` | `200` ($2.0\ \mu\text{s}$) | `10` ($0.1\ \mu\text{s}$) | **$557\text{ ticks}$** ($5.57\ \mu\text{s}$) | **$179,533\text{ Hz}$** | $45.26\ \mu\text{s}$ | $\approx 68.4\ \mu\text{s}$ |

---

## 三、 阶段 0：SOF 同步与启动延时（`GPTMR1_CH0` / SHT）

`GPTMR1_CH0` 在初始化后处于自由运行状态，由 TRGM0 接入的 SOF 信号复位起跑。以 **SOF 沿到达时刻 $T = 0$** 为基准：

| 绝对时刻 ($\text{ticks}$) | 绝对时刻 ($\mu\text{s}$) | 硬件/软件动作细节 | 触发条件 / 寄存器配置 |
| :--- | :--- | :--- | :--- |
| $0$ | $0.00$ | **SOF 沿到达**：经 TRGM0 送入 `GPTMR1_SYNCI`，强制复位 CH0 计数器归零并开始递增。 | USB 模式为双沿 (`both`)，EXT/TMR 模式为上升沿 (`rising`)。 |
| $0 \sim 2010$ | $0.00 \sim 20.10$ | **SOF 保护延时**：等待 CH0 计数器累加至 `cmp[0]`。 | 保持拉低状态（若需进一步压短总扫描时间，可调小 `SENSOR_MATRIX_SOF_ST_TIME`）。 |
| **$2010$** | **$20.10$** | **`CH0` 匹配 `cmp[0]`**：<br>1. CH0 输出引脚电平拉高；<br>2. 产生 DMA 请求，经 DMAMUX 触发 **`HDMA_CH6` (`DMA_SET`)**。 | `cmp[0] = 10 + SENSOR_MATRIX_SOF_ST_TIME (2000) = 2010`<br>`dma_request_event = gptmr_dma_request_on_cmp0` |
| $2060$ | $20.60$ | **`CH0` 匹配 `cmp[1]`**：CH0 输出拉低（脉宽 $50\text{ ticks} = 0.50\ \mu\text{s}$）。此后 CH0 继续自由递增，供 `isr_hdma` 读取 `sof_tick`。 | `cmp[1] = 10 + 2000 + 50 = 2060` |

---

## 四、 阶段 1：`DMA_SET`（HDMA CH6）帧前装载链表

自 **$T = 20.10\ \mu\text{s}$ ($2010\text{ ticks}$)** 起，`HDMA_CH6` 连续执行 **$9$ 个链表描述符**（仅 Desc 0 为硬件握手，其余均为 `NORMAL` 无握手连发）：

| 描述符 | 搬运模式 | 传输数据量 | 源地址 $\rightarrow$ 目的地址 | 硬件控制作用 |
| :--- | :--- | :--- | :--- | :--- |
| **Desc 0** | Handshake | $4\text{ B}$ ($1\text{W}$) | `STATUS_REG[preload_beg]` $\rightarrow$ `STATUS_REG[active_beg]` | 置位系统扫描 Busy 标志。 |
| **Desc 1** | Normal | $4\text{ B}$ ($1\text{W}$) | `TMR_CHANNEL_REG[dma_disable]` $\rightarrow$ `TMR_DMA (CH1).CR` | 关停 CH1 计数器。 |
| **Desc 2** | Normal | $4\text{ B}$ ($1\text{W}$) | `TMR_CHANNEL_REG[adc_disable]` $\rightarrow$ `TMR_ADC (CH2).CR` | 关停 CH2 计数器。 |
| **Desc 3** | Normal | **$64\text{ B}$ ($16\text{W}$)** | `MUX_ADDR_REG[preload]` $\rightarrow$ `MUX_ADDR_REG[shadow]` | 拷贝 **$8$ 组** GPIO SET/CLR 掩码到阴影区（已移除末尾清零项）。 |
| **Desc 4** | Normal | $12\text{ B}$ ($3\text{W}$) | `SENSOR_SCAN_ARRAY_REG[preload]` $\rightarrow$ `[shadow]` | 拷贝扫描计数与序列配置。 |
| **Desc 5** | Normal | $28\text{ B}$ ($7\text{W}$) | `DMA_SEL_REG.LLPOINTER..CTRL` $\rightarrow$ `HDMA_CH7` 寄存器 | 7T Burst 递减重载并使能 `DMA_SEL`（首项直接为切 **SEL 0**）。 |
| **Desc 6** | Normal | $4\text{ B}$ ($1\text{W}$) | `TMR_CHANNEL_REG[dma_enable]` $\rightarrow$ `TMR_DMA (CH1).CR` | 使能 CH1。 |
| **Desc 7** | Normal | $4\text{ B}$ ($1\text{W}$) | `TMR_CHANNEL_REG[adc_enable]` $\rightarrow$ `TMR_ADC (CH2).CR` | 使能 CH2。 |
| **Desc 8** | Normal | $4\text{ B}$ ($1\text{W}$) | `TMR_CHANNEL_REG[gcr]` $\rightarrow$ `GPTMR1.GCR` | **软同步清零 CH1 与 CH2**，链表自动回环至 Desc 0。 |

* **装载耗时 $\Delta_{\text{set}}$**：共搬运 **$128\text{ B}$** 有效数据与 $9$ 次描述符预取，在 AHB SRAM 总线上耗时约 **$150 \sim 180\text{ ticks}$ ($1.50 \sim 1.80\ \mu\text{s}$)**。
* **硬件同步起跑时刻 $T_{\text{sync}}$**：$T_{\text{sync}} = 2010 + \Delta_{\text{set}} \approx \mathbf{2160\text{ ticks}\ (21.60\ \mu\text{s})}$。此时 `CH1` 与 `CH2` 计数器同步归零并起跑。

---

## 五、 阶段 2：单路 SEL 周期内部微观时序拆解 ($457\text{ ticks} / 4.57\ \mu\text{s}$)

在 `CH1 (TMR_DMA)` 和 `CH2 (TMR_ADC)` 同步运行期间，每个 Slot 内局部计数器从 $0$ 数到 $456$，内部微观时序关系如下：

```text
Ticks:  0        70  71       116        171                            447  457
        |--------|---|--------|----------|------------------------------|----|
CH1     |________/¯¯¯¯¯¯¯¯¯¯¯¯\__________|______________________________|____| (DMA Trigger)
CH2     |________________________________/¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯\____| (ADC Trigger)
        |<-ST(70)->|
                 |<---- MUX -> ADC Wait (101 ticks / 1.01us) ---->|
                 |   |<--- Analog Settling (ADC_ST = 100) ------->|
                 |-- DMA 切 MUX + 并行搬运上一路 80B --|
                                         |<--- ADC Conv (276 ticks) --->|<-M->|
```

### 1. 周期内逐节点时间表

| 局部 $\text{cnt}$ ($\text{ticks}$) | 局部时刻 ($\mu\text{s}$) | 阶段持续时间 | 硬件/模拟信号微观动作 |
| :--- | :--- | :--- | :--- |
| $0$ | $0.00$ | — | 周期起点，CH1 & CH2 计数器同步归零。 |
| $0 \sim 70$ | $0.00 \sim 0.70$ | $0.70\ \mu\text{s}$ | 等待 `TMR_ST_TIME`（$70\text{ ticks}$）。与上一周期尾部 $0.10\ \mu\text{s}$ 合计形成 **$0.80\ \mu\text{s}$** 的安全隔离，确保 ADC 内部 DMA 已将上一路末点写入 `seq_buff`。 |
| **$70$** | **$0.70$** | — | **`CH1` 匹配 `cmp[0]`**：<br>1. CH1 输出拉高；<br>2. 触发 **`HDMA_CH7 (DMA_SEL)`** 握手，以 2T Burst 连续写入 GPIO `DO.CLEAR`/`DO.SET` 切换 MUX 地址；<br>3. 紧接着发起无握手 DMA，将上一路采好的 `seq_buff`（$20\text{W} = 80\text{ B}$）搬至 `raw_buff`（与模拟建立窗口并行，不推迟 MUX 切换）。 |
| $71$ | $0.71$ | — | `TMR_ST_TIME + SEL_HT_TIME1`（$70 + 1$），GPIO 地址切换参考点。 |
| $116$ | $1.16$ | — | `CH1` 匹配 `cmp[1]`（$70 + 1 + 45 = 116$），CH1 输出拉低（脉宽 $0.46\ \mu\text{s}$）。 |
| $71 \sim 171$ | $0.71 \sim 1.71$ | **$1.00\ \mu\text{s}$** | **模拟通路建立窗口 (`ADC_ST_TIME = 100`)**：供 MUX 导通及霍尔输出 RC 网络稳定；同时上一路 $80\text{ B}$ 的 DMA 搬运在前半段完成。 |
| **$171$** | **$1.71$** | — | **`CH2` 匹配 `cmp[0]` ($171$)**：CH2 输出拉高，经 TRGM0 硬件同步触发 **ADC0/1/2/3** 并行序列转换。 |
| $171 \sim 217$ | $1.71 \sim 2.17$ | $0.46\ \mu\text{s}$ | **ADC 转换 #1** ($23\text{ cycles} @ 50\text{MHz}$): ADC0(CH2 r0)、ADC1(CH9 r0)、ADC2(CH12 r0)、ADC3(CH4 r0) |
| $217 \sim 263$ | $2.17 \sim 2.63$ | $0.46\ \mu\text{s}$ | **ADC 转换 #2** ($23\text{ cycles} @ 50\text{MHz}$): ADC0(CH2 r1)、ADC1(CH9 r1)、ADC2(CH12 r1)、ADC3(CH4 r1) |
| $263 \sim 309$ | $2.63 \sim 3.09$ | $0.46\ \mu\text{s}$ | **ADC 转换 #3** ($23\text{ cycles} @ 50\text{MHz}$): ADC0(CH1 r0)、ADC1(CH13 r0)、ADC2(CH8 r0)、ADC3(CH3 r0) |
| $309 \sim 355$ | $3.09 \sim 3.55$ | $0.46\ \mu\text{s}$ | **ADC 转换 #4** ($23\text{ cycles} @ 50\text{MHz}$): ADC0(CH1 r1)、ADC1(CH13 r1)、ADC2(CH8 r1)、ADC3(CH3 r1)<br>$\rightarrow$ **ADC1 & ADC3 序列转换完成** |
| $355 \sim 401$ | $3.55 \sim 4.01$ | $0.46\ \mu\text{s}$ | **ADC 转换 #5** ($23\text{ cycles} @ 50\text{MHz}$): ADC0(CH15 r0)、ADC2(CH0 r0) |
| $401 \sim 447$ | $4.01 \sim 4.47$ | $0.46\ \mu\text{s}$ | **ADC 转换 #6** ($23\text{ cycles} @ 50\text{MHz}$): ADC0(CH15 r1)、ADC2(CH0 r1)<br>$\rightarrow$ **ADC0 & ADC2 序列转换完成**（全部 $20\text{ words}$ 写入 `seq_buff`） |
| **$447$** | **$4.47$** | — | **`CH2` 匹配 `cmp[1]` ($171 + 276 = 447$)**：CH2 输出拉低。 |
| $447 \sim 457$ | $4.47 \sim 4.57$ | **$0.10\ \mu\text{s}$** | **周期尾部安全余量 (`MARGIN = 10 ticks`)**：确保脉冲可靠拉低，为下一周期提供有效上升沿。 |
| $457$ | $4.57$ | — | 达到 `reload = 457`，CH1 & CH2 归零进入下一 Slot。 |

### 2. MUX 切换 $\rightarrow$ ADC 开始采样等待时间专项说明

* **硬件数字间隔**：`config_adc.cmp[0] - config_dma.cmp[0]` $= 171 - 70 = \mathbf{101\text{ ticks}\ (1.01\ \mu\text{s})}$，由 `SENSOR_MATRIX_SEL_HT_TIME1` ($1\text{ tick}$) 与 `SENSOR_MATRIX_ADC_ST_TIME` ($100\text{ ticks}$) 决定。
* **有效模拟稳定时间**：扣除 DMAMUX/HDMA 总线仲裁与写 GPIO 寄存器的固有响应延迟，以及外部 MUX 芯片自身的导通开关时间后，实际留给模拟通路 RC 充电稳定的有效窗口约为 **$0.80 \sim 0.90\ \mu\text{s}$**。
* **调优方式**：若示波器观测到相邻 SEL 通道存在模拟串扰，只需调大 `hall_scan_5100cfg.h` 中的 `SENSOR_MATRIX_ADC_ST_TIME`（每 $+10$ 即 $+0.10\ \mu\text{s}$），自动计算宏会同步更新 `SENSOR_MATRIX_TMR_SEL_FREQ`。

---

## 六、 阶段 3：`DMA_SEL`（HDMA CH7）精简 9 拍全流水线扫描表

优化后的 `DMA_SEL` 链表移除了原版开头的 `1. dummy` 空拍以及末尾多余的第 $9$ 次切回 `0` 与 `Slot 10` 空等，共仅触发 **$9$ 次** `CH1` 硬件握手（`Slot 0` $\sim$ `Slot 8`），达到 $8$ 路流水线的理论最短周期。

假设 $\Delta_{\text{set}} \approx 150\text{ ticks} = 1.50\ \mu\text{s}$（即 $T_{\text{sync}} \approx 21.60\ \mu\text{s}$）：

| Slot | 相对 $T_{\text{sync}}$ ($\text{ticks} / \mu\text{s}$) | 握手点 (相对 $T_{\text{sync}}$) | 相对 SOF 触发点 ($\mu\text{s}$) | `DMA_SEL` 链表动作（先握手，后无握手） | 本 Slot 内 ADC 转换对象 ($+1.71 \sim +4.47\ \mu\text{s}$) |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **Slot 0** | $0 \sim 457$ ($0.00 \sim 4.57$) | $70\text{ ticks}$ ($0.70\ \mu\text{s}$) | $22.30$ | **握手 #1**：`DMA_SEL_REG` 直接切 GPIO $\rightarrow$ **SEL 0**；随后无握手更新下帧 Ping-Pong `LLPOINTER`。 | **采样 SEL 0** $\rightarrow$ `seq_buff[0..19]` |
| **Slot 1** | $457 \sim 914$ ($4.57 \sim 9.14$) | $527\text{ ticks}$ ($5.27\ \mu\text{s}$) | $26.87$ | **握手 #2**：切 GPIO $\rightarrow$ **SEL 1**；搬运 **SEL 0** ($80\text{ B}$) $\rightarrow$ `raw_buff[0..19]`。 | **采样 SEL 1** $\rightarrow$ `seq_buff[0..19]` |
| **Slot 2** | $914 \sim 1371$ ($9.14 \sim 13.71$) | $984\text{ ticks}$ ($9.84\ \mu\text{s}$) | $31.44$ | **握手 #3**：切 GPIO $\rightarrow$ **SEL 2**；搬运 **SEL 1** ($80\text{ B}$) $\rightarrow$ `raw_buff[20..39]`。 | **采样 SEL 2** $\rightarrow$ `seq_buff[0..19]` |
| **Slot 3** | $1371 \sim 1828$ ($13.71 \sim 18.28$) | $1441\text{ ticks}$ ($14.41\ \mu\text{s}$) | $36.01$ | **握手 #4**：切 GPIO $\rightarrow$ **SEL 3**；搬运 **SEL 2** ($80\text{ B}$) $\rightarrow$ `raw_buff[40..59]`。 | **采样 SEL 3** $\rightarrow$ `seq_buff[0..19]` |
| **Slot 4** | $1828 \sim 2285$ ($18.28 \sim 22.85$) | $1898\text{ ticks}$ ($18.98\ \mu\text{s}$) | $40.58$ | **握手 #5**：切 GPIO $\rightarrow$ **SEL 4**；搬运 **SEL 3** ($80\text{ B}$) $\rightarrow$ `raw_buff[60..79]`。 | **采样 SEL 4** $\rightarrow$ `seq_buff[0..19]` |
| **Slot 5** | $2285 \sim 2742$ ($22.85 \sim 27.42$) | $2355\text{ ticks}$ ($23.55\ \mu\text{s}$) | $45.15$ | **握手 #6**：切 GPIO $\rightarrow$ **SEL 5**；搬运 **SEL 4** ($80\text{ B}$) $\rightarrow$ `raw_buff[80..99]`。 | **采样 SEL 5** $\rightarrow$ `seq_buff[0..19]` |
| **Slot 6** | $2742 \sim 3199$ ($27.42 \sim 31.99$) | $2812\text{ ticks}$ ($28.12\ \mu\text{s}$) | $49.72$ | **握手 #7**：切 GPIO $\rightarrow$ **SEL 6**；搬运 **SEL 5** ($80\text{ B}$) $\rightarrow$ `raw_buff[100..119]`。 | **采样 SEL 6** $\rightarrow$ `seq_buff[0..19]` |
| **Slot 7** | $3199 \sim 3656$ ($31.99 \sim 36.56$) | $3269\text{ ticks}$ ($32.69\ \mu\text{s}$) | $54.29$ | **握手 #8**：切 GPIO $\rightarrow$ **SEL 7**；搬运 **SEL 6** ($80\text{ B}$) $\rightarrow$ `raw_buff[120..139]`。 | **采样 SEL 7** $\rightarrow$ `seq_buff[0..19]` |
| **Slot 8** | $3656 \sim 3726$ ($36.56 \sim 37.26$) | **$3726\text{ ticks}$** ($37.26\ \mu\text{s}$) | **$58.86$** | **握手 #9（不切 GPIO，直接收尾）**：<br>1. Desc 4.1 停 `TMR_DMA`；<br>2. Desc 4.2 立即停 `TMR_ADC`（此时 $\text{cnt} \approx 75 \ll 171$，消除多余采样）；<br>3. Desc 4.3 搬运 **SEL 7** ($80\text{ B}$) $\rightarrow$ `raw_buff[140..159]`；<br>4. Desc 4.4 清零 Busy 标志；<br>5. Desc 4.5 翻转魔数并触发 **HDMA TC 中断**。 | 定时器已提前关停，无冗余 ADC 触发。 |

### 优化前后对比

| 架构版本 | 握手次数 | `DMA_SEL` 扫描段耗时 | 每帧 ADC 触发次数 | 整帧总耗时（含 $20.1\ \mu\text{s}$ SOF 延时） |
| :--- | :--- | :--- | :--- | :--- |
| 原版流水线（`reload = 457`） | $11$ 次（含首拍 dummy 与末拍空等） | $46.40\ \mu\text{s}$ ($10 \times 4.57 + 0.70$) | $10$ 次（含 $2$ 次无效空采） | $\approx 69.5\ \mu\text{s}$ |
| **当前精简版（`reload = 457`）** | **$9$ 次（理论最短）** | **$37.26\ \mu\text{s}$** ($8 \times 4.57 + 0.70$) | **$8$ 次（零浪费）** | **实测 $60.38 \sim 60.54\ \mu\text{s}$（省 $9.14\ \mu\text{s}$）** |

---

## 七、 阶段 4：中断服务与帧空闲余量

| 绝对时刻 ($\text{ticks}$) | 绝对时刻 ($\mu\text{s}$) | 系统与 CPU 动作 |
| :--- | :--- | :--- |
| **$\approx 6045$**<br>*(实测 $6038 \sim 6054$)* | **$\approx 60.45$** | **进入 `isr_hdma()`**（Slot 8 握手 #9 后经 $80\text{ B}$ 搬运与进中断响应，约 $+1.6\ \mu\text{s}$）：<br>1. 入口第一时间读取 `mcycle` 与 `CH0` 计数值 `sof_now`（避免回调耗时污染时间戳）；<br>2. 清除 HDMA 传输完成标志，用预缓存的 `s_cpu_mhz` 计算 `scan_period_us` 与 `scan_time_us`；<br>3. 调用 `hall_recv_cb(raw_buff[bufidx])` 提交本帧 $160\text{ words}$ 完整数据。 |
| $6045 \sim 12500$ | $60.45 \sim 125.00$ | **CPU 帧空闲窗口（持续约 $64.55\ \mu\text{s} / 6455\text{ ticks}$，占总周期 $51.6\%$）**：<br>用于霍尔行程解算、滤波、校准算法运算及 USB 报文上报，等待下一个 $8\text{ kHz}$ SOF 沿到达。 |

---

## 八、 实测验证指标与工程避坑指南

### 1. 调试器核对基准（实测 $50$ 万帧验证通过）

在调试器中观察 `g_sensor_matrix_timing` 结构体，符合以下数值即代表硬件扫描链处于健康锁定状态：

| 监测字段 | 理论 / 实测达标值 | 验证含义 |
| :--- | :--- | :--- |
| **`sof_tick`** | **$6038 \sim 6054\text{ ticks}$** ($60.38 \sim 60.54\ \mu\text{s}$) | 50 万帧实测抖动仅 $16\text{ ticks}$ ($0.16\ \mu\text{s}$)，证明 SOF 同步、DMA 链表与定时器零丢拍。 |
| **`scan_period_us`** | **稳定 $125\ \mu\text{s}$**（允许 $124 \sim 125$ 取整跳变） | $8\text{ kHz}$ 帧周期严格锁定。 |
| **`adc_seq_len`** | **`6`** | 单路最长序列（$3$ 通道 $\times 2$ 轮）配置生效。 |
| **`adc_window_ticks`** | **`276`** ($2.76\ \mu\text{s}$) | ADC $50\text{ MHz}$ 分频与采样/转换周期配置正确。 |
| **`copy_words`** | **`20`** ($80\text{ bytes}$) | 单次 SEL 的 $10$ 通道 $\times 2$ 轮紧密搬运长度正确。 |

### 2. 实测定位出的关键工程注意事项

1. **禁止在 `isr_hdma()` 内调用 `clock_get_frequency()`**：
   * `clock_get_frequency()` 内部会读取 PLL 寄存器并执行 64 位运算；在 $8\text{ kHz}$ 高频中断中连续调用两次会白白消耗约 **$5.26\ \mu\text{s}$**（实测导致函数末尾 `sof_tick` 从 `6054` 拉高到 `6580`）。
   * **正确做法**：在 `sensor_matrix_init()` 中预先缓存 `s_cpu_mhz = clock_get_frequency(clock_cpu0) / 1000000U`，中断内直接使用缓存值做除法。
2. **严格隔离 `MMX_HDMA_IRQ` 中断优先级与临界区**：
   * 必须保证 `MMX_HDMA_IRQ` 独享最高中断优先级（如 `7`），将 USB 等耗时中断降至 `5` $\sim$ `6`，同时避免在 `hall_recv_cb` 唤醒的任务中出现长关中断临界区，否则会推迟中断响应并导致 `scan_period_us` 在 $121 \sim 128\ \mu\text{s}$ 间抖动。
3. **空闲态 MUX 地址停留说明**：
   * 精简掉第 $9$ 次清零后，帧空闲窗口内地址线 `C2/C3/C4` 会保持在 **SEL 7**（原版保持在 **SEL 0**）。由于硬件未连接 `MUX_EN` 使能脚，地址线在空闲期必然导通某一路，对常规模拟开关无影响；仅当硬件上 SEL 7 挂有特殊大负载时，才需保留切回安全通道的动作。