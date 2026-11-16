#pragma once
// Обёртка для JUCE: juce::Image из данных SnareImage.
// Подключайте после JUCE-модулей (juce_gui_basics).
#include <juce_gui_basics/juce_gui_basics.h>
#include "SnareImage.h"

namespace snareimg
{
    // Картинка 186x124 в оригинальном размере (1 пиксель арта = 1 пиксель).
    inline juce::Image makeImage()
    {
        juce::Image img (juce::Image::ARGB, kWidth, kHeight, false);
        for (int y = 0; y < kHeight; ++y)
            for (int x = 0; x < kWidth; ++x)
            {
                const uint8_t* c = kPalette[kPixels[y * kWidth + x]];
                img.setPixelAt (x, y, juce::Colour (c[0], c[1], c[2]));
            }
        return img;
    }

    // Рисует картинку в прямоугольник без сглаживания (чёткие пиксели).
    // Пример в Component::paint:  snareimg::draw (g, getLocalBounds().toFloat());
    inline void draw (juce::Graphics& g, juce::Rectangle<float> area)
    {
        static const juce::Image img = makeImage();
        g.setImageResamplingQuality (juce::Graphics::lowResamplingQuality);   // nearest neighbour
        g.drawImage (img, area, juce::RectanglePlacement::centred);
    }
}
