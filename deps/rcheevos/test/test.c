#include "internal.h"
#include "rurl.h"

#include "smw_snes.h"
#include "galaga_nes.h"

#include <stddef.h>
#include <string.h>
#include <assert.h>

#include "lua.h"
#include "lauxlib.h"

typedef struct {
  unsigned char* ram;
  unsigned size;
}
memory_t;

static unsigned peekb(unsigned address, memory_t* memory) {
  return address < memory->size ? memory->ram[address] : 0;
}

static unsigned peek(unsigned address, unsigned num_bytes, void* ud) {
  memory_t* memory = (memory_t*)ud;

  switch (num_bytes) {
    case 1: return peekb(address, memory);

    case 2: return peekb(address, memory) |
                   peekb(address + 1, memory) << 8;

    case 4: return peekb(address, memory) |
                   peekb(address + 1, memory) << 8 |
                   peekb(address + 2, memory) << 16 |
                   peekb(address + 3, memory) << 24;
  }

  return 0;
}

static void parse_operand(rc_operand_t* self, const char** memaddr) {
  int ret = rc_parse_operand(self, memaddr, 1, NULL, 0);
  assert(ret >= 0);
  assert(**memaddr == 0);
  self->previous = 0;
  self->prior = 0;
}

static void comp_operand(rc_operand_t* self, char expected_type, char expected_size, unsigned expected_value) {
  assert(expected_type == self->type);
  assert(expected_size == self->size);
  assert(expected_value == self->value);
}

static void parse_comp_operand(const char* memaddr, char expected_type, char expected_size, unsigned expected_value) {
  rc_operand_t self;
  int ret;

  ret = rc_parse_operand(&self, &memaddr, 1, NULL, 0);
  assert(ret >= 0);
  assert(*memaddr == 0);

  comp_operand(&self, expected_type, expected_size, expected_value);
}

static void parse_error_operand(const char* memaddr, int valid_chars) {
  rc_operand_t self;
  int ret;
  const char* begin = memaddr;

  ret = rc_parse_operand(&self, &memaddr, 1, NULL, 0);
  assert(ret < 0);
  assert(memaddr - begin == valid_chars);
}

static void parse_comp_operand_value(const char* memaddr, memory_t* memory, unsigned expected_value) {
  rc_operand_t self;
  unsigned value;

  rc_parse_operand(&self, &memaddr, 1, NULL, 0);
  value = rc_evaluate_operand(&self, peek, memory, NULL);

  assert(value == expected_value);
}

static void test_operand(void) {
  {
    /*------------------------------------------------------------------------
    TestParseVariableAddress
    ------------------------------------------------------------------------*/

    /* sizes */
    parse_comp_operand("0xH1234", RC_OPERAND_ADDRESS, RC_OPERAND_8_BITS, 0x1234U);
    parse_comp_operand("0x 1234", RC_OPERAND_ADDRESS, RC_OPERAND_16_BITS, 0x1234U);
    parse_comp_operand("0x1234", RC_OPERAND_ADDRESS, RC_OPERAND_16_BITS, 0x1234U);
    parse_comp_operand("0xW1234", RC_OPERAND_ADDRESS, RC_OPERAND_24_BITS, 0x1234U);
    parse_comp_operand("0xX1234", RC_OPERAND_ADDRESS, RC_OPERAND_32_BITS, 0x1234U);
    parse_comp_operand("0xL1234", RC_OPERAND_ADDRESS, RC_OPERAND_LOW, 0x1234U);
    parse_comp_operand("0xU1234", RC_OPERAND_ADDRESS, RC_OPERAND_HIGH, 0x1234U);
    parse_comp_operand("0xM1234", RC_OPERAND_ADDRESS, RC_OPERAND_BIT_0, 0x1234U);
    parse_comp_operand("0xN1234", RC_OPERAND_ADDRESS, RC_OPERAND_BIT_1, 0x1234U);
    parse_comp_operand("0xO1234", RC_OPERAND_ADDRESS, RC_OPERAND_BIT_2, 0x1234U);
    parse_comp_operand("0xP1234", RC_OPERAND_ADDRESS, RC_OPERAND_BIT_3, 0x1234U);
    parse_comp_operand("0xQ1234", RC_OPERAND_ADDRESS, RC_OPERAND_BIT_4, 0x1234U);
    parse_comp_operand("0xR1234", RC_OPERAND_ADDRESS, RC_OPERAND_BIT_5, 0x1234U);
    parse_comp_operand("0xS1234", RC_OPERAND_ADDRESS, RC_OPERAND_BIT_6, 0x1234U);
    parse_comp_operand("0xT1234", RC_OPERAND_ADDRESS, RC_OPERAND_BIT_7, 0x1234U);

    /* sizes (ignore case) */
    parse_comp_operand("0Xh1234", RC_OPERAND_ADDRESS, RC_OPERAND_8_BITS, 0x1234U);
    parse_comp_operand("0xx1234", RC_OPERAND_ADDRESS, RC_OPERAND_32_BITS, 0x1234U);
    parse_comp_operand("0xl1234", RC_OPERAND_ADDRESS, RC_OPERAND_LOW, 0x1234U);
    parse_comp_operand("0xu1234", RC_OPERAND_ADDRESS, RC_OPERAND_HIGH, 0x1234U);
    parse_comp_operand("0xm1234", RC_OPERAND_ADDRESS, RC_OPERAND_BIT_0, 0x1234U);
    parse_comp_operand("0xn1234", RC_OPERAND_ADDRESS, RC_OPERAND_BIT_1, 0x1234U);
    parse_comp_operand("0xo1234", RC_OPERAND_ADDRESS, RC_OPERAND_BIT_2, 0x1234U);
    parse_comp_operand("0xp1234", RC_OPERAND_ADDRESS, RC_OPERAND_BIT_3, 0x1234U);
    parse_comp_operand("0xq1234", RC_OPERAND_ADDRESS, RC_OPERAND_BIT_4, 0x1234U);
    parse_comp_operand("0xr1234", RC_OPERAND_ADDRESS, RC_OPERAND_BIT_5, 0x1234U);
    parse_comp_operand("0xs1234", RC_OPERAND_ADDRESS, RC_OPERAND_BIT_6, 0x1234U);
    parse_comp_operand("0xt1234", RC_OPERAND_ADDRESS, RC_OPERAND_BIT_7, 0x1234U);

    /* addresses */
    parse_comp_operand("0xH0000", RC_OPERAND_ADDRESS, RC_OPERAND_8_BITS, 0x0000U);
    parse_comp_operand("0xH12345678", RC_OPERAND_ADDRESS, RC_OPERAND_8_BITS, 0x12345678U);
    parse_comp_operand("0xHABCD", RC_OPERAND_ADDRESS, RC_OPERAND_8_BITS, 0xABCDU);
    parse_comp_operand("0xhabcd", RC_OPERAND_ADDRESS, RC_OPERAND_8_BITS, 0xABCDU);
  }

  {
    /*------------------------------------------------------------------------
    TestParseVariableDeltaMem
    ------------------------------------------------------------------------*/

    /* sizes */
    parse_comp_operand("d0xH1234", RC_OPERAND_DELTA, RC_OPERAND_8_BITS, 0x1234U);
    parse_comp_operand("d0x 1234", RC_OPERAND_DELTA, RC_OPERAND_16_BITS, 0x1234U);
    parse_comp_operand("d0x1234", RC_OPERAND_DELTA, RC_OPERAND_16_BITS, 0x1234U);
    parse_comp_operand("d0xW1234", RC_OPERAND_DELTA, RC_OPERAND_24_BITS, 0x1234U);
    parse_comp_operand("d0xX1234", RC_OPERAND_DELTA, RC_OPERAND_32_BITS, 0x1234U);
    parse_comp_operand("d0xL1234", RC_OPERAND_DELTA, RC_OPERAND_LOW, 0x1234U);
    parse_comp_operand("d0xU1234", RC_OPERAND_DELTA, RC_OPERAND_HIGH, 0x1234U);
    parse_comp_operand("d0xM1234", RC_OPERAND_DELTA, RC_OPERAND_BIT_0, 0x1234U);
    parse_comp_operand("d0xN1234", RC_OPERAND_DELTA, RC_OPERAND_BIT_1, 0x1234U);
    parse_comp_operand("d0xO1234", RC_OPERAND_DELTA, RC_OPERAND_BIT_2, 0x1234U);
    parse_comp_operand("d0xP1234", RC_OPERAND_DELTA, RC_OPERAND_BIT_3, 0x1234U);
    parse_comp_operand("d0xQ1234", RC_OPERAND_DELTA, RC_OPERAND_BIT_4, 0x1234U);
    parse_comp_operand("d0xR1234", RC_OPERAND_DELTA, RC_OPERAND_BIT_5, 0x1234U);
    parse_comp_operand("d0xS1234", RC_OPERAND_DELTA, RC_OPERAND_BIT_6, 0x1234U);
    parse_comp_operand("d0xT1234", RC_OPERAND_DELTA, RC_OPERAND_BIT_7, 0x1234U);

    /* ignores case */
    parse_comp_operand("D0Xh1234", RC_OPERAND_DELTA, RC_OPERAND_8_BITS, 0x1234U);

    /* addresses */
    parse_comp_operand("d0xH0000", RC_OPERAND_DELTA, RC_OPERAND_8_BITS, 0x0000U);
    parse_comp_operand("d0xH12345678", RC_OPERAND_DELTA, RC_OPERAND_8_BITS, 0x12345678U);
    parse_comp_operand("d0xHABCD", RC_OPERAND_DELTA, RC_OPERAND_8_BITS, 0xABCDU);
    parse_comp_operand("d0xhabcd", RC_OPERAND_DELTA, RC_OPERAND_8_BITS, 0xABCDU);
  }

  {
    /*------------------------------------------------------------------------
    TestParseVariablePriorMem
    ------------------------------------------------------------------------*/

    /* sizes */
    parse_comp_operand("p0xH1234", RC_OPERAND_PRIOR, RC_OPERAND_8_BITS, 0x1234U);
    parse_comp_operand("p0x 1234", RC_OPERAND_PRIOR, RC_OPERAND_16_BITS, 0x1234U);
    parse_comp_operand("p0x1234", RC_OPERAND_PRIOR, RC_OPERAND_16_BITS, 0x1234U);
    parse_comp_operand("p0xW1234", RC_OPERAND_PRIOR, RC_OPERAND_24_BITS, 0x1234U);
    parse_comp_operand("p0xX1234", RC_OPERAND_PRIOR, RC_OPERAND_32_BITS, 0x1234U);
    parse_comp_operand("p0xL1234", RC_OPERAND_PRIOR, RC_OPERAND_LOW, 0x1234U);
    parse_comp_operand("p0xU1234", RC_OPERAND_PRIOR, RC_OPERAND_HIGH, 0x1234U);
    parse_comp_operand("p0xM1234", RC_OPERAND_PRIOR, RC_OPERAND_BIT_0, 0x1234U);
    parse_comp_operand("p0xN1234", RC_OPERAND_PRIOR, RC_OPERAND_BIT_1, 0x1234U);
    parse_comp_operand("p0xO1234", RC_OPERAND_PRIOR, RC_OPERAND_BIT_2, 0x1234U);
    parse_comp_operand("p0xP1234", RC_OPERAND_PRIOR, RC_OPERAND_BIT_3, 0x1234U);
    parse_comp_operand("p0xQ1234", RC_OPERAND_PRIOR, RC_OPERAND_BIT_4, 0x1234U);
    parse_comp_operand("p0xR1234", RC_OPERAND_PRIOR, RC_OPERAND_BIT_5, 0x1234U);
    parse_comp_operand("p0xS1234", RC_OPERAND_PRIOR, RC_OPERAND_BIT_6, 0x1234U);
    parse_comp_operand("p0xT1234", RC_OPERAND_PRIOR, RC_OPERAND_BIT_7, 0x1234U);

    /* ignores case */
    parse_comp_operand("P0Xh1234", RC_OPERAND_PRIOR, RC_OPERAND_8_BITS, 0x1234U);

    /* addresses */
    parse_comp_operand("p0xH0000", RC_OPERAND_PRIOR, RC_OPERAND_8_BITS, 0x0000U);
    parse_comp_operand("p0xH12345678", RC_OPERAND_PRIOR, RC_OPERAND_8_BITS, 0x12345678U);
    parse_comp_operand("p0xHABCD", RC_OPERAND_PRIOR, RC_OPERAND_8_BITS, 0xABCDU);
    parse_comp_operand("p0xhabcd", RC_OPERAND_PRIOR, RC_OPERAND_8_BITS, 0xABCDU);
  }

  {
    /*------------------------------------------------------------------------
    TestParseVariableValue
    ------------------------------------------------------------------------*/

    /* decimal - values don't actually have size, default is RC_OPERAND_8_BITS */
    parse_comp_operand("123", RC_OPERAND_CONST, RC_OPERAND_8_BITS, 123U);
    parse_comp_operand("123456", RC_OPERAND_CONST, RC_OPERAND_8_BITS, 123456U);
    parse_comp_operand("0", RC_OPERAND_CONST, RC_OPERAND_8_BITS, 0U);
    parse_comp_operand("0000000000", RC_OPERAND_CONST, RC_OPERAND_8_BITS, 0U);
    parse_comp_operand("4294967295", RC_OPERAND_CONST, RC_OPERAND_8_BITS, 4294967295U);

    /* hex - 'H' prefix, not '0x'! */
    parse_comp_operand("H123", RC_OPERAND_CONST, RC_OPERAND_8_BITS, 0x123U);
    parse_comp_operand("HABCD", RC_OPERAND_CONST, RC_OPERAND_8_BITS, 0xABCDU);
    parse_comp_operand("h123", RC_OPERAND_CONST, RC_OPERAND_8_BITS, 0x123U);
    parse_comp_operand("habcd", RC_OPERAND_CONST, RC_OPERAND_8_BITS, 0xABCDU);
    parse_comp_operand("HFFFFFFFF", RC_OPERAND_CONST, RC_OPERAND_8_BITS, 4294967295U);

    /* '0x' is an address */
    parse_comp_operand("0x123", RC_OPERAND_ADDRESS, RC_OPERAND_16_BITS, 0x123U);

    /* hex without prefix */
    parse_error_operand("ABCD", 0);

    /* more than 32-bits (error), will be constrained to 32-bits */
    parse_comp_operand("4294967296", RC_OPERAND_CONST, RC_OPERAND_8_BITS, 4294967295U);

    /* negative value (error), will be "wrapped around": -1 = 0x100000000 - 1 = 0xFFFFFFFF = 4294967295 */
    parse_comp_operand("-1", RC_OPERAND_CONST, RC_OPERAND_8_BITS, 4294967295U);
  }

  {
    /*------------------------------------------------------------------------
    TestVariableGetValue
    ------------------------------------------------------------------------*/

    unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
    memory_t memory;
    memory.ram = ram;
    memory.size = sizeof(ram);

    /* value */
    parse_comp_operand_value("0", &memory, 0x00U);

    /* eight-bit */
    parse_comp_operand_value("0xh0", &memory, 0x00U);
    parse_comp_operand_value("0xh1", &memory, 0x12U);
    parse_comp_operand_value("0xh4", &memory, 0x56U);
    parse_comp_operand_value("0xh5", &memory, 0x00U); /* out of range */

    /* sixteen-bit */
    parse_comp_operand_value("0x 0", &memory, 0x1200U);
    parse_comp_operand_value("0x 3", &memory, 0x56ABU);
    parse_comp_operand_value("0x 4", &memory, 0x0056U); /* out of range */

    /* thirty-two-bit */
    parse_comp_operand_value("0xx0", &memory, 0xAB341200U);
    parse_comp_operand_value("0xx1", &memory, 0x56AB3412U);
    parse_comp_operand_value("0xx3", &memory, 0x000056ABU); /* out of range */

    /* nibbles */
    parse_comp_operand_value("0xu0", &memory, 0x0U);
    parse_comp_operand_value("0xu1", &memory, 0x1U);
    parse_comp_operand_value("0xu4", &memory, 0x5U);
    parse_comp_operand_value("0xu5", &memory, 0x0U); /* out of range */

    parse_comp_operand_value("0xl0", &memory, 0x0U);
    parse_comp_operand_value("0xl1", &memory, 0x2U);
    parse_comp_operand_value("0xl4", &memory, 0x6U);
    parse_comp_operand_value("0xl5", &memory, 0x0U); /* out of range */

    /* bits */
    parse_comp_operand_value("0xm0", &memory, 0x0U);
    parse_comp_operand_value("0xm3", &memory, 0x1U);
    parse_comp_operand_value("0xn3", &memory, 0x1U);
    parse_comp_operand_value("0xo3", &memory, 0x0U);
    parse_comp_operand_value("0xp3", &memory, 0x1U);
    parse_comp_operand_value("0xq3", &memory, 0x0U);
    parse_comp_operand_value("0xr3", &memory, 0x1U);
    parse_comp_operand_value("0xs3", &memory, 0x0U);
    parse_comp_operand_value("0xt3", &memory, 0x1U);
    parse_comp_operand_value("0xm5", &memory, 0x0U); /* out of range */
  }

  {
    /*------------------------------------------------------------------------
    TestVariableGetValueDelta
    ------------------------------------------------------------------------*/

    unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
    memory_t memory;
    rc_operand_t op;
    const char* memaddr;

    memory.ram = ram;
    memory.size = sizeof(ram);

    memaddr = "d0xh1";
    parse_operand(&op, &memaddr);

    assert(rc_evaluate_operand(&op, peek, &memory, NULL) == 0x00); /* first call gets uninitialized value */
    assert(rc_evaluate_operand(&op, peek, &memory, NULL) == 0x12); /* second gets current value */

    /* RC_OPERAND_DELTA is always one frame behind */
    ram[1] = 0x13;
    assert(rc_evaluate_operand(&op, peek, &memory, NULL) == 0x12U);

    ram[1] = 0x14;
    assert(rc_evaluate_operand(&op, peek, &memory, NULL) == 0x13U);

    ram[1] = 0x15;
    assert(rc_evaluate_operand(&op, peek, &memory, NULL) == 0x14U);

    ram[1] = 0x16;
    assert(rc_evaluate_operand(&op, peek, &memory, NULL) == 0x15U);

    assert(rc_evaluate_operand(&op, peek, &memory, NULL) == 0x16U);
    assert(rc_evaluate_operand(&op, peek, &memory, NULL) == 0x16U);
  }

  {
    /*------------------------------------------------------------------------
    TestVariableGetValueDelta
    ------------------------------------------------------------------------*/

    unsigned char ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
    memory_t memory;
    rc_operand_t op;
    const char* memaddr;

    memory.ram = ram;
    memory.size = sizeof(ram);

    memaddr = "p0xh1";
    parse_operand(&op, &memaddr);

    /* RC_OPERAND_PRIOR only updates when the memory value changes */
    assert(rc_evaluate_operand(&op, peek, &memory, NULL) == 0x00); /* first call gets uninitialized value */
    assert(rc_evaluate_operand(&op, peek, &memory, NULL) == 0x00); /* value only changes when memory changes */

    ram[1] = 0x13;
    assert(rc_evaluate_operand(&op, peek, &memory, NULL) == 0x12U);
    assert(rc_evaluate_operand(&op, peek, &memory, NULL) == 0x12U);
    assert(rc_evaluate_operand(&op, peek, &memory, NULL) == 0x12U);
    assert(rc_evaluate_operand(&op, peek, &memory, NULL) == 0x12U);

    ram[1] = 0x14;
    assert(rc_evaluate_operand(&op, peek, &memory, NULL) == 0x13U);

    ram[1] = 0x15;
    assert(rc_evaluate_operand(&op, peek, &memory, NULL) == 0x14U);

    ram[1] = 0x16;
    assert(rc_evaluate_operand(&op, peek, &memory, NULL) == 0x15U);
    assert(rc_evaluate_operand(&op, peek, &memory, NULL) == 0x15U);
    assert(rc_evaluate_operand(&op, peek, &memory, NULL) == 0x15U);
  }
}

