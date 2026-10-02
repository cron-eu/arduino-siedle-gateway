/*
 * Host unit tests for the siedle_proto component.
 *
 * Reference values were generated with lambda/siedle-lib.js, so firmware and Lambda stay in sync.
 */
#include "siedle_proto.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

static void test_fields_match_lambda(void)
{
    // captured "door open" frame
    TEST_ASSERT_EQUAL_UINT8(3, siedle_cmd_signal(1181356688u));
    TEST_ASSERT_EQUAL_UINT16(164, siedle_cmd_src(1181356688u));
    TEST_ASSERT_EQUAL_UINT16(424, siedle_cmd_dst(1181356688u));

    // captured "ring" frame
    TEST_ASSERT_EQUAL_UINT8(2, siedle_cmd_signal(1151144720u));
    TEST_ASSERT_EQUAL_UINT16(196, siedle_cmd_src(1151144720u));
    TEST_ASSERT_EQUAL_UINT16(116, siedle_cmd_dst(1151144720u));
}

static void test_make_matches_lambda(void)
{
    TEST_ASSERT_EQUAL_UINT32(1181356688u, siedle_cmd_make(3, 164, 424));
    TEST_ASSERT_EQUAL_UINT32(1174491228u, siedle_cmd_make(3, 23, 5));
    TEST_ASSERT_EQUAL_UINT32(1141051484u, siedle_cmd_make(2, 23, 12));
}

static void test_make_roundtrip(void)
{
    siedle_cmd_t cmd = siedle_cmd_make(11, 0x1AB, 0x0CD);
    TEST_ASSERT_EQUAL_UINT8(11, siedle_cmd_signal(cmd));
    TEST_ASSERT_EQUAL_UINT16(0x1AB, siedle_cmd_src(cmd));
    TEST_ASSERT_EQUAL_UINT16(0x0CD, siedle_cmd_dst(cmd));
    // every frame starts with a 0 bit (falling edge)
    TEST_ASSERT_EQUAL_UINT32(0, cmd >> 31);
}

static void test_signal_names(void)
{
    TEST_ASSERT_EQUAL_STRING("ring", siedle_signal_name(SIEDLE_SIGNAL_RING));
    TEST_ASSERT_EQUAL_STRING("door open", siedle_signal_name(SIEDLE_SIGNAL_DOOR_OPEN));
    TEST_ASSERT_NULL(siedle_signal_name(15));
}

static void test_encode_merges_equal_bits(void)
{
    siedle_run_t runs[SIEDLE_FRAME_BITS];
    // 0b0100...0001 -> 0, 1, 29x0, 1
    size_t n = siedle_encode_runs(0x40000001u, SIEDLE_BIT_US, runs, SIEDLE_FRAME_BITS);
    TEST_ASSERT_EQUAL(4, n);
    TEST_ASSERT_EQUAL_UINT8(0, runs[0].level);
    TEST_ASSERT_EQUAL_UINT32(1 * SIEDLE_BIT_US, runs[0].duration_us);
    TEST_ASSERT_EQUAL_UINT8(1, runs[1].level);
    TEST_ASSERT_EQUAL_UINT32(1 * SIEDLE_BIT_US, runs[1].duration_us);
    TEST_ASSERT_EQUAL_UINT8(0, runs[2].level);
    TEST_ASSERT_EQUAL_UINT32(29 * SIEDLE_BIT_US, runs[2].duration_us);
    TEST_ASSERT_EQUAL_UINT8(1, runs[3].level);
    TEST_ASSERT_EQUAL_UINT32(1 * SIEDLE_BIT_US, runs[3].duration_us);
}

static void test_encode_buffer_too_small(void)
{
    siedle_run_t runs[2];
    TEST_ASSERT_EQUAL(0, siedle_encode_runs(0x55555555u, SIEDLE_BIT_US, runs, 2));
}

