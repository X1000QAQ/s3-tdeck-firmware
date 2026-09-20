#pragma once

/**
 * WGS-84 <-> GCJ-02 coordinate transform (a.k.a. 火星坐标 / 国测局坐标).
 *
 * Chinese raster tile providers (高德/Gaode, 腾讯, Bing China ...) publish tiles in
 * GCJ-02. GPS and Meshtastic node positions are WGS-84. Without this transform,
 * everything drawn on a Chinese map is offset by roughly 300-600 m.
 *
 * Reference implementation of the well-known public algorithm.
 */
#include <cmath>
#include <stdint.h>

class CoordTransform
{
  public:
    static constexpr double kPi = 3.14159265358979323846;
    // Krasovsky 1940 ellipsoid
    static constexpr double A = 6378245.0;
    static constexpr double EE = 0.00669342162296594323;

    static bool outOfChina(double lat, double lon)
    {
        return (lon < 72.004 || lon > 137.8347 || lat < 0.8293 || lat > 55.8271);
    }

    static void wgs2gcj(double &lat, double &lon)
    {
        if (outOfChina(lat, lon))
            return;
        double dLat = transformLat(lon - 105.0, lat - 35.0);
        double dLon = transformLon(lon - 105.0, lat - 35.0);
        double radLat = lat / 180.0 * kPi;
        double magic = std::sin(radLat);
        magic = 1 - EE * magic * magic;
        double sqrtMagic = std::sqrt(magic);
        dLat = (dLat * 180.0) / ((A * (1 - EE)) / (magic * sqrtMagic) * kPi);
        dLon = (dLon * 180.0) / (A / sqrtMagic * std::cos(radLat) * kPi);
        lat += dLat;
        lon += dLon;
    }

    // inverse transform by iteration (accurate to ~1e-7 deg)
    static void gcj2wgs(double &lat, double &lon)
    {
        if (outOfChina(lat, lon))
            return;
        double wLat = lat, wLon = lon;
        for (int i = 0; i < 3; i++) {
            double gLat = wLat, gLon = wLon;
            wgs2gcj(gLat, gLon);
            wLat += lat - gLat;
            wLon += lon - gLon;
        }
        lat = wLat;
        lon = wLon;
    }

  private:
    static double transformLat(double x, double y)
    {
        double ret = -100.0 + 2.0 * x + 3.0 * y + 0.2 * y * y + 0.1 * x * y + 0.2 * std::sqrt(std::fabs(x));
        ret += (20.0 * std::sin(6.0 * x * kPi) + 20.0 * std::sin(2.0 * x * kPi)) * 2.0 / 3.0;
        ret += (20.0 * std::sin(y * kPi) + 40.0 * std::sin(y / 3.0 * kPi)) * 2.0 / 3.0;
        ret += (160.0 * std::sin(y / 12.0 * kPi) + 320 * std::sin(y * kPi / 30.0)) * 2.0 / 3.0;
        return ret;
    }

    static double transformLon(double x, double y)
    {
        double ret = 300.0 + x + 2.0 * y + 0.1 * x * x + 0.1 * x * y + 0.1 * std::sqrt(std::fabs(x));
        ret += (20.0 * std::sin(6.0 * x * kPi) + 20.0 * std::sin(2.0 * x * kPi)) * 2.0 / 3.0;
        ret += (20.0 * std::sin(x * kPi) + 40.0 * std::sin(x / 3.0 * kPi)) * 2.0 / 3.0;
        ret += (150.0 * std::sin(x / 12.0 * kPi) + 300.0 * std::sin(x / 30.0 * kPi)) * 2.0 / 3.0;
        return ret;
    }
};
