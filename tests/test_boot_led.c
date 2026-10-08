/* boot_led: the bootloader's LED patterns, measured millisecond by
   millisecond. Duty within APP_LED_DUTY_PCT_MAX (CLAUDE.md 6.5), the double
   flash as the phase 5 plan draws it (2.7), and code N with the
   application's own timing. */

#include "app/boot_led.h"

#include "app/app_config.h"
#include "app/app_fault.h"
#include "test_check.h"

#define PERIODS   10u

/* Lit milliseconds and rising edges over span ms from 'from' - wrapping
   through 0xFFFFFFFF like HAL_GetTick() does. code < 0: the double flash. */
static void measure(int code, uint32_t from, uint32_t span, uint32_t *lit, uint32_t *edges)
{
  int prev = 0;
  *lit = 0u;
  *edges = 0u;
  for (uint32_t i = 0u; i < span; i++)
  {
    const uint32_t t = from + i;
    const int on = (code < 0) ? boot_led_double_on(t) : boot_led_code_on((uint32_t)code, t);
    if (on)
    {
      (*lit)++;
      if (!prev)
      {
        (*edges)++;
      }
    }
    prev = on;
  }
}

static void test_double_flash(void)
{
  uint32_t lit;
  uint32_t edges;

  measure(-1, 0u, PERIODS * BOOT_LED_PERIOD_MS, &lit, &edges);
  CHECK_EQ(lit, PERIODS * 2u * BOOT_LED_ON_MS);     /* 100 ms per second */
  CHECK_EQ(edges, PERIODS * 2u);                    /* two flashes per second */
  CHECK(lit * 100u <= APP_LED_DUTY_PCT_MAX * PERIODS * BOOT_LED_PERIOD_MS);

  /* 50 on, 150 off, 50 on, 750 off. */
  CHECK(boot_led_double_on(0u));
  CHECK(boot_led_double_on(49u));
  CHECK(!boot_led_double_on(50u));
  CHECK(!boot_led_double_on(199u));
  CHECK(boot_led_double_on(200u));
  CHECK(boot_led_double_on(249u));
  CHECK(!boot_led_double_on(250u));
  CHECK(!boot_led_double_on(999u));
  CHECK(boot_led_double_on(1000u));

  /* Across the 32-bit tick wrap (day 49): the period running at the wrap is
     cut to 296 ms and still holds its two flashes, so this 5 s window has 12
     instead of 10 - once in 49 days. What must hold is that no flash is ever
     longer than BOOT_LED_ON_MS: the LED cannot sit lit. */
  measure(-1, 0xFFFFFFFFu - 2500u, 5000u, &lit, &edges);
  CHECK_EQ(lit, edges * BOOT_LED_ON_MS);
  CHECK(edges <= 10u + 2u);
}

static void test_codes(void)
{
  CHECK_EQ(BOOT_LED_CODE_HAL_INIT, 1u);
  CHECK_EQ(BOOT_LED_CODE_UNCONFIGURED, 3u);
  CHECK_EQ(BOOT_LED_CODE_CAN, 5u);
  CHECK_EQ(BOOT_LED_CODE_UNCONFIGURED, APP_FAULT_CFG_CRC);
  CHECK_EQ(BOOT_LED_CODE_CAN, APP_FAULT_CAN);
  CHECK_EQ(BOOT_LED_CODE_PERIOD_MS(3u), 2650u);    /* 3x50 + 2x250 + 2000 */

  for (uint32_t n = 1u; n <= APP_LED_CODE_MAX; n++)
  {
    const uint32_t period = BOOT_LED_CODE_PERIOD_MS(n);
    uint32_t lit;
    uint32_t edges;

    measure((int)n, 0u, PERIODS * period, &lit, &edges);
    CHECK_EQ(lit, PERIODS * n * APP_LED_CODE_ON_MS);
    CHECK_EQ(edges, PERIODS * n);
    CHECK(lit * 100u <= APP_LED_DUTY_PCT_MAX * PERIODS * period);

    /* The group starts at 0, flashes are ON + OFF apart, and the gap
       follows the last one. */
    CHECK(boot_led_code_on(n, 0u));
    CHECK(!boot_led_code_on(n, APP_LED_CODE_ON_MS));
    CHECK(boot_led_code_on(n, (n - 1u) * (APP_LED_CODE_ON_MS + APP_LED_CODE_OFF_MS)));
    CHECK(!boot_led_code_on(n, period - 1u));
  }

  /* Nothing to say, or nonsense to say: dark. */
  uint32_t lit;
  uint32_t edges;
  measure(0, 0u, 10000u, &lit, &edges);
  CHECK_EQ(lit, 0u);
  measure((int)APP_LED_CODE_MAX + 1, 0u, 10000u, &lit, &edges);
  CHECK_EQ(lit, 0u);
}

int main(void)
{
  test_double_flash();
  test_codes();
  return TEST_REPORT("test_boot_led");
}
