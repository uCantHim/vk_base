#include "ui/elements/Text.h"

#include "ui/DrawInfo.h"



trc::ui::Text::Text(std::string str, ui32 fontIndex)
    :
    Text(wstringConverter.from_bytes(std::move(str)), fontIndex)
{
}

trc::ui::Text::Text(std::wstring str, ui32 fontIndex)
    :
    printedText(std::move(str)),
    fontIndex(fontIndex)
{
}

void trc::ui::Text::draw(DrawList& drawList, vec2 globalPos, vec2 globalSize)
{
    types::Text textInfo{
        .fontIndex = fontIndex,
        .letters = {}
    };

    // Create letter descriptions
    ivec2 glyphPos{ 0, 0 };
    for (wchar_t character : printedText)
    {
        if (character == '\n')
        {
            glyphPos.x = 0.0f;
            glyphPos.y += FontRegistry::getFontInfo(fontIndex).maxGlyphHeight;
            continue;
        }

        auto& glyph = FontRegistry::getGlyph(fontIndex, character);
        textInfo.letters.emplace_back(types::LetterInfo{
            .characterCode = character,
            .glyphOffsetPixels = glyphPos,
            .glyphSizePixels = glyph.metaInPixels.size,
            .bearingYPixels = glyph.metaInPixels.bearingY
        });

        glyphPos.x += glyph.metaInPixels.advance;
    }

    drawList.emplace_back(DrawInfo{
        .elem = ElementDrawInfo{
            .pos = globalPos,
            .size = globalSize,
            .background = vec4(0.0f)
        },
        .type = std::move(textInfo)
    });
}

void trc::ui::Text::print(std::string str)
{
    printedText = wstringConverter.from_bytes(std::move(str));
}

void trc::ui::Text::print(std::wstring str)
{
    printedText = std::move(str);
}