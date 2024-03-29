/*
  ==============================================================================

   This file is part of the JUCE library - "Jules' Utility Class Extensions"
   Copyright 2004-11 by Raw Material Software Ltd.

  ------------------------------------------------------------------------------

   JUCE can be redistributed and/or modified under the terms of the GNU General
   Public License (Version 2), as published by the Free Software Foundation.
   A copy of the license is included in the JUCE distribution, or can be found
   online at www.gnu.org/licenses.

   JUCE is distributed in the hope that it will be useful, but WITHOUT ANY
   WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
   A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

  ------------------------------------------------------------------------------

   To release a closed-source product which uses JUCE, commercial licenses are
   available: visit www.rawmaterialsoftware.com/juce for more information.

  ==============================================================================
*/

TextLayout::Glyph::Glyph (const int glyphCode_, const Point<float>& anchor_, float width_) noexcept
    : glyphCode (glyphCode_), anchor (anchor_), width (width_)
{
}

TextLayout::Glyph::Glyph (const Glyph& other) noexcept
    : glyphCode (other.glyphCode), anchor (other.anchor), width (other.width)
{
}

TextLayout::Glyph& TextLayout::Glyph::operator= (const Glyph& other) noexcept
{
    glyphCode = other.glyphCode;
    anchor = other.anchor;
    width = other.width;
    return *this;
}

TextLayout::Glyph::~Glyph() noexcept {}

//==============================================================================
TextLayout::Run::Run() noexcept
    : colour (0xff000000)
{
}

TextLayout::Run::Run (const Range<int>& range, const int numGlyphsToPreallocate)
    : colour (0xff000000), stringRange (range)
{
    glyphs.ensureStorageAllocated (numGlyphsToPreallocate);
}

TextLayout::Run::Run (const Run& other)
    : font (other.font),
      colour (other.colour),
      glyphs (other.glyphs),
      stringRange (other.stringRange)
{
}

TextLayout::Run::~Run() noexcept {}

//==============================================================================
TextLayout::Line::Line() noexcept
    : ascent (0.0f), descent (0.0f), leading (0.0f)
{
}

TextLayout::Line::Line (const Range<int>& stringRange_, const Point<float>& lineOrigin_,
                        const float ascent_, const float descent_, const float leading_,
                        const int numRunsToPreallocate)
    : stringRange (stringRange_), lineOrigin (lineOrigin_),
      ascent (ascent_), descent (descent_), leading (leading_)
{
    runs.ensureStorageAllocated (numRunsToPreallocate);
}

TextLayout::Line::Line (const Line& other)
    : stringRange (other.stringRange), lineOrigin (other.lineOrigin),
      ascent (other.ascent), descent (other.descent), leading (other.leading)
{
    runs.addCopiesOf (other.runs);
}

TextLayout::Line::~Line() noexcept
{
}

Range<float> TextLayout::Line::getLineBoundsX() const noexcept
{
    Range<float> range;
    bool isFirst = true;

    for (int i = runs.size(); --i >= 0;)
    {
        const Run* run = runs.getUnchecked(i);
        jassert (run != nullptr);

        if (run->glyphs.size() > 0)
        {
            float minX = run->glyphs.getReference(0).anchor.x;
            float maxX = minX;

            for (int j = run->glyphs.size(); --j > 0;)
            {
                const Glyph& glyph = run->glyphs.getReference (j);
                const float x = glyph.anchor.x;
                minX = jmin (minX, x);
                maxX = jmax (maxX, x + glyph.width);
            }

            if (isFirst)
            {
                isFirst = false;
                range = Range<float> (minX, maxX);
            }
            else
            {
                range = range.getUnionWith (Range<float> (minX, maxX));
            }
        }
    }

    return range + lineOrigin.x;
}

//==============================================================================
TextLayout::TextLayout()
    : width (0), justification (Justification::topLeft)
{
}

TextLayout::TextLayout (const TextLayout& other)
    : width (other.width),
      justification (other.justification)
{
    lines.addCopiesOf (other.lines);
}

#if JUCE_COMPILER_SUPPORTS_MOVE_SEMANTICS
TextLayout::TextLayout (TextLayout&& other) noexcept
    : lines (static_cast <OwnedArray<Line>&&> (other.lines)),
      width (other.width),
      justification (other.justification)
{
}

TextLayout& TextLayout::operator= (TextLayout&& other) noexcept
{
    lines = static_cast <OwnedArray<Line>&&> (other.lines);
    width = other.width;
    justification = other.justification;
    return *this;
}
#endif

