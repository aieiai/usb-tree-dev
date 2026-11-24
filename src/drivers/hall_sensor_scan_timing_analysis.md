# 8 kHz 霍尔传感器硬件扫描微观时序分析文档

## 一、 系统基础参数与基准设置

* **主控制器配置**：
  * **GPTMR1 驱动时钟**：$100\text{ MHz}$（$1\text{ tick} = 10\text{ ns} = 0.01\ \mu\text{s}$）
  * **ADC 转换时钟**：$50\text{ MHz}$（$1\text{ ADC cycle} = 20\text{ ns} = 0.02\ \mu\text{s}$）
* **扫描帧与通道规格**：
  * **帧刷新率**：$8\text{ kHz}$（总帧周期 $125.00\ \mu\text{s} = 12,500\text{ ticks}$）
  * **SEL 通道数**：8 路（SEL 0 ~ SEL 7）
  * **单次采样通道数**：4 个 ADC $\times$ 5 通道（最长序列 6 次转换，共 20 words = 80 字节）
* **时序修正目标**：
  * **`SENSOR_MATRIX_TMR_SEL_FREQ`**：改为 **`218800`**
  * **单路 SEL 周期 (`reload`)**：$457\text{ ticks} = 4.57\ \mu\text{s}$
  * **周期尾部余量**：$0.10\ \mu\text{s}$ ($10\text{ ticks}$)

---

## 二、 代码配置修改清单

为保证硬件计数器不发生溢出且脉冲正常拉低，需针对 $100\text{ MHz}$ 时钟源更新以下配置宏：

### 1. `hall_scan_5100cfg.h`
```c
/* 调整 SEL 周期频率，使得 reload = 100MHz / 218800 = 457 ticks (4.57us) */
#define SENSOR_MATRIX_TMR_SEL_FREQ          218800

/* 时序参数定义 (单位: ticks @ 100MHz) */
#define SENSOR_MATRIX_SOF_ST_TIME           2000    /* SOF 保护延时: 20.00 us */
#define SENSOR_MATRIX_TMR_ST_TIME           70      /* DMA 触发等待: 0.70 us */
#define SENSOR_MATRIX_ADC_ST_TIME           100     /* 模拟通路建立时间: 1.00 us */
```

### 2. `drv_hall.h`
```c
/* GPTMR 100MHz 换算宏修正：1 tick = 10 ns */
#define SENSOR_MATRIX_TMR_TICKS_TO_NS(t)    ((uint32_t)(((uint64_t)(t)) * 10ULL))
```

---

## 三、 阶段 0：SOF 同步与启动延时（`GPTMR1_CH0` / SHT）

`GPTMR1_CH0` 在初始化后处于自由运行状态，由 TRGM0 接入的外部 SOF 信号复位起跑。以 **SOF 沿到达时刻 $T = 0$** 为基准：

| 绝对时刻 ($\text{ticks}$) | 绝对时刻 ($\mu\text{s}$) | 硬件/软件动作细节 | 触发条件 / 寄存器配置 |
| :--- | :--- | :--- | :--- |
| $0$ | $0.00$ | **SOF 沿到达**：经 TRGM0 送入 `GPTMR1_SYNCI`，强制复位 CH0 计数器归零并开始递增。 | USB 模式为双沿 (`both`)，EXT/TMR 模式为上升沿 (`rising`)。 |
| $0 \sim 2010$ | $0.00 \sim 20.10$ | **SOF 保护延时**：等待 CH0 计数器累加至 `cmp[0]`。 | 保持拉低状态。 |
| **$2010$** | **$20.10$** | **`CH0` 匹配 `cmp[0]`**：<br>1. CH0 输出引脚电平拉高；<br>2. 产生 DMA 请求，经 DMAMUX 触发 **`HDMA_CH6` (`DMA_SET`)**。 | `cmp[0] = 10 + SENSOR_MATRIX_SOF_ST_TIME (2000) = 2010`<br>`dma_request_event = gptmr_dma_request_on_cmp0` |
| $2060$ | $20.60$ | **`CH0` 匹配 `cmp[1]`**：CH0 输出拉低（脉冲宽度 $50\text{ ticks} = 0.50\ \mu\text{s}$）。此后 CH0 继续自由递增，供 `isr_hdma` 读取 `sof_tick`。 | `cmp[1] = 10 + 2000 + 50 = 2060` |

