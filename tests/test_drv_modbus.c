/* drv_modbus against the frames printed in the sensor datasheet:
   docs/[Hshop.vn] SHT RS485 Modbus-RTU Temperature Humidity Sensor.pdf,
   pages 7-11. Every byte below is copied from a page image, not computed -
   so a CRC implementation that agrees with itself but not with the sensor
   fails here. */

#include "app/drv_modbus.h"
#include "test_check.h"

#include <string.h>

typedef struct
{
  const char *where;
  uint8_t     body[8];
  uint8_t     n;          /* bytes before the CRC */
  uint8_t     crc_lo;
  uint8_t     crc_hi;
} vector_t;

static const vector_t k_vectors[] =
{
  { "p7 request T+RH",       {0x01, 0x03, 0x00, 0x00, 0x00, 0x02},       6, 0xC4, 0x0B },
  { "p7 reply T+RH",         {0x01, 0x03, 0x04, 0x01, 0x02, 0xFF, 0x33}, 7, 0x5B, 0xEA },
  { "p7 request T",          {0x01, 0x03, 0x00, 0x01, 0x00, 0x01},       6, 0xD5, 0xCA },
  { "p8 request RH",         {0x01, 0x03, 0x00, 0x00, 0x00, 0x01},       6, 0x84, 0x0A },
  { "p9 request address",    {0xFF, 0x03, 0x07, 0xD0, 0x00, 0x01},       6, 0x91, 0x59 },
  { "p9 reply address 1",    {0x01, 0x03, 0x02, 0x00, 0x01},             5, 0x79, 0x84 },
  { "p10 reply address 5",   {0x05, 0x03, 0x02, 0x00, 0x05},             5, 0x89, 0x87 },
  { "p10 write address 5",   {0x01, 0x06, 0x07, 0xD0, 0x00, 0x05},       6, 0x49, 0x44 },
  { "p11 write baud 2400",   {0x01, 0x06, 0x07, 0xD1, 0x00, 0x00},       6, 0xD8, 0x87 },
  { "p11 write baud 4800",   {0x01, 0x06, 0x07, 0xD1, 0x00, 0x01},       6, 0x19, 0x47 },
  { "p11 write baud 9600",   {0x01, 0x06, 0x07, 0xD1, 0x00, 0x02},       6, 0x59, 0x46 },
};

/* p.7, the one complete reply with real numbers in it. */
static const uint8_t k_reply_p7[9] = { 0x01, 0x03, 0x04, 0x01, 0x02, 0xFF, 0x33, 0x5B, 0xEA };

static const mb_read_req_t k_req_th  = { 0x01u, SHT_READ_FIRST_REG, SHT_READ_COUNT };
static const mb_read_req_t k_req_any = { MB_ADDR_ANY, SHT_REG_ADDRESS, 1u };

static void put_crc(uint8_t *f, uint8_t body_len)
{
  const uint16_t c = drv_modbus_crc16(f, body_len);
  f[body_len] = (uint8_t)(c & 0xFFu);
  f[body_len + 1u] = (uint8_t)(c >> 8);
}

static void test_crc_vectors(void)
{
  for (size_t i = 0; i < sizeof(k_vectors) / sizeof(k_vectors[0]); i++)
  {
    const uint16_t c = drv_modbus_crc16(k_vectors[i].body, k_vectors[i].n);
    if (((c & 0xFFu) != k_vectors[i].crc_lo) || ((c >> 8) != k_vectors[i].crc_hi))
    {
      printf("  vector \"%s\": got %02X %02X\n", k_vectors[i].where, c & 0xFFu, c >> 8);
    }
    CHECK_EQ(c & 0xFFu, k_vectors[i].crc_lo);
    CHECK_EQ(c >> 8, k_vectors[i].crc_hi);
  }
}

static void test_build(void)
{
  uint8_t out[MB_READ_REQ_LEN];
  static const uint8_t p7[8] = { 0x01, 0x03, 0x00, 0x00, 0x00, 0x02, 0xC4, 0x0B };
  static const uint8_t p9[8] = { 0xFF, 0x03, 0x07, 0xD0, 0x00, 0x01, 0x91, 0x59 };

  CHECK_EQ(drv_modbus_build_read(&k_req_th, out), MB_READ_REQ_LEN);
  CHECK(memcmp(out, p7, sizeof(p7)) == 0);

  CHECK_EQ(drv_modbus_build_read(&k_req_any, out), MB_READ_REQ_LEN);
  CHECK(memcmp(out, p9, sizeof(p9)) == 0);

  const mb_read_req_t zero = { 0x01u, 0u, 0u };
  const mb_read_req_t many = { 0x01u, 0u, MB_READ_MAX_REGS + 1u };
  CHECK_EQ(drv_modbus_build_read(&zero, out), 0);
  CHECK_EQ(drv_modbus_build_read(&many, out), 0);
}

static void test_expected_len(void)
{
  /* Grows byte by byte the way the receiver sees it. */
  CHECK_EQ(drv_modbus_expected_len(k_reply_p7, 0), 0);
  CHECK_EQ(drv_modbus_expected_len(k_reply_p7, 1), 0);
  CHECK_EQ(drv_modbus_expected_len(k_reply_p7, 2), 0);       /* func known, byte count not yet */
  for (uint8_t n = 3; n <= 9; n++)
  {
    CHECK_EQ(drv_modbus_expected_len(k_reply_p7, n), 9);
  }

  const uint8_t exc[2] = { 0x01, 0x83 };
  CHECK_EQ(drv_modbus_expected_len(exc, 2), MB_EXCEPTION_LEN);

  const uint8_t odd[3] = { 0x01, 0x10, 0x00 };
  CHECK_EQ(drv_modbus_expected_len(odd, 3), 0);               /* unknown function: ends on silence */
}