TextLayout& TextLayout::operator= (const TextLayout& other)
{
    width = other.width;
    justification = other.justification;
    lines.clear();
    lines.addCopiesOf (other.lines);
    return *this;
}

TextLayout::~TextLayout()
{
}

float TextLayout::getHeight() const noexcept
{
    const Line* const lastLine = lines.getLast();

    return lastLine != nullptr ? lastLine->lineOrigin.y + lastLine->descent
                               : 0;
}

TextLayout::Line& TextLayout::getLine (const int index) const
{
    return *lines[index];
}

void TextLayout::ensureStorageAllocated (int numLinesNeeded)
{
    lines.ensureStorageAllocated (numLinesNeeded);
}

void TextLayout::addLine (Line* line)
{
    lines.add (line);
}

void TextLayout::draw (Graphics& g, const Rectangle<float>& area) const
{
    const Point<float> origin (justification.appliedToRectangle (Rectangle<float> (0, 0, width, getHeight()), area).getPosition());

    LowLevelGraphicsContext& context = *g.getInternalContext();

    for (int i = 0; i < getNumLines(); ++i)
    {
        const Line& line = getLine (i);
        const Point<float> lineOrigin (origin + line.lineOrigin);

        for (int j = 0; j < line.runs.size(); ++j)
        {
            const Run* const run = line.runs.getUnchecked (j);
            jassert (run != nullptr);
            context.setFont (run->font);
            context.setFill (run->colour);

            for (int k = 0; k < run->glyphs.size(); ++k)
            {
                const Glyph& glyph = run->glyphs.getReference (k);
                context.drawGlyph (glyph.glyphCode, AffineTransform::translation (lineOrigin.x + glyph.anchor.x,
                                                                                  lineOrigin.y + glyph.anchor.y));
            }
        }
    }
}

void TextLayout::createLayout (const AttributedString& text, float maxWidth)
{
    lines.clear();
    width = maxWidth;
    justification = text.getJustification();

    if (! createNativeLayout (text))
        createStandardLayout (text);

    recalculateWidth(text);
}

//==============================================================================
namespace TextLayoutHelpers
{
    struct FontAndColour
    {
        FontAndColour (const Font* font_) noexcept   : font (font_), colour (0xff000000) {}

        const Font* font;
        Colour colour;

        bool operator!= (const FontAndColour& other) const noexcept
        {
            return (font != other.font && *font != *other.font) || colour != other.colour;
        }
    };

    struct RunAttribute
    {
        RunAttribute (const FontAndColour& fontAndColour_, const Range<int>& range_) noexcept
            : fontAndColour (fontAndColour_), range (range_)
        {}

        FontAndColour fontAndColour;
        Range<int> range;
    };

    struct Token
    {
        Token (const String& t, const Font& f, const Colour& c, const bool isWhitespace_)
            : text (t), font (f), colour (c),
              area (font.getStringWidth (t), roundToInt (f.getHeight())),
              isWhitespace (isWhitespace_),
              isNewLine (t.containsChar ('\n') || t.containsChar ('\r'))
        {}

        const String text;
        const Font font;
        const Colour colour;
        Rectangle<int> area;
        int line, lineHeight;
        const bool isWhitespace, isNewLine;

    private:
        Token& operator= (const Token&);
    };

    class TokenList
    {
    public:
        TokenList() noexcept  : totalLines (0) {}