---

## 四、 阶段 1：`DMA_SET`（HDMA CH6）帧前装载链表

自 **$T = 20.10\ \mu\text{s}$ ($2010\text{ ticks}$)** 起，`HDMA_CH6` 连续执行 **9 个链表描述符**：

| 描述符 | 搬运模式 | 传输数据量 | 源地址 $\rightarrow$ 目的地址 | 硬件控制作用 |
| :--- | :--- | :--- | :--- | :--- |
| **Desc 0** | Handshake | $4\text{ B}$ (1W) | `STATUS_REG[preload_beg]` $\rightarrow$ `STATUS_REG[active_beg]` | 置位系统扫描 Busy 标志。 |
| **Desc 1** | Normal | $4\text{ B}$ (1W) | `TMR_CHANNEL_REG[dma_disable]` $\rightarrow$ `TMR_DMA (CH1).CR` | 关停 CH1 计数器。 |
| **Desc 2** | Normal | $4\text{ B}$ (1W) | `TMR_CHANNEL_REG[adc_disable]` $\rightarrow$ `TMR_ADC (CH2).CR` | 关停 CH2 计数器。 |
| **Desc 3** | Normal | $72\text{ B}$ (18W) | `MUX_ADDR_REG[preload]` $\rightarrow$ `MUX_ADDR_REG[shadow]` | 拷贝 9 组 GPIO SET/CLR 掩码到阴影区。 |
| **Desc 4** | Normal | $12\text{ B}$ (3W) | `SENSOR_SCAN_ARRAY_REG[preload]` $\rightarrow$ `shadow` | 拷贝扫描计数与序列配置。 |
| **Desc 5** | Normal | $28\text{ B}$ (7W) | `DMA_SEL_REG.LLPOINTER...CTRL` $\rightarrow$ `HDMA_CH7` 寄存器 | 7T Burst 递减重载并使能 `DMA_SEL` 通道。 |
| **Desc 6** | Normal | $4\text{ B}$ (1W) | `TMR_CHANNEL_REG[dma_enable]` $\rightarrow$ `TMR_DMA (CH1).CR` | 使能 CH1。 |
| **Desc 7** | Normal | $4\text{ B}$ (1W) | `TMR_CHANNEL_REG[adc_enable]` $\rightarrow$ `TMR_ADC (CH2).CR` | 使能 CH2。 |
| **Desc 8** | Normal | $4\text{ B}$ (1W) | `TMR_CHANNEL_REG[gcr]` $\rightarrow$ `GPTMR1.GCR` | **软同步清零 CH1 与 CH2**，链表自动回环至 Desc 0。 |

* **装载耗时 $\Delta_{\text{set}}$**：共搬运 $132\text{ B}$ 有效数据与 9 次描述符预取，在 AHB SRAM 总线上耗时约 **$100 \sim 150\text{ ticks}$ ($1.00 \sim 1.50\ \mu\text{s}$)**。
* **硬件同步起跑时刻 $T_{\text{sync}}$**：$T_{\text{sync}} = 2010 + \Delta_{\text{set}} \approx \mathbf{2160\text{ ticks}\ (21.60\ \mu\text{s})}$。此时 `CH1` 与 `CH2` 计数器强制清零并同步开始递增。

---

## 五、 阶段 2：单路 SEL 周期内部微观时序拆解 ($457\text{ ticks} / 4.57\ \mu\text{s}$)

在 `CH1 (TMR_DMA)` 和 `CH2 (TMR_ADC)` 同步运行期间，每个 Slot 内局部计数器从 $0$ 数到 $456$，内部微观时间节点如下：

```
Ticks:  0       70    71          116         171                        447   457
        |-------|-----|-----------|-----------|--------------------------|-----|
CH1     |______/¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯\___________|________________________________| (DMA Trigger)
CH2     |_____________________________________/¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯\______| (ADC Trigger)
        |<-ST-->|<--HT1-->|
        |       |<- MUX Switch + DMA Raw Buffer Copy ->|
        |                 |<-- ADC Analog ST -->|<---- ADC Conversion Window ---->|<-Margin->|
```

