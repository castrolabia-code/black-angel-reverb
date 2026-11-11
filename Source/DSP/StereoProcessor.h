// KB REVERB - BLACK ANGEL
// Copyright (C) 2026 KB Audio. Licensed under the GNU AGPLv3 (see LICENSE).
#pragma once

#include "Util/DspUtil.h"

namespace ba
{
/** Mid/side width for the wet signal. 0 % = mono, 100 % = unchanged,
    200 % = side doubled. */
class StereoProcessor
{
public:
    void prepare (double fs) { width.reset (fs, 0.05); width.setImmediate (width.getTarget()); }
    void setWidth (float w01to2) noexcept { width.setTarget (std::min (std::max (w01to2, 0.0f), 2.0f)); }
    void snap() noexcept { width.setImmediate (width.getTarget()); }

    void process (float& l, float& r) noexcept
    {
        const float w = width.next();
        const float m = 0.5f * (l + r);
        const float s = 0.5f * (l - r) * w;
        l = m + s;
        r = m - s;
    }

private:
    LinearSmoother width;
};
} // namespace ba