        void createLayout (const AttributedString& text, TextLayout& layout)
        {
            tokens.ensureStorageAllocated (64);
            layout.ensureStorageAllocated (totalLines);

            addTextRuns (text);

            layoutRuns ((int) layout.getWidth());

            int charPosition = 0;
            int lineStartPosition = 0;
            int runStartPosition = 0;

            ScopedPointer<TextLayout::Line> currentLine;
            ScopedPointer<TextLayout::Run> currentRun;

            bool needToSetLineOrigin = true;

            for (int i = 0; i < tokens.size(); ++i)
            {
                const Token* const t = tokens.getUnchecked (i);
                const Point<float> tokenPos (t->area.getPosition().toFloat());

                Array <int> newGlyphs;
                Array <float> xOffsets;
                t->font.getGlyphPositions (t->text.trimEnd(), newGlyphs, xOffsets);

                if (currentRun == nullptr)  currentRun  = new TextLayout::Run();
                if (currentLine == nullptr) currentLine = new TextLayout::Line();

                currentRun->glyphs.ensureStorageAllocated (currentRun->glyphs.size() + newGlyphs.size());

                for (int j = 0; j < newGlyphs.size(); ++j)
                {
                    if (needToSetLineOrigin)
                    {
                        needToSetLineOrigin = false;
                        currentLine->lineOrigin = tokenPos.translated (0, t->font.getAscent());
                    }

                    const float x = xOffsets.getUnchecked (j);
                    currentRun->glyphs.add (TextLayout::Glyph (newGlyphs.getUnchecked(j),
                                                               Point<float> (tokenPos.getX() + x, 0),
                                                               xOffsets.getUnchecked (j + 1) - x));
                    ++charPosition;
                }

                if (t->isWhitespace || t->isNewLine)
                    ++charPosition;

                const Token* const nextToken = tokens [i + 1];

                if (nextToken == nullptr) // this is the last token
                {
                    addRun (currentLine, currentRun.release(), t, runStartPosition, charPosition);
                    currentLine->stringRange = Range<int> (lineStartPosition, charPosition);
                    layout.addLine (currentLine.release());
                }
                else
                {
                    if (t->font != nextToken->font || t->colour != nextToken->colour)
                    {
                        addRun (currentLine, currentRun.release(), t, runStartPosition, charPosition);
                        runStartPosition = charPosition;
                    }

                    if (t->line != nextToken->line)
                    {
                        if (currentRun == nullptr)
                            currentRun = new TextLayout::Run();

                        addRun (currentLine, currentRun.release(), t, runStartPosition, charPosition);
                        currentLine->stringRange = Range<int> (lineStartPosition, charPosition);
                        layout.addLine (currentLine.release());

                        runStartPosition = charPosition;
                        lineStartPosition = charPosition;
                        needToSetLineOrigin = true;
                    }
                }
            }

            if ((text.getJustification().getFlags() & (Justification::right | Justification::horizontallyCentred)) != 0)
            {
                const int totalW = (int) layout.getWidth();
                const bool isCentred = (text.getJustification().getFlags() & Justification::horizontallyCentred) != 0;

                for (int i = 0; i < layout.getNumLines(); ++i)
                {
                    float dx = (float) (totalW - getLineWidth (i));

                    if (isCentred)
                        dx /= 2.0f;

                    layout.getLine(i).lineOrigin.x += dx;
                }
            }
        }

    private:
        static void addRun (TextLayout::Line* glyphLine, TextLayout::Run* glyphRun,
                            const Token* const t, const int start, const int end)
        {
            glyphRun->stringRange = Range<int> (start, end);
            glyphRun->font = t->font;
            glyphRun->colour = t->colour;
            glyphLine->ascent = jmax (glyphLine->ascent, t->font.getAscent());
            glyphLine->descent = jmax (glyphLine->descent, t->font.getDescent());
            glyphLine->runs.add (glyphRun);
        }

        static int getCharacterType (const juce_wchar c) noexcept
        {
            if (c == '\r' || c == '\n')
                return 0;

            return CharacterFunctions::isWhitespace (c) ? 2 : 1;
        }

        void appendText (const AttributedString& text, const Range<int>& stringRange,
                         const Font& font, const Colour& colour)
        {
            const String stringText (text.getText().substring (stringRange.getStart(), stringRange.getEnd()));
            String::CharPointerType t (stringText.getCharPointer());
            String currentString;
            int lastCharType = 0;

            for (;;)
            {
                const juce_wchar c = t.getAndAdvance();
                if (c == 0)
                    break;

                const int charType = getCharacterType (c);

                if (charType == 0 || charType != lastCharType)
                {
                    if (currentString.isNotEmpty())
                        tokens.add (new Token (currentString, font, colour,
                                               lastCharType == 2 || lastCharType == 0));

                    currentString = String::charToString (c);

                    if (c == '\r' && *t == '\n')
                        currentString += t.getAndAdvance();
                }
                else
                {
                    currentString += c;
                }

                lastCharType = charType;
            }

            if (currentString.isNotEmpty())
                tokens.add (new Token (currentString, font, colour, lastCharType == 2));
        }

        void layoutRuns (const int maxWidth)
        {
            int x = 0, y = 0, h = 0;
            int i;

            for (i = 0; i < tokens.size(); ++i)
            {
                Token* const t = tokens.getUnchecked(i);
                t->area.setPosition (x, y);
                t->line = totalLines;
                x += t->area.getWidth();
                h = jmax (h, t->area.getHeight());

                const Token* const nextTok = tokens[i + 1];

                if (nextTok == nullptr)
                    break;

                if (t->isNewLine || ((! nextTok->isWhitespace) && x + nextTok->area.getWidth() > maxWidth))
                {
                    setLastLineHeight (i + 1, h);
                    x = 0;
                    y += h;
                    h = 0;
                    ++totalLines;
                }
            }

            setLastLineHeight (jmin (i + 1, tokens.size()), h);
            ++totalLines;
        }