| 局部 $\text{cnt}$ ($\text{ticks}$) | 局部时刻 ($\mu\text{s}$) | 阶段持续时间 | 硬件/模拟信号微观动作 |
| :--- | :--- | :--- | :--- |
| $0$ | $0.00$ | — | 周期起点，CH1 & CH2 计数器同步归零。 |
| $0 \sim 70$ | $0.00 \sim 0.70$ | $0.70\ \mu\text{s}$ | 等待 `TMR_ST_TIME`（$70\text{ ticks}$）。 |
| **$70$** | **$0.70$** | — | **`CH1` 匹配 `cmp[0]`**：<br>1. CH1 输出电平拉高；<br>2. 触发 **`HDMA_CH7 (DMA_SEL)`** 握手，写入 GPIO `CLEAR`/`SET` 寄存器切换 MUX 通道；<br>3. 紧接着发起无握手 DMA 搬运，将上一路采好的 `seq_buff`（$20\text{W} = 80\text{ B}$）转存至 `raw_buff`。 |
| $71$ | $0.71$ | — | `TMR_ST_TIME + SEL_HT_TIME1`（$70 + 1$），GPIO 引脚电平切换完成点。 |
| $116$ | $1.16$ | — | `CH1` 匹配 `cmp[1]`（$70 + 1 + 45 = 116$），CH1 输出拉低（脉冲宽度 $0.46\ \mu\text{s}$）。 |
| $71 \sim 171$ | $0.71 \sim 1.71$ | **$1.00\ \mu\text{s}$** | **模拟通路建立窗口 (`ADC_ST_TIME = 100`)**：提供给板级霍尔传感器及 Multiplexer 的 RC 滤波电路稳定时间。上一路 $80\text{ B}$ 的 DMA 搬运在此窗口前半段（约 $0.70 \sim 1.20\ \mu\text{s}$）内同步完成。 |
| **$171$** | **$1.71$** | — | **`CH2` 匹配 `cmp[0]` ($171$)**：CH2 输出拉高，经 TRGM0 硬件同步触发 **ADC0/1/2/3** 并行序列采样。 |
| $171 \sim 217$ | $1.71 \sim 2.17$ | $0.46\ \mu\text{s}$ | **ADC 转换 #1** ($23\text{ cycles} @ 50\text{MHz}$): ADC0(CH2 r0)、ADC1(CH9 r0)、ADC2(CH12 r0)、ADC3(CH4 r0) |
| $217 \sim 263$ | $2.17 \sim 2.63$ | $0.46\ \mu\text{s}$ | **ADC 转换 #2** ($23\text{ cycles} @ 50\text{MHz}$): ADC0(CH2 r1)、ADC1(CH9 r1)、ADC2(CH12 r1)、ADC3(CH4 r1) |
| $263 \sim 309$ | $2.63 \sim 3.09$ | $0.46\ \mu\text{s}$ | **ADC 转换 #3** ($23\text{ cycles} @ 50\text{MHz}$): ADC0(CH1 r0)、ADC1(CH13 r0)、ADC2(CH8 r0)、ADC3(CH3 r0) |
| $309 \sim 355$ | $3.09 \sim 3.55$ | $0.46\ \mu\text{s}$ | **ADC 转换 #4** ($23\text{ cycles} @ 50\text{MHz}$): ADC0(CH1 r1)、ADC1(CH13 r1)、ADC2(CH8 r1)、ADC3(CH3 r1)<br>$\rightarrow$ **ADC1 & ADC3 序列转换完成** |
| $355 \sim 401$ | $3.55 \sim 4.01$ | $0.46\ \mu\text{s}$ | **ADC 转换 #5** ($23\text{ cycles} @ 50\text{MHz}$): ADC0(CH15 r0)、ADC2(CH0 r0) |
| $401 \sim 447$ | $4.01 \sim 4.47$ | $0.46\ \mu\text{s}$ | **ADC 转换 #6** ($23\text{ cycles} @ 50\text{MHz}$): ADC0(CH15 r1)、ADC2(CH0 r1)<br>$\rightarrow$ **ADC0 & ADC2 序列转换完成**（全部 20 words 自动写入 `seq_buff`） |
| **$447$** | **$4.47$** | — | **`CH2` 匹配 `cmp[1]` ($171 + 276 = 447$)**：CH2 输出拉低。 |
| $447 \sim 457$ | $4.47 \sim 4.57$ | **$0.10\ \mu\text{s}$** | **周期尾部安全余量 ($10\text{ ticks}$)**：确保电平可靠拉低，使得下一周期能产生有效的逻辑上升沿。 |
| $457$ | $4.57$ | — | 达到 `reload = 457`，CH1 & CH2 归零，启动下一个 Slot 计数。 |

