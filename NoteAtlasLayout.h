/*
   Copyright (c) 2026 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include <algorithm>

namespace spectroscope::note_atlas {

constexpr int midiNoteCount = 128;
constexpr int columns = 8;
constexpr int rows = midiNoteCount / columns;
constexpr int rasterScale = 2;
constexpr int labelWidth = 36;
constexpr int labelHeight = 17;
constexpr int horizontalPadding = 2;
constexpr int verticalPadding = 1;
constexpr int cellWidth = (labelWidth + 2 * horizontalPadding) * rasterScale;
constexpr int cellHeight = (labelHeight + 2 * verticalPadding) * rasterScale;
constexpr int imageWidth = columns * cellWidth;
constexpr int imageHeight = rows * cellHeight;

struct PixelBounds {
	int x { 0 };
	int y { 0 };
	int width { cellWidth };
	int height { cellHeight };
};

constexpr int validMidiNote(int midiNote) noexcept
{
	return std::clamp(midiNote, 0, midiNoteCount - 1);
}

constexpr PixelBounds pixelBoundsForMidiNote(int midiNote) noexcept
{
	const auto validNote = validMidiNote(midiNote);
	return { (validNote % columns) * cellWidth, (validNote / columns) * cellHeight,
		cellWidth, cellHeight };
}

}