        void setLastLineHeight (int i, const int height) noexcept
        {
            while (--i >= 0)
            {
                Token* const tok = tokens.getUnchecked (i);

                if (tok->line == totalLines)
                    tok->lineHeight = height;
                else
                    break;
            }
        }

        int getLineWidth (const int lineNumber) const noexcept
        {
            int maxW = 0;

            for (int i = tokens.size(); --i >= 0;)
            {
                const Token* const t = tokens.getUnchecked (i);

                if (t->line == lineNumber && ! t->isWhitespace)
                    maxW = jmax (maxW, t->area.getRight());
            }

            return maxW;
        }

        void addTextRuns (const AttributedString& text)
        {
            Font defaultFont;
            Array<RunAttribute> runAttributes;

            {
                const int stringLength = text.getText().length();
                int rangeStart = 0;
                FontAndColour lastFontAndColour (nullptr);

                // Iterate through every character in the string
                for (int i = 0; i < stringLength; ++i)
                {
                    FontAndColour newFontAndColour (&defaultFont);
                    const int numCharacterAttributes = text.getNumAttributes();

                    for (int j = 0; j < numCharacterAttributes; ++j)
                    {
                        const AttributedString::Attribute* const attr = text.getAttribute (j);

                        // Check if the current character falls within the range of a font attribute
                        if (attr->getFont() != nullptr && (i >= attr->range.getStart()) && (i < attr->range.getEnd()))
                            newFontAndColour.font = attr->getFont();

                        // Check if the current character falls within the range of a foreground colour attribute
                        if (attr->getColour() != nullptr && (i >= attr->range.getStart()) && (i < attr->range.getEnd()))
                            newFontAndColour.colour = *attr->getColour();
                    }

                    if (i > 0 && (newFontAndColour != lastFontAndColour || i == stringLength - 1))
                    {
                        runAttributes.add (RunAttribute (lastFontAndColour,
                                                         Range<int> (rangeStart, (i < stringLength - 1) ? i : (i + 1))));
                        rangeStart = i;
                    }

                    lastFontAndColour = newFontAndColour;
                }
            }

            for (int i = 0; i < runAttributes.size(); ++i)
            {
                const RunAttribute& r = runAttributes.getReference(i);
                appendText (text, r.range, *(r.fontAndColour.font), r.fontAndColour.colour);
            }
        }

        OwnedArray<Token> tokens;
        int totalLines;

        JUCE_DECLARE_NON_COPYABLE (TokenList);
    };
}

//==============================================================================
void TextLayout::createLayoutWithBalancedLineLengths (const AttributedString& text, float maxWidth)
{
    const float minimumWidth = maxWidth / 2.0f;
    float bestWidth = maxWidth;
    float bestLineProportion = 0.0f;

    while (maxWidth > minimumWidth)
    {
        createLayout (text, maxWidth);

        if (getNumLines() < 2)
            return;

        const float line1 = lines.getUnchecked (lines.size() - 1)->getLineBoundsX().getLength();
        const float line2 = lines.getUnchecked (lines.size() - 2)->getLineBoundsX().getLength();
        const float shortestLine = jmin (line1, line2);
        const float prop = (shortestLine > 0) ? jmax (line1, line2) / shortestLine : 1.0f;

        if (prop > 0.9f)
            return;

        if (prop > bestLineProportion)
        {
            bestLineProportion = prop;
            bestWidth = maxWidth;
        }

        maxWidth -= 10.0f;
    }

    if (bestWidth != maxWidth)
        createLayout (text, bestWidth);
}

//==============================================================================
void TextLayout::createStandardLayout (const AttributedString& text)
{
    TextLayoutHelpers::TokenList l;
    l.createLayout (text, *this);
}

void TextLayout::recalculateWidth(const AttributedString& text)
{
    if (lines.size() > 0 && text.getReadingDirection() != AttributedString::rightToLeft)
    {
        Range<float> range (lines.getFirst()->getLineBoundsX());

        int i;
        for (i = lines.size(); --i > 0;)
            range = range.getUnionWith (lines.getUnchecked(i)->getLineBoundsX());

        for (i = lines.size(); --i >= 0;)
            lines.getUnchecked(i)->lineOrigin.x -= range.getStart();

        width = range.getLength();
    }
}
