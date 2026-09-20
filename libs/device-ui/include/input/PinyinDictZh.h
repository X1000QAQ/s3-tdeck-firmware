#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

// 简体中文拼音词典（GB2312 一级字，按词频排序），见 source/input/pinyin_dict_zh.c
extern const lv_pinyin_dict_t lv_pinyin_dict_zh[];
// 繁体词典（Big5 一级字，按词频排序）
// 2026-09-19 繁体词库已移除（保留简体）

#ifdef __cplusplus
}
#endif
