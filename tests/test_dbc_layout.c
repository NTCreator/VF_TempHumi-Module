/* The firmware's TH_Data / TH_Status layout against the DBC file itself.

   check_contracts.sh stops the build when contracts/ stops matching VF_Main.
   It cannot tell whether the FIRMWARE matches the DBC: after somebody copies a
   new DBC across and runs --update, the hashes agree again and the build goes
   green - with the old packing code. This test closes that gap: it reads the
   DBC in contracts/ (the Makefile passes its path), finds every TH_Data_xx /
   TH_Status_xx, and checks each signal against what app_frame actually packs:

     - IDs of all ten modules, DLC 8, sender TH_xx;
     - the exact set of signal names - a signal added, removed or renamed in
       the DBC fails here;
     - start bit, length, Intel order, sign, factor, offset, unit;
     - that the bits app_frame writes for a field are exactly the bits the DBC
       gives that signal (field set to all ones, packed, compared as a 64-bit
       mask) - and that the signals tile the 8 bytes with no gap and no overlap.

   Usage: test_dbc_layout.exe <path to the one .dbc in contracts/> */

#include "app/app_frame.h"
#include "test_check.h"

#include <stdlib.h>
#include <string.h>

#define MODULES        10u
#define LINE_MAX_LEN   8192
#define NAME_LEN       64
#define FIELD_LEN      32
#define SIGNALS_MAX    16

typedef struct
{
  char     name[NAME_LEN];
  unsigned start;
  unsigned len;
  char     order;       /* '1' = Intel */
  char     sign;        /* '+' or '-' */
  char     factor[FIELD_LEN];
  char     offset[FIELD_LEN];
  char     unit[FIELD_LEN];
} dbc_signal_t;

typedef struct
{
  unsigned     id;
  char         name[NAME_LEN];
  unsigned     dlc;
  char         sender[NAME_LEN];
  unsigned     n_signals;
  dbc_signal_t sig[SIGNALS_MAX];
  int          found;
} dbc_msg_t;

/* What the firmware means by each signal: the DBC must say the same.
   set() writes a raw value into the one field that carries the signal,
   truncated to the field's width. */
typedef struct
{
  const char *name;
  char        sign;
  const char *factor;
  const char *offset;
  const char *unit;
  void      (*set)(app_frame_data_t *d, app_frame_status_t *s, uint16_t raw);
} expect_t;

static void d_temp(app_frame_data_t *d, app_frame_status_t *s, uint16_t v)   { (void)s; d->t_x10 = (int16_t)v; }
static void d_rh(app_frame_data_t *d, app_frame_status_t *s, uint16_t v)     { (void)s; d->rh_x10 = v; }
static void d_age(app_frame_data_t *d, app_frame_status_t *s, uint16_t v)    { (void)s; d->age_x100ms = v; }
static void d_cnt(app_frame_data_t *d, app_frame_status_t *s, uint16_t v)    { (void)s; d->counter = (uint8_t)v; }
static void d_flags(app_frame_data_t *d, app_frame_status_t *s, uint16_t v)  { (void)s; d->flags = (uint8_t)v; }
static void s_fw(app_frame_data_t *d, app_frame_status_t *s, uint16_t v)     { (void)d; s->fw_version = (uint8_t)v; }
static void s_flags(app_frame_data_t *d, app_frame_status_t *s, uint16_t v)  { (void)d; s->status_flags = (uint8_t)v; }
static void s_node(app_frame_data_t *d, app_frame_status_t *s, uint16_t v)   { (void)d; s->node_id = (uint8_t)v; }
static void s_period(app_frame_data_t *d, app_frame_status_t *s, uint16_t v) { (void)d; s->tx_period_x10ms = (uint8_t)v; }
static void s_ok(app_frame_data_t *d, app_frame_status_t *s, uint16_t v)     { (void)d; s->modbus_ok = v; }
static void s_err(app_frame_data_t *d, app_frame_status_t *s, uint16_t v)    { (void)d; s->modbus_err = (uint8_t)v; }
static void s_seq(app_frame_data_t *d, app_frame_status_t *s, uint16_t v)    { (void)d; s->cmd_seq = (uint8_t)v; }

/* All ones shows WHICH bits a field occupies, but looks the same in either
   byte order. A pattern with distinct bytes shows the ORDER: for Intel (@1)
   the low byte of the value must land at the DBC start bit. */
#define RAW_ALL_ONES   0xFFFFu
#define RAW_PATTERN    0x12A5u

/* Units and factors are the firmware's wire units: x10 degC = factor 0.1,
   DataAge in 0.1 s, TxPeriod in 10 ms. */