static void parse_condition(rc_condition_t* self, const char* memaddr) {
  int ret;
  rc_scratch_t scratch;

  ret = 0;
  rc_parse_condition(&ret, self, &scratch, &memaddr, NULL, 0);
  assert(ret >= 0);
  assert(*memaddr == 0);
}

static void parse_comp_condition(
  const char* memaddr, char expected_type,
  char expected_left_type, char expected_left_size, unsigned expected_left_value,
  char expected_operator,
  char expected_right_type, char expected_right_size, unsigned expected_right_value,
  int expected_required_hits
) {
  rc_condition_t self;
  parse_condition(&self, memaddr);

  assert(self.type == expected_type);
  comp_operand(&self.operand1, expected_left_type, expected_left_size, expected_left_value);
  assert(self.oper == expected_operator);
  comp_operand(&self.operand2, expected_right_type, expected_right_size, expected_right_value);
  assert(self.required_hits == expected_required_hits);
}

static void parse_test_condition(const char* memaddr, memory_t* memory, int value) {
  rc_condition_t self;
  int ret;
  rc_scratch_t scratch;

  ret = 0;
  rc_parse_condition(&ret, &self, &scratch, &memaddr, NULL, 0);
  assert(ret >= 0);
  assert(*memaddr == 0);

  ret = rc_test_condition(&self, 0, peek, memory, NULL);

  assert((ret && value) || (!ret && !value));
}

static void test_condition(void) {
  {
    /*------------------------------------------------------------------------
    TestParseConditionMemoryComparisonValue
    ------------------------------------------------------------------------*/

    /* different comparisons */
    parse_comp_condition(
      "0xH1234=8",
      RC_CONDITION_STANDARD,
      RC_OPERAND_ADDRESS, RC_OPERAND_8_BITS, 0x1234U,
      RC_CONDITION_EQ,
      RC_OPERAND_CONST, RC_OPERAND_8_BITS, 8U,
      0
    );

    parse_comp_condition(
      "0xH1234==8",
      RC_CONDITION_STANDARD,
      RC_OPERAND_ADDRESS, RC_OPERAND_8_BITS, 0x1234U,
      RC_CONDITION_EQ,
      RC_OPERAND_CONST, RC_OPERAND_8_BITS, 8U,
      0
    );

    parse_comp_condition(
      "0xH1234!=8",
      RC_CONDITION_STANDARD,
      RC_OPERAND_ADDRESS, RC_OPERAND_8_BITS, 0x1234U,
      RC_CONDITION_NE,
      RC_OPERAND_CONST, RC_OPERAND_8_BITS, 8U,
      0
    );

    parse_comp_condition(
      "0xH1234<8",
      RC_CONDITION_STANDARD,
      RC_OPERAND_ADDRESS, RC_OPERAND_8_BITS, 0x1234U,
      RC_CONDITION_LT,
      RC_OPERAND_CONST, RC_OPERAND_8_BITS, 8U,
      0
    );

    parse_comp_condition(
      "0xH1234<=8",
      RC_CONDITION_STANDARD,
      RC_OPERAND_ADDRESS, RC_OPERAND_8_BITS, 0x1234U,
      RC_CONDITION_LE,
      RC_OPERAND_CONST, RC_OPERAND_8_BITS, 8U,
      0
    );

    parse_comp_condition(
      "0xH1234>8",
      RC_CONDITION_STANDARD,
      RC_OPERAND_ADDRESS, RC_OPERAND_8_BITS, 0x1234U,
      RC_CONDITION_GT,
      RC_OPERAND_CONST, RC_OPERAND_8_BITS, 8U,
      0
    );

    parse_comp_condition(
      "0xH1234>=8",
      RC_CONDITION_STANDARD,
      RC_OPERAND_ADDRESS, RC_OPERAND_8_BITS, 0x1234U,
      RC_CONDITION_GE,
      RC_OPERAND_CONST, RC_OPERAND_8_BITS, 8U,
      0
    );

    /* delta */
    parse_comp_condition(
      "d0xH1234=8",
      RC_CONDITION_STANDARD,
      RC_OPERAND_DELTA, RC_OPERAND_8_BITS, 0x1234U,
      RC_CONDITION_EQ,
      RC_OPERAND_CONST, RC_OPERAND_8_BITS, 8U,
      0
    );

    /* flags */
    parse_comp_condition(
      "R:0xH1234=8",
      RC_CONDITION_RESET_IF,
      RC_OPERAND_ADDRESS, RC_OPERAND_8_BITS, 0x1234U,
      RC_CONDITION_EQ,
      RC_OPERAND_CONST, RC_OPERAND_8_BITS, 8U,
      0
    );

    parse_comp_condition(
      "P:0xH1234=8",
      RC_CONDITION_PAUSE_IF,
      RC_OPERAND_ADDRESS, RC_OPERAND_8_BITS, 0x1234U,
      RC_CONDITION_EQ,
      RC_OPERAND_CONST, RC_OPERAND_8_BITS, 8U,
      0
    );

    parse_comp_condition(
      "A:0xH1234=8",
      RC_CONDITION_ADD_SOURCE,
      RC_OPERAND_ADDRESS, RC_OPERAND_8_BITS, 0x1234U,
      RC_CONDITION_EQ,
      RC_OPERAND_CONST, RC_OPERAND_8_BITS, 8U,
      0
    );

    parse_comp_condition(
      "B:0xH1234=8",
      RC_CONDITION_SUB_SOURCE,
      RC_OPERAND_ADDRESS, RC_OPERAND_8_BITS, 0x1234U,
      RC_CONDITION_EQ,
      RC_OPERAND_CONST, RC_OPERAND_8_BITS, 8U,
      0
    );

    parse_comp_condition(
      "C:0xH1234=8",
      RC_CONDITION_ADD_HITS,
      RC_OPERAND_ADDRESS, RC_OPERAND_8_BITS, 0x1234U,
      RC_CONDITION_EQ,
      RC_OPERAND_CONST, RC_OPERAND_8_BITS, 8U,
      0
    );

    /* hit count */
    parse_comp_condition(
      "0xH1234=8(1)",
      RC_CONDITION_STANDARD,
      RC_OPERAND_ADDRESS, RC_OPERAND_8_BITS, 0x1234U,
      RC_CONDITION_EQ,
      RC_OPERAND_CONST, RC_OPERAND_8_BITS, 8U,
      1
    );

    parse_comp_condition(
      "0xH1234=8.1.", /* legacy format */
      RC_CONDITION_STANDARD,
      RC_OPERAND_ADDRESS, RC_OPERAND_8_BITS, 0x1234U,
      RC_CONDITION_EQ,
      RC_OPERAND_CONST, RC_OPERAND_8_BITS, 8U,
      1
    );

    parse_comp_condition(
      "0xH1234=8(100)",
      RC_CONDITION_STANDARD,
      RC_OPERAND_ADDRESS, RC_OPERAND_8_BITS, 0x1234U,
      RC_CONDITION_EQ,
      RC_OPERAND_CONST, RC_OPERAND_8_BITS, 8U,
      100
    );
  }

  {
    /*------------------------------------------------------------------------
    TestParseConditionMemoryComparisonHexValue
    ------------------------------------------------------------------------*/

    /* hex value is interpreted as a 16-bit memory reference */
    parse_comp_condition(
      "0xH1234=0x80",
      RC_CONDITION_STANDARD,
      RC_OPERAND_ADDRESS, RC_OPERAND_8_BITS, 0x1234U,
      RC_CONDITION_EQ,
      RC_OPERAND_ADDRESS, RC_OPERAND_16_BITS, 0x80U,
      0
    );
  }

  {
    /*------------------------------------------------------------------------
    TestParseConditionMemoryComparisonMemory
    ------------------------------------------------------------------------*/

    parse_comp_condition(
      "0xL1234!=0xU3456",
      RC_CONDITION_STANDARD,
      RC_OPERAND_ADDRESS, RC_OPERAND_LOW, 0x1234U,
      RC_CONDITION_NE,
      RC_OPERAND_ADDRESS, RC_OPERAND_HIGH, 0x3456U,
      0
    );
  }

  {
    /*------------------------------------------------------------------------
    TestConditionCompare
    ------------------------------------------------------------------------*/

    unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
    memory_t memory;

    memory.ram = ram;
    memory.size = sizeof(ram);

    /* values */
    parse_test_condition("0xH0001=18", &memory, 1);
    parse_test_condition("0xH0001!=18", &memory, 0);
    parse_test_condition("0xH0001<=18", &memory, 1);
    parse_test_condition("0xH0001>=18", &memory, 1);
    parse_test_condition("0xH0001<18", &memory, 0);
    parse_test_condition("0xH0001>18", &memory, 0);
    parse_test_condition("0xH0001>0", &memory, 1);
    parse_test_condition("0xH0001!=0", &memory, 1);

    /* memory */
    parse_test_condition("0xH0001<0xH0002", &memory, 1);
    parse_test_condition("0xH0001>0xH0002", &memory, 0);
    parse_test_condition("0xH0001=0xH0001", &memory, 1);
    parse_test_condition("0xH0001!=0xH0002", &memory, 1);
  }

  {
    /*------------------------------------------------------------------------
    TestConditionCompareDelta
    ------------------------------------------------------------------------*/

    unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
    memory_t memory;
    rc_condition_t cond;

    memory.ram = ram;
    memory.size = sizeof(ram);

    parse_condition(&cond, "0xH0001>d0xH0001");

    /* initial delta value is 0, 0x12 > 0 */
    assert(rc_test_condition(&cond, 0, peek, &memory, NULL) == 1);

    /* delta value is now 0x12, 0x12 = 0x12 */
    assert(rc_test_condition(&cond, 0, peek, &memory, NULL) == 0);

    /* delta value is now 0x12, 0x11 < 0x12 */
    ram[1] = 0x11;
    assert(rc_test_condition(&cond, 0, peek, &memory, NULL) == 0);

    /* delta value is now 0x13, 0x12 > 0x11 */
    ram[1] = 0x12;
    assert(rc_test_condition(&cond, 0, peek, &memory, NULL) == 1);
  }
}