static void test_parse_p7(void)
{
  mb_reply_t r;
  memset(&r, 0, sizeof(r));
  CHECK_EQ(drv_modbus_parse_read(k_reply_p7, 9, &k_req_th, &r), MB_OK);
  CHECK_EQ(r.addr, 0x01);
  CHECK_EQ(r.regs[0], 258);                    /* 25.8 %RH */
  CHECK_EQ((int16_t)r.regs[1], -205);          /* -20.5 C, two's complement */
}

static void test_parse_address_scan(void)
{
  /* p.10: the sensor at address 5 answers a 0xFF read with its own address. */
  const uint8_t reply5[7] = { 0x05, 0x03, 0x02, 0x00, 0x05, 0x89, 0x87 };
  mb_reply_t r;
  memset(&r, 0, sizeof(r));
  CHECK_EQ(drv_modbus_parse_read(reply5, 7, &k_req_any, &r), MB_OK);
  CHECK_EQ(r.addr, 5);
  CHECK_EQ(r.regs[0], 5);

  /* The same frame is the wrong answer to a read addressed to sensor 1. */
  const mb_read_req_t to1 = { 0x01u, SHT_REG_ADDRESS, 1u };
  CHECK_EQ(drv_modbus_parse_read(reply5, 7, &to1, &r), MB_ERR_ADDR);
}

static void test_parse_errors(void)
{
  uint8_t f[MB_RX_BUF_LEN];
  mb_reply_t r;

  /* Truncated at every length. Below 5 bytes it cannot even be an exception;
     from 5 to 8 the header announces 9. */
  for (uint8_t n = 0; n < 9; n++)
  {
    CHECK_EQ(drv_modbus_parse_read(k_reply_p7, n, &k_req_th, &r), MB_ERR_SHORT);
  }

  /* One extra byte after a complete frame. */
  memcpy(f, k_reply_p7, 9);
  f[9] = 0x00;
  CHECK_EQ(drv_modbus_parse_read(f, 10, &k_req_th, &r), MB_ERR_LEN);

  /* Every single-bit flip in the p.7 reply is caught. Flips in the byte count
     change the announced length, so they show as SHORT or LEN instead of CRC -
     still an error, which is the point. */
  for (uint8_t byte = 0; byte < 9; byte++)
  {
    for (uint8_t bit = 0; bit < 8; bit++)
    {
      memcpy(f, k_reply_p7, 9);
      f[byte] ^= (uint8_t)(1u << bit);
      const mb_status_t st = drv_modbus_parse_read(f, 9, &k_req_th, &r);
      CHECK(st != MB_OK);
      if ((byte != 1) && (byte != 2))
      {
        CHECK_EQ(st, MB_ERR_CRC);
      }
    }
  }

  /* Good CRC, wrong address. */
  memcpy(f, k_reply_p7, 7);
  f[0] = 0x02;
  put_crc(f, 7);
  CHECK_EQ(drv_modbus_parse_read(f, 9, &k_req_th, &r), MB_ERR_ADDR);

  /* Good CRC, one register instead of two (byte count 2). */
  const uint8_t one_reg_body[5] = { 0x01, 0x03, 0x02, 0x01, 0x02 };
  memcpy(f, one_reg_body, 5);
  put_crc(f, 5);
  CHECK_EQ(drv_modbus_parse_read(f, 7, &k_req_th, &r), MB_ERR_LEN);

  /* Good CRC, function 0x04 instead of 0x03 - cannot be sized, ends on silence. */
  const uint8_t f04_body[7] = { 0x01, 0x04, 0x04, 0x01, 0x02, 0xFF, 0x33 };
  memcpy(f, f04_body, 7);
  put_crc(f, 7);
  CHECK_EQ(drv_modbus_parse_read(f, 9, &k_req_th, &r), MB_ERR_FUNC);

  /* Exception 0x02 (illegal data address) to our 0x03. */
  const uint8_t exc_body[3] = { 0x01, 0x83, 0x02 };
  memcpy(f, exc_body, 3);
  put_crc(f, 3);
  memset(&r, 0, sizeof(r));
  CHECK_EQ(drv_modbus_parse_read(f, 5, &k_req_th, &r), MB_ERR_EXCEPTION);
  CHECK_EQ(r.exception, 0x02);

  /* An exception to a function we did not send. */
  const uint8_t exc6_body[3] = { 0x01, 0x86, 0x02 };
  memcpy(f, exc6_body, 3);
  put_crc(f, 3);
  CHECK_EQ(drv_modbus_parse_read(f, 5, &k_req_th, &r), MB_ERR_FUNC);

  /* An exception with a bad CRC is a CRC error, not an exception. */
  memcpy(f, exc_body, 3);
  put_crc(f, 3);
  f[4] ^= 0x01u;
  CHECK_EQ(drv_modbus_parse_read(f, 5, &k_req_th, &r), MB_ERR_CRC);
}

int main(void)
{
  test_crc_vectors();
  test_build();
  test_expected_len();
  test_parse_p7();
  test_parse_address_scan();
  test_parse_errors();
  return TEST_REPORT("test_drv_modbus");
}
