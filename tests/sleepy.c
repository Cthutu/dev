//> use: core

#include <core/core.h>
#include <test.h>

TEST_CASE(sleepy, one)
{
    TimePoint start = time_now();
    time_sleep_ms(1000);
    TEST_ASSERT_GE(time_elapsed(start, time_now()), time_from_ms(900));
}

TEST_CASE(sleepy, two)
{
    TimePoint start = time_now();
    time_sleep_ms(1000);
    TEST_ASSERT_GE(time_elapsed(start, time_now()), time_from_ms(900));
}

TEST_CASE(sleepy, three)
{
    TimePoint start = time_now();
    time_sleep_ms(1000);
    TEST_ASSERT_GE(time_elapsed(start, time_now()), time_from_ms(900));
}

TEST_CASE(sleepy, four)
{
    TimePoint start = time_now();
    time_sleep_ms(1000);
    TEST_ASSERT_GE(time_elapsed(start, time_now()), time_from_ms(900));
}

TEST_CASE(sleepy, five)
{
    TimePoint start = time_now();
    time_sleep_ms(1000);
    TEST_ASSERT_GE(time_elapsed(start, time_now()), time_from_ms(900));
}

TEST_CASE(sleepy, six)
{
    TimePoint start = time_now();
    time_sleep_ms(1000);
    TEST_ASSERT_GE(time_elapsed(start, time_now()), time_from_ms(900));
}

TEST_CASE(sleepy, seven)
{
    TimePoint start = time_now();
    time_sleep_ms(1000);
    TEST_ASSERT_GE(time_elapsed(start, time_now()), time_from_ms(900));
}

TEST_CASE(sleepy, eight)
{
    TimePoint start = time_now();
    time_sleep_ms(1000);
    TEST_ASSERT_GE(time_elapsed(start, time_now()), time_from_ms(900));
}

TEST_CASE(sleepy, nine)
{
    TimePoint start = time_now();
    time_sleep_ms(1000);
    TEST_ASSERT_GE(time_elapsed(start, time_now()), time_from_ms(900));
}

TEST_CASE(sleepy, ten)
{
    TimePoint start = time_now();
    time_sleep_ms(1000);
    TEST_ASSERT_GE(time_elapsed(start, time_now()), time_from_ms(900));
}