static const expect_t k_data[] =
{
  { "Temperature", '-', "0.1", "0", "degC", d_temp  },
  { "Humidity",    '+', "0.1", "0", "%RH",  d_rh    },
  { "DataAge",     '+', "0.1", "0", "s",    d_age   },
  { "Counter",     '+', "1",   "0", "",     d_cnt   },
  { "Flags",       '+', "1",   "0", "",     d_flags },
};

static const expect_t k_status[] =
{
  { "FwVersion",   '+', "1",  "0", "",   s_fw     },
  { "StatusFlags", '+', "1",  "0", "",   s_flags  },
  { "NodeId",      '+', "1",  "0", "",   s_node   },
  { "TxPeriod",    '+', "10", "0", "ms", s_period },
  { "ModbusOk",    '+', "1",  "0", "",   s_ok     },
  { "ModbusErr",   '+', "1",  "0", "",   s_err    },
  { "CmdSeq",      '+', "1",  "0", "",   s_seq    },
};

static dbc_msg_t g_data[MODULES];
static dbc_msg_t g_status[MODULES];

static void strip_eol(char *s)
{
  size_t n = strlen(s);
  while ((n > 0u) && ((s[n - 1u] == '\n') || (s[n - 1u] == '\r')))
  {
    s[--n] = '\0';
  }
}

/* " SG_ Temperature : 0|16@1- (0.1,0) [-40|60] "degC" Vector__XXX" */
static int parse_signal(const char *line, dbc_signal_t *out)
{
  memset(out, 0, sizeof(*out));
  const int n = sscanf(line, " SG_ %63s : %u|%u@%c%c (%31[^,],%31[^)])",
                       out->name, &out->start, &out->len, &out->order, &out->sign,
                       out->factor, out->offset);
  if (n != 7)
  {
    return 0;
  }
  const char *q1 = strchr(strchr(line, ']') != NULL ? strchr(line, ']') : line, '"');
  const char *q2 = (q1 != NULL) ? strchr(q1 + 1, '"') : NULL;
  if ((q1 == NULL) || (q2 == NULL) || ((size_t)(q2 - q1 - 1) >= sizeof(out->unit)))
  {
    return 0;
  }
  memcpy(out->unit, q1 + 1, (size_t)(q2 - q1 - 1));
  return 1;
}

static int load_dbc(const char *path)
{
  FILE *f = fopen(path, "r");
  if (f == NULL)
  {
    printf("  cannot open %s\n", path);
    return 0;
  }

  static char line[LINE_MAX_LEN];
  dbc_msg_t *cur = NULL;

  while (fgets(line, sizeof(line), f) != NULL)
  {
    strip_eol(line);
    if (strncmp(line, "BO_ ", 4) == 0)
    {
      unsigned id = 0u;
      unsigned dlc = 0u;
      char name[NAME_LEN] = { 0 };
      char sender[NAME_LEN] = { 0 };
      cur = NULL;
      if (sscanf(line, "BO_ %u %63[^:]: %u %63s", &id, name, &dlc, sender) != 4)
      {
        continue;
      }
      unsigned idx = 0u;
      dbc_msg_t *table = NULL;
      if (sscanf(name, "TH_Data_%u", &idx) == 1)
      {
        table = g_data;
      }
      else if (sscanf(name, "TH_Status_%u", &idx) == 1)
      {
        table = g_status;
      }
      if ((table != NULL) && (idx >= 1u) && (idx <= MODULES))
      {
        cur = &table[idx - 1u];
        cur->found++;
        cur->id = id;
        cur->dlc = dlc;
        snprintf(cur->name, sizeof(cur->name), "%s", name);
        snprintf(cur->sender, sizeof(cur->sender), "%s", sender);
      }
    }
    else if ((cur != NULL) && (strncmp(line, " SG_ ", 5) == 0))
    {
      if (cur->n_signals >= SIGNALS_MAX)
      {
        printf("  %s: too many signals\n", cur->name);
        g_failures++;
        continue;
      }
      if (!parse_signal(line, &cur->sig[cur->n_signals]))
      {
        printf("  cannot parse: %s\n", line);
        g_failures++;
        continue;
      }
      cur->n_signals++;
    }
    else if (line[0] == '\0')
    {
      cur = NULL;    /* a blank line ends a message block */
    }
  }
  fclose(f);
  return 1;
}

static uint64_t mask_of(unsigned start, unsigned len)
{
  return ((len >= 64u) ? ~0ull : ((1ull << len) - 1ull)) << start;
}

