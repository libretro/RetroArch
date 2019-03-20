#ifndef INTERNAL_H
#define INTERNAL_H

#include "rcheevos.h"

#define RC_TAG2(x,y) x ## y
#define RC_TAG(x,y) RC_TAG2(x,y)
#define RC_OFFSETOF(s, f) ((int)(long long)(&((s*)0)->f))
#define RC_ALIGNOF(t) RC_OFFSETOF(struct RC_TAG(_unnamed, __LINE__) {char c; t d;}, d)

#define RC_ALLOC(t, p, o, s) ((t*)rc_alloc(p, o, sizeof(t), RC_ALIGNOF(t), s))

typedef union {
  rc_operand_t operand;
  rc_condition_t condition;
  rc_condset_t condset;
  rc_trigger_t trigger;
  rc_term_t term;
  rc_expression_t expression;
  rc_lboard_t lboard;
  rc_richpresence_t richpresence;
  rc_richpresence_display_t richpresence_display;
  rc_richpresence_display_part_t richpresence_part;
  rc_richpresence_lookup_t richpresence_lookup;
  rc_richpresence_lookup_item_t richpresence_lookup_item;
}
rc_scratch_t;

void* rc_alloc(void* pointer, int* offset, int size, int alignment, rc_scratch_t* scratch);
char* rc_alloc_str(void* pointer, int* offset, const char* text, int length);

void rc_parse_trigger_internal(rc_trigger_t* self, int* ret, void* buffer, rc_scratch_t* scratch, const char** memaddr, lua_State* L, int funcs_ndx);

rc_condset_t* rc_parse_condset(int* ret, void* buffer, rc_scratch_t* scratch, const char** memaddr, lua_State* L, int funcs_ndx);
int rc_test_condset(rc_condset_t* self, int* reset, rc_peek_t peek, void* ud, lua_State* L);
void rc_reset_condset(rc_condset_t* self);

rc_condition_t* rc_parse_condition(int* ret, void* buffer, rc_scratch_t* scratch, const char** memaddr, lua_State* L, int funcs_ndx);
int rc_test_condition(rc_condition_t* self, unsigned add_buffer, rc_peek_t peek, void* ud, lua_State* L);

int rc_parse_operand(rc_operand_t* self, const char** memaddr, int is_trigger, lua_State* L, int funcs_ndx);
unsigned rc_evaluate_operand(rc_operand_t* self, rc_peek_t peek, void* ud, lua_State* L);

rc_term_t* rc_parse_term(int* ret, void* buffer, rc_scratch_t* scratch, const char** memaddr, lua_State* L, int funcs_ndx);
unsigned rc_evaluate_term(rc_term_t* self, rc_peek_t peek, void* ud, lua_State* L);

rc_expression_t* rc_parse_expression(int* ret, void* buffer, rc_scratch_t* scratch, const char** memaddr, lua_State* L, int funcs_ndx);
unsigned rc_evaluate_expression(rc_expression_t* self, rc_peek_t peek, void* ud, lua_State* L);

void rc_parse_value_internal(rc_value_t* self, int* ret, void* buffer, void* scratch, const char** memaddr, lua_State* L, int funcs_ndx);

void rc_parse_lboard_internal(rc_lboard_t* self, int* ret, void* buffer, void* scratch, const char* memaddr, lua_State* L, int funcs_ndx);

const char* rc_parse_line(const char* line, const char** end);
void rc_parse_richpresence_internal(rc_richpresence_t* self, int* ret, void* buffer, void* scratch, const char* script, lua_State* L, int funcs_ndx);

#endif /* INTERNAL_H */
