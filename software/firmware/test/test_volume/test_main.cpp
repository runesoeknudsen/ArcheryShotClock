#include <unity.h>

#include "core/volume.h"

void test_volume_is_clamped_to_the_0_100_range() {
  TEST_ASSERT_EQUAL_UINT8(0, Core::clampVolume(-20));
  TEST_ASSERT_EQUAL_UINT8(0, Core::clampVolume(0));
  TEST_ASSERT_EQUAL_UINT8(60, Core::clampVolume(60));
  TEST_ASSERT_EQUAL_UINT8(100, Core::clampVolume(100));
  TEST_ASSERT_EQUAL_UINT8(100, Core::clampVolume(255));
}

void test_zero_volume_is_silence() {
  TEST_ASSERT_EQUAL_INT16(0, Core::scaleSample(18000, 0));
  TEST_ASSERT_EQUAL_INT16(0, Core::scaleSample(-18000, 0));
}

void test_full_volume_leaves_the_sample_unchanged() {
  TEST_ASSERT_EQUAL_INT16(18000, Core::scaleSample(18000, 100));
  TEST_ASSERT_EQUAL_INT16(-18000, Core::scaleSample(-18000, 100));
}

void test_half_volume_scales_the_sample_linearly() {
  TEST_ASSERT_EQUAL_INT16(9000, Core::scaleSample(18000, 50));
  TEST_ASSERT_EQUAL_INT16(-9000, Core::scaleSample(-18000, 50));
}

void setUp() {}
void tearDown() {}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_volume_is_clamped_to_the_0_100_range);
  RUN_TEST(test_zero_volume_is_silence);
  RUN_TEST(test_full_volume_leaves_the_sample_unchanged);
  RUN_TEST(test_half_volume_scales_the_sample_linearly);
  return UNITY_END();
}