static void parse_trigger(rc_trigger_t** self, void* buffer, const char* memaddr) {
  int ret = rc_trigger_size(memaddr);
  assert(ret >= 0);
  memset(buffer, 0xEE, ret + 128);

  *self = rc_parse_trigger(buffer, memaddr, NULL, 0);
  assert(*self != NULL);
  assert(*((int*)((char*)buffer + ret)) == 0xEEEEEEEE);
}

static void comp_trigger(rc_trigger_t* self, memory_t* memory, int expected_result) {
  int ret = rc_test_trigger(self, peek, memory, NULL);
  assert(expected_result == ret);
}

static rc_condition_t* condset_get_cond(rc_condset_t* condset, int ndx) {
  rc_condition_t* cond = condset->conditions;

  while (ndx-- != 0) {
    assert(cond != NULL);
    cond = cond->next;
  }

  assert(cond != NULL);
  return cond;
}

static rc_condset_t* trigger_get_set(rc_trigger_t* trigger, int ndx) {
  rc_condset_t* condset = trigger->alternative;

  if (ndx-- == 0) {
    assert(trigger->requirement != NULL);
    return trigger->requirement;
  }

  while (ndx-- != 0) {
    condset = condset->next;
    assert(condset != NULL);
  }

  assert(condset != NULL);
  return condset;
}

