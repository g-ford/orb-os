#pragma once
// A cyclic list on a wheel: the app picker wraps, so its rows are a ring with the current entry in the middle.
namespace wheel_layout {

// Fill `outIndex[0..n)` with the entries (0..visible-1) to draw, in order, current one included, so that every
// entry appears exactly once and the current one is at row visible / 2. `outIndex` needs room for `visible`
// entries. Returns n (= visible).
inline int ring_rows(int visible, int position, int *outIndex) {
    if (visible <= 0) return 0;
    const int above = visible / 2;
    for (int i = 0; i < visible; ++i)
        outIndex[i] = ((position + (i - above)) % visible + visible) % visible;
    return visible;
}

} // namespace wheel_layout
