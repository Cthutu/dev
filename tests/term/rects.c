//> use: core term

#include <core/core.h>
#include <term/term.h>
#include <test.h>

TEST_CASE(rects, rect_constructs_expected_values)
{
    TermRect rect = term_rect(3, 4, 5, 6);

    TEST_ASSERT_EQ(rect.x, 3);
    TEST_ASSERT_EQ(rect.y, 4);
    TEST_ASSERT_EQ(rect.width, 5);
    TEST_ASSERT_EQ(rect.height, 6);
}

TEST_CASE(rects, from_points_normalises_bounds)
{
    TermRect rect = term_rect_from_points(7, 9, 3, 4);

    TEST_ASSERT_EQ(rect.x, 3);
    TEST_ASSERT_EQ(rect.y, 4);
    TEST_ASSERT_EQ(rect.width, 5);
    TEST_ASSERT_EQ(rect.height, 6);
}

TEST_CASE(rects, union_spans_both_rectangles)
{
    TermRect a      = term_rect(2, 3, 4, 2);
    TermRect b      = term_rect(5, 1, 3, 5);
    TermRect result = term_rect_union(a, b);

    TEST_ASSERT(term_rect_equals(result, term_rect(2, 1, 6, 5)));
}

TEST_CASE(rects, intersection_returns_shared_area)
{
    TermRect a      = term_rect(1, 2, 5, 4);
    TermRect b      = term_rect(3, 1, 4, 3);
    TermRect result = term_rect_intersection(a, b);

    TEST_ASSERT(term_rect_equals(result, term_rect(3, 2, 3, 2)));
}

TEST_CASE(rects, intersection_of_disjoint_rectangles_is_empty)
{
    TermRect a      = term_rect(0, 0, 2, 2);
    TermRect b      = term_rect(2, 0, 3, 3);
    TermRect result = term_rect_intersection(a, b);

    TEST_ASSERT(term_rect_equals(result, term_rect(0, 0, 0, 0)));
    TEST_ASSERT(term_rect_is_empty(result));
}

TEST_CASE(rects, equals_matches_all_fields)
{
    TermRect rect = term_rect(4, 5, 6, 7);

    TEST_ASSERT(term_rect_equals(rect, term_rect(4, 5, 6, 7)));
    TEST_ASSERT(!term_rect_equals(rect, term_rect(4, 5, 6, 8)));
}

TEST_CASE(rects, contains_uses_inclusive_exclusive_edges)
{
    TermRect rect = term_rect(2, 3, 4, 5);

    TEST_ASSERT(term_rect_contains(rect, 2, 3));
    TEST_ASSERT(term_rect_contains(rect, 5, 7));
    TEST_ASSERT(!term_rect_contains(rect, 6, 7));
    TEST_ASSERT(!term_rect_contains(rect, 5, 8));
}

TEST_CASE(rects, overlaps_requires_positive_shared_area)
{
    TermRect a = term_rect(1, 1, 3, 3);
    TermRect b = term_rect(4, 4, 2, 2);
    TermRect c = term_rect(2, 2, 2, 2);

    TEST_ASSERT(!term_rect_overlaps(a, b));
    TEST_ASSERT(term_rect_overlaps(a, c));
}

TEST_CASE(rects, is_empty_when_either_dimension_is_zero)
{
    TEST_ASSERT(term_rect_is_empty(term_rect(0, 0, 0, 4)));
    TEST_ASSERT(term_rect_is_empty(term_rect(0, 0, 4, 0)));
    TEST_ASSERT(!term_rect_is_empty(term_rect(0, 0, 1, 1)));
}

TEST_CASE(rects, clip_outputs_clipped_and_local_rectangles)
{
    TermRect a = term_rect(3, 4, 5, 4);
    TermRect b = term_rect(5, 2, 4, 5);

    TermRect clipped = term_rect(0, 0, 0, 0);
    TermRect local   = term_rect(0, 0, 0, 0);

    TEST_ASSERT(term_rect_clip(a, b, &clipped, &local));
    TEST_ASSERT(term_rect_equals(clipped, term_rect(5, 4, 3, 3)));
    TEST_ASSERT(term_rect_equals(local, term_rect(2, 0, 3, 3)));
}

TEST_CASE(rects, clip_returns_false_when_rectangles_do_not_overlap)
{
    TermRect a = term_rect(0, 0, 2, 2);
    TermRect b = term_rect(2, 2, 3, 3);

    TermRect clipped = term_rect(9, 9, 9, 9);
    TermRect local   = term_rect(8, 8, 8, 8);

    TEST_ASSERT(!term_rect_clip(a, b, &clipped, &local));
}
