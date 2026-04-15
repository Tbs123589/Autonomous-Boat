# BOAT_STM32 船载控制系统标准开发工程

本项目是基于 **STM32H743IIT6**（根据实际型号修改）的开发标准模版。

本文前面部分为协作文档，后面部分为工程配置描述。

---

## 1. 工程目录结构说明
本项目采用分层设计，请遵循以下规范进行代码添加，以方便后续合并工程：

```text
BOAT_STM32/
├── Core/             # CubeMX自动生成，存放中断处理及初始化入口 (main.c)
├── Drivers/          # CMSIS与HAL底层驱动库
├── MDK-ARM/          # Keil 工程文件及编译中间件
├── Middlewares/      # 未来存放 RTOS、DSP 库或算法组件
├── UserApp/          # [重点] 开发者存放应用层逻辑代码 (.c/.h)
│   ├── Sensors/      # 建议存放 BMI088/BMM150 等传感器封装驱动
│   └── Modules/      # 存放控制算法、导航逻辑等
├── .vscode/          # VSCode 辅助配置文件
└── Boat_STM32.ioc    # CubeMX 配置文件（修改硬件配置请先打开此文件）
```

### 1.1 目录权责详细说明

* **`Core/` (底层初始化层) - main函数**
    * **核心内容**：**由 CubeMX 自动生成**的 `main.c`、中断处理 `stm32h7xx_it.c` 以及系统时钟配置。
    * 仅在此处添加**外设初始化逻辑**（如 `MX_GPIO_Init`）。业务逻辑代码严禁大量堆积在 `main.c` 中，应通过调用 `UserApp` 中的接口实现。
* **`Drivers/` (硬件抽象层) - 不用改**
    * 包含 ST 官方提供的 **HAL 库**和 **CMSIS 底层内核支持**。
* **`MDK-ARM/` (工程构建层) - 不用改**
    * **核心内容**：Keil 工程文件 (`.uvprojx`)、分散加载文件 (`.sct`) 以及编译产生的中间文件 (`Build/`)。
* **`UserApp/` (应用与驱动层) —— [开发者主要工作区]**
    * **`Sensors/` (硬件抽象/BSP)**：存放具体硬件的驱动（如 BMI088）。它负责屏蔽底层的 SPI/I2C 细节，向上层提供纯粹的数据接口。
    * **`Modules/` (功能模块)**：存放控制逻辑（如 PID 算法、卡尔曼滤波、航线调度）。该层不应直接操作寄存器或 HAL 库函数，而应调用 `Sensors/` 提供的接口。
* **`Boat_STM32.ioc` (工程灵魂)**
    * 这是整个工程的硬件蓝图。**任何引脚修改必须先在 ioc 中调整**。修改后重新生成代码时，CubeMX 会扫描整个工程，并确保 `Core/` 文件夹同步更新，同时保留 `USER CODE` 标签内的逻辑。

---

### 1.2 协作开发流向图
为了帮助大家理解合并逻辑，可以参考以下开发流程：
1.  **硬件修改**：打开 `.ioc` -> 分配引脚 -> 生成代码 -> 更新引脚表。
2.  **驱动编写**：在 `UserApp/Sensors` 创建新外设驱动。
3.  **逻辑实现**：在 `UserApp/Modules` 编写控制代码并调用驱动。
4.  **接口关联**：在 `Core/main.c` 的 `while(1)` 中调用 `Modules` 的入口函数。

---

### 💡 技巧建议
“如果一个 `.c` 文件里出现了 `HAL_` 开头的函数，它通常应该待在 `Sensors/`；如果它只涉及数学运算或状态跳转，它应该待在 `Modules/`。” 这样定义能极大减少合并代码时的冲突。
---

## 2. 开发者扩展指引
为了保持工程的整洁和可维护性，请遵循以下步骤添加新功能：

### 2.1 新增外设与引脚
1. **CubeMX 配置**：
   * 打开 `Boat_STM32.ioc`。
   * 配置外设，并为引脚设置 **User Label**（例如：`DEBUG_UART_TX`）。
   * 点击 `Generate Code` 生成代码。

### 2.2 代码编写规范
* **底层驱动 (BSP)**：若新增传感器或硬件模块，请在 `UserApp/Sensors` 或 `UserApp/Bsp` 下新建 `.c/.h` 文件封装驱动。
* **业务逻辑**：具体的控制算法或任务逻辑请放在 `UserApp/Modules`。
* **接口隔离**：尽量不要在 `main.c` 中直接写复杂的业务代码。在 `main.c` 的初始化区域调用你的初始化函数，在 `while(1)` 中调用你的逻辑入口。

---

## 3. 开发环境与配置提醒
* **IDE**: Keil uVision v5.32+ (必须勾选 **Use MicroLib**)
* **下载设置**: 请确保 `Debug` -> `Settings` -> `Flash Download` 中勾选了 **Reset and Run**。

---

## 4. 协作开发规范（必读）

### 4.1 串口调试与 Log 规范
为了统一调试信息的输出格式并确保系统兼容性，请遵循以下约定：

* **重定向机制**：后续若实现串口驱动，请务必在 `USER CODE` 区域关联 `huart1`，并确保在 Keil 项目属性中勾选 **Target -> Use MicroLib**。

### 4.2 CubeMX 代码保护
在重新生成代码（Generate Code）时，请务必将你的代码写在 `/* USER CODE BEGIN */` 和 `/* USER CODE END */` 之间，否则会被 CubeMX 覆盖。

### 4.3 内存一致性问题 (H7 D-Cache)
由于开启了 D-Cache，若后续使用 DMA 传输数据，请注意：
1. 使用 `__attribute__((section(".RAM_D1")))` 定义 DMA 缓冲区。
2. 或在传输前后调用 `SCB_CleanDCache()` 和 `SCB_InvalidateDCache()`。

---

## 5. 快速上手步骤
1. 打开 `MDK-ARM/Boat_STM32.uvprojx`。
2. 点击 **Rebuild All** 检查编译环境。
3. 连接调试器（ST-Link/DAP），点击 **Download**。
4. 观察板载 LED 或通过串口助手查看启动日志。

---
以下为工程配置描述
---

## 1. 开发环境要求
为确保工程能够正常编译与调试，请开发者安装以下工具：
* **IDE**: Keil uVision v5.32 或更高版本。
* **Compiler**: ARM Compiler V6 (AC6) —— *建议使用，H7 在 AC6 下性能更优*。
* **STM32CubeMX**: v6.10.0 或更高版本。
* **DFP Pack**: `Keil.STM32H7xx_DFP` 最新版。

---

## 2. 硬件资源配置
### 时钟树 (Clock Configuration)
* **外部晶振 (HSE)**: 25.0 MHz
* **主频 (SYSCLK)**: 480 MHz (VOS0)
* **AHB 总线 (HCLK)**: 240 MHz (HPRE Div 2)
* **APB 总线**: 120 MHz

### 标准工程关键接口定义 (SPI/I2C/UART)
| 模块 | 接口 | 引脚 | 说明 |
| :--- | :--- | :--- | :--- |
| **Debug Console** | UART1 | PA9(TX), PA10(RX) | 115200 8-N-1 |


---
**Maintainer:** [wjj]  
**Update:** 2026-04-15

---
