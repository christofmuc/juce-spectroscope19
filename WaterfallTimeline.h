/*
   Copyright (c) 2026 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

namespace spectroscope::waterfall {

inline int nextRow(int currentRow, int rowCount) noexcept
{
	return rowCount > 0 ? (currentRow + 1) % rowCount : 0;
}

// A complete circular history starts immediately after the newest row and
// finishes on the newest row. Sampling texel centres avoids blending the two
// sides of the circular-buffer seam.
inline float oldestRowCentre(int newestRow, int rowCount) noexcept
{
	return rowCount > 0
		? (static_cast<float>(newestRow) + 1.5f) / static_cast<float>(rowCount)
		: 0.0f;
}

inline float historySpan(int rowCount) noexcept
{
	return rowCount > 0
		? static_cast<float>(rowCount - 1) / static_cast<float>(rowCount)
		: 0.0f;
}

inline float textureCoordinate(float progress, int newestRow, int rowCount) noexcept
{
	return oldestRowCentre(newestRow, rowCount) + progress * historySpan(rowCount);
}

} // namespace spectroscope::waterfall
