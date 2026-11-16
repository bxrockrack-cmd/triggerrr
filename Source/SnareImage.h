#pragma once
// Пиксель-арт малого барабана 186x124, 32 цвета. Без зависимостей (чистый C++11).
// Данные лежат в SnareImageData.cpp: палитра + индекс цвета на каждый пиксель.
#include <cstdint>
#include <cstddef>

namespace snareimg
{
    constexpr int kWidth       = 186;
    constexpr int kHeight      = 124;
    constexpr int kPaletteSize = 32;

    extern const uint8_t kPalette[kPaletteSize][3];     // R, G, B
    extern const uint8_t kPixels[kWidth * kHeight];     // индекс в kPalette, строка за строкой

    // Заполняет буфер RGBA (размер kWidth * kHeight * 4 байт).
    inline void toRGBA (uint8_t* out)
    {
        for (int i = 0; i < kWidth * kHeight; ++i)
        {
            const uint8_t* c = kPalette[kPixels[i]];
            out[i * 4 + 0] = c[0];
            out[i * 4 + 1] = c[1];
            out[i * 4 + 2] = c[2];
            out[i * 4 + 3] = 255;
        }
    }
}