---

## 六、 阶段 3：`DMA_SEL`（HDMA CH7）11 拍全流水线扫描表

`DMA_SEL` 链表包含 **11 次** `CH1` 硬件握手。假设 $\Delta_{\text{set}} \approx 150\text{ ticks} = 1.50\ \mu\text{s}$（即 $T_{\text{sync}} = 21.60\ \mu\text{s}$）：

| Slot | 相对 $T_{\text{sync}}$ ($\text{ticks} / \mu\text{s}$) | 握手点 (相对 $T_{\text{sync}}$) | 相对 SOF 触发点 ($\mu\text{s}$) | `DMA_SEL` 链表动作 (先握手切 MUX，后无握手搬运) | 本 Slot 内 ADC 转换对象 ($+1.71 \sim +4.47\ \mu\text{s}$) |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **Slot 0** | $0 \sim 457$ ($0.00 \sim 4.57$) | $70\text{ ticks}$ ($0.70\ \mu\text{s}$) | $22.30$ | **握手 #1**：Desc 1 (Dummy 4B) $\rightarrow$ Desc 2 (更新下帧 Ping-Pong 链表指针)。 | 触发 1 次 dummy 采样（数据在下一拍被覆盖）。 |
| **Slot 1** | $457 \sim 914$ ($4.57 \sim 9.14$) | $527\text{ ticks}$ ($5.27\ \mu\text{s}$) | $26.87$ | **握手 #2**：Desc 3.1 (pre)，GPIO 切换至 **SEL 0**（无搬运）。 | **采样 SEL 0** $\rightarrow$ `seq_buff[0..19]` |
| **Slot 2** | $914 \sim 1371$ ($9.14 \sim 13.71$) | $984\text{ ticks}$ ($9.84\ \mu\text{s}$) | $31.44$ | **握手 #3**：切 GPIO 到 **SEL 1**；搬运 **SEL 0** ($80\text{B}$) $\rightarrow$ `raw_buff[0..19]`。 | **采样 SEL 1** $\rightarrow$ `seq_buff[0..19]` |
| **Slot 3** | $1371 \sim 1828$ ($13.71 \sim 18.28$) | $1441\text{ ticks}$ ($14.41\ \mu\text{s}$) | $36.01$ | **握手 #4**：切 GPIO 到 **SEL 2**；搬运 **SEL 1** ($80\text{B}$) $\rightarrow$ `raw_buff[20..39]`。 | **采样 SEL 2** $\rightarrow$ `seq_buff[0..19]` |
| **Slot 4** | $1828 \sim 2285$ ($18.28 \sim 22.85$) | $1898\text{ ticks}$ ($18.98\ \mu\text{s}$) | $40.58$ | **握手 #5**：切 GPIO 到 **SEL 3**；搬运 **SEL 2** ($80\text{B}$) $\rightarrow$ `raw_buff[40..59]`。 | **采样 SEL 3** $\rightarrow$ `seq_buff[0..19]` |
| **Slot 5** | $2285 \sim 2742$ ($22.85 \sim 27.42$) | $2355\text{ ticks}$ ($23.55\ \mu\text{s}$) | $45.15$ | **握手 #6**：切 GPIO 到 **SEL 4**；搬运 **SEL 3** ($80\text{B}$) $\rightarrow$ `raw_buff[60..79]`。 | **采样 SEL 4** $\rightarrow$ `seq_buff[0..19]` |
| **Slot 6** | $2742 \sim 3199$ ($27.42 \sim 31.99$) | $2812\text{ ticks}$ ($28.12\ \mu\text{s}$) | $49.72$ | **握手 #7**：切 GPIO 到 **SEL 5**；搬运 **SEL 4** ($80\text{B}$) $\rightarrow$ `raw_buff[80..99]`。 | **采样 SEL 5** $\rightarrow$ `seq_buff[0..19]` |
| **Slot 7** | $3199 \sim 3656$ ($31.99 \sim 36.56$) | $3269\text{ ticks}$ ($32.69\ \mu\text{s}$) | $54.29$ | **握手 #8**：切 GPIO 到 **SEL 6**；搬运 **SEL 5** ($80\text{B}$) $\rightarrow$ `raw_buff[100..119]`。 | **采样 SEL 6** $\rightarrow$ `seq_buff[0..19]` |
| **Slot 8** | $3656 \sim 4113$ ($36.56 \sim 41.13$) | $3726\text{ ticks}$ ($37.26\ \mu\text{s}$) | $58.86$ | **握手 #9**：切 GPIO 到 **SEL 7**；搬运 **SEL 6** ($80\text{B}$) $\rightarrow$ `raw_buff[120..139]`。 | **采样 SEL 7** $\rightarrow$ `seq_buff[0..19]` |
| **Slot 9** | $4113 \sim 4570$ ($41.13 \sim 45.70$) | $4183\text{ ticks}$ ($41.83\ \mu\text{s}$) | $63.43$ | **握手 #10**：切 GPIO 到 **全清零**；搬运 **SEL 7** ($80\text{B}$) $\rightarrow$ `raw_buff[140..159]`。 | 触发 1 次 dummy 采样（结果被忽略）。 |
| **Slot 10** | $4570 \sim 4640$ ($45.70 \sim 46.40$) | **$4640\text{ ticks}$** ($46.40\ \mu\text{s}$) | **$68.00$** | **握手 #11 (收尾描述符)**：<br>1. Desc 5.1 停 `TMR_DMA`；<br>2. Desc 5.2 停 `TMR_ADC`；<br>3. Desc 5.3 清零 Busy 标志；<br>4. Desc 5.4 翻转魔数并触发 **HDMA TC 中断**。 | 定时器已停止，无转换。 |

