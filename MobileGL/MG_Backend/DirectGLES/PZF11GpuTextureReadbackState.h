// MobilePZ PZF11 - GL-independent classification for model-texture GPU readback.
#pragma once

#include "PZF5ReadbackStats.h"

namespace MobilePZ::PZF11 {

enum class GpuContentClass {
    NoData = 0,
    AllZero,
    RgbAlphaZero,
    NonTransparent,
};

enum class ComparisonClass {
    NoData = 0,
    CpuZeroGpuZero,
    CpuZeroGpuRgbAlphaZero,
    CpuZeroGpuNonTransparent,
    CpuUsefulGpuZero,
    CpuUsefulGpuRgbAlphaZero,
    CpuUsefulGpuNonTransparent,
    CpuUnknownGpuZero,
    CpuUnknownGpuRgbAlphaZero,
    CpuUnknownGpuNonTransparent,
};

inline GpuContentClass ClassifyGpu(const ::MobilePZ::PZF5::ReadbackStats& stats) {
    if (stats.pixels == 0) return GpuContentClass::NoData;
    if (stats.zeroRgba == stats.pixels) return GpuContentClass::AllZero;
    if (stats.alphaZero == stats.pixels && stats.rgbNonzero != 0) {
        return GpuContentClass::RgbAlphaZero;
    }
    if (stats.alphaMid + stats.alphaFull != 0) return GpuContentClass::NonTransparent;
    return GpuContentClass::NoData;
}

inline const char* GpuContentClassName(GpuContentClass value) {
    switch (value) {
    case GpuContentClass::AllZero: return "GPU_ALL_ZERO";
    case GpuContentClass::RgbAlphaZero: return "GPU_RGB_ALPHA_ZERO";
    case GpuContentClass::NonTransparent: return "GPU_NONTRANSPARENT";
    default: return "GPU_NO_DATA";
    }
}

inline const char* CpuContentClassName(GpuContentClass value) {
    switch (value) {
    case GpuContentClass::AllZero: return "CPU_ALL_ZERO";
    case GpuContentClass::RgbAlphaZero: return "CPU_RGB_ALPHA_ZERO";
    case GpuContentClass::NonTransparent: return "CPU_NONTRANSPARENT";
    default: return "CPU_NO_DATA";
    }
}

inline ComparisonClass Compare(bool cpuAllZero, bool cpuUseful,
                               const ::MobilePZ::PZF5::ReadbackStats& stats) {
    const auto gpu = ClassifyGpu(stats);
    if (gpu == GpuContentClass::NoData) return ComparisonClass::NoData;
    if (cpuAllZero) {
        if (gpu == GpuContentClass::AllZero) return ComparisonClass::CpuZeroGpuZero;
        if (gpu == GpuContentClass::RgbAlphaZero) return ComparisonClass::CpuZeroGpuRgbAlphaZero;
        return ComparisonClass::CpuZeroGpuNonTransparent;
    }
    if (cpuUseful) {
        if (gpu == GpuContentClass::AllZero) return ComparisonClass::CpuUsefulGpuZero;
        if (gpu == GpuContentClass::RgbAlphaZero) return ComparisonClass::CpuUsefulGpuRgbAlphaZero;
        return ComparisonClass::CpuUsefulGpuNonTransparent;
    }
    if (gpu == GpuContentClass::AllZero) return ComparisonClass::CpuUnknownGpuZero;
    if (gpu == GpuContentClass::RgbAlphaZero) return ComparisonClass::CpuUnknownGpuRgbAlphaZero;
    return ComparisonClass::CpuUnknownGpuNonTransparent;
}

inline const char* ComparisonClassName(ComparisonClass value) {
    switch (value) {
    case ComparisonClass::CpuZeroGpuZero: return "CPU_ZERO_GPU_ZERO";
    case ComparisonClass::CpuZeroGpuRgbAlphaZero: return "CPU_ZERO_GPU_RGB_ALPHA_ZERO";
    case ComparisonClass::CpuZeroGpuNonTransparent: return "CPU_ZERO_GPU_NONTRANSPARENT";
    case ComparisonClass::CpuUsefulGpuZero: return "CPU_USEFUL_GPU_ZERO";
    case ComparisonClass::CpuUsefulGpuRgbAlphaZero: return "CPU_USEFUL_GPU_RGB_ALPHA_ZERO";
    case ComparisonClass::CpuUsefulGpuNonTransparent: return "CPU_USEFUL_GPU_NONTRANSPARENT";
    case ComparisonClass::CpuUnknownGpuZero: return "CPU_UNKNOWN_GPU_ZERO";
    case ComparisonClass::CpuUnknownGpuRgbAlphaZero: return "CPU_UNKNOWN_GPU_RGB_ALPHA_ZERO";
    case ComparisonClass::CpuUnknownGpuNonTransparent: return "CPU_UNKNOWN_GPU_NONTRANSPARENT";
    default: return "NO_DATA";
    }
}

} // namespace MobilePZ::PZF11