static uint64_t le64(const uint8_t b[APP_FRAME_DLC])
{
  uint64_t v = 0u;
  for (unsigned i = 0u; i < APP_FRAME_DLC; i++)
  {
    v |= (uint64_t)b[i] << (8u * i);
  }
  return v;
}

/* Pack a frame whose only non-zero field is this one, holding raw. */
static uint64_t pack_one(const expect_t *e, int is_data, uint16_t raw)
{
  app_frame_data_t d;
  app_frame_status_t s;
  uint8_t out[APP_FRAME_DLC];
  memset(&d, 0, sizeof(d));
  memset(&s, 0, sizeof(s));
  e->set(&d, &s, raw);
  if (is_data)
  {
    app_frame_pack_data(&d, out);
  }
  else
  {
    app_frame_pack_status(&s, out);
  }
  return le64(out);
}

static void check_message(const dbc_msg_t *m, const expect_t *exp, size_t n_exp, int is_data,
                          unsigned want_id, unsigned module)
{
  char sender[NAME_LEN];
  snprintf(sender, sizeof(sender), "TH_%02u", module);

  CHECK_EQ(m->found, 1);
  CHECK_EQ(m->id, want_id);
  CHECK_EQ(m->dlc, APP_FRAME_DLC);
  CHECK(strcmp(m->sender, sender) == 0);
  CHECK_EQ(m->n_signals, n_exp);   /* nothing added, nothing missing */

  uint64_t covered = 0u;
  for (size_t e = 0; e < n_exp; e++)
  {
    const dbc_signal_t *sg = NULL;
    for (unsigned k = 0u; k < m->n_signals; k++)
    {
      if (strcmp(m->sig[k].name, exp[e].name) == 0)
      {
        sg = &m->sig[k];
      }
    }
    if (sg == NULL)
    {
      printf("  %s: no signal %s in the DBC\n", m->name, exp[e].name);
      g_failures++;
      continue;
    }

    CHECK_EQ(sg->order, '1');                         /* Intel */
    CHECK_EQ(sg->sign, exp[e].sign);
    CHECK(strcmp(sg->factor, exp[e].factor) == 0);
    CHECK(strcmp(sg->offset, exp[e].offset) == 0);
    CHECK(strcmp(sg->unit, exp[e].unit) == 0);

    /* 1. The bits app_frame writes for this field == the bits the DBC names. */
    const uint64_t want = mask_of(sg->start, sg->len);
    const uint64_t got = pack_one(&exp[e], is_data, RAW_ALL_ONES);
    if (got != want)
    {
      printf("  %s.%s: firmware bits %016llX, DBC %u|%u = %016llX\n", m->name, sg->name,
             (unsigned long long)got, sg->start, sg->len, (unsigned long long)want);
    }
    CHECK(got == want);
    CHECK((covered & want) == 0u);                    /* no overlap */
    covered |= want;

    /* 2. Byte order: the pattern, cut to the DBC length, lands at the DBC
          start bit low byte first. */
    const uint64_t pat = (uint64_t)RAW_PATTERN & mask_of(0u, sg->len);
    const uint64_t got_pat = pack_one(&exp[e], is_data, RAW_PATTERN);
    if (got_pat != (pat << sg->start))
    {
      printf("  %s.%s: pattern packed as %016llX, Intel %u|%u wants %016llX\n", m->name, sg->name,
             (unsigned long long)got_pat, sg->start, sg->len,
             (unsigned long long)(pat << sg->start));
    }
    CHECK(got_pat == (pat << sg->start));
  }
  CHECK(covered == ~0ull);                            /* all 64 bits used */
}

int main(int argc, char **argv)
{
  if (argc != 2)
  {
    printf("test_dbc_layout: need exactly one DBC path, got %d\n"
           "  (contracts/ must hold exactly one .dbc - see contracts/README.md)\n", argc - 1);
    return 1;
  }
  printf("  DBC: %s\n", argv[1]);
  if (!load_dbc(argv[1]))
  {
    return 1;
  }

  for (unsigned i = 0u; i < MODULES; i++)
  {
    const uint8_t node = (uint8_t)(APP_NODE_ID_MIN + i);
    check_message(&g_data[i], k_data, sizeof(k_data) / sizeof(k_data[0]), 1,
                  app_frame_id_data(node), i + 1u);
    check_message(&g_status[i], k_status, sizeof(k_status) / sizeof(k_status[0]), 0,
                  app_frame_id_status(node), i + 1u);
  }
  return TEST_REPORT("test_dbc_layout");
}