static void test_trigger(void) {
  char buffer[2048];
    
  {
    /*------------------------------------------------------------------------
    TestSimpleSets
    Only standard conditions, no alt groups
    ------------------------------------------------------------------------*/

    unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
    memory_t memory;
    rc_trigger_t* trigger;

    memory.ram = ram;
    memory.size = sizeof(ram);

    parse_trigger(&trigger, buffer, "0xH0001=18"); /* one condition, true */
    comp_trigger(trigger, &memory, 1);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 1U);
    comp_trigger(trigger, &memory, 1);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 2U);

    parse_trigger(&trigger, buffer, "0xH0001!=18"); /* one condition, false */
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 0U);

    parse_trigger(&trigger, buffer, "0xH0001=18_0xH0002=52"); /* two conditions, true */
    comp_trigger(trigger, &memory, 1);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 1U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 1U);

    parse_trigger(&trigger, buffer, "0xH0001=18_0xH0002>52"); /* two conditions, false */
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 1U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 0U);

    parse_trigger(&trigger, buffer, "0xH0001=18_0xH0002=52_0xL0004=6"); /* three conditions, true */
    comp_trigger(trigger, &memory, 1);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 1U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 1U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 2)->current_hits == 1U);

    parse_trigger(&trigger, buffer, "0xH0001=16_0xH0002=52_0xL0004=6"); /* three conditions, first false */
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 1U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 2)->current_hits == 1U);

    parse_trigger(&trigger, buffer, "0xH0001=18_0xH0002=50_0xL0004=6"); /* three conditions, first false */
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 1U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 2)->current_hits == 1U);

    parse_trigger(&trigger, buffer, "0xH0001=18_0xH0002=52_0xL0004=4"); /* three conditions, first false */
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 1U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 1U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 2)->current_hits == 0U);

    parse_trigger(&trigger, buffer, "0xH0001=16_0xH0002=50_0xL0004=4"); /* three conditions, all false */
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 2)->current_hits == 0U);
  }

  {
    /*------------------------------------------------------------------------
    TestPauseIf
    ------------------------------------------------------------------------*/

    unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
    memory_t memory;
    rc_trigger_t* trigger;

    memory.ram = ram;
    memory.size = sizeof(ram);

    parse_trigger(&trigger, buffer, "0xH0001=18_P:0xH0002=52_P:0xL0x0004=6");
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 1U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 2)->current_hits == 0U); /* Also true, but processing stops on first PauseIf */

    ram[2] = 0;
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 0U); /* PauseIf goes to 0 when false */
    assert(condset_get_cond(trigger_get_set(trigger, 0), 2)->current_hits == 1U); /* PauseIf stays at 1 when false */

    ram[4] = 0;
    comp_trigger(trigger, &memory, 1);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 1U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 2)->current_hits == 0U); /* PauseIf goes to 0 when false */
  }

  {
    /*------------------------------------------------------------------------
    TestPauseIfHitCountOne
    ------------------------------------------------------------------------*/

    unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
    memory_t memory;
    rc_trigger_t* trigger;

    memory.ram = ram;
    memory.size = sizeof(ram);

    parse_trigger(&trigger, buffer, "0xH0001=18_P:0xH0002=52.1.");
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 1U);

    ram[2] = 0;
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 1U); /* PauseIf with HitCount doesn't automatically go back to 0 */
  }

  {
    /*------------------------------------------------------------------------
    TestPauseIfHitCountTwo
    ------------------------------------------------------------------------*/

    unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
    memory_t memory;
    rc_trigger_t* trigger;

    memory.ram = ram;
    memory.size = sizeof(ram);

    parse_trigger(&trigger, buffer, "0xH0001=18_P:0xH0002=52.2.");
    comp_trigger(trigger, &memory, 1); /* PauseIf counter hasn't reached HitCount target, non-PauseIf condition still true */
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 1U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 1U);

    comp_trigger(trigger, &memory, 0); /* PauseIf counter has reached HitCount target, non-PauseIf conditions ignored */
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 1U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 2U);

    ram[2] = 0;
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 1U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 2U); /* PauseIf with HitCount doesn't automatically go back to 0 */
  }

  {
    /*------------------------------------------------------------------------
    TestPauseIfHitReset
    ------------------------------------------------------------------------*/

    unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
    memory_t memory;
    rc_trigger_t* trigger;

    memory.ram = ram;
    memory.size = sizeof(ram);

    parse_trigger(&trigger, buffer, "0xH0001=18_P:0xH0002=52.1._R:0xH0003=1SR:0xH0003=2");
    comp_trigger(trigger, &memory, 0); /* Trigger PauseIf, non-PauseIf conditions ignored */
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 1U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 2)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 1), 0)->current_hits == 0U);

    ram[2] = 0;
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 1U); /* PauseIf with HitCount doesn't automatically go back to 0 */
    assert(condset_get_cond(trigger_get_set(trigger, 0), 2)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 1), 0)->current_hits == 0U);

    ram[3] = 1;
    comp_trigger(trigger, &memory, 0); /* ResetIf in Paused group is ignored */
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 1U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 2)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 1), 0)->current_hits == 0U);

    ram[3] = 2;
    comp_trigger(trigger, &memory, 0); /* ResetIf in alternate group is honored, PauseIf does not retrigger and non-PauseIf condition is true */
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 0U); /* ResetIf causes entire achievement to fail */
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 2)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 1), 0)->current_hits == 0U);

    ram[3] = 3;
    comp_trigger(trigger, &memory, 1); /* ResetIf no longer true, achievement allowed to trigger */
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 1U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 2)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 1), 0)->current_hits == 0U);
  }

  {
    /*------------------------------------------------------------------------
    TestResetIf
    ------------------------------------------------------------------------*/

    unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
    memory_t memory;
    rc_trigger_t* trigger;

    memory.ram = ram;
    memory.size = sizeof(ram);

    parse_trigger(&trigger, buffer, "0xH0001=18_R:0xH0002=50_R:0xL0x0004=4");
    comp_trigger(trigger, &memory, 1);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 1U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 2)->current_hits == 0U);

    comp_trigger(trigger, &memory, 1);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 2U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 2)->current_hits == 0U);

    ram[2] = 50;
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 0U); /* True, but ResetIf also resets true marker */
    assert(condset_get_cond(trigger_get_set(trigger, 0), 2)->current_hits == 0U);

    ram[4] = 0x54;
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 0U); /* True, but ResetIf also resets true marker */
    assert(condset_get_cond(trigger_get_set(trigger, 0), 2)->current_hits == 0U); /* Also true, but processing stop on first ResetIf */

    ram[2] = 52;
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 2)->current_hits == 0U); /* True, but ResetIf also resets true marker */

    ram[4] = 0x56;
    comp_trigger(trigger, &memory, 1);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 1U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 2)->current_hits == 0U);
  }

  {
    /*------------------------------------------------------------------------
    TestHitCount
    ------------------------------------------------------------------------*/

    unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
    memory_t memory;
    rc_trigger_t* trigger;

    memory.ram = ram;
    memory.size = sizeof(ram);

    parse_trigger(&trigger, buffer, "0xH0001=20(2)_0xH0002=52");
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 1U);

    ram[1] = 20;
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 1U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 2U);

    comp_trigger(trigger, &memory, 1);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 2U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 3U);

    comp_trigger(trigger, &memory, 1);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 2U); /* hits stop increment once count it reached */
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 4U);
  }

  {
    /*------------------------------------------------------------------------
    TestHitCountResetIf
    Verifies that ResetIf resets HitCounts
    ------------------------------------------------------------------------*/

    unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
    memory_t memory;
    rc_trigger_t* trigger;

    memory.ram = ram;
    memory.size = sizeof(ram);

    parse_trigger(&trigger, buffer, "0xH0001=18(2)_0xH0002=52_R:0xL0004=4");
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 1U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 1U);

    comp_trigger(trigger, &memory, 1);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 2U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 2U);

    comp_trigger(trigger, &memory, 1);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 2U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 3U);

    ram[4] = 0x54;
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 0U);

    ram[4] = 0x56;
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 1U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 1U);

    comp_trigger(trigger, &memory, 1);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 2U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 2U);
  }

  {
    /*------------------------------------------------------------------------
    TestHitCountResetIfHitCount
    Verifies that ResetIf with HitCount target only resets HitCounts when target is met
    ------------------------------------------------------------------------*/

    unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
    memory_t memory;
    rc_trigger_t* trigger;

    memory.ram = ram;
    memory.size = sizeof(ram);

    parse_trigger(&trigger, buffer, "0xH0001=18(2)_0xH0002=52_R:0xL0004=4.2.");
    comp_trigger(trigger, &memory, 0); /* HitCounts on conditions 1 and 2 are incremented */
    comp_trigger(trigger, &memory, 1); /* HitCounts on conditions 1 and 2 are incremented, cond 1 is now true so entire achievement is true */
    comp_trigger(trigger, &memory, 1); /* HitCount on condition 2 is incremented, cond 1 already met its target HitCount */
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 2U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 3U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 2)->current_hits == 0U); /* ResetIf HitCount should still be 0 */

    ram[4] = 0x54;

    /* first hit on ResetIf should not reset anything */
    comp_trigger(trigger, &memory, 1);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 2U); /* condition 1 stopped at it's HitCount target */
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 4U); /* condition 2 continues to increment */
    assert(condset_get_cond(trigger_get_set(trigger, 0), 2)->current_hits == 1U); /* ResetIf HitCount should be 1 */

    /* second hit on ResetIf should reset everything */
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 2)->current_hits == 0U); /* ResetIf HitCount should also be reset */
  }

  {
    /*------------------------------------------------------------------------
    TestAddHitsResetIf
    Verifies that ResetIf works with AddHits
    ------------------------------------------------------------------------*/

    unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
    memory_t memory;
    rc_trigger_t* trigger;

    memory.ram = ram;
    memory.size = sizeof(ram);

    parse_trigger(&trigger, buffer, "C:0xH0001=18_R:0xL0004=6(3)"); /* never(repeated(3, byte(1) == 18 || low(4) == 6)) */
    comp_trigger(trigger, &memory, 1); /* result is true, no non-reset conditions */
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 1U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 1U);

    comp_trigger(trigger, &memory, 0); /* total hits met (2 for each condition, only needed 3 total) (2 hits on condition 2 is not enough), result is always false if reset */
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 0U);
  }

  {
    /*------------------------------------------------------------------------
    TestHitCountResetIfHitCountOne
    Verifies that ResetIf HitCount(1) behaves like ResetIf without a HitCount
    ------------------------------------------------------------------------*/

    unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
    memory_t memory;
    rc_trigger_t* trigger;

    memory.ram = ram;
    memory.size = sizeof(ram);

    parse_trigger(&trigger, buffer, "0xH0001=18(2)_0xH0002=52_R:0xL0004=4.1.");
    comp_trigger(trigger, &memory, 0); /* HitCounts on conditions 1 and 2 are incremented */
    comp_trigger(trigger, &memory, 1); /* HitCounts on conditions 1 and 2 are incremented, cond 1 is now true so entire achievement is true */
    comp_trigger(trigger, &memory, 1); /* HitCount on condition 2 is incremented, cond 1 already met its target HitCount */
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 2U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 3U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 2)->current_hits == 0U); /* ResetIf HitCount should still be 0 */

    ram[4] = 0x54;

    /* ResetIf HitCount(1) should behave just like ResetIf without a HitCount - all items, including ResetIf should be reset. */
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 2)->current_hits == 0U); /* ResetIf HitCount should also be reset */
  }

  {
    /*------------------------------------------------------------------------
    TestHitCountPauseIf
    Verifies that PauseIf stops HitCount processing
    ------------------------------------------------------------------------*/

    unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
    memory_t memory;
    rc_trigger_t* trigger;

    memory.ram = ram;
    memory.size = sizeof(ram);

    parse_trigger(&trigger, buffer, "0xH0001=18(2)_0xH0002=52_P:0xL0004=4");
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 1U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 1U);

    ram[4] = 0x54;
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 1U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 1U);

    ram[4] = 0x56;
    comp_trigger(trigger, &memory, 1);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 2U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 2U);

    ram[4] = 0x54;
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 2U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 2U);

    ram[4] = 0x56;
    comp_trigger(trigger, &memory, 1);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 2U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 3U);
  }

  {
    /*------------------------------------------------------------------------
    TestHitCountPauseIfResetIf
    Verifies that PauseIf prevents ResetIf processing
    ------------------------------------------------------------------------*/

    unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
    memory_t memory;
    rc_trigger_t* trigger;

    memory.ram = ram;
    memory.size = sizeof(ram);

    parse_trigger(&trigger, buffer, "0xH0001=18(2)_R:0xH0002=50_P:0xL0004=4");
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 1U);

    ram[4] = 0x54; /* pause */
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 1U);

    ram[2] = 50; /* reset (but still paused) */
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 1U);

    ram[4] = 0x56; /* unpause (still reset) */
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 0U);

    ram[2] = 52; /* unreset */
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 1U);

    comp_trigger(trigger, &memory, 1);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 2U);
  }

  {
    /*------------------------------------------------------------------------
    TestAddSource
    ------------------------------------------------------------------------*/

    unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
    memory_t memory;
    rc_trigger_t* trigger;

    memory.ram = ram;
    memory.size = sizeof(ram);

    parse_trigger(&trigger, buffer, "A:0xH0001=0_0xH0002=22");
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 0U);

    ram[2] = 4; /* sum is correct */
    comp_trigger(trigger, &memory, 1);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 0U); /* AddSource condition does not have hit tracking */
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 1U);

    ram[1] = 0; /* first condition is true, but not sum */
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 0U); /* AddSource condition does not have hit tracking */
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 1U);

    ram[2] = 22; /* first condition is true, sum is correct */
    comp_trigger(trigger, &memory, 1);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 0U); /* AddSource condition does not have hit tracking */
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 2U);
  }

  {
    /*------------------------------------------------------------------------
    TestSubSource
    ------------------------------------------------------------------------*/

    unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
    memory_t memory;
    rc_trigger_t* trigger;

    memory.ram = ram;
    memory.size = sizeof(ram);

    parse_trigger(&trigger, buffer, "B:0xH0002=0_0xH0001=14"); /* NOTE: SubSource subtracts the first value from the second! */
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 0U);

    ram[2] = 4; /* difference is correct */
    comp_trigger(trigger, &memory, 1);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 0U); /* SubSource condition does not have hit tracking */
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 1U);

    ram[1] = 0; /* first condition is true, but not difference */
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 0U); /* SubSource condition does not have hit tracking */
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 1U);

    ram[2] = 14; /* first condition is true, value is negative inverse of expected value */
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 0U); /* SubSource condition does not have hit tracking */
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 1U);

    ram[1] = 28; /* difference is correct again */
    comp_trigger(trigger, &memory, 1);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 0U); /* SubSource condition does not have hit tracking */
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 2U);
  }

  {
    /*------------------------------------------------------------------------
    TestAddSubSource
    ------------------------------------------------------------------------*/

    unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
    memory_t memory;
    rc_trigger_t* trigger;

    memory.ram = ram;
    memory.size = sizeof(ram);

    parse_trigger(&trigger, buffer, "A:0xH0001=0_B:0xL0002=0_0xL0004=14"); /* byte(1) - low(2) + low(4) == 14 */
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 2)->current_hits == 0U);

    ram[1] = 12; /* total is correct */
    comp_trigger(trigger, &memory, 1);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 0U); /* AddSource condition does not have hit tracking */
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 0U); /* SubSource condition does not have hit tracking */
    assert(condset_get_cond(trigger_get_set(trigger, 0), 2)->current_hits == 1U);

    ram[1] = 0; /* first condition is true, but not total */
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 0U); /* AddSource condition does not have hit tracking */
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 0U); /* SubSource condition does not have hit tracking */
    assert(condset_get_cond(trigger_get_set(trigger, 0), 2)->current_hits == 1U);

    ram[4] = 18; /* byte(4) would make total true, but not low(4) */
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 0U); /* AddSource condition does not have hit tracking */
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 0U); /* SubSource condition does not have hit tracking */
    assert(condset_get_cond(trigger_get_set(trigger, 0), 2)->current_hits == 1U);

    ram[2] = 1;
    ram[4] = 15; /* difference is correct again */
    comp_trigger(trigger, &memory, 1);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 0U); /* AddSource condition does not have hit tracking */
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 0U); /* SubSource condition does not have hit tracking */
    assert(condset_get_cond(trigger_get_set(trigger, 0), 2)->current_hits == 2U);
  }

  {
    /*------------------------------------------------------------------------
    TestAddHits
    ------------------------------------------------------------------------*/

    unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
    memory_t memory;
    rc_trigger_t* trigger;

    rc_condset_t* condset;

    memory.ram = ram;
    memory.size = sizeof(ram);

    parse_trigger(&trigger, buffer, "C:0xH0001=18(2)_0xL0004=6(4)"); /* repeated(4, byte(1) == 18 || low(4) == 6) */
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 1U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 1U);

    comp_trigger(trigger, &memory, 1); /* total hits met (2 for each condition) */
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 2U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 2U);

    comp_trigger(trigger, &memory, 1);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 2U); /* threshold met, stop incrementing */
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 2U); /* total met prevents incrementing even though individual tally has not reached total */

    rc_reset_condset(trigger->requirement);

    for (condset = trigger->alternative; condset != NULL; condset = condset->next) {
      rc_reset_condset(condset);
    }

    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 1U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 1U);

    ram[1] = 16;
    comp_trigger(trigger, &memory, 0); /* 1 + 2 < 4, not met */
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 1U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 2U);

    comp_trigger(trigger, &memory, 1); /* 1 + 3 = 4, met */
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 1U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 3U);
  }

  {
    /*------------------------------------------------------------------------
    TestAndNext
    ------------------------------------------------------------------------*/

    unsigned char ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
    memory_t memory;
    rc_trigger_t* trigger;

    memory.ram = ram;
    memory.size = sizeof(ram);

    /* once(byte(0x0001) == 20 && byte(0x0002) == 20 && byte(0x0003) == 20) */
    parse_trigger(&trigger, buffer, "N:0xH0001=20_N:0xH0002=20_0xH0003=20.3.");
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 2)->current_hits == 0U);

    ram[3] = 20; /* final condition is not enough to reset */
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 2)->current_hits == 0U);

    ram[2] = 20; /* two conditions is not enough to reset */
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 2)->current_hits == 0U);

    ram[1] = 20; /* all three conditions true, only count hit on final */
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 2)->current_hits == 1U);

    ram[2] = 30; /* middle condition not true */
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 2)->current_hits == 1U);

    ram[2] = 20; /* all three conditions true */
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 2)->current_hits == 2U);

    ram[3] = 30; /* third condition not true */
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 2)->current_hits == 2U);

    ram[3] = 20; /* all three conditions true, HitCount reached */
    comp_trigger(trigger, &memory, 1);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 2)->current_hits == 3U);

    comp_trigger(trigger, &memory, 1); /* HitCount reached */
    assert(condset_get_cond(trigger_get_set(trigger, 0), 2)->current_hits == 3U);
  }

  {
    /*------------------------------------------------------------------------
    TestAndNextBoundaries
    ------------------------------------------------------------------------*/

    unsigned char ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
    memory_t memory;
    rc_trigger_t* trigger;
    char buffer[2048];

    memory.ram = ram;
    memory.size = sizeof(ram);

    /* byte(0x0000) == 0 && once(byte(0x0001) == 20 && byte(0x0002) == 20 && byte(0x0003) == 20) && byte(0x0000) == 0 */
    parse_trigger(&trigger, buffer, "0xH0000=0_N:0xH0001=20_N:0xH0002=20_0xH0003=20.1._0xH0000=0");
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 1U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 2)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 3)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 4)->current_hits == 1U);

    ram[3] = 20; /* final condition is not enough to reset */
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 2U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 2)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 3)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 4)->current_hits == 2U);

    ram[2] = 20; /* two conditions is not enough to reset */
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 3U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 2)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 3)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 4)->current_hits == 3U);

    ram[1] = 20; /* all three conditions true, whole trigger is true */
    comp_trigger(trigger, &memory, 1);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 4U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 2)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 3)->current_hits == 1U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 4)->current_hits == 4U);
  }

  {
    /*------------------------------------------------------------------------
    TestAndNextReset
    ------------------------------------------------------------------------*/

    unsigned char ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
    memory_t memory;
    rc_trigger_t* trigger;

    memory.ram = ram;
    memory.size = sizeof(ram);

    /* byte(0x0000) == 0 && never(byte(0x0001) == 20 && byte(0x0002) == 20 && byte(0x0003) == 20) */
    parse_trigger(&trigger, buffer, "0xH0000=0_N:0xH0001=20_N:0xH0002=20_R:0xH0003=20");
    comp_trigger(trigger, &memory, 1);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 1U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 2)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 3)->current_hits == 0U);

    ram[3] = 20; /* final condition is not enough to reset */
    comp_trigger(trigger, &memory, 1);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 2U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 2)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 3)->current_hits == 0U);

    ram[2] = 20; /* two conditions is not enough to reset */
    comp_trigger(trigger, &memory, 1);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 3U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 2)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 3)->current_hits == 0U);

    ram[1] = 20; /* all three conditions true, reset */
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 2)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 3)->current_hits == 0U);

    ram[2] = 30; /* middle condition not true */
    comp_trigger(trigger, &memory, 1);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 1U);

    ram[2] = 20; /* all three conditions true */
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 0U);

    ram[3] = 30; /* third condition not true */
    comp_trigger(trigger, &memory, 1);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 1U);

    ram[3] = 20; /* all three conditions true */
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 0U);
  }

  {
    /*------------------------------------------------------------------------
    TestAndNextPause
    ------------------------------------------------------------------------*/

    unsigned char ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
    memory_t memory;
    rc_trigger_t* trigger;

    memory.ram = ram;
    memory.size = sizeof(ram);

    /* byte(0x0000) == 0 && never(byte(0x0001) == 20 && byte(0x0002) == 20 && byte(0x0003) == 20) */
    parse_trigger(&trigger, buffer, "0xH0000=0_N:0xH0001=20_N:0xH0002=20_P:0xH0003=20");
    comp_trigger(trigger, &memory, 1);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 1U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 2)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 3)->current_hits == 0U);

    ram[3] = 20; /* final condition is not enough to pause */
    comp_trigger(trigger, &memory, 1);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 2U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 2)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 3)->current_hits == 0U);

    ram[2] = 20; /* two conditions is not enough to pause */
    comp_trigger(trigger, &memory, 1);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 3U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 2)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 3)->current_hits == 0U);

    ram[1] = 20; /* all three conditions true, pause */
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 3U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 2)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 3)->current_hits == 1U);

    ram[2] = 30; /* middle condition not true */
    comp_trigger(trigger, &memory, 1);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 4U);

    ram[2] = 20; /* all three conditions true */
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 4U);

    ram[3] = 30; /* third condition not true */
    comp_trigger(trigger, &memory, 1);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 5U);

    ram[3] = 20; /* all three conditions true */
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 5U);
  }

  {
      /*------------------------------------------------------------------------
      TestAddHits
      ------------------------------------------------------------------------*/

      unsigned char ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
      memory_t memory;
      rc_trigger_t* trigger;
      char buffer[2048];

      rc_condset_t* condset;

      memory.ram = ram;
      memory.size = sizeof(ram);

      parse_trigger(&trigger, buffer, "C:0xH0001=18(2)_0xL0004=6(4)"); /* repeated(4, byte(1) == 18 || low(4) == 6) */
      comp_trigger(trigger, &memory, 0);
      assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 1U);
      assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 1U);

      comp_trigger(trigger, &memory, 1); /* total hits met (2 for each condition) */
      assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 2U);
      assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 2U);

      comp_trigger(trigger, &memory, 1);
      assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 2U); /* threshold met, stop incrementing */
      assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 2U); /* total met prevents incrementing even though individual tally has not reached total */

      rc_reset_condset(trigger->requirement);

      for (condset = trigger->alternative; condset != NULL; condset = condset->next)
      {
          rc_reset_condset(condset);
      }

      comp_trigger(trigger, &memory, 0);
      assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 1U);
      assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 1U);

      ram[1] = 16;
      comp_trigger(trigger, &memory, 0); /* 1 + 2 < 4, not met */
      assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 1U);
      assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 2U);

      comp_trigger(trigger, &memory, 1); /* 1 + 3 = 4, met */
      assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 1U);
      assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 3U);
  }

  {
    /*------------------------------------------------------------------------
    TestAndNextAddSource
    ------------------------------------------------------------------------*/

    unsigned char ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
    memory_t memory;
    rc_trigger_t* trigger;

    memory.ram = ram;
    memory.size = sizeof(ram);

    /* once(byte(0x0001) + byte(0x0002) == 20 && byte(0x0003) == 20) */
    parse_trigger(&trigger, buffer, "A:0xH0001=0_N:0xH0002=20_0xH0003=20.1.");
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 2)->current_hits == 0U);

    ram[3] = 20; /* final condition is true */
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 2)->current_hits == 0U);

    ram[2] = 20; /* AddSource condition would be true if AddSource ignored */
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 2)->current_hits == 0U);

    ram[2] = 10;
    ram[1] = 10; /* AddSource condition true only via addition, whole trigger is true */
    comp_trigger(trigger, &memory, 1);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 2)->current_hits == 1U);
  }

  {
    /*------------------------------------------------------------------------
    TestAndNextChangesTo
    ------------------------------------------------------------------------*/

    unsigned char ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
    memory_t memory;
    rc_trigger_t* trigger;
    char buffer[2048];

    memory.ram = ram;
    memory.size = sizeof(ram);

    /* byte(0x0001) ~> 18 */
    parse_trigger(&trigger, buffer, "N:0xH0001=18_d0xH0001!=18");

    /* value already 18, initial delta value is 0, so considered changed */
    comp_trigger(trigger, &memory, 1);

    /* value already 18 */
    comp_trigger(trigger, &memory, 0);

    /* value no longer 18 */
    ram[1] = 20;
    comp_trigger(trigger, &memory, 0);

    /* value changes to 18 */
    ram[1] = 18;
    comp_trigger(trigger, &memory, 1);

    /* value already 18 */
    comp_trigger(trigger, &memory, 0);
  }

  {
    /*------------------------------------------------------------------------
    TestAltGroups
    ------------------------------------------------------------------------*/

    unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
    memory_t memory;
    rc_trigger_t* trigger;
    char buffer[2048];

    memory.ram = ram;
    memory.size = sizeof(ram);

    parse_trigger(&trigger, buffer, "0xH0001=16S0xH0002=52S0xL0004=6");

    /* core not true, both alts are */
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 1), 0)->current_hits == 1U);
    assert(condset_get_cond(trigger_get_set(trigger, 2), 0)->current_hits == 1U);

    ram[1] = 16; /* core and both alts true */
    comp_trigger(trigger, &memory, 1);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 1U);
    assert(condset_get_cond(trigger_get_set(trigger, 1), 0)->current_hits == 2U);
    assert(condset_get_cond(trigger_get_set(trigger, 2), 0)->current_hits == 2U);

    ram[4] = 0; /* core and first alt true */
    comp_trigger(trigger, &memory, 1);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 2U);
    assert(condset_get_cond(trigger_get_set(trigger, 1), 0)->current_hits == 3U);
    assert(condset_get_cond(trigger_get_set(trigger, 2), 0)->current_hits == 2U);

    ram[2] = 0; /* core true, but neither alt is */
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 3U);
    assert(condset_get_cond(trigger_get_set(trigger, 1), 0)->current_hits == 3U);
    assert(condset_get_cond(trigger_get_set(trigger, 2), 0)->current_hits == 2U);

    ram[4] = 6; /* core and second alt true */
    comp_trigger(trigger, &memory, 1);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 4U);
    assert(condset_get_cond(trigger_get_set(trigger, 1), 0)->current_hits == 3U);
    assert(condset_get_cond(trigger_get_set(trigger, 2), 0)->current_hits == 3U);
  }

  {
    /*------------------------------------------------------------------------
    TestResetIfInAltGroup
    Verifies that a ResetIf resets everything regardless of where it is
    ------------------------------------------------------------------------*/

    unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
    memory_t memory;
    rc_trigger_t* trigger;
    char buffer[2048];

    memory.ram = ram;
    memory.size = sizeof(ram);

    parse_trigger(&trigger, buffer, "0xH0001=18(1)_R:0xH0000=1S0xH0002=52(1)S0xL0004=6(1)_R:0xH0000=2");
    comp_trigger(trigger, &memory, 1);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 1U);
    assert(condset_get_cond(trigger_get_set(trigger, 1), 0)->current_hits == 1U);
    assert(condset_get_cond(trigger_get_set(trigger, 2), 0)->current_hits == 1U);

    ram[0] = 1; /* reset in core group resets everything */
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 1), 0)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 2), 0)->current_hits == 0U);

    ram[0] = 0;
    comp_trigger(trigger, &memory, 1);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 1U);
    assert(condset_get_cond(trigger_get_set(trigger, 1), 0)->current_hits == 1U);
    assert(condset_get_cond(trigger_get_set(trigger, 2), 0)->current_hits == 1U);

    ram[0] = 2; /* reset in alt group resets everything */
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 1), 0)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 2), 0)->current_hits == 0U);
  }

  {
    /*------------------------------------------------------------------------
    TestPauseIfInAltGroup
    Verifies that PauseIf only pauses the group it's in
    ------------------------------------------------------------------------*/

    unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
    memory_t memory;
    rc_trigger_t* trigger;
    char buffer[2048];

    memory.ram = ram;
    memory.size = sizeof(ram);

    parse_trigger(&trigger, buffer, "0xH0001=18_P:0xH0000=1S0xH0002=52S0xL0004=6_P:0xH0000=2");
    comp_trigger(trigger, &memory, 1);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 1U);
    assert(condset_get_cond(trigger_get_set(trigger, 1), 0)->current_hits == 1U);
    assert(condset_get_cond(trigger_get_set(trigger, 2), 0)->current_hits == 1U);

    ram[0] = 1; /* pause in core group only pauses core group */
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 1U);
    assert(condset_get_cond(trigger_get_set(trigger, 1), 0)->current_hits == 2U);
    assert(condset_get_cond(trigger_get_set(trigger, 2), 0)->current_hits == 2U);

    ram[0] = 0;
    comp_trigger(trigger, &memory, 1);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 2U);
    assert(condset_get_cond(trigger_get_set(trigger, 1), 0)->current_hits == 3U);
    assert(condset_get_cond(trigger_get_set(trigger, 2), 0)->current_hits == 3U);

    ram[0] = 2; /* pause in alt group only pauses alt group */
    comp_trigger(trigger, &memory, 1);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 3U);
    assert(condset_get_cond(trigger_get_set(trigger, 1), 0)->current_hits == 4U);
    assert(condset_get_cond(trigger_get_set(trigger, 2), 0)->current_hits == 3U);
  }

  {
    /*------------------------------------------------------------------------
    TestPauseIfResetIfAltGroup
    ------------------------------------------------------------------------*/

    unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
    memory_t memory;
    rc_trigger_t* trigger;
    char buffer[2048];

    memory.ram = ram;
    memory.size = sizeof(ram);

    parse_trigger(&trigger, buffer, "0xH0000=0.1._0xH0000=2SP:0xH0001=18_R:0xH0002=52");
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 1U);

    ram[0] = 1; /* move off HitCount */
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 1U);

    ram[1] = 16; /* unpause alt group, HitCount should be reset */
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 0U);

    ram[0] = 0;
    ram[1] = 18; /* repause alt group, reset hitcount target, hitcount should be set */
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 1U);

    ram[0] = 2; /* trigger win condition. alt group has no normal conditions, it should be considered false */
    comp_trigger(trigger, &memory, 0);
  }
}

