# TinyTextDisplay

一个基于 ESP32 的小型蓝牙滚动字幕显示项目。

TinyTextDisplay 使用 ESP32 驱动 ST7789P3 SPI TFT LCD，通过蓝牙接收文本信息，并在屏幕上进行滚动显示。

项目特点是自制中文字库系统，支持在资源有限的嵌入式设备上显示中文、繁体中文以及日文字符。

---

## ✨ 功能特点

- 基于 ESP32 开发
- 蓝牙无线发送文字
- TFT LCD 滚动字幕显示
- 自制点阵字库系统
- 支持 CJK 字符显示：
  - 简体中文
  - 繁体中文
  - 日文假名
  - 日文汉字
- 支持 ASCII 字符
- SPI 屏幕驱动
- 低资源占用
- 可扩展的信息显示平台


---

# 硬件

当前测试硬件：

| 部件 | 型号 |
|-|-|
| 主控 | ESP32 |
| 显示屏 | ST7789 SPI TFT LCD |
| 屏幕尺寸 | 2.25 英寸 |
| 分辨率 | 284×76 |

---

## 屏幕连接

| LCD | ESP32 |
|-|-|
| CS | GPIO5 |
| DC | GPIO17 |
| RST | GPIO16 |
| MOSI | GPIO23 |
| SCK | GPIO18 |
| VCC | 3.3V |
| GND | GND |

---

# 软件环境

## Arduino IDE

推荐：

- Arduino IDE 2.x
- ESP32 Arduino Core

需要：

- ESP32 开发板支持包
- SPI 库

---

# 字库系统

TinyTextDisplay 使用自制字库生成工具，将字体文件转换为适用于 ESP32 的点阵数据。

支持：

- 中文
- 繁体中文
- 日文
- ASCII
- 标点符号


字体来源：

- Noto CJK 字体系列
