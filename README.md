# Smart Watch · STM32F411 智能手表

基于 STM32F411CEU6 的触屏智能手表固件。表盘显示 RTC 时间、日期和温湿度，向右滑动可打开功能菜单，使用秒表、倒计时或查看传感器读数。界面运行在 240 × 280 的 ST7789 屏幕上，触摸输入来自 CST816，后台采集由 FreeRTOS 调度。

仓库根目录是手表应用（APP）。首次给空白芯片烧录时，还需要配套的 32 KiB Bootloader；后续可通过蓝牙更新 APP。[硬件原理图](智能手表原理图.pdf) · [Bootloader 工程与升级工具](Smart_Watch_BootLoader.zip)

## 手表功能

| 功能 | 当前固件的行为 |
| --- | --- |
| 表盘与计时 | 显示时分、日期和星期，可在设置页修改时间和日期。功能菜单提供秒表和最长 59 分 59 秒的倒计时。 |
| 环境测量 | AHT21 提供温度和相对湿度，SPL06-001 提供气压，气压页面以 hPa 显示。 |
| 运动与光学采集 | MPU6050 页面显示三轴加速度和角速度原始值；LSM303DLHC 页面显示磁场原始值和加速度；EM7028 页面显示光学原始值。 |
| 显示与休眠 | 支持背光调节。连续 15 秒无操作后熄屏并进入 STOP 模式，按 KEY1 可唤醒。秒表在休眠前暂停，运行中的倒计时通过 RTC 闹钟唤醒手表。 |
| 蓝牙 | APP 支持开关蓝牙模块，开启后每 500 ms 通过 USART1 发送测试字符 `A`。固件升级由 Bootloader 接收。 |

表盘上的步数、电量和血氧数值目前是界面示例值。查看实际采样时，可打开温湿度、气压或光学数据页面。[表盘定义](Middlewares/MyUI/generated/screens/gg_main.c) · [采样数据显示](APP/Src/Services/watch_display.c)

## 构建 APP

在终端准备好 CMake 3.22 或以上版本、Ninja、Arm GNU Toolchain（`arm-none-eabi-gcc` 和 `arm-none-eabi-g++`）及 Python 3。工程自带 STM32 HAL、FreeRTOS 10.3.1 和 LVGL 9.4.0。日常开发可用 VS Code，外设配置可用 STM32CubeMX 打开 [Smart_Watch.ioc](Smart_Watch.ioc)。

```sh
git clone https://github.com/A-Cold-Dream/Smart_Watch.git
cd Smart_Watch
cmake --preset Release
cmake --build --preset Release
```

构建结束后，`build/Release/` 中有两个用途不同的文件：

| 产物 | 用途 |
| --- | --- |
| `Smart_Watch.elf` | 用 ST-Link 烧录或调试。文件包含 APP 的加载地址，以及 Bootloader 启动校验所需的长度和 CRC32 记录。 |
| `Smart_Watch.bin` | 通过蓝牙发送的 APP 镜像。Bootloader 在接收并校验整包后写入就绪记录。 |

调试构建可将两条 CMake 命令中的 `Release` 都换成 `Debug`。Python 字体压缩和固件打包由 CMake 自动执行。

## 首次烧录与蓝牙升级

STM32F411CEU6 的 512 KiB Flash 按下表划分。APP 的链接地址和中断向量表均从 `0x08008000` 开始。[链接脚本](STM32F411XX_FLASH.ld) · [固件打包脚本](cmake/package_app.py)

| 区域 | 地址范围（含首尾） | 容量 |
| --- | --- | ---: |
| Bootloader | `0x08000000–0x08007FFF` | 32 KiB |
| APP 镜像 | `0x08008000–0x0807FFEF` | 491,504 字节 |
| APP 就绪记录 | `0x0807FFF0–0x0807FFFF` | 16 字节 |

1. **安装 Bootloader。** 将仓库里的 `Smart_Watch_BootLoader.zip` 解压到与 `Smart_Watch` 并列的位置。在解压得到的 Bootloader 工程根目录运行 `cmake --preset Release -B out/Release`，再运行 `cmake --build out/Release`。用 STM32CubeProgrammer 和 ST-Link 将 `out/Release/Smart_Watch_BootLoader.bin` 写到 `0x08000000`。
2. **首次启动 APP。** 在 STM32CubeProgrammer 中选择根工程生成的 `Smart_Watch.elf`，按扇区擦除并下载，保留 Bootloader 所在的扇区 0 和 1。复位后，Bootloader 校验 APP，短按 KEY1 即可进入表盘。
3. **后续通过蓝牙升级。** 复位后在 Bootloader 的按键界面长按 KEY1 约 0.8 秒。若 APP 缺失或校验失败，手表会自动进入升级等待界面。在有 BLE 适配器的电脑上，从 **Bootloader 工程根目录** 运行：

   ```sh
   python -m pip install bleak
   python tools/ota_ble.py scan
   python tools/ota_ble.py send "扫描到的设备地址" "../Smart_Watch/build/Release/Smart_Watch.bin"
   ```

   手表显示 `Update complete` 后短按 KEY1 启动。若发送工具提示有多个可用特征，可先用 `python tools/ota_ble.py inspect "扫描到的设备地址"` 查看 UUID，再按压缩包内的 README 指定写入和通知特征。

## 源码导览

| 想了解什么 | 从这里开始 |
| --- | --- |
| 任务如何分工 | [Core/Src/freertos.c](Core/Src/freertos.c) 创建显示、按键、蓝牙和传感器任务。 |
| 数据如何显示到表盘 | [watch_display.c](APP/Src/Services/watch_display.c) 读取采样结果、更新 LVGL 控件，并将 RGB565 像素送到 LCD。界面布局位于 [Middlewares/MyUI](Middlewares/MyUI)。 |
| 熄屏后怎样保持倒计时 | [WatchTimer.c](APP/Src/Tasks/WatchTimer.c) 管理秒表和倒计时，[watch_power.c](APP/Src/Services/watch_power.c) 协调外设休眠、RTC 唤醒及恢复。 |
| 如何接入硬件 | [Components](Components) 包含屏幕、触摸和传感器驱动，[BSP](BSP) 提供按键、LED 和软件 I²C。 |

## 许可证

项目采用 [MIT 许可证](LICENSE)。仓库内第三方组件及 GUI Guider 生成文件的授权条款见对应目录或文件头。
