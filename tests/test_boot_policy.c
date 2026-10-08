/* boot_policy: what the bootloader does at power-up (phase 5 plan 2.2) and
   whether an application is there to jump to. CAN2 section 5, invariant 5. */

#include "app/boot_policy.h"

#include "app/app_cfg.h"
#include "app/app_config.h"
#include "test_check.h"

#include <string.h>

#define SRAM_TOP     0x20003000u   /* _estack of the application linker script */
#define APP_ENTRY    0x08003195u   /* a plausible Reset_Handler: odd, inside the region */

static void test_power_up_table(void)
{
  for (unsigned state = 0u; state < 256u; state++)
  {
    /* No application: rescue, whatever the config says. */
    CHECK_EQ(boot_policy_at_power_up(false, (uint8_t)state), BOOT_POLICY_STAY_RESCUE);

    /* Application: listen only with a config the application will accept. */
    CHECK_EQ(boot_policy_at_power_up(true, (uint8_t)state),
             (state == APP_CFG_OK) ? BOOT_POLICY_LISTEN : BOOT_POLICY_STAY_UNCONFIGURED);
  }

  /* Each named reason, by name. */
  CHECK_EQ(boot_policy_at_power_up(true, APP_CFG_BLANK), BOOT_POLICY_STAY_UNCONFIGURED);
  CHECK_EQ(boot_policy_at_power_up(true, APP_CFG_BAD_MAGIC), BOOT_POLICY_STAY_UNCONFIGURED);
  CHECK_EQ(boot_policy_at_power_up(true, APP_CFG_BAD_VERSION), BOOT_POLICY_STAY_UNCONFIGURED);
  CHECK_EQ(boot_policy_at_power_up(true, APP_CFG_BAD_CRC), BOOT_POLICY_STAY_UNCONFIGURED);
  CHECK_EQ(boot_policy_at_power_up(true, APP_CFG_BAD_TYPE), BOOT_POLICY_STAY_UNCONFIGURED);
  CHECK_EQ(boot_policy_at_power_up(true, APP_CFG_BAD_NODE), BOOT_POLICY_STAY_UNCONFIGURED);
  CHECK_EQ(boot_policy_at_power_up(true, APP_CFG_BAD_RESERVED), BOOT_POLICY_STAY_UNCONFIGURED);
}

static void test_real_pages(void)
{
  /* Through app_cfg_check, as main.c does it: the invariant "jumps <=> the
     application will transmit" on real records. */
  app_cfg_t c;

  memset(&c, 0xFF, sizeof(c));             /* blank page: a new module */
  CHECK_EQ(boot_policy_at_power_up(true, app_cfg_check(&c)), BOOT_POLICY_STAY_UNCONFIGURED);

  app_cfg_defaults(&c);                    /* factory record: node 0xFF */
  CHECK_EQ(boot_policy_at_power_up(true, app_cfg_check(&c)), BOOT_POLICY_STAY_UNCONFIGURED);

  for (uint8_t node = 0x30u; node <= 0x39u; node++)
  {
    app_cfg_defaults(&c);
    c.node_id = node;
    app_cfg_seal(&c);
    CHECK_EQ(boot_policy_at_power_up(true, app_cfg_check(&c)), BOOT_POLICY_LISTEN);

    c.crc32 ^= 1u;                         /* power lost mid-write */
    CHECK_EQ(boot_policy_at_power_up(true, app_cfg_check(&c)), BOOT_POLICY_STAY_UNCONFIGURED);
  }
}

static void test_app_present(void)
{
  /* The normal case: stack at the top of SRAM, entry odd and inside. */
  CHECK(boot_policy_app_present(SRAM_TOP, APP_ENTRY));
  CHECK_EQ(BOOT_SRAM_BASE + BOOT_SRAM_SIZE, SRAM_TOP);

  /* Erased flash - the case that matters. */
  CHECK(!boot_policy_app_present(0xFFFFFFFFu, 0xFFFFFFFFu));
  CHECK(!boot_policy_app_present(SRAM_TOP, 0xFFFFFFFFu));
  CHECK(!boot_policy_app_present(0xFFFFFFFFu, APP_ENTRY));
  CHECK(!boot_policy_app_present(0u, 0u));

  /* Stack pointer: SRAM only, both ends included. */
  CHECK(boot_policy_app_present(BOOT_SRAM_BASE, APP_ENTRY));
  CHECK(!boot_policy_app_present(BOOT_SRAM_BASE - 4u, APP_ENTRY));
  CHECK(!boot_policy_app_present(SRAM_TOP + 4u, APP_ENTRY));
  CHECK(!boot_policy_app_present(0x10001000u, APP_ENTRY));     /* CCM RAM: not where the app's stack is */
  CHECK(!boot_policy_app_present(APP_IMAGE_BASE, APP_ENTRY));  /* a flash address */

  /* Reset vector: odd (Thumb). */
  CHECK(!boot_policy_app_present(SRAM_TOP, APP_ENTRY - 1u));

  /* Reset vector: inside [APP_IMAGE_BASE, APP_CFG_FLASH_ADDR). */
  CHECK(boot_policy_app_present(SRAM_TOP, APP_IMAGE_BASE + 1u));
  CHECK(boot_policy_app_present(SRAM_TOP, APP_IMAGE_BASE + APP_IMAGE_SIZE - 1u));
  CHECK(!boot_policy_app_present(SRAM_TOP, APP_IMAGE_BASE - 1u));          /* the bootloader's own code */
  CHECK(!boot_policy_app_present(SRAM_TOP, APP_BOOT_BASE + 0x1C5u));        /* an application linked at 0x08000000 */
  CHECK(!boot_policy_app_present(SRAM_TOP, APP_CFG_FLASH_ADDR + 1u));       /* the config page */
  CHECK(!boot_policy_app_present(SRAM_TOP, 0x1FFFD801u));                    /* system memory */
}

int main(void)
{
  test_power_up_table();
  test_real_pages();
  test_app_present();
  return TEST_REPORT("test_boot_policy");
}
