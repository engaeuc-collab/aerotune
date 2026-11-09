#pragma once
#include <vector>
#include <cmath>
#include <algorithm>

// Circular buffer with 4-point Hermite (cubic) fractional read.
// read (d) returns the sample that was written d samples ago (d >= 1).
class DelayLine
{
public:
    void prepare (double sampleRate)
    {
        int n = 1;
        while (n < (int) (sampleRate * 0.12)) n <<= 1;
        buf.assign ((size_t) n, 0.0f);
        mask = n - 1;
        pos = 0;
    }

    void clear() { std::fill (buf.begin(), buf.end(), 0.0f); pos = 0; }

    inline void push (float x)
    {
        pos = (pos + 1) & mask;
        buf[(size_t) pos] = x;
    }

    inline float read (float delay) const
    {
        const int   i = (int) delay;
        const float t = delay - (float) i;

        const float y0 = buf[(size_t) ((pos - (i - 1)) & mask)];
        const float y1 = buf[(size_t) ((pos - i) & mask)];
        const float y2 = buf[(size_t) ((pos - (i + 1)) & mask)];
        const float y3 = buf[(size_t) ((pos - (i + 2)) & mask)];

        const float c1 = 0.5f * (y2 - y0);
        const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
        const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
        return ((c3 * t + c2) * t + c1) * t + y1;
    }

private:
    std::vector<float> buf;
    int mask = 0, pos = 0;
};
