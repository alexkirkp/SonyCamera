#pragma once

// Pure crop arithmetic for RGGB output, kept free of LibRaw/Windows types so it can be unit tested.

struct CropRect
{
    int left = 0;
    int top = 0;
    int width = 0;
    int height = 0;
    bool adjusted = false;   // true if the requested crop had to be changed
};

// Turns a requested crop (pixels trimmed from each edge) into a safe rectangle inside a
// rawWidth x rawHeight frame. Left/top are rounded down to even so the 2x2 Bayer pattern keeps its
// phase; width/height are rounded down to even so a row never has a dangling half pair. Negative
// values are treated as 0. A crop that leaves nothing (or does not fit) falls back to the full frame.
inline CropRect
ResolveRawCrop(int rawWidth, int rawHeight, int top, int left, int bottom, int right)
{
    CropRect r;
    r.width = rawWidth & ~1;
    r.height = rawHeight & ~1;
    r.adjusted = (r.width != rawWidth) || (r.height != rawHeight);

    if (top < 0) { top = 0; r.adjusted = true; }
    if (left < 0) { left = 0; r.adjusted = true; }
    if (bottom < 0) { bottom = 0; r.adjusted = true; }
    if (right < 0) { right = 0; r.adjusted = true; }

    // Widen to 64 bits so absurd values can't overflow the sums
    long long w = (long long)rawWidth - left - right;
    long long h = (long long)rawHeight - top - bottom;

    if (w < 2 || h < 2 || left >= rawWidth || top >= rawHeight)
    {
        r.adjusted = r.adjusted || left != 0 || right != 0 || top != 0 || bottom != 0;
        return r;   // full (even-sized) frame
    }

    int evenLeft = left & ~1;
    int evenTop = top & ~1;
    w = (long long)rawWidth - evenLeft - right;
    h = (long long)rawHeight - evenTop - bottom;
    w &= ~1LL;
    h &= ~1LL;

    if (w < 2 || h < 2)
    {
        r.adjusted = true;
        return r;
    }

    if (evenLeft != left || evenTop != top || w != (long long)rawWidth - left - right || h != (long long)rawHeight - top - bottom)
    {
        r.adjusted = true;
    }

    r.left = evenLeft;
    r.top = evenTop;
    r.width = (int)w;
    r.height = (int)h;
    return r;
}
