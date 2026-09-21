
#define TFT_CS 12

// ST7789 TFT LCD
#define ST7789_CS TFT_CS
#define ST7789_RS 11  // DC
#define ST7789_SDA 41 // MOSI
#define ST7789_SCK 40
#define ST7789_RESET -1
#define ST7789_MISO 38
#define ST7789_BUSY -1
#define ST7789_BL 42
#define ST7789_SPI_HOST SPI2_HOST
#define TFT_BL 42
#define SPI_FREQUENCY 40000000
#define SPI_READ_FREQUENCY 16000000
#define TFT_HEIGHT 320
#define TFT_WIDTH 240
#define TFT_OFFSET_X 0
#define TFT_OFFSET_Y 0
#define TFT_OFFSET_ROTATION 0
#define SCREEN_ROTATE
#define SCREEN_TRANSITION_FRAMERATE 5
#define BRIGHTNESS_DEFAULT 130 // Medium Low Brightness
#define USE_TFTDISPLAY 1
// v33: 本克隆板【没有】T-Deck 官方键盘（输入是摇杆 encoder + 触摸）
// 但官方变体启用了 HAS_PHYSICAL_KEYBOARD ⇒ 固件每 ~50ms 去敲 0x55 键盘
// ⇒ 397 次/分钟 I²C 超时错误刷屏（实测定点：addr=0x55）
// #define HAS_PHYSICAL_KEYBOARD 1

#define HAS_TOUCHSCREEN 1
#define SCREEN_TOUCH_INT 16
#define TOUCH_I2C_PORT 0
#define TOUCH_SLAVE_ADDRESS 0x5D // GT911

#define USE_POWERSAVE
#define SLEEP_TIME 120

#define TB_PRESS 0
#define BUTTON_ACTIVE_LOW true
#define BUTTON_ACTIVE_PULLUP true

#define GPS_DEFAULT_NOT_PRESENT 1

// ---- Step2 硬件适配（本板克隆板）----
#define PIN_BUZZER 6      // 蜂鸣器 GPIO6（反编译商家 v279 确认，PWM 驱动）
// v35: 恢复 RTC —— 点名探针证明 20Hz 噪声元凶是【键盘 0x55】(v33 已修)，RTC 清白。
// 用户已更换 RTC 备份电池 ⇒ 验证时间能否跨重启保留。
#define PCF8563_RTC 0x51  // 板上 PCF8563 RTC（2.7.26 用 SensorLib → 宏放 variant.h，与 tbeam-s3-core 同款）
#define TCXO_OPTIONAL     // 先按 TCXO 1.8V 试，失败自动退回 XTAL（与商家 v279 一致）
#define GPS_RX_PIN 19 // Step2 硬件适配：本板实测 RX=GPIO19（非官方 44）
#define GPS_TX_PIN 20 // Step2 硬件适配：本板实测 TX=GPIO20（非官方 43）

// Have SPI interface SD card slot
// #define HAS_SDCARD // --> needs to be in platform.ini for device-ui
#define SPI_MOSI (41)
#define SPI_SCK (40)
#define SPI_MISO (38)
#define SPI_CS (39)
#define SDCARD_CS SPI_CS
#define SD_SPI_FREQUENCY 50000000U

#define BATTERY_PIN 4 // A battery voltage measurement pin, voltage divider connected here to measure battery voltage
// ratio of voltage divider = 2.0 (RD2=100k, RD3=100k)
//
// 2026-09-21 实测校准（★ 经一轮纠错）：2.0 → **2.027**
//   同一状态（都在充电中）实测：CLI/屏读 **4.28V** ↔ 万用表 **4.207V** ⇒ 比值 0.98294
//   ⇒ 2.062 × 0.98294 = **2.027** ✓
//   ⚠️ 曾经改成 2.062 是**错的**：那对数据（屏 4.05V / 万用表 4.175V）不是同一时刻取的，
//      算出来偏大约 1.7% ⇒ 静置电池被显示成 4.26V ⇒ 越过 4200mV 而被【误判为在充电】✗
//   正确做法：必须【同一状态、同一时刻】比（充电中比充电中、静置比静置）✓
//   参考：上游/商家变体用 2.11（偏高约 4%）；本板实测 2.027
#define ADC_MULTIPLIER 2.027
#define ADC_CHANNEL ADC1_GPIO4_CHANNEL

// keyboard
#define I2C_SDA 18 // I2C pins for this board
#define I2C_SCL 8
#define KB_POWERON 10                  // must be set to HIGH
#define KB_SLAVE_ADDRESS TDECK_KB_ADDR // 0x55
#define KB_BL_PIN 46                   // not used for now

// trackball
#define HAS_TRACKBALL 1
#define TB_UP 3
#define TB_DOWN 15
#define TB_LEFT 1
#define TB_RIGHT 2
#define TB_PRESS 0 // BUTTON_PIN
#define TB_DIRECTION FALLING
#define TB_THRESHOLD 3

// microphone
#define ES7210_SCK 47
#define ES7210_DIN 14
#define ES7210_LRCK 21
#define ES7210_MCLK 48

// dac / amp
#define HAS_I2S
#define DAC_I2S_BCK 7
#define DAC_I2S_WS 5
#define DAC_I2S_DOUT 6
#define DAC_I2S_MCLK 21 // GPIO lrck mic

// LoRa
#define USE_SX1262
#define USE_SX1268

#define LORA_SCK 40
#define LORA_MISO 38
#define LORA_MOSI 41
#define LORA_CS 9

#define LORA_DIO0 -1 // a No connect on the SX1262 module
#define LORA_RESET 17
#define LORA_DIO1 45 // SX1262 IRQ
#define LORA_DIO2 13 // SX1262 BUSY
#define LORA_DIO3    // Not connected on PCB, but internally on the TTGO SX1262, if DIO3 is high the TXCO is enabled

#define SX126X_CS LORA_CS // FIXME - we really should define LORA_CS instead
#define SX126X_DIO1 LORA_DIO1
#define SX126X_BUSY LORA_DIO2
#define SX126X_RESET LORA_RESET
// Not really an E22 but TTGO seems to be trying to clone that
#define SX126X_DIO2_AS_RF_SWITCH
#define SX126X_DIO3_TCXO_VOLTAGE 1.8
// Internally the TTGO module hooks the SX1262-DIO2 in to control the TX/RX switch (which is the default for the sx1262interface
// code)

// 本克隆板无 T-Deck 官方键盘（输入=摇杆 encoder + 触摸）
// 关闭 UI 库里的键盘 I²C 轮询（0x55）—— 那是 20Hz 报错的真源头
#define TDECK_NO_I2C_KEYBOARD 1