static void test_encode_decode_roundtrip(void)
{
    const siedle_cmd_t cmds[] = { 1181356688u, 1151144720u, 1155339024u, 0x55555554u, 0x7FFFFFFFu, 0x00000000u };
    for (size_t i = 0; i < sizeof(cmds) / sizeof(cmds[0]); i++) {
        siedle_run_t runs[SIEDLE_FRAME_BITS];
        size_t n = siedle_encode_runs(cmds[i], SIEDLE_BIT_US, runs, SIEDLE_FRAME_BITS);
        TEST_ASSERT_NOT_EQUAL(0, n);
        siedle_cmd_t decoded = 0;
        TEST_ASSERT_EQUAL(SIEDLE_DECODE_OK, siedle_decode_runs(runs, n, SIEDLE_BIT_US, &decoded));
        TEST_ASSERT_EQUAL_HEX32(cmds[i], decoded);
    }
}

static void test_decode_tolerates_timing_jitter(void)
{
    // 0b0110 followed by idle: durations off by up to 40% of a bit
    const siedle_run_t runs[] = {
        { 0, 2700 },  // 1 bit (rounds down)
        { 1, 3300 },  // 2 bits (rounds down from 1.65)
        { 0, 1300 },  // 1 bit
        { 1, 0 },     // idle until the end
    };
    siedle_cmd_t cmd = 0;
    TEST_ASSERT_EQUAL(SIEDLE_DECODE_OK, siedle_decode_runs(runs, 4, SIEDLE_BIT_US, &cmd));
    TEST_ASSERT_EQUAL_HEX32(0x6FFFFFFFu, cmd);
}

static void test_decode_pads_missing_idle_bits(void)
{
    // capture ended after the last low run, the rest is idle bus
    const siedle_run_t runs[] = { { 0, 2000 }, { 1, 2000 }, { 0, 4000 } };
    siedle_cmd_t cmd = 0;
    TEST_ASSERT_EQUAL(SIEDLE_DECODE_TRUNCATED, siedle_decode_runs(runs, 3, SIEDLE_BIT_US, &cmd));

    const siedle_run_t runs_idle[] = { { 0, 2000 }, { 1, 2000 }, { 0, 4000 }, { 1, 2000 } };
    TEST_ASSERT_EQUAL(SIEDLE_DECODE_OK, siedle_decode_runs(runs_idle, 4, SIEDLE_BIT_US, &cmd));
    TEST_ASSERT_EQUAL_HEX32(0x4FFFFFFFu, cmd);
}

static void test_decode_ignores_surplus_idle(void)
{
    // the last high run usually lasts much longer than the frame
    const siedle_run_t runs[] = { { 0, 62000 }, { 1, 500000 } };
    siedle_cmd_t cmd = 0;
    TEST_ASSERT_EQUAL(SIEDLE_DECODE_OK, siedle_decode_runs(runs, 2, SIEDLE_BIT_US, &cmd));
    TEST_ASSERT_EQUAL_HEX32(0x00000001u, cmd);
}

static void test_decode_errors(void)
{
    siedle_cmd_t cmd = 0xDEADBEEFu;
    const siedle_run_t starts_high[] = { { 1, 2000 }, { 0, 2000 } };
    TEST_ASSERT_EQUAL(SIEDLE_DECODE_NO_START, siedle_decode_runs(starts_high, 2, SIEDLE_BIT_US, &cmd));
    TEST_ASSERT_EQUAL(SIEDLE_DECODE_NO_START, siedle_decode_runs(starts_high, 0, SIEDLE_BIT_US, &cmd));

    const siedle_run_t glitch[] = { { 0, 2000 }, { 1, 400 }, { 0, 2000 }, { 1, 0 } };
    TEST_ASSERT_EQUAL(SIEDLE_DECODE_GLITCH, siedle_decode_runs(glitch, 4, SIEDLE_BIT_US, &cmd));

    // output must stay untouched on error
    TEST_ASSERT_EQUAL_HEX32(0xDEADBEEFu, cmd);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_fields_match_lambda);
    RUN_TEST(test_make_matches_lambda);
    RUN_TEST(test_make_roundtrip);
    RUN_TEST(test_signal_names);
    RUN_TEST(test_encode_merges_equal_bits);
    RUN_TEST(test_encode_buffer_too_small);
    RUN_TEST(test_encode_decode_roundtrip);
    RUN_TEST(test_decode_tolerates_timing_jitter);
    RUN_TEST(test_decode_pads_missing_idle_bits);
    RUN_TEST(test_decode_ignores_surplus_idle);
    RUN_TEST(test_decode_errors);
    return UNITY_END();
}
