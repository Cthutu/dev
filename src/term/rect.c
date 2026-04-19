//------------------------------------------------------------------------------
// TermRect utilities
//
// Copyright (C)2026 Matt Davies, all rights reserved
//------------------------------------------------------------------------------

#include <term/term.h>

//------------------------------------------------------------------------------
// term_rect
//
// Construct a rectangle directly from an origin and size.
//------------------------------------------------------------------------------

TermRect term_rect(u16 x, u16 y, u16 width, u16 height)
{
    TermRect rect;
    rect.x      = x;
    rect.y      = y;
    rect.width  = width;
    rect.height = height;
    return rect;
}

//------------------------------------------------------------------------------
// term_rect_from_points
//
// Construct a rectangle from two inclusive corner points.
//------------------------------------------------------------------------------

TermRect term_rect_from_points(u16 x0, u16 y0, u16 x1, u16 y1)
{
    TermRect rect;
    rect.x      = MIN(x0, x1);
    rect.y      = MIN(y0, y1);
    rect.width  = (MAX(x0, x1) - rect.x) + 1;
    rect.height = (MAX(y0, y1) - rect.y) + 1;
    return rect;
}

//------------------------------------------------------------------------------
// term_rect_union
//
// Return the smallest rectangle that fully contains both inputs.
//------------------------------------------------------------------------------

TermRect term_rect_union(TermRect a, TermRect b)
{
    u16 x0 = MIN(a.x, b.x);
    u16 y0 = MIN(a.y, b.y);
    u16 x1 = MAX(a.x + a.width, b.x + b.width);
    u16 y1 = MAX(a.y + a.height, b.y + b.height);
    return term_rect_from_points(x0, y0, x1 - 1, y1 - 1);
}

//------------------------------------------------------------------------------
// term_rect_intersection
//
// Return the overlapping region shared by both rectangles.
//------------------------------------------------------------------------------

TermRect term_rect_intersection(TermRect a, TermRect b)
{
    u16 x0 = MAX(a.x, b.x);
    u16 y0 = MAX(a.y, b.y);
    u16 x1 = MIN(a.x + a.width, b.x + b.width);
    u16 y1 = MIN(a.y + a.height, b.y + b.height);
    if (x1 <= x0 || y1 <= y0) {
        return term_rect(0, 0, 0, 0);
    }
    return term_rect_from_points(x0, y0, x1 - 1, y1 - 1);
}

//------------------------------------------------------------------------------
// term_rect_equals
//
// Test whether two rectangles have identical origin and size.
//------------------------------------------------------------------------------

bool term_rect_equals(TermRect a, TermRect b)
{
    return a.x == b.x && a.y == b.y && a.width == b.width &&
           a.height == b.height;
}

//------------------------------------------------------------------------------
// term_rect_contains
//
// Test whether a point lies within the rectangle bounds.
//------------------------------------------------------------------------------

bool term_rect_contains(TermRect rect, u16 x, u16 y)
{
    return x >= rect.x && x < rect.x + rect.width && y >= rect.y &&
           y < rect.y + rect.height;
}

//------------------------------------------------------------------------------
// term_rect_overlaps
//
// Test whether two rectangles overlap with positive shared area.
//------------------------------------------------------------------------------

bool term_rect_overlaps(TermRect a, TermRect b)
{
    return a.x < b.x + b.width && a.x + a.width > b.x && a.y < b.y + b.height &&
           a.y + a.height > b.y;
}

//------------------------------------------------------------------------------
// term_rect_is_empty
//
// Test whether a rectangle has zero width or height.
//------------------------------------------------------------------------------

bool term_rect_is_empty(TermRect rect)
{
    return rect.width == 0 || rect.height == 0;
}

//------------------------------------------------------------------------------
// term_rect_clip
//
// Clip rectangle `a` to rectangle `b`, returning both the clipped rectangle and
// its local offset relative to `a`.
//------------------------------------------------------------------------------

bool term_rect_clip(TermRect  a,
                    TermRect  b,
                    TermRect* out_clipped_rect,
                    TermRect* out_local_rect)
{
    u16 x0 = MAX(a.x, b.x);
    u16 y0 = MAX(a.y, b.y);
    u16 x1 = MIN(a.x + a.width, b.x + b.width);
    u16 y1 = MIN(a.y + a.height, b.y + b.height);

    if (x1 <= x0 || y1 <= y0) {
        return false;
    }

    // Output the clipped rectangle
    out_clipped_rect->x      = x0;
    out_clipped_rect->y      = y0;
    out_clipped_rect->width  = x1 - x0;
    out_clipped_rect->height = y1 - y0;

    // Output the local rectangle
    out_local_rect->x        = x0 - a.x;
    out_local_rect->y        = y0 - a.y;
    out_local_rect->width    = out_clipped_rect->width;
    out_local_rect->height   = out_clipped_rect->height;

    return true;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
