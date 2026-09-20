#include "graphics/map/MapTileSettings.h"
#include "lv_conf.h"
#include "lvgl.h"

uint8_t MapTileSettings::zoomLevel = 13;   // current zoomLevel
uint8_t MapTileSettings::zoomDefault = 13; // default for initial or home position
uint16_t MapTileSettings::tileSize = 256;
int16_t MapTileSettings::tileProviderId = -1;       // default url index to load from (backup service)
uint32_t MapTileSettings::cacheSize = 50 * 1024;    // LV_FS_CACHE_FROM_BUFFER（原 50KB 上调：单瓦片 9~30KB，需覆盖多片连续读）
uint32_t MapTileSettings::uniqueId = 0xFFFFFFFF;    // to be updated with node number
float MapTileSettings::defaultLat = 39.9042f; // 默认地图中心（原为伦敦）：无定位/无 home 时用，改成中国境内
float MapTileSettings::defaultLon = 116.4074f;
char MapTileSettings::prefix[MapTileSettings::PREFIX_SIZE] = "/maps";        // default map tile directory
char MapTileSettings::tileStyle[MapTileSettings::TILE_STYLE_SIZE] = "";      // { osm/, atlas/, ...}
char MapTileSettings::tileDir[MapTileSettings::TILE_STYLE_SIZE] = "";        // tileStyle without legacy .pmtiles extension
char MapTileSettings::tileFormat[MapTileSettings::TILE_FORMAT_SIZE] = "jpg"; // 本机瓦片是 jpg；png 图层仍有回退
bool MapTileSettings::pmTiles = false;                                       // selected style is a .pmtiles archive
bool MapTileSettings::debug = false;                                         // draw tile frame and info
bool MapTileSettings::save = false;                                          // ok to save tile back to SD card
bool MapTileSettings::gcjOffset = true;                                      // 默认开启：高德等国内瓦片为 GCJ-02
#ifdef MAP_TILES_GREY
bool MapTileSettings::colorTiles = false;
#else
bool MapTileSettings::colorTiles = true;
#endif

void MapTileSettings::setTileStyle(const char *p)
{
    styleToDir(p, tileStyle, TILE_STYLE_SIZE);
    styleToDir(tileStyle, tileDir, TILE_STYLE_SIZE);
    appendSlash(tileStyle);
    appendSlash(tileDir);
    updateGcjFromStyle();
}

void MapTileSettings::styleToDir(const char *style, char *dst, size_t dstSize)
{
    copyBounded(dst, dstSize, style);
    std::string_view dstView(dst);

    if (dstView.size() >= PMTILES_EXTENSION_LEN &&
        dstView.compare(dstView.size() - PMTILES_EXTENSION_LEN, PMTILES_EXTENSION_LEN, PMTILES_EXTENSION) == 0) {
        dst[dstView.size() - PMTILES_EXTENSION_LEN] = '\0';
        dstView = std::string_view(dst, dstView.size() - PMTILES_EXTENSION_LEN);
    }

    if (!dstView.empty() && dstView.back() == '/') {
        dst[dstView.size() - 1] = '\0';
    }
}