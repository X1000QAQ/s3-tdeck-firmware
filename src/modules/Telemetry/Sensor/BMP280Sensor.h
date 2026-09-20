#include "configuration.h"

#if !MESHTASTIC_EXCLUDE_ENVIRONMENTAL_SENSOR && __has_include(<Adafruit_BMP280.h>)

#include "../mesh/generated/meshtastic/telemetry.pb.h"
#include "TelemetrySensor.h"
#include <Adafruit_BMP280.h>

class BMP280Sensor : public TelemetrySensor
{
  private:
    Adafruit_BMP280 bmp280;

  public:
    BMP280Sensor();
    virtual bool getMetrics(meshtastic_Telemetry *measurement) override;
    virtual bool initDevice(TwoWire *bus, ScanI2C::FoundDevice *dev) override;
    // v38: 2.7.9 式延迟初始化（不接收扫描器指针，改从全局 nodeTelemetrySensorsMap 取总线/地址）
    virtual int32_t runOnce() override;
};

#endif