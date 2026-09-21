#pragma once

#include <stdint.h>
#include <string.h>
#include <string_view>

/**
 * Global settings for raster tile map
 */
class MapTileSettings
{
  public:
    static constexpr size_t PREFIX_SIZE = 10;
    static constexpr size_t TILE_STYLE_SIZE = 24;
    static constexpr size_t TILE_FORMAT_SIZE = 10;
    static constexpr const char *PMTILES_EXTENSION = ".pmtiles";
    static constexpr size_t PMTILES_EXTENSION_LEN = std::string_view(PMTILES_EXTENSION).size();

    MapTileSettings() = default;
    static uint8_t getDefaultZoom(void) { return zoomDefault; }

    // ================= v64: 统一的位置/缩放 合法性校验 =================
    //   why：GeoPoint 的 Web Mercator 换算【不校验纬度】✗
    //        非法纬度(>90° 或 NaN) ⇒ log(负数)=NaN ⇒ uint32_t(NaN)=0
    //        ⇒ yTile=0（最北一行）⇒ 那里没有瓦片 ⇒ 地图【全灰且完全静默】✗
    //   ⇒ 所有来源（home / 实时位置 / 节点中心 / 默认）进入地图前都必须过这里 ✓
    //
    //   ★ 注意包含 NaN 判断：!(x >= a && x <= b) 对 NaN 成立 ⇒ 返回 false ✓
    //
    //   v73: (0,0) 是「未设置位置」的哨兵值，必须判为非法 ✗
    //        why：节点/本机缓存里没有位置时就是 lat=0 lon=0（不是 NaN、也没超范围 ✗）
    //             旧版只查范围 ⇒ (0,0) 被判成合法 ⇒ 地图中心落到几内亚湾「Null Island」
    //             ⇒ SD 上自然没有那里的中国瓦片 ⇒ 【地图全灰】✗（静默、且长按存 home 会把 0,0 永久写进 NVS ✗）
    //        实测：刷 v1.3 后 `v71 map center: lat=0.000000 lon=0.000000 zoom=13 src=own-cache`
    //              ⇒ 瓦片请求 13/4096/4096 ⇒ 全灰 ✓（复现）
    static bool isValidLatLon(float lat, float lon)
    {
        if (!(lat >= -85.0511f && lat <= 85.0511f)) return false;
        if (!(lon >= -180.0f && lon <= 180.0f)) return false;
        if (lat == 0.0f && lon == 0.0f) return false; // v73: 未设置位置的哨兵值
        return true;
    }
    static bool isValidZoom(uint8_t z) { return z >= 1 && z <= 19; }

    // 归一：位置非法 ⇒ 回退编译期默认（默认中心）；缩放非法 ⇒ 回退 zoomDefault
    // 返回值：true = 原值合法
    static bool sanitize(float &lat, float &lon, uint8_t &zoom)
    {
        bool posOk = isValidLatLon(lat, lon);
        bool zoomOk = isValidZoom(zoom);
        if (!posOk) { lat = defaultLat; lon = defaultLon; }
        if (!zoomOk) { zoom = zoomDefault; }
        return posOk && zoomOk;
    }
    static void setDefaultZoom(uint8_t zoom) { zoomDefault = zoom; }

    static uint8_t getZoomLevel(void) { return zoomLevel; }
    static void setZoomLevel(uint8_t level) { zoomLevel = level; }

    static int16_t getTileSize(void) { return tileSize; }
    static void setTileSize(uint16_t size) { tileSize = size; }

    static uint32_t getCacheSize(void) { return cacheSize; }

    static float getDefaultLat(void) { return defaultLat; }
    static void setDefaultLat(float lat) { defaultLat = lat; }

    static float getDefaultLon(void) { return defaultLon; }
    static void setDefaultLon(float lon) { defaultLon = lon; }

    static const char *getPrefix(void) { return prefix; }
    static void setPrefix(const char *p) { copyBounded(prefix, PREFIX_SIZE, p); }

    static const char *getTileStyle(void) { return tileStyle; }
    static void setTileStyle(const char *p);

    // directory holding z/x/y tiles for the selected style
    static const char *getTileDir(void) { return tileDir; }
    static bool isPMTiles(void) { return pmTiles; }
    static void setPMTiles(bool enabled) { pmTiles = enabled; }

    // strips the legacy archive extension and trailing slash from a style name
    static void styleToDir(const char *style, char *dst, size_t dstSize);

    static const char *getTileFormat(void) { return tileFormat; }
    static void setTileFormat(const char *p) { copyBounded(tileFormat, TILE_FORMAT_SIZE, p); }

    static int16_t getTileProvider(void) { return tileProviderId; }
    static void setTileProvider(int16_t id) { tileProviderId = id; }

    static uint32_t getUniqueId(void) { return uniqueId; }
    static void setUniqueId(uint32_t id) { uniqueId = id; }

    static bool color(void) { return colorTiles; }
    static void setColor(bool on) { colorTiles = on; }

    static bool getDebug(void) { return debug; }
    static void setDebug(bool on) { debug = on; }

    static bool saveOK(void) { return save; }
    static void setSaveOK(bool ok) { save = ok; }

    // GCJ-02 offset correction (Chinese tile providers, e.g. 高德/Gaode)
    static bool getGcjOffset(void) { return gcjOffset; }
    static void setGcjOffset(bool on) { gcjOffset = on; }

    // OSM and most western providers are WGS-84; Chinese providers (Gaode/Tencent) are GCJ-02.
    // Auto-detect from the style directory name so that both kinds of tile sets work.
    static void updateGcjFromStyle()
    {
        // v26: 恢复 GCJ-02 换算（用户实测：开启时才准 ✓）；样式名含 osm 的仍自动关闭
        gcjOffset = true;
        for (const char *p = tileStyle; *p; p++) {
            if ((p[0] == 'o' || p[0] == 'O') && (p[1] == 's' || p[1] == 'S') && (p[2] == 'm' || p[2] == 'M'))
                gcjOffset = false;
        }
    }

  private:
    static void appendSlash(char *dst)
    {
        size_t len = strlen(dst);
        if (len > 0 && dst[len - 1] != '/' && len + 1 < TILE_STYLE_SIZE) {
            dst[len] = '/';
            dst[len + 1] = '\0';
        }
    }

    static void copyBounded(char *dst, size_t dstSize, const char *src)
    {
        if (!dst || dstSize == 0) {
            return;
        }
        if (!src) {
            dst[0] = '\0';
            return;
        }

        strncpy(dst, src, dstSize - 1);
        dst[dstSize - 1] = '\0';
    }

    static uint8_t zoomLevel;
    static uint8_t zoomDefault;
    static uint16_t tileSize;
    static int16_t tileProviderId;
    static bool colorTiles;
    static uint32_t cacheSize;
    static uint32_t uniqueId;
    static float defaultLat;
    static float defaultLon;
    static char prefix[];
    static char tileStyle[];
    static char tileDir[];
    static char tileFormat[];
    static bool pmTiles;
    static bool debug;
    static bool save;
    static bool gcjOffset;
};