---

## 七、 阶段 4：中断服务与帧空闲余量

| 绝对时刻 ($\text{ticks}$) | 绝对时刻 ($\mu\text{s}$) | 系统与 CPU 动作 |
| :--- | :--- | :--- |
| **$\approx 6850$** | **$\approx 68.50$** | **进入 `isr_hdma()`**：<br>1. 清除 HDMA 通道传输完成标志；<br>2. 调用 `hall_recv_cb(raw_buff[bufidx])` 将完整的 160 words 采样帧提交上层处理；<br>3. 读取 `GPTMR1_CH0` 的当前计数值并写入 `g_sensor_matrix_timing.sof_tick`。 |
| $6850 \sim 12500$ | $68.50 \sim 125.00$ | **CPU 帧空闲窗口（持续约 $56.50\ \mu\text{s} / 5650\text{ ticks}$，占总周期 $45.2\%$）**：<br>用于矩阵解算、数字滤波、校准算法运算及 USB / CAN 通讯报文打包。等待下一个 8 kHz SOF 信号触发。 |

---

## 八、 实测调试与验证指标

在软件在线调试（J-Link / DAPLink）时，可通过观察 `g_sensor_matrix_timing` 结构体成员验证实际时序是否与设计符合：

1. **`sof_tick` 验证**：
   * 理论预期值：$\mathbf{6800 \sim 6900\text{ ticks}}$
   * 换算公式：$\text{Time}_{(\mu\text{s})} = \text{sof\_tick} \times 0.01$
   * 若实测超过 $7000\text{ ticks}$，需检查 AHB 总线是否有高优先级 DMA 抢占总线带宽。
2. **`adc_window_ticks` 验证**：
   * 理论预期值：$\mathbf{276\text{ ticks}}$ ($2.76\ \mu\text{s}$)
   * 验证 ADC 序列长度与 $50\text{ MHz}$ 分频系数配置正确。
3. **`copy_words` 验证**：
   * 理论预期值：$\mathbf{20\text{ words}}$ ($80\text{ bytes}$)
   * 验证每次 SEL 搬运的物理通道数据完整性。