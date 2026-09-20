#include "configuration.h"
#include "detect/ScanI2C.h"
#include "gps/RTC.h"
#include "main.h"

#if !MESHTASTIC_EXCLUDE_ENVIRONMENTAL_SENSOR && __has_include(<Adafruit_BMP280.h>)

#include "../mesh/generated/meshtastic/telemetry.pb.h"
#include "BMP280Sensor.h"
#include "TelemetrySensor.h"
#include <Adafruit_BMP280.h>
#include <typeinfo>
#include <Wire.h>

BMP280Sensor::BMP280Sensor() : TelemetrySensor(meshtastic_TelemetrySensorType_BMP280, "BMP280") {}

// ===================================================================================
// v43（方案 B）：完全自写的 BMP280 I2C 驱动 —— 不依赖 Adafruit 的 begin()/readXxx()
//   why：Adafruit 的 begin() 内部会调用 _wire->begin()（不带参数），在双端口/自定义引脚
//        的板子上会把总线指向默认引脚(21/22)，导致 chip id 读不到 ⇒ 初始化必失败。
//   这里直接用 TwoWire 的读写原语 + Bosch 官方补偿公式（数据手册 4.2.3 / 4.3.1）。
// ===================================================================================

#define BMP280_REG_CALIB   0x88
#define BMP280_REG_CHIPID  0xD0
#define BMP280_REG_RESET   0xE0
#define BMP280_REG_CONFIG  0xF5
#define BMP280_REG_CTRL    0xF4
#define BMP280_REG_DATA    0xF7
#define BMP280_CHIP_ID     0x58   // BMP280
#define BME280_CHIP_ID     0x60   // v46: BME280（克隆板常用，寄存器/补偿公式与 BMP280 一致）

// 哨兵值（初始化/读取失败时上报，便于远程判断）
#define BMP280_FAIL_SENTINEL_TEMP (-88.0F)   // v52 指纹
#define BMP280_FAIL_SENTINEL_PRESS (777.0F)   // v52 指纹
#define BMP280_SENTINEL_NO_MAPPED (-97.0F)   // v44: 连映射总线都没有
#define BMP280_SENTINEL_MAPPED_FAIL (-98.0F) // v44: 映射总线探测失败
#define BMP280_SENTINEL_UNKNOWN_ID (-96.0F)  // v46: 芯片有响应但 id 既非 0x58 也非 0x60

namespace {
// --- 本文件内状态（文件作用域，避免改头文件）---
TwoWire *g_bus = nullptr;
float g_sentinelTemp = BMP280_FAIL_SENTINEL_TEMP;
float g_sentinelPress = BMP280_FAIL_SENTINEL_PRESS;
uint8_t g_lastChipId = 0;
bool g_scanDone = false;   // v47
bool g_probeOnceDone = false;   // v55: 诊断/总线恢复只做一次
int  g_retryCount = 0;          // v58: 有界重试计数
uint8_t g_addr = 0;
bool g_ready = false;

uint16_t dig_T1 = 0;
int16_t dig_T2 = 0, dig_T3 = 0;
uint16_t dig_P1 = 0;
int16_t dig_P2 = 0, dig_P3 = 0, dig_P4 = 0, dig_P5 = 0, dig_P6 = 0, dig_P7 = 0, dig_P8 = 0, dig_P9 = 0;
int32_t t_fine = 0;

bool rd(TwoWire *b, uint8_t addr, uint8_t reg, uint8_t *buf, size_t len)
{
    if (!b)
        return false;
    b->beginTransmission(addr);
    b->write(reg);
    if (b->endTransmission(false) != 0)
        return false;
    if (b->requestFrom((int)addr, (int)len) != (int)len)
        return false;
    for (size_t i = 0; i < len; i++)
        buf[i] = b->read();
    return true;
}

bool wr(TwoWire *b, uint8_t addr, uint8_t reg, uint8_t val)
{
    if (!b)
        return false;
    b->beginTransmission(addr);
    b->write(reg);
    b->write(val);
    return b->endTransmission() == 0;
}

// 在给定总线上探测芯片（读 chip id）
bool probe(TwoWire *b, uint8_t addr)
{
    uint8_t id = 0;
    if (!rd(b, addr, BMP280_REG_CHIPID, &id, 1))
        return false;
    g_lastChipId = id;   // v46: 记录真实 id，便于远程诊断
    // v46: BME280(0x60) 与 BMP280(0x58) 的数据寄存器与补偿公式完全一致，都接受
    return (id == BMP280_CHIP_ID || id == BME280_CHIP_ID);
}

bool loadCalib(TwoWire *b, uint8_t addr)
{
    uint8_t c[24];
    if (!rd(b, addr, BMP280_REG_CALIB, c, 24))
        return false;
    dig_T1 = (uint16_t)(c[1] << 8 | c[0]);
    dig_T2 = (int16_t)(c[3] << 8 | c[2]);
    dig_T3 = (int16_t)(c[5] << 8 | c[4]);
    dig_P1 = (uint16_t)(c[7] << 8 | c[6]);
    dig_P2 = (int16_t)(c[9] << 8 | c[8]);
    dig_P3 = (int16_t)(c[11] << 8 | c[10]);
    dig_P4 = (int16_t)(c[13] << 8 | c[12]);
    dig_P5 = (int16_t)(c[15] << 8 | c[14]);
    dig_P6 = (int16_t)(c[17] << 8 | c[16]);
    dig_P7 = (int16_t)(c[19] << 8 | c[18]);
    dig_P8 = (int16_t)(c[21] << 8 | c[20]);
    dig_P9 = (int16_t)(c[23] << 8 | c[22]);
    return dig_T1 != 0 || dig_P1 != 0;
}

// Bosch 官方补偿算法
int32_t compensateT(int32_t adc_T)
{
    int32_t var1 = ((((adc_T >> 3) - ((int32_t)dig_T1 << 1))) * ((int32_t)dig_T2)) >> 11;
    int32_t var2 = (((((adc_T >> 4) - ((int32_t)dig_T1)) * ((adc_T >> 4) - ((int32_t)dig_T1))) >> 12) *
                    ((int32_t)dig_T3)) >> 14;
    t_fine = var1 + var2;
    return (t_fine * 5 + 128) >> 8; // 0.01 °C
}

uint32_t compensateP(int32_t adc_P)
{
    int64_t var1, var2, p;
    var1 = ((int64_t)t_fine) - 128000;
    var2 = var1 * var1 * (int64_t)dig_P6;
    var2 = var2 + ((var1 * (int64_t)dig_P5) << 17);
    var2 = var2 + (((int64_t)dig_P4) << 35);
    var1 = ((var1 * var1 * (int64_t)dig_P3) >> 8) + ((var1 * (int64_t)dig_P2) << 12);
    var1 = (((((int64_t)1) << 47) + var1)) * ((int64_t)dig_P1) >> 33;
    if (var1 == 0)
        return 0;
    p = 1048576 - adc_P;
    p = (((p << 31) - var2) * 3125) / var1;
    var1 = (((int64_t)dig_P9) * (p >> 13) * (p >> 13)) >> 25;
    var2 = (((int64_t)dig_P8) * p) >> 19;
    p = ((p + var1 + var2) >> 8) + (((int64_t)dig_P7) << 4);
    return (uint32_t)p; // Q24.8 Pa ⇒ /256 = Pa
}
} // namespace

bool BMP280Sensor::getMetrics(meshtastic_Telemetry *measurement)
{
    // v58 精简版：没就绪就不上报（返回 false ⇒ 上层视为无效 ✓）不再注入哨兵假值 ✗
    if (!g_ready || !g_bus)
        return false;

    measurement->variant.environment_metrics.has_temperature = true;
    measurement->variant.environment_metrics.has_barometric_pressure = true;

    uint8_t d[6];
    if (!rd(g_bus, g_addr, BMP280_REG_DATA, d, 6)) {
        measurement->variant.environment_metrics.temperature = BMP280_FAIL_SENTINEL_TEMP;
        measurement->variant.environment_metrics.barometric_pressure = BMP280_FAIL_SENTINEL_PRESS;
        return true;
    }

    int32_t adc_P = (int32_t)(((uint32_t)d[0] << 12) | ((uint32_t)d[1] << 4) | ((uint32_t)d[2] >> 4));
    int32_t adc_T = (int32_t)(((uint32_t)d[3] << 12) | ((uint32_t)d[4] << 4) | ((uint32_t)d[5] >> 4));

    float temp = compensateT(adc_T) / 100.0F;
    float pres = compensateP(adc_P) / 256.0F / 100.0F; // Pa → hPa

    measurement->variant.environment_metrics.temperature = temp;
    measurement->variant.environment_metrics.barometric_pressure = pres;
    return true;
}

// 保留新机制入口（本板不走它）
bool BMP280Sensor::initDevice(TwoWire *bus, ScanI2C::FoundDevice *dev)
{
    if (!bus || !dev)
        return false;
    g_bus = bus;
    g_addr = dev->address.address;
    if (!probe(g_bus, g_addr) || !loadCalib(g_bus, g_addr))
        return false;
    wr(g_bus, g_addr, BMP280_REG_CONFIG, 0x00);
    wr(g_bus, g_addr, BMP280_REG_CTRL, 0x27); // ×1/×1/normal
    g_ready = true;
    return true;
}

// v43: 延迟初始化 —— 依次在候选总线上探测 chip id(0x58)，谁认就用谁
int32_t BMP280Sensor::runOnce()
{
    // v65: 【撤销 v63 的 uiReady 门】✗ —— 实测在 device-ui(LVGL) 构建下
    //      screen->setup() 之后的置位代码执行不到 ⇒ 门永久关闭 ⇒ 传感器彻底不跑 ✗
    // ===== v58 精简版：只保留必需 =====
    //   why：v47~v57 的诊断代码缺一次性保护 ⇒ 每秒重跑 4 次 I²C 探测 + Wire.begin()
    //        ⇒ 与 SD/SPI 争用 ⇒ 地图瓦片全失败（实测 227 次/90s）✗
    //   现在：① 开机只做一次总线恢复 ✓ ② 只试 0x76/0x77 读 chip id ✓ ③ 有界重试 ✓
    static bool recovered = false;
    if (!recovered) {
        recovered = true;
        TwoWire *sb0 = nodeTelemetrySensorsMap[sensorType].second ? nodeTelemetrySensorsMap[sensorType].second : &Wire;
        pinMode(I2C_SCL, OUTPUT);
        pinMode(I2C_SDA, INPUT_PULLUP);
        for (int i = 0; i < 9; i++) {
            digitalWrite(I2C_SCL, LOW);
            delayMicroseconds(5);
            digitalWrite(I2C_SCL, HIGH);
            delayMicroseconds(5);
        }
        pinMode(I2C_SDA, OUTPUT);
        digitalWrite(I2C_SDA, LOW);
        delayMicroseconds(5);
        digitalWrite(I2C_SDA, HIGH);
        delayMicroseconds(5);
        sb0->end();
        delay(20);
        sb0->begin(I2C_SDA, I2C_SCL, 100000);
        delay(20);
        LOG_INFO("v58: one-shot bus recovery done");
    }

    // ================= v60: 恢复之后【补做 RTC 发现】=================
    //   why：开机 I²C 扫描跑在总线还是"卡死态"时 ⇒ 没扫到 RTC ✗
    //        ⇒ RTC.cpp 那道门 (rtc_found.address == PCF8563_RTC) 永远关着 ✗
    //   做法：恢复完总线后自己探一次 0x51 ⇒ ACK 就【手动把 rtc_found 填上】+ 读一次 RTC ✓
    //   只做一次 ✓（不会每秒跑 ✗）
    static bool rtcDiscovered = false;
    if (!rtcDiscovered) {
        rtcDiscovered = true;
        TwoWire *rb = nodeTelemetrySensorsMap[sensorType].second ? nodeTelemetrySensorsMap[sensorType].second : &Wire;
        rb->beginTransmission(0x51);
        if (rb->endTransmission() == 0) {
            rtc_found = ScanI2C::DeviceAddress(ScanI2C::I2CPort::WIRE, 0x51);
            RTCSetResult r = readFromRTC();
            LOG_INFO("v60: RTC found after recovery (0x51 ACK), readFromRTC=%d", (int)r);
        } else {
            LOG_WARN("v60: RTC 0x51 not answering even after recovery");
        }
    }

    if (g_ready)
        return DEFAULT_SENSOR_MINIMUM_WAIT_TIME_BETWEEN_READS;

    TwoWire *sb = nodeTelemetrySensorsMap[sensorType].second ? nodeTelemetrySensorsMap[sensorType].second : &Wire;
    for (uint8_t a = 0x76; a <= 0x77; a++) {
        uint8_t id = 0;
        if (rd(sb, a, BMP280_REG_CHIPID, &id, 1) && (id == BMP280_CHIP_ID || id == BME280_CHIP_ID)) {
            g_lastChipId = id;
            if (loadCalib(sb, a)) {
                wr(sb, a, BMP280_REG_CONFIG, 0x00);
                wr(sb, a, BMP280_REG_CTRL, 0x27);
                g_addr = a;
                g_bus = sb;
                g_ready = true;
                LOG_INFO("v58: sensor ready at 0x%02X (chipid=0x%02X)", a, id);
                return DEFAULT_SENSOR_MINIMUM_WAIT_TIME_BETWEEN_READS;
            }
        }
    }

    g_retryCount++;
    if (g_retryCount <= 3) {
        LOG_WARN("v58: probe failed try %d/3, retry in 2s", g_retryCount);
        return 2000;
    }
    return 60000;
}

#endif
