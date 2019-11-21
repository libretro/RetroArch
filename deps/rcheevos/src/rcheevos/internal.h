#ifndef INTERNAL_H
#define INTERNAL_H

#include "rcheevos.h"

#define RC_ALLOW_ALIGN(T) struct __align_ ## T { char ch; T t; };
RC_ALLOW_ALIGN(rc_condition_t)
RC_ALLOW_ALIGN(rc_condset_t)
RC_ALLOW_ALIGN(rc_expression_t)
RC_ALLOW_ALIGN(rc_lboard_t)
RC_ALLOW_ALIGN(rc_memref_value_t)
RC_ALLOW_ALIGN(rc_operand_t)
RC_ALLOW_ALIGN(rc_richpresence_t)
RC_ALLOW_ALIGN(rc_richpresence_display_t)
RC_ALLOW_ALIGN(rc_richpresence_display_part_t)
RC_ALLOW_ALIGN(rc_richpresence_lookup_t)
RC_ALLOW_ALIGN(rc_richpresence_lookup_item_t)
RC_ALLOW_ALIGN(rc_term_t)
RC_ALLOW_ALIGN(rc_trigger_t)
RC_ALLOW_ALIGN(rc_value_t)
RC_ALLOW_ALIGN(char)

#define RC_ALIGNOF(T) (sizeof(struct __align_ ## T) - sizeof(T))

#define RC_ALLOC(t, p) ((t*)rc_alloc((p)->buffer, &(p)->offset, sizeof(t), RC_ALIGNOF(t), &(p)->scratch))

typedef struct {
  rc_memref_t memref_buffer[16];
  rc_memref_t *memref;
  int memref_count;
  int memref_size;

  union
  {
    rc_operand_t operand;
    rc_condition_t condition;
    rc_condset_t condset;
    rc_trigger_t trigger;
    rc_term_t term;
    rc_expression_t expression;
    rc_lboard_t lboard;
    rc_memref_value_t memref_value;
    rc_richpresence_t richpresence;
    rc_richpresence_display_t richpresence_display;
    rc_richpresence_display_part_t richpresence_part;
    rc_richpresence_lookup_t richpresence_lookup;
    rc_richpresence_lookup_item_t richpresence_lookup_item;
  } obj;
}
rc_scratch_t;

typedef struct {
  unsigned add_value;       /* AddSource/SubSource */
  unsigned add_hits;        /* AddHits */
  unsigned add_address;     /* AddAddress */

  rc_peek_t peek;
  void* peek_userdata;
  lua_State* L;

  unsigned measured_value;  /* Measured */
  char was_reset;           /* ResetIf triggered */
  char has_hits;            /* one of more hit counts is non-zero */
}
rc_eval_state_t;

typedef struct {
  int offset;

  lua_State* L;
  int funcs_ndx;

  void* buffer;
  rc_scratch_t scratch;

  rc_memref_value_t** first_memref;

  unsigned measured_target;
}
rc_parse_state_t;

void rc_init_parse_state(rc_parse_state_t* parse, void* buffer, lua_State* L, int funcs_ndx);
void rc_init_parse_state_memrefs(rc_parse_state_t* parse, rc_memref_value_t** memrefs);
void rc_destroy_parse_state(rc_parse_state_t* parse);

void* rc_alloc(void* pointer, int* offset, int size, int alignment, rc_scratch_t* scratch);
char* rc_alloc_str(rc_parse_state_t* parse, const char* text, int length);

rc_memref_value_t* rc_alloc_memref_value(rc_parse_state_t* parse, unsigned address, char size, char is_bcd, char is_indirect);
void rc_update_memref_values(rc_memref_value_t* memref, rc_peek_t peek, void* ud);
void rc_update_memref_value(rc_memref_value_t* memref, rc_peek_t peek, void* ud);
rc_memref_value_t* rc_get_indirect_memref(rc_memref_value_t* memref, rc_eval_state_t* eval_state);

void rc_parse_trigger_internal(rc_trigger_t* self, const char** memaddr, rc_parse_state_t* parse);

rc_condset_t* rc_parse_condset(const char** memaddr, rc_parse_state_t* parse);
int rc_test_condset(rc_condset_t* self, rc_eval_state_t* eval_state);
void rc_reset_condset(rc_condset_t* self);

rc_condition_t* rc_parse_condition(const char** memaddr, rc_parse_state_t* parse, int is_indirect);
int rc_test_condition(rc_condition_t* self, rc_eval_state_t* eval_state);

int rc_parse_operand(rc_operand_t* self, const char** memaddr, int is_trigger, int is_indirect, rc_parse_state_t* parse);
unsigned rc_evaluate_operand(rc_operand_t* self, rc_eval_state_t* eval_state);

rc_term_t* rc_parse_term(const char** memaddr, int is_indirect, rc_parse_state_t* parse);
int rc_evaluate_term(rc_term_t* self, rc_eval_state_t* eval_state);

rc_expression_t* rc_parse_expression(const char** memaddr, rc_parse_state_t* parse);
int rc_evaluate_expression(rc_expression_t* self, rc_eval_state_t* eval_state);

void rc_parse_value_internal(rc_value_t* self, const char** memaddr, rc_parse_state_t* parse);

void rc_parse_lboard_internal(rc_lboard_t* self, const char* memaddr, rc_parse_state_t* parse);

void rc_parse_richpresence_internal(rc_richpresence_t* self, const char* script, rc_parse_state_t* parse);

#endif /* INTERNAL_H */