static void parse_comp_term(const char* memaddr, char expected_var_size, unsigned expected_address, int is_bcd, int is_const) {
  rc_term_t self;
  rc_scratch_t scratch;
  int ret;

  ret = 0;
  rc_parse_term(&ret, &self, &scratch, &memaddr, NULL, 0);
  assert(ret >= 0);
  assert(*memaddr == 0);

  assert(is_const || self.operand1.size == expected_var_size);
  assert(self.operand1.value == expected_address);
  assert(self.operand1.is_bcd == is_bcd);
  assert(self.invert == 0);
  assert(self.operand2.size == RC_OPERAND_8_BITS);
  assert(self.operand2.value == 0U);
  assert(!is_const || self.operand1.type == RC_OPERAND_CONST);
}

static void parse_comp_term_fp(const char* memaddr, char expected_var_size, unsigned expected_address, double fp) {
  rc_term_t self;
  rc_scratch_t scratch;
  int ret;
  
  ret = 0;
  rc_parse_term(&ret, &self, &scratch, &memaddr, NULL, 0);
  assert(ret >= 0);
  assert(*memaddr == 0);

  assert(self.operand1.size == expected_var_size);
  assert(self.operand1.value == expected_address);
  assert(self.operand2.type == RC_OPERAND_FP);
  assert(self.operand2.fp_value == fp);
}

static void parse_comp_term_mem(const char* memaddr, char expected_size_1, unsigned expected_address_1, char expected_size_2, unsigned expected_address_2) {
  rc_term_t self;
  rc_scratch_t scratch;
  int ret;
  
  ret = 0;
  rc_parse_term(&ret, &self, &scratch, &memaddr, NULL, 0);
  assert(ret >= 0);
  assert(*memaddr == 0);

  assert(self.operand1.size == expected_size_1);
  assert(self.operand1.value == expected_address_1);
  assert(self.operand2.size == expected_size_2);
  assert(self.operand2.value == expected_address_2);
}

static void parse_comp_term_value(const char* memaddr, memory_t* memory, unsigned value) {
  rc_term_t self;
  rc_scratch_t scratch;
  int ret;
  
  ret = 0;
  rc_parse_term(&ret, &self, &scratch, &memaddr, NULL, 0);
  assert(ret >= 0);
  assert(*memaddr == 0);

  assert(rc_evaluate_term(&self, peek, memory, NULL) == value);
}

static void test_term(void) {
  {
    /*------------------------------------------------------------------------
    TestClauseParseFromString
    ------------------------------------------------------------------------*/

    /* sizes */
    parse_comp_term("0xH1234", RC_OPERAND_8_BITS, 0x1234U, 0, 0);
    parse_comp_term("0x 1234", RC_OPERAND_16_BITS, 0x1234U, 0, 0);
    parse_comp_term("0x1234", RC_OPERAND_16_BITS, 0x1234U, 0, 0);
    parse_comp_term("0xW1234", RC_OPERAND_24_BITS, 0x1234U, 0, 0);
    parse_comp_term("0xX1234", RC_OPERAND_32_BITS, 0x1234U, 0, 0);
    parse_comp_term("0xL1234", RC_OPERAND_LOW, 0x1234U, 0, 0);
    parse_comp_term("0xU1234", RC_OPERAND_HIGH, 0x1234U, 0, 0);
    parse_comp_term("0xM1234", RC_OPERAND_BIT_0, 0x1234U, 0, 0);
    parse_comp_term("0xN1234", RC_OPERAND_BIT_1, 0x1234U, 0, 0);
    parse_comp_term("0xO1234", RC_OPERAND_BIT_2, 0x1234U, 0, 0);
    parse_comp_term("0xP1234", RC_OPERAND_BIT_3, 0x1234U, 0, 0);
    parse_comp_term("0xQ1234", RC_OPERAND_BIT_4, 0x1234U, 0, 0);
    parse_comp_term("0xR1234", RC_OPERAND_BIT_5, 0x1234U, 0, 0);
    parse_comp_term("0xS1234", RC_OPERAND_BIT_6, 0x1234U, 0, 0);
    parse_comp_term("0xT1234", RC_OPERAND_BIT_7, 0x1234U, 0, 0);

    /* BCD */
    parse_comp_term("B0xH1234", RC_OPERAND_8_BITS, 0x1234U, 1, 0);
    parse_comp_term("B0xX1234", RC_OPERAND_32_BITS, 0x1234U, 1, 0);
    parse_comp_term("b0xH1234", RC_OPERAND_8_BITS, 0x1234U, 1, 0);

    /* Value */
    parse_comp_term("V1234", 0, 1234, 0, 1);
    parse_comp_term("V+1", 0, 1, 0, 1);
    parse_comp_term("V-1", 0, 0xFFFFFFFFU, 0, 1);
    parse_comp_term("V-2", 0, 0xFFFFFFFEU, 0, 1); /* twos compliment still works for addition */
  }

  {
    /*------------------------------------------------------------------------
    TestClauseParseFromStringMultiply
    ------------------------------------------------------------------------*/

    parse_comp_term_fp("0xH1234", RC_OPERAND_8_BITS, 0x1234U, 1.0);
    parse_comp_term_fp("0xH1234*1", RC_OPERAND_8_BITS, 0x1234U, 1.0);
    parse_comp_term_fp("0xH1234*3", RC_OPERAND_8_BITS, 0x1234U, 3.0);
    parse_comp_term_fp("0xH1234*0.5", RC_OPERAND_8_BITS, 0x1234U, 0.5);
    parse_comp_term_fp("0xH1234*.5", RC_OPERAND_8_BITS, 0x1234U, 0.5);
    parse_comp_term_fp("0xH1234*-1", RC_OPERAND_8_BITS, 0x1234U, -1.0);
  }

  {
    /*------------------------------------------------------------------------
    TestClauseParseFromStringMultiplyAddress
    ------------------------------------------------------------------------*/

    parse_comp_term_mem("0xH1234", RC_OPERAND_8_BITS, 0x1234U, RC_OPERAND_8_BITS, 0U);
    parse_comp_term_mem("0xH1234*0xH3456", RC_OPERAND_8_BITS, 0x1234U, RC_OPERAND_8_BITS, 0x3456U);
    parse_comp_term_mem("0xH1234*0xL2222", RC_OPERAND_8_BITS, 0x1234U, RC_OPERAND_LOW, 0x2222U);
    parse_comp_term_mem("0xH1234*0x1111", RC_OPERAND_8_BITS, 0x1234U, RC_OPERAND_16_BITS, 0x1111U);
  }

  {
    /*------------------------------------------------------------------------
    TestClauseGetValue
    ------------------------------------------------------------------------*/

    unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
    memory_t memory;

    memory.ram = ram;
    memory.size = sizeof(ram);

    /* value */
    parse_comp_term_value("V6", &memory, 6);
    parse_comp_term_value("V6*2", &memory, 12);
    parse_comp_term_value("V6*0.5", &memory, 3);
    parse_comp_term_value("V-6", &memory, (unsigned)(-6));
    parse_comp_term_value("V-6*2", &memory, (unsigned)(-12));

    /* memory */
    parse_comp_term_value("0xH01", &memory, 0x12);
    parse_comp_term_value("0x0001", &memory, 0x3412);

    /* BCD encoding */
    parse_comp_term_value("B0xH01", &memory, 12);
    parse_comp_term_value("B0x0001", &memory, 3412);

    /* multiplication */
    parse_comp_term_value("0xH01*4", &memory, 0x12 * 4); /* multiply by constant */
    parse_comp_term_value("0xH01*0.5", &memory, 0x12 / 2); /* multiply by fraction */
    parse_comp_term_value("0xH01*0xH02", &memory, 0x12 * 0x34); /* multiply by second address */
    parse_comp_term_value("0xH01*0xT02", &memory, 0); /* multiply by bit */
    parse_comp_term_value("0xH01*~0xT02", &memory, 0x12); /* multiply by inverse bit */
    parse_comp_term_value("0xH01*~0xH02", &memory, 0x12 * (0x34 ^ 0xff)); /* multiply by inverse byte */
  }
}

static void parse_comp_value(const char* memaddr, memory_t* memory, unsigned expected_value) {
  rc_value_t* self;
  char buffer[2048];
  int ret;

  ret = rc_value_size(memaddr);
  assert(ret >= 0);
  memset(buffer, 0xEE, ret + 128);

  self = rc_parse_value(buffer, memaddr, NULL, 0);
  assert(self != NULL);
  assert(*((int*)((char*)buffer + ret)) == 0xEEEEEEEE);

  assert(rc_evaluate_value(self, peek, memory, NULL) == expected_value);
}

