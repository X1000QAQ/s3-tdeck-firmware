# T-Deck（ESP32-S3）自定义固件 —— 中文版 / 地图与 RTC 修复

> 基于 **Meshtastic firmware v2.7.26** 的**个人定制分支** ✓
> 目标平台：**T-Deck 克隆板**（ESP32-S3，无官方键盘，硬件与官方变体有多处差异）

## 这个分支做了什么

| 方向 | 内容 |
|---|---|
| **地图** | 修复"瓦片全灰"根因：地图中心 4 级来源（GPS>home>节点中心>默认）**无任何校验** ✗ ⇒ 非法纬度使 Web Mercator 算出 `yTile=0`（北极）⇒ 全灰且**完全静默** ✗ ⇒ 现新增统一校验 `sanitize()` 并让四来源全部过校验 ✓ |
| **时间** | 移植上游 `#11494`（RTC/NTP 时同步）：RTC 读取路径补 `settimeofday()` ⇒ POSIX 系统时钟同步 ⇒ UI 显示真实时间 ✓；另补 RTC 发现（开机 I²C 扫描扫不到时，在总线恢复后补探 `0x51`）✓ |
| **传感器** | 自写 BMP280/BME280 裸 I²C 驱动（避免通用库 `wire.begin()` 重配总线 ✗）+ 开机**一次性**总线恢复 + 有界重试 ✓ |
| **汉化** | 界面中文；预设下拉保持英文 ✓；About 标题译中（内容保持原文 ✓）；补齐 16 条词条 ✓ |
| **开箱即用** | CN 区域 / LONG_FAST / 频点1 / 时区 CST-8 / 默认关 WiFi / 位置精度 32 ✓ |
| **其它** | 环境模块延迟初始化并注册读数列表 ✓；繁体精简等 ✓ |

## 硬件（本分支适配的板子）

- T-Deck 克隆板（ESP32-S3）· 3.2" 触摸 TFT · GPS(RX 19/TX 20) · SD 卡
- I²C：SDA 18 / SCL 8（触摸 + 传感器 0x76/0x77 + RTC PCF8563 0x51）
- SPI：SCK 40 / MISO 38 / MOSI 41 / SD_CS 39（**屏/电台/SD 共用** ✗ 需注意时序）
- ⚠️ GPIO21 = IP5306 电源键线（**不是**蜂鸣器 ✗）；蜂鸣器在 GPIO6
- ⚠️ 无官方键盘（0x55 地址是空的 ✗ ⇒ 已关其轮询）

## 构建

```bash
cd <repo>
/home/x1000qaq/.pio-venv/bin/pio run -e t-deck-tft     # 或用你本机的 pio
```

- UI 库（device-ui）**已内嵌**在 `libs/device-ui/` ✓ ⇒ **clone 即编** ✓（无需外部依赖）
- 产物：`.pio/build/t-deck-tft/firmware-t-deck-tft-<版本>.<git短哈希>.bin`
  ⚠️ 文件名含 **git 短哈希** ⇒ 脚本里**别写死名字** ✗

## 烧录

**三种方式**（预编译固件见 [Releases](https://github.com/X1000QAQ/s3-tdeck-firmware/releases)）：

| 场景 | 用哪个 | 偏移 |
|---|---|---|
| **常规升级**（设备已在跑 Meshtastic） | 应用 `…bin` | `0x10000` |
| **空板 / 分区表被改过** | 整片 `…factory.bin` | `0x0` |
| **分件写** | `bootloader.bin` → `0x0`，`partitions.bin` → `0x8000`，应用 → `0x10000` | 各自偏移 |

```bash
# 常规（推荐）
python -m esptool --chip esp32s3 --port <COM> write-flash 0x10000 <app.bin>
# 整片
python -m esptool --chip esp32s3 --port <COM> write-flash 0x0 <factory.bin>
```

⚠️ 两种方式**不要混用**（只写应用就够的场景别顺手全擦）
（设备进下载模式：按住中键 + 插 USB）

## 许可与致谢

- 本分支基于 **Meshtastic firmware**（**GPLv3**）✓ ⇒ 本分支同样以 **GPLv3** 发布 ✓
  （见仓库根 `LICENSE`；上游原文见 `README-upstream.md`）
- 上游项目：https://github.com/meshtastic/firmware

### 特别致谢

- 本项目适配的硬件（T-Deck 克隆板）由 **arkbird**（闲鱼）**自行设计并免费分享** ✓
  —— 他也是 LoRa 爱好者，把自己做的 **S3（T-Deck）** 与 **C3（Lora_C3_v1.4）**
  两套小板连同配套资料一起分享给了我，本项目的折腾才有起点 ✓
- ⚠️ arkbird 提供的**原始固件为闭源** ✗ ⇒ **未包含在本仓库** ✓
  （本仓库只包含基于 Meshtastic 开源部分 + 我自己的修改 ✓ 请勿向本仓库索取其固件 ✗）

## 免责

个人自用与学习目的 ✓ 请遵守当地无线电法规 ✓（CN 区域 tx_power 已按法规上限设置 ✓）