static void test_value(void) {
  {
    /*------------------------------------------------------------------------
    TestAdditionSimple
    ------------------------------------------------------------------------*/

    unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
    memory_t memory;
    
    memory.ram = ram;
    memory.size = sizeof(ram);

    parse_comp_value("0xH0001_0xH0002", &memory, 0x12U + 0x34U); /* TestAdditionSimple */
    parse_comp_value("0xH0001*100_0xH0002*0.5_0xL0003", &memory, 0x12U * 100 + 0x34U / 2 + 0x0B);/* TestAdditionComplex */
    parse_comp_value("0xH0001$0xH0002", &memory, 0x34U);/* TestMaximumSimple */
    parse_comp_value("0xH0001_0xH0004*3$0xH0002*0xL0003", &memory, 0x34U * 0xBU);/* TestMaximumComplex */
  }

  {
    /*------------------------------------------------------------------------
    TestFormatValue
    ------------------------------------------------------------------------*/

    char buffer[64];

    rc_format_value(buffer, sizeof(buffer), 12345, RC_FORMAT_VALUE);
    assert(!strcmp("12345", buffer));

    rc_format_value(buffer, sizeof(buffer), 12345, RC_FORMAT_OTHER);
    assert(!strcmp("012345", buffer));

    rc_format_value(buffer, sizeof(buffer), 12345, RC_FORMAT_SCORE);
    assert(!strcmp("012345 Points", buffer));

    rc_format_value(buffer, sizeof(buffer), 12345, RC_FORMAT_SECONDS);
    assert(!strcmp("205:45", buffer));

    rc_format_value(buffer, sizeof(buffer), 12345, RC_FORMAT_CENTISECS);
    assert(!strcmp("02:03.45", buffer));

    rc_format_value(buffer, sizeof(buffer), 12345, RC_FORMAT_FRAMES);
    assert(!strcmp("03:25.75", buffer));

    rc_format_value(buffer, sizeof(buffer), 345, RC_FORMAT_SECONDS);
    assert(!strcmp("05:45", buffer));

    rc_format_value(buffer, sizeof(buffer), 345, RC_FORMAT_CENTISECS);
    assert(!strcmp("00:03.45", buffer));

    rc_format_value(buffer, sizeof(buffer), 345, RC_FORMAT_FRAMES);
    assert(!strcmp("00:05.75", buffer));
  }

  {
    /*------------------------------------------------------------------------
    TestParseMemValueFormat
    ------------------------------------------------------------------------*/

    assert(rc_parse_format("VALUE") == RC_FORMAT_VALUE);
    assert(rc_parse_format("SECS") == RC_FORMAT_SECONDS);
    assert(rc_parse_format("TIMESECS") == RC_FORMAT_SECONDS);
    assert(rc_parse_format("TIME") == RC_FORMAT_FRAMES);
    assert(rc_parse_format("FRAMES") == RC_FORMAT_FRAMES);
    assert(rc_parse_format("SCORE") == RC_FORMAT_SCORE);
    assert(rc_parse_format("POINTS") == RC_FORMAT_SCORE);
    assert(rc_parse_format("MILLISECS") == RC_FORMAT_CENTISECS);
    assert(rc_parse_format("OTHER") == RC_FORMAT_OTHER);
    assert(rc_parse_format("INVALID") == RC_FORMAT_VALUE);
  }
}

static rc_lboard_t* parse_lboard(const char* memaddr, void* buffer) {
  int ret;
  rc_lboard_t* self;

  ret = rc_lboard_size(memaddr);
  assert(ret >= 0);
  memset(buffer, 0xEE, ret + 128);

  self = rc_parse_lboard(buffer, memaddr, NULL, 0);
  assert(self != NULL);
  assert(*((int*)((char*)buffer + ret)) == 0xEEEEEEEE);

  return self;
}

static void lboard_check(const char* memaddr, int expected_ret) {
  int ret = rc_lboard_size(memaddr);
  assert(ret == expected_ret);
}

typedef struct {
  int active, submitted;
}
lboard_test_state_t;

static void lboard_reset(rc_lboard_t* lboard, lboard_test_state_t* state) {
  rc_reset_lboard(lboard);
  state->active = state->submitted = 0;
}

static unsigned lboard_evaluate(rc_lboard_t* lboard, lboard_test_state_t* test, memory_t* memory) {
  unsigned value;

  switch (rc_evaluate_lboard(lboard, &value, peek, memory, NULL)) {
    case RC_LBOARD_STARTED:
      test->active = 1;
      break;

    case RC_LBOARD_CANCELED:
      test->active = 0;
      break;

    case RC_LBOARD_TRIGGERED:
      test->active = 0;
      test->submitted = 1;
      break;
  }

  return value;
}

static void test_lboard(void) {
  char buffer[2048];

  {
    /*------------------------------------------------------------------------
    TestSimpleLeaderboard
    ------------------------------------------------------------------------*/

    unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
    memory_t memory;
    rc_lboard_t* lboard;
    lboard_test_state_t state;
    unsigned value;
    
    memory.ram = ram;
    memory.size = sizeof(ram);

    lboard = parse_lboard("STA:0xH00=1::CAN:0xH00=2::SUB:0xH00=3::VAL:0xH02", buffer);
    state.active = state.submitted = 0;

    assert(!state.active);
    assert(!state.submitted);

    value = lboard_evaluate(lboard, &state, &memory);
    assert(!state.active);
    assert(!state.submitted);

    ram[0] = 3; /* submit value, but not active */
    value = lboard_evaluate(lboard, &state, &memory);
    assert(!state.active);
    assert(!state.submitted);

    ram[0] = 2; /* cancel value, but not active */
    value = lboard_evaluate(lboard, &state, &memory);
    assert(!state.active);
    assert(!state.submitted);

    ram[0] = 1; /* start value */
    value = lboard_evaluate(lboard, &state, &memory);
    assert(state.active);
    assert(!state.submitted);

    ram[0] = 2; /* cancel value */
    value = lboard_evaluate(lboard, &state, &memory);
    assert(!state.active);
    assert(!state.submitted);

    ram[0] = 3; /* submit value, but not active */
    value = lboard_evaluate(lboard, &state, &memory);
    assert(!state.active);
    assert(!state.submitted);

    ram[0] = 1; /* start value */
    value = lboard_evaluate(lboard, &state, &memory);
    assert(state.active);
    assert(!state.submitted);

    ram[0] = 3; /* submit value */
    value = lboard_evaluate(lboard, &state, &memory);
    assert(!state.active);
    assert(state.submitted);
    assert(value == 0x34U);
  }

  {
    /*------------------------------------------------------------------------
    TestStartAndCancelSameFrame
    ------------------------------------------------------------------------*/

    unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
    memory_t memory;
    rc_lboard_t* lboard;
    lboard_test_state_t state;
    
    memory.ram = ram;
    memory.size = sizeof(ram);

    lboard = parse_lboard("STA:0xH00=0::CAN:0xH01=18::SUB:0xH00=3::VAL:0xH02", buffer);
    state.active = state.submitted = 0;

    lboard_evaluate(lboard, &state, &memory);
    assert(!state.active);
    assert(!state.submitted);

    ram[1] = 0x13; /* disables cancel */
    lboard_evaluate(lboard, &state, &memory);
    assert(state.active);
    assert(!state.submitted);

    ram[1] = 0x12; /* enables cancel */
    lboard_evaluate(lboard, &state, &memory);
    assert(!state.active);
    assert(!state.submitted);

    ram[1] = 0x13; /* disables cancel, but start condition still true, so it shouldn't restart */
    lboard_evaluate(lboard, &state, &memory);
    assert(!state.active);
    assert(!state.submitted);

    ram[0] = 0x01; /* disables start; no effect this frame, but next frame can restart */
    lboard_evaluate(lboard, &state, &memory);
    assert(!state.active);
    assert(!state.submitted);

    ram[0] = 0x00; /* enables start */
    lboard_evaluate(lboard, &state, &memory);
    assert(state.active);
    assert(!state.submitted);
  }

  {
    /*------------------------------------------------------------------------
    TestStartAndSubmitSameFrame
    ------------------------------------------------------------------------*/

    unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
    memory_t memory;
    rc_lboard_t* lboard;
    lboard_test_state_t state;
    unsigned value;
    
    memory.ram = ram;
    memory.size = sizeof(ram);

    lboard = parse_lboard("STA:0xH00=0::CAN:0xH01=10::SUB:0xH01=18::VAL:0xH02", buffer);
    state.active = state.submitted = 0;

    value = lboard_evaluate(lboard, &state, &memory);
    assert(!state.active);
    assert(state.submitted);
    assert(value == 0x34U);

    ram[1] = 0; /* disable submit, value should not be resubmitted, */
    value = lboard_evaluate(lboard, &state, &memory); /* start is still true, but leaderboard should not reactivate */
    assert(!state.active);

    ram[0] = 1; /* disable start */
    value = lboard_evaluate(lboard, &state, &memory);
    assert(!state.active);

    ram[0] = 0; /* reenable start, leaderboard should reactivate */
    value = lboard_evaluate(lboard, &state, &memory);
    assert(state.active);
  }

  {
    /*------------------------------------------------------------------------
    TestProgress
    ------------------------------------------------------------------------*/

    unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
    memory_t memory;
    rc_lboard_t* lboard;
    lboard_test_state_t state;
    unsigned value;
    
    memory.ram = ram;
    memory.size = sizeof(ram);

    lboard = parse_lboard("STA:0xH00=0::CAN:0xH00=2::SUB:0xH00=3::PRO:0xH04::VAL:0xH02", buffer);
    state.active = state.submitted = 0;

    value = lboard_evaluate(lboard, &state, &memory);
    assert(state.active);
    assert(value == 0x56U);

    lboard = parse_lboard("STA:0xH00=0::CAN:0xH00=2::SUB:0xH00=3::VAL:0xH02", buffer);
    state.active = state.submitted = 0;

    value = lboard_evaluate(lboard, &state, &memory);
    assert(state.active);
    assert(value == 0x34U);
  }

  {
    /*------------------------------------------------------------------------
    TestStartAndCondition
    ------------------------------------------------------------------------*/

    unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
    memory_t memory;
    rc_lboard_t* lboard;
    lboard_test_state_t state;
    
    memory.ram = ram;
    memory.size = sizeof(ram);

    lboard = parse_lboard("STA:0xH00=0_0xH01=0::CAN:0xH01=10::SUB:0xH01=18::VAL:0xH02", buffer);
    state.active = state.submitted = 0;

    lboard_evaluate(lboard, &state, &memory);
    assert(!state.active);

    ram[1] = 0; /* second part of start condition is true */
    lboard_evaluate(lboard, &state, &memory);
    assert(state.active);
  }

  {
    /*------------------------------------------------------------------------
    TestStartOrCondition
    ------------------------------------------------------------------------*/

    unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
    memory_t memory;
    rc_lboard_t* lboard;
    lboard_test_state_t state;
    
    memory.ram = ram;
    memory.size = sizeof(ram);

    lboard = parse_lboard("STA:S0xH00=1S0xH01=1::CAN:0xH01=10::SUB:0xH01=18::VAL:0xH02", buffer);
    state.active = state.submitted = 0;

    lboard_evaluate(lboard, &state, &memory);
    assert(!state.active);

    ram[1] = 1; /* second part of start condition is true */
    lboard_evaluate(lboard, &state, &memory);
    assert(state.active);

    ram[1] = 0;
    lboard_reset(lboard, &state);
    lboard_evaluate(lboard, &state, &memory);
    assert(!state.active);

    ram[0] = 1; /* first part of start condition is true */
    lboard_evaluate(lboard, &state, &memory);
    assert(state.active);
  }

  {
    /*------------------------------------------------------------------------
    TestCancelOrCondition
    ------------------------------------------------------------------------*/

    unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
    memory_t memory;
    rc_lboard_t* lboard;
    lboard_test_state_t state;
    
    memory.ram = ram;
    memory.size = sizeof(ram);

    lboard = parse_lboard("STA:0xH00=0::CAN:S0xH01=12S0xH02=12::SUB:0xH00=3::VAL:0xH02", buffer);
    state.active = state.submitted = 0;

    lboard_evaluate(lboard, &state, &memory);
    assert(state.active);

    ram[2] = 12; /* second part of cancel condition is true */
    lboard_evaluate(lboard, &state, &memory);
    assert(!state.active);

    ram[2] = 0; /* second part of cancel condition is false */
    lboard_reset(lboard, &state);
    lboard_evaluate(lboard, &state, &memory);
    assert(state.active);

    ram[1] = 12; /* first part of cancel condition is true */
    lboard_evaluate(lboard, &state, &memory);
    assert(!state.active);
  }

  {
    /*------------------------------------------------------------------------
    TestSubmitAndCondition
    ------------------------------------------------------------------------*/

    unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
    memory_t memory;
    rc_lboard_t* lboard;
    lboard_test_state_t state;
    
    memory.ram = ram;
    memory.size = sizeof(ram);

    lboard = parse_lboard("STA:0xH00=0::CAN:0xH01=10::SUB:0xH01=18_0xH03=18::VAL:0xH02", buffer);
    state.active = state.submitted = 0;

    lboard_evaluate(lboard, &state, &memory);
    assert(state.active);

    ram[3] = 18; 
    lboard_evaluate(lboard, &state, &memory);
    assert(!state.active);
    assert(state.submitted);
  }

  {
    /*------------------------------------------------------------------------
    TestSubmitOrCondition
    ------------------------------------------------------------------------*/

    unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
    memory_t memory;
    rc_lboard_t* lboard;
    lboard_test_state_t state;
    
    memory.ram = ram;
    memory.size = sizeof(ram);

    lboard = parse_lboard("STA:0xH00=0::CAN:0xH01=10::SUB:S0xH01=12S0xH03=12::VAL:0xH02", buffer);
    state.active = state.submitted = 0;

    lboard_evaluate(lboard, &state, &memory);
    assert(state.active);

    ram[3] = 12; /* second part of submit condition is true */
    lboard_evaluate(lboard, &state, &memory);
    assert(!state.active);
    assert(state.submitted);

    ram[3] = 0;
    lboard_reset(lboard, &state);
    lboard_evaluate(lboard, &state, &memory);
    assert(state.active);

    ram[1] = 12; /* first part of submit condition is true */
    lboard_evaluate(lboard, &state, &memory);
    assert(!state.active);
    assert(state.submitted);
  }

  {
    /*------------------------------------------------------------------------
    TestUnparsableStringWillNotStart
    We'll test for errors in the memaddr field instead
    ------------------------------------------------------------------------*/

    lboard_check("STA:0xH00=0::CAN:0xH00=2::SUB:0xH00=3::PRO:0xH04::VAL:0xH02::GARBAGE", RC_INVALID_LBOARD_FIELD);
    lboard_check("CAN:0xH00=2::SUB:0xH00=3::PRO:0xH04::VAL:0xH02", RC_MISSING_START);
    lboard_check("STA:0xH00=0::SUB:0xH00=3::PRO:0xH04::VAL:0xH02", RC_MISSING_CANCEL);
    lboard_check("STA:0xH00=0::CAN:0xH00=2::PRO:0xH04::VAL:0xH02", RC_MISSING_SUBMIT);
    lboard_check("STA:0xH00=0::CAN:0xH00=2::SUB:0xH00=3::PRO:0xH04", RC_MISSING_VALUE);
    lboard_check("STA:0xH00=0::CAN:0xH00=2::SUB:0xH00=3::PRO:0xH04::VAL:0xH02::STA:0=0", RC_DUPLICATED_START);
    lboard_check("STA:0xH00=0::CAN:0xH00=2::SUB:0xH00=3::PRO:0xH04::VAL:0xH02::CAN:0=0", RC_DUPLICATED_CANCEL);
    lboard_check("STA:0xH00=0::CAN:0xH00=2::SUB:0xH00=3::PRO:0xH04::VAL:0xH02::SUB:0=0", RC_DUPLICATED_SUBMIT);
    lboard_check("STA:0xH00=0::CAN:0xH00=2::SUB:0xH00=3::PRO:0xH04::VAL:0xH02::VAL:0", RC_DUPLICATED_VALUE);
    lboard_check("STA:0xH00=0::CAN:0xH00=2::SUB:0xH00=3::PRO:0xH04::VAL:0xH02::PRO:0", RC_DUPLICATED_PROGRESS);
  }
}

static rc_richpresence_t* parse_richpresence(const char* script, void* buffer) {
  int ret;
  rc_richpresence_t* self;

  ret = rc_richpresence_size(script);
  assert(ret >= 0);
  memset(buffer, 0xEE, ret + 128);

  self = rc_parse_richpresence(buffer, script, NULL, 0);
  assert(self != NULL);
  assert(*((int*)((char*)buffer + ret)) == 0xEEEEEEEE);

  return self;
}

static void test_richpresence(void) {
  {
    /*------------------------------------------------------------------------
    TestStaticDisplayString
    ------------------------------------------------------------------------*/
    unsigned char ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
    memory_t memory;
    rc_richpresence_t* richpresence;
    char buffer[2048];
    char output[128];
    int result;

    memory.ram = ram;
    memory.size = sizeof(ram);

    richpresence = parse_richpresence("Display:\nHello, world!", buffer);
    result = rc_evaluate_richpresence(richpresence, output, sizeof(output), peek, &memory, NULL);
    assert(strcmp(output, "Hello, world!") == 0);
    assert(result == 13);
  }

  {
    /*------------------------------------------------------------------------
    TestEscapedComment
    ------------------------------------------------------------------------*/
    unsigned char ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
    memory_t memory;
    rc_richpresence_t* richpresence;
    char buffer[2048];
    char output[128];
    int result;

    memory.ram = ram;
    memory.size = sizeof(ram);

    richpresence = parse_richpresence("Display:\nWhat \\// Where", buffer);
    result = rc_evaluate_richpresence(richpresence, output, sizeof(output), peek, &memory, NULL);
    assert(strcmp(output, "What // Where") == 0);
    assert(result == 13);
  }

  {
    /*------------------------------------------------------------------------
    TestEscapedBackslash
    ------------------------------------------------------------------------*/
    unsigned char ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
    memory_t memory;
    rc_richpresence_t* richpresence;
    char buffer[2048];
    char output[128];
    int result;

    memory.ram = ram;
    memory.size = sizeof(ram);

    richpresence = parse_richpresence("Display:\nWhat \\\\ Where", buffer);
    result = rc_evaluate_richpresence(richpresence, output, sizeof(output), peek, &memory, NULL);
    assert(strcmp(output, "What \\ Where") == 0);
    assert(result == 12);
  }

  {
    /*------------------------------------------------------------------------
    TestPartiallyEscapedComment
    ------------------------------------------------------------------------*/
    unsigned char ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
    memory_t memory;
    rc_richpresence_t* richpresence;
    char buffer[2048];
    char output[128];
    int result;

    memory.ram = ram;
    memory.size = sizeof(ram);

    richpresence = parse_richpresence("Display:\nWhat \\/// Where", buffer);
    result = rc_evaluate_richpresence(richpresence, output, sizeof(output), peek, &memory, NULL);
    assert(strcmp(output, "What /") == 0);
    assert(result == 6);
  }

  {
    /*------------------------------------------------------------------------
    TestTrailingBackslash
    ------------------------------------------------------------------------*/
    unsigned char ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
    memory_t memory;
    rc_richpresence_t* richpresence;
    char buffer[2048];
    char output[128];
    int result;

    memory.ram = ram;
    memory.size = sizeof(ram);

    richpresence = parse_richpresence("Display:\nWhat \\", buffer);
    result = rc_evaluate_richpresence(richpresence, output, sizeof(output), peek, &memory, NULL);
    assert(strcmp(output, "What ") == 0);
    assert(result == 5);
  }

  {
    /*------------------------------------------------------------------------
    TestConditionalDisplay
    ------------------------------------------------------------------------*/
    unsigned char ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
    memory_t memory;
    rc_richpresence_t* richpresence;
    char buffer[2048];
    char output[128];
    int result;

    memory.ram = ram;
    memory.size = sizeof(ram);

    richpresence = parse_richpresence("Display:\n?0xH0000=0?Zero\n?0xH0000=1?One\nOther", buffer);
    result = rc_evaluate_richpresence(richpresence, output, sizeof(output), peek, &memory, NULL);
    assert(strcmp(output, "Zero") == 0);
    assert(result == 4);

    ram[0] = 1;
    result = rc_evaluate_richpresence(richpresence, output, sizeof(output), peek, &memory, NULL);
    assert(strcmp(output, "One") == 0);
    assert(result == 3);

    ram[0] = 2;
    result = rc_evaluate_richpresence(richpresence, output, sizeof(output), peek, &memory, NULL);
    assert(strcmp(output, "Other") == 0);
    assert(result == 5);
  }

  {
    /*------------------------------------------------------------------------
    TestConditionalDisplayOutOfOrder
    ------------------------------------------------------------------------*/
    unsigned char ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
    memory_t memory;
    rc_richpresence_t* richpresence;
    char buffer[2048];
    char output[128];
    int result;

    memory.ram = ram;
    memory.size = sizeof(ram);

    richpresence = parse_richpresence("Display:\nOther\n?0xH0000=0?Zero\n?0xH0000=1?One", buffer);
    result = rc_evaluate_richpresence(richpresence, output, sizeof(output), peek, &memory, NULL);
    assert(strcmp(output, "Other") == 0);
    assert(result == 5);
  }

  {
    /*------------------------------------------------------------------------
    TestConditionalDisplayNoDefault
    ------------------------------------------------------------------------*/
    int result = rc_richpresence_size("Display:\n?0xH0000=0?Zero");
    assert(result == RC_MISSING_DISPLAY_STRING);
  }

  {
    /*------------------------------------------------------------------------
    TestConditionalDisplayCommonPrefix
    ------------------------------------------------------------------------*/
    unsigned char ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
    memory_t memory;
    rc_richpresence_t* richpresence;
    char buffer[2048];
    char output[128];

    memory.ram = ram;
    memory.size = sizeof(ram);

    richpresence = parse_richpresence("Display:\n?0xH0000=0_0xH0001=18?First\n?0xH0000=0?Second\nThird", buffer);
    rc_evaluate_richpresence(richpresence, output, sizeof(output), peek, &memory, NULL);
    assert(strcmp(output, "First") == 0);

    ram[1] = 1;
    rc_evaluate_richpresence(richpresence, output, sizeof(output), peek, &memory, NULL);
    assert(strcmp(output, "Second") == 0);

    ram[0] = 1;
    rc_evaluate_richpresence(richpresence, output, sizeof(output), peek, &memory, NULL);
    assert(strcmp(output, "Third") == 0);

    ram[0] = 0;
    ram[1] = 18;
    richpresence = parse_richpresence("Display:\n?0xH0000=0?First\n?0xH0000=0_0xH0001=18?Second\nThird", buffer);
    rc_evaluate_richpresence(richpresence, output, sizeof(output), peek, &memory, NULL);
    assert(strcmp(output, "First") == 0);

    ram[1] = 1;
    rc_evaluate_richpresence(richpresence, output, sizeof(output), peek, &memory, NULL);
    assert(strcmp(output, "First") == 0);

    ram[0] = 1;
    rc_evaluate_richpresence(richpresence, output, sizeof(output), peek, &memory, NULL);
    assert(strcmp(output, "Third") == 0);
  }

  {
    /*------------------------------------------------------------------------
    TestConditionalDisplayDuplicatedCondition
    ------------------------------------------------------------------------*/
    unsigned char ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
    memory_t memory;
    rc_richpresence_t* richpresence;
    char buffer[2048];
    char output[128];

    memory.ram = ram;
    memory.size = sizeof(ram);

    richpresence = parse_richpresence("Display:\n?0xH0000=0?First\n?0xH0000=0?Second\nThird", buffer);
    rc_evaluate_richpresence(richpresence, output, sizeof(output), peek, &memory, NULL);
    assert(strcmp(output, "First") == 0);

    ram[0] = 1;
    rc_evaluate_richpresence(richpresence, output, sizeof(output), peek, &memory, NULL);
    assert(strcmp(output, "Third") == 0);
  }

  {
    /*------------------------------------------------------------------------
    TestConditionalDisplayInvalidCondition
    ------------------------------------------------------------------------*/
    int result = rc_richpresence_size("Display:\n?BANANA?First\nOther");
    assert(result == RC_INVALID_MEMORY_OPERAND);
  }

  {
    /*------------------------------------------------------------------------
    TestValueMacro
    ------------------------------------------------------------------------*/
    unsigned char ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
    memory_t memory;
    rc_richpresence_t* richpresence;
    char buffer[2048];
    char output[128];
    int result;

    memory.ram = ram;
    memory.size = sizeof(ram);

    richpresence = parse_richpresence("Format:Points\nFormatType=VALUE\n\nDisplay:\n@Points(0x 0001) Points", buffer);
    result = rc_evaluate_richpresence(richpresence, output, sizeof(output), peek, &memory, NULL);
    assert(strcmp(output, "13330 Points") == 0);
    assert(result == 12);

    ram[1] = 20;
    result = rc_evaluate_richpresence(richpresence, output, sizeof(output), peek, &memory, NULL);
    assert(strcmp(output, "13332 Points") == 0);
    assert(result == 12);
  }

  {
    /*------------------------------------------------------------------------
    TestFramesMacro
    ------------------------------------------------------------------------*/
    unsigned char ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
    memory_t memory;
    rc_richpresence_t* richpresence;
    char buffer[2048];
    char output[128];
    int result;

    memory.ram = ram;
    memory.size = sizeof(ram);

    richpresence = parse_richpresence("Format:Frames\nFormatType=FRAMES\n\nDisplay:\n@Frames(0x 0001)", buffer);
    result = rc_evaluate_richpresence(richpresence, output, sizeof(output), peek, &memory, NULL);
    assert(strcmp(output, "03:42.16") == 0);
    assert(result == 8);

    ram[1] = 20;
    result = rc_evaluate_richpresence(richpresence, output, sizeof(output), peek, &memory, NULL);
    assert(strcmp(output, "03:42.20") == 0);
    assert(result == 8);
  }

  {
    /*------------------------------------------------------------------------
    TestValueMacroFormula
    ------------------------------------------------------------------------*/
    unsigned char ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
    memory_t memory;
    rc_richpresence_t* richpresence;
    char buffer[2048];
    char output[128];
    int result;

    memory.ram = ram;
    memory.size = sizeof(ram);

    richpresence = parse_richpresence("Format:Points\nFormatType=VALUE\n\nDisplay:\n@Points(0xH0001*100_0xH0002) Points", buffer);
    result = rc_evaluate_richpresence(richpresence, output, sizeof(output), peek, &memory, NULL);
    assert(strcmp(output, "1852 Points") == 0);
    assert(result == 11);

    ram[1] = 0x20;
    result = rc_evaluate_richpresence(richpresence, output, sizeof(output), peek, &memory, NULL);
    assert(strcmp(output, "3252 Points") == 0);
    assert(result == 11);
  }

  {
    /*------------------------------------------------------------------------
    TestUndefinedMacro
    ------------------------------------------------------------------------*/
    unsigned char ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
    memory_t memory;
    rc_richpresence_t* richpresence;
    char buffer[2048];
    char output[128];
    int result;

    memory.ram = ram;
    memory.size = sizeof(ram);

    richpresence = parse_richpresence("Display:\n@Points(0x 0001) Points", buffer);
    result = rc_evaluate_richpresence(richpresence, output, sizeof(output), peek, &memory, NULL);
    assert(strcmp(output, "[Unknown macro]Points(0x 0001) Points") == 0);
    assert(result == 37);
  }

  {
    /*------------------------------------------------------------------------
    TestEscapedMacro
    ------------------------------------------------------------------------*/
    unsigned char ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
    memory_t memory;
    rc_richpresence_t* richpresence;
    char buffer[2048];
    char output[128];
    int result;

    memory.ram = ram;
    memory.size = sizeof(ram);

    richpresence = parse_richpresence("Format:Points\nFormatType=VALUE\n\nDisplay:\n\\@Points(0x 0001) \\@@Points(0x 0001) Points", buffer);
    result = rc_evaluate_richpresence(richpresence, output, sizeof(output), peek, &memory, NULL);
    assert(strcmp(output, "@Points(0x 0001) @13330 Points") == 0);
    assert(result == 30);
  }

  {
    /*------------------------------------------------------------------------
    TestLookup
    ------------------------------------------------------------------------*/
    unsigned char ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
    memory_t memory;
    rc_richpresence_t* richpresence;
    char buffer[2048];
    char output[128];
    int result;

    memory.ram = ram;
    memory.size = sizeof(ram);

    richpresence = parse_richpresence("Lookup:Location\n0=Zero\n1=One\n\nDisplay:\nAt @Location(0xH0000)", buffer);
    result = rc_evaluate_richpresence(richpresence, output, sizeof(output), peek, &memory, NULL);
    assert(strcmp(output, "At Zero") == 0);
    assert(result == 7);

    ram[0] = 1;
    result = rc_evaluate_richpresence(richpresence, output, sizeof(output), peek, &memory, NULL);
    assert(strcmp(output, "At One") == 0);
    assert(result == 6);

    ram[0] = 2; /* no entry */
    result = rc_evaluate_richpresence(richpresence, output, sizeof(output), peek, &memory, NULL);
    assert(strcmp(output, "At ") == 0);
    assert(result == 3);
  }

  {
    /*------------------------------------------------------------------------
    TestLookupFormula
    ------------------------------------------------------------------------*/
    unsigned char ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
    memory_t memory;
    rc_richpresence_t* richpresence;
    char buffer[2048];
    char output[128];
    int result;

    memory.ram = ram;
    memory.size = sizeof(ram);

    richpresence = parse_richpresence("Lookup:Location\n0=Zero\n1=One\n\nDisplay:\nAt @Location(0xH0000*0.5)", buffer);
    result = rc_evaluate_richpresence(richpresence, output, sizeof(output), peek, &memory, NULL);
    assert(strcmp(output, "At Zero") == 0);
    assert(result == 7);

    ram[0] = 1;
    result = rc_evaluate_richpresence(richpresence, output, sizeof(output), peek, &memory, NULL);
    assert(strcmp(output, "At Zero") == 0);
    assert(result == 7);

    ram[0] = 2; /* no entry */
    result = rc_evaluate_richpresence(richpresence, output, sizeof(output), peek, &memory, NULL);
    assert(strcmp(output, "At One") == 0);
    assert(result == 6);
  }

  {
    /*------------------------------------------------------------------------
    TestLookupRepeated
    ------------------------------------------------------------------------*/
    unsigned char ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
    memory_t memory;
    rc_richpresence_t* richpresence;
    char buffer[2048];
    char output[128];
    int result;

    memory.ram = ram;
    memory.size = sizeof(ram);

    richpresence = parse_richpresence("Lookup:Location\n0=Zero\n1=One\n\nDisplay:\nAt @Location(0xH0000), Near @Location(0xH0001)", buffer);
    result = rc_evaluate_richpresence(richpresence, output, sizeof(output), peek, &memory, NULL);
    assert(strcmp(output, "At Zero, Near ") == 0);
    assert(result == 14);

    ram[1] = 1;
    result = rc_evaluate_richpresence(richpresence, output, sizeof(output), peek, &memory, NULL);
    assert(strcmp(output, "At Zero, Near One") == 0);
    assert(result == 17);

    ram[0] = 1;
    result = rc_evaluate_richpresence(richpresence, output, sizeof(output), peek, &memory, NULL);
    assert(strcmp(output, "At One, Near One") == 0);
    assert(result == 16);
  }

  {
    /*------------------------------------------------------------------------
    TestLookupMultiple
    ------------------------------------------------------------------------*/
    unsigned char ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
    memory_t memory;
    rc_richpresence_t* richpresence;
    char buffer[2048];
    char output[128];
    int result;

    memory.ram = ram;
    memory.size = sizeof(ram);

    richpresence = parse_richpresence("Lookup:Location\n0=Zero\n1=One\n\nLookup:Location2\n0=zero\n1=one\n\nDisplay:\nAt @Location(0xH0000), Near @Location2(0xH0001)", buffer);
    result = rc_evaluate_richpresence(richpresence, output, sizeof(output), peek, &memory, NULL);
    assert(strcmp(output, "At Zero, Near ") == 0);
    assert(result == 14);

    ram[1] = 1;
    result = rc_evaluate_richpresence(richpresence, output, sizeof(output), peek, &memory, NULL);
    assert(strcmp(output, "At Zero, Near one") == 0);
    assert(result == 17);

    ram[0] = 1;
    result = rc_evaluate_richpresence(richpresence, output, sizeof(output), peek, &memory, NULL);
    assert(strcmp(output, "At One, Near one") == 0);
    assert(result == 16);
  }

  {
    /*------------------------------------------------------------------------
    TestLookupAndValue
    ------------------------------------------------------------------------*/
    unsigned char ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
    memory_t memory;
    rc_richpresence_t* richpresence;
    char buffer[2048];
    char output[128];
    int result;

    memory.ram = ram;
    memory.size = sizeof(ram);

    richpresence = parse_richpresence("Lookup:Location\n0=Zero\n1=One\n\nFormat:Location2\nFormatType=VALUE\n\nDisplay:\nAt @Location(0xH0000), Near @Location2(0xH0001)", buffer);
    result = rc_evaluate_richpresence(richpresence, output, sizeof(output), peek, &memory, NULL);
    assert(strcmp(output, "At Zero, Near 18") == 0);
    assert(result == 16);

    ram[1] = 1;
    result = rc_evaluate_richpresence(richpresence, output, sizeof(output), peek, &memory, NULL);
    assert(strcmp(output, "At Zero, Near 1") == 0);
    assert(result == 15);

    ram[0] = 1;
    result = rc_evaluate_richpresence(richpresence, output, sizeof(output), peek, &memory, NULL);
    assert(strcmp(output, "At One, Near 1") == 0);
    assert(result == 14);
  }

  {
    /*------------------------------------------------------------------------
    TestLookupHexKeys
    ------------------------------------------------------------------------*/
    unsigned char ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
    memory_t memory;
    rc_richpresence_t* richpresence;
    char buffer[2048];
    char output[128];
    int result;

    memory.ram = ram;
    memory.size = sizeof(ram);

    richpresence = parse_richpresence("Lookup:Location\n0x00=Zero\n0x01=One\n\nDisplay:\nAt @Location(0xH0000)", buffer);
    result = rc_evaluate_richpresence(richpresence, output, sizeof(output), peek, &memory, NULL);
    assert(strcmp(output, "At Zero") == 0);
    assert(result == 7);

    ram[0] = 1;
    result = rc_evaluate_richpresence(richpresence, output, sizeof(output), peek, &memory, NULL);
    assert(strcmp(output, "At One") == 0);
    assert(result == 6);

    ram[0] = 2; /* no entry */
    result = rc_evaluate_richpresence(richpresence, output, sizeof(output), peek, &memory, NULL);
    assert(strcmp(output, "At ") == 0);
    assert(result == 3);
  }

  {
    /*------------------------------------------------------------------------
    TestLookupDefault
    ------------------------------------------------------------------------*/
    unsigned char ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
    memory_t memory;
    rc_richpresence_t* richpresence;
    char buffer[2048];
    char output[128];
    int result;

    memory.ram = ram;
    memory.size = sizeof(ram);

    richpresence = parse_richpresence("Lookup:Location\n0=Zero\n1=One\n*=Star\n\nDisplay:\nAt @Location(0xH0000)", buffer);
    result = rc_evaluate_richpresence(richpresence, output, sizeof(output), peek, &memory, NULL);
    assert(strcmp(output, "At Zero") == 0);
    assert(result == 7);

    ram[0] = 1;
    result = rc_evaluate_richpresence(richpresence, output, sizeof(output), peek, &memory, NULL);
    assert(strcmp(output, "At One") == 0);
    assert(result == 6);

    ram[0] = 2; /* no entry */
    result = rc_evaluate_richpresence(richpresence, output, sizeof(output), peek, &memory, NULL);
    assert(strcmp(output, "At Star") == 0);
    assert(result == 7);
  }

  {
    /*------------------------------------------------------------------------
    TestLookupCRLF
    ------------------------------------------------------------------------*/
    unsigned char ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
    memory_t memory;
    rc_richpresence_t* richpresence;
    char buffer[2048];
    char output[128];
    int result;

    memory.ram = ram;
    memory.size = sizeof(ram);

    richpresence = parse_richpresence("Lookup:Location\r\n0=Zero\r\n1=One\r\n\r\nDisplay:\r\nAt @Location(0xH0000)\r\n", buffer);
    result = rc_evaluate_richpresence(richpresence, output, sizeof(output), peek, &memory, NULL);
    assert(strcmp(output, "At Zero") == 0);
    assert(result == 7);

    ram[0] = 1;
    result = rc_evaluate_richpresence(richpresence, output, sizeof(output), peek, &memory, NULL);
    assert(strcmp(output, "At One") == 0);
    assert(result == 6);

    ram[0] = 2; /* no entry */
    result = rc_evaluate_richpresence(richpresence, output, sizeof(output), peek, &memory, NULL);
    assert(strcmp(output, "At ") == 0);
    assert(result == 3);
  }

  {
    /*------------------------------------------------------------------------
    TestLookupAfterDisplay
    ------------------------------------------------------------------------*/
    unsigned char ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
    memory_t memory;
    rc_richpresence_t* richpresence;
    char buffer[2048];
    char output[128];
    int result;

    memory.ram = ram;
    memory.size = sizeof(ram);

    richpresence = parse_richpresence("Display:\nAt @Location(0xH0000)\n\nLookup:Location\n0=Zero\n1=One", buffer);
    result = rc_evaluate_richpresence(richpresence, output, sizeof(output), peek, &memory, NULL);
    assert(strcmp(output, "At Zero") == 0);
    assert(result == 7);

    ram[0] = 1;
    result = rc_evaluate_richpresence(richpresence, output, sizeof(output), peek, &memory, NULL);
    assert(strcmp(output, "At One") == 0);
    assert(result == 6);

    ram[0] = 2; /* no entry */
    result = rc_evaluate_richpresence(richpresence, output, sizeof(output), peek, &memory, NULL);
    assert(strcmp(output, "At ") == 0);
    assert(result == 3);
  }

  {
    /*------------------------------------------------------------------------
    TestLookupWhitespace
    ------------------------------------------------------------------------*/
    unsigned char ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
    memory_t memory;
    rc_richpresence_t* richpresence;
    char buffer[2048];
    char output[128];
    int result;

    memory.ram = ram;
    memory.size = sizeof(ram);

    richpresence = parse_richpresence("Lookup:Location\n0= Zero \n1= One \n\nDisplay:\nAt '@Location(0xH0000)' ", buffer);
    result = rc_evaluate_richpresence(richpresence, output, sizeof(output), peek, &memory, NULL);
    assert(strcmp(output, "At ' Zero ' ") == 0);
    assert(result == 12);

    ram[0] = 1;
    result = rc_evaluate_richpresence(richpresence, output, sizeof(output), peek, &memory, NULL);
    assert(strcmp(output, "At ' One ' ") == 0);
    assert(result == 11);

    ram[0] = 2; /* no entry */
    result = rc_evaluate_richpresence(richpresence, output, sizeof(output), peek, &memory, NULL);
    assert(strcmp(output, "At '' ") == 0);
    assert(result == 6);
  }

  {
    /*------------------------------------------------------------------------
    TestRandomTextBetweenSections
    ------------------------------------------------------------------------*/
    unsigned char ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
    memory_t memory;
    rc_richpresence_t* richpresence;
    char buffer[2048];
    char output[128];

    memory.ram = ram;
    memory.size = sizeof(ram);

    /* Anything that doesn't begin with "Format:" "Lookup:" or "Display:" is ignored. People sometimes
       use this logic to add comments to the Rich Presence script - particularly author comments */
    richpresence = parse_richpresence("Locations are fun!\nLookup:Location\n0=Zero\n1=One\n\nDisplay goes here\nDisplay:\nAt @Location(0xH0000)\n\nWritten by User3", buffer);
    rc_evaluate_richpresence(richpresence, output, sizeof(output), peek, &memory, NULL);
    assert(strcmp(output, "At Zero") == 0);

    ram[0] = 1;
    rc_evaluate_richpresence(richpresence, output, sizeof(output), peek, &memory, NULL);
    assert(strcmp(output, "At One") == 0);

    ram[0] = 2; /* no entry */
    rc_evaluate_richpresence(richpresence, output, sizeof(output), peek, &memory, NULL);
    assert(strcmp(output, "At ") == 0);
  }

  {
    /*------------------------------------------------------------------------
    TestComments
    ------------------------------------------------------------------------*/
    unsigned char ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
    memory_t memory;
    rc_richpresence_t* richpresence;
    char buffer[2048];
    char output[128];

    memory.ram = ram;
    memory.size = sizeof(ram);

    richpresence = parse_richpresence("// Locations are fun!\nLookup:Location // lookup\n0=Zero // 0\n1=One // 1\n\n//Display goes here\nDisplay: // display\nAt @Location(0xH0000) // text\n\n//Written by User3", buffer);
    rc_evaluate_richpresence(richpresence, output, sizeof(output), peek, &memory, NULL);
    assert(strcmp(output, "At Zero") == 0);

    ram[0] = 1;
    rc_evaluate_richpresence(richpresence, output, sizeof(output), peek, &memory, NULL);
    assert(strcmp(output, "At One") == 0);

    ram[0] = 2; /* no entry */
    rc_evaluate_richpresence(richpresence, output, sizeof(output), peek, &memory, NULL);
    assert(strcmp(output, "At ") == 0);
  }

  {
    /*------------------------------------------------------------------------
    TestConditionalDisplaySharedLookup
    ------------------------------------------------------------------------*/
    unsigned char ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
    memory_t memory;
    rc_richpresence_t* richpresence;
    char buffer[2048];
    char output[128];

    memory.ram = ram;
    memory.size = sizeof(ram);

    richpresence = parse_richpresence("Lookup:Location\n0x00=Zero\n0x01=One\n\nDisplay:\n?0xH0001=18?At @Location(0xH0000)\nNear @Location(0xH0000)", buffer);
    rc_evaluate_richpresence(richpresence, output, sizeof(output), peek, &memory, NULL);
    assert(strcmp(output, "At Zero") == 0);

    ram[0] = 1;
    rc_evaluate_richpresence(richpresence, output, sizeof(output), peek, &memory, NULL);
    assert(strcmp(output, "At One") == 0);

    ram[1] = 17;
    rc_evaluate_richpresence(richpresence, output, sizeof(output), peek, &memory, NULL);
    assert(strcmp(output, "Near One") == 0);

    ram[0] = 0;
    rc_evaluate_richpresence(richpresence, output, sizeof(output), peek, &memory, NULL);
    assert(strcmp(output, "Near Zero") == 0);
  }
}

static void test_lua(void) {
  {
    /*------------------------------------------------------------------------
    TestLua
    ------------------------------------------------------------------------*/

#ifndef RC_DISABLE_LUA

    lua_State* L;
    const char* luacheevo = "return { test = function(peek, ud) return peek(0, 4, ud) end }";
    unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
    memory_t memory;
    rc_trigger_t* trigger;
    char buffer[2048];

    memory.ram = ram;
    memory.size = sizeof(ram);

    L = luaL_newstate();
    luaL_loadbufferx(L, luacheevo, strlen(luacheevo), "luacheevo.lua", "t");
    lua_call(L, 0, 1);

    memory.ram = ram;
    memory.size = sizeof(ram);

    trigger = rc_parse_trigger(buffer, "@test=0xX0", L, 1);
    assert(rc_test_trigger(trigger, peek, &memory, L) != 0);

#endif /* RC_DISABLE_LUA */
  }
}

int main(void) {
  test_operand();
  test_condition();
  test_trigger();
  test_term();
  test_value();
  test_lboard();
  test_richpresence();
  test_lua();

  return 0;
}
