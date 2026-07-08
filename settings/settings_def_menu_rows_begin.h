/* Menu-region emitter set for the single-source settings def files.
 * Every generated region includes this, the def file, then the
 * matching end header; no include guard by design - the set must
 * expand for every region. */
#define S_ACTION(T, n, us, sub) \
                  SDESC_ACTION_ROW(T),
#define S_ACTION_EX(T, n, sd, ok, rp, c, us, sub) \
                  SDESC_ACTION_ROW_EX(T, sd, ok, rp, c),
#define S_ACTION_EX_NS(T, n, sd, ok, rp, c, us) \
                  SDESC_ACTION_ROW_EX(T, sd, ok, rp, c),
#define S_ACTION_LV(T, TV, n, sd, ok, rp, c, us, sub) \
                  SDESC_ACTION_ROW_LV(T, TV, sd, ok, rp, c),
#define S_ACTION_LV_NS(T, TV, n, sd, ok, rp, c, us) \
                  SDESC_ACTION_ROW_LV(T, TV, sd, ok, rp, c),
#define S_ACTION_NS(T, n, us) \
                  SDESC_ACTION_ROW(T),
#define S_BOOL(f, T, n, d, sd, df, c, us, sub) \
                  SDESC_BOOL_ROW(f, T, d, sd, df, c),
#define S_BOOL_DF(f, T, n, resolver, sd, df, c, ok, rp, sta, sel, lf, rt, ui, us, sub) \
                  SDESC_BOOL_ROW_DF(f, T, resolver, sd, df, c, ok, rp, sta, sel, lf, rt, ui),
#define S_BOOL_CH(f, T, n, d, sd, df, c, ok, rp, sta, sel, lf, rt, ui, chg, us, sub) \
                  SDESC_BOOL_ROW_CH(f, T, d, sd, df, c, ok, rp, sta, sel, lf, rt, ui, chg),
#define S_BOOL_EX(f, T, n, d, sd, df, c, ok, rp, sta, sel, lf, rt, ui, us, sub) \
                  SDESC_BOOL_ROW_EX(f, T, d, sd, df, c, ok, rp, sta, sel, lf, rt, ui),
#define S_BOOL_EX_NS(f, T, n, d, sd, df, c, ok, rp, sta, sel, lf, rt, ui, us) \
                  SDESC_BOOL_ROW_EX(f, T, d, sd, df, c, ok, rp, sta, sel, lf, rt, ui),
#define S_BOOL_LV(f, T, TV, n, d, sd, df, c, us, sub) \
                  SDESC_BOOL_ROW_LV(f, T, TV, d, sd, df, c),
#define S_BOOL_LV_NS(f, T, TV, n, d, sd, df, c, us) \
                  SDESC_BOOL_ROW_LV(f, T, TV, d, sd, df, c),
#define S_BOOL_NS(f, T, n, d, sd, df, c, us) \
                  SDESC_BOOL_ROW(f, T, d, sd, df, c),
#define S_DIR(f, T, n, d, el, sd, c, sta, us, sub) \
                  SDESC_DIR_ROW(f, T, d, el, sd, c, sta),
#define S_DIR_NS(f, T, n, d, el, sd, c, sta, us) \
                  SDESC_DIR_ROW(f, T, d, el, sd, c, sta),
#define S_FLOAT(f, T, n, d, rnd, sd, df, c, mn, mx, st, ok, rp, us, sub) \
                  SDESC_FLOAT_ROW(f, T, d, rnd, sd, df, c, mn, mx, st, ok, rp),
#define S_FLOAT_EX(f, T, n, d, rnd, sd, df, c, mn, mx, st, ok, rp, sta, sel, lf, rt, ui, us, sub) \
                  SDESC_FLOAT_ROW_EX(f, T, d, rnd, sd, df, c, mn, mx, st, ok, rp, sta, sel, lf, rt, ui),
#define S_FLOAT_EX_NS(f, T, n, d, rnd, sd, df, c, mn, mx, st, ok, rp, sta, sel, lf, rt, ui, us) \
                  SDESC_FLOAT_ROW_EX(f, T, d, rnd, sd, df, c, mn, mx, st, ok, rp, sta, sel, lf, rt, ui),
#define S_FLOAT_LV(f, T, TV, n, d, rnd, sd, df, c, us, sub) \
                  SDESC_FLOAT_ROW_LV(f, T, TV, d, rnd, sd, df, c),
#define S_FLOAT_LV_NS(f, T, TV, n, d, rnd, sd, df, c, us) \
                  SDESC_FLOAT_ROW_LV(f, T, TV, d, rnd, sd, df, c),
#define S_FLOAT_NS(f, T, n, d, rnd, sd, df, c, mn, mx, st, ok, rp, us) \
                  SDESC_FLOAT_ROW(f, T, d, rnd, sd, df, c, mn, mx, st, ok, rp),
#define S_INT(f, T, n, d, sd, df, c, mn, mx, st, ob, ok, rp, us, sub) \
                  SDESC_INT_ROW(f, T, d, sd, df, c, mn, mx, st, ob, ok, rp),
#define S_INT_AT(offs, T, n, d, sd, df, c, mn, mx, st, ob, ok, rp, us, sub) \
                  SDESC_INT_ROW_AT(offs, T, d, sd, df, c, mn, mx, st, ob, ok, rp),
#define S_INT_AT_NS(offs, T, n, d, sd, df, c, mn, mx, st, ob, ok, rp, us) \
                  SDESC_INT_ROW_AT(offs, T, d, sd, df, c, mn, mx, st, ob, ok, rp),
#define S_INT_EX(f, T, n, d, sd, df, c, mn, mx, st, ob, ok, rp, sta, sel, lf, rt, ui, us, sub) \
                  SDESC_INT_ROW_EX(f, T, d, sd, df, c, mn, mx, st, ob, ok, rp, sta, sel, lf, rt, ui),
#define S_INT_EX_NS(f, T, n, d, sd, df, c, mn, mx, st, ob, ok, rp, sta, sel, lf, rt, ui, us) \
                  SDESC_INT_ROW_EX(f, T, d, sd, df, c, mn, mx, st, ob, ok, rp, sta, sel, lf, rt, ui),
#define S_INT_NS(f, T, n, d, sd, df, c, mn, mx, st, ob, ok, rp, us) \
                  SDESC_INT_ROW(f, T, d, sd, df, c, mn, mx, st, ob, ok, rp),
#define S_PATH(f, T, n, d, sd, c, vals, rp, ui, us, sub) \
                  SDESC_PATH_ROW(f, T, d, sd, c, vals, rp, ui),
#define S_PATH_DS(f, T, n, df2, sd, c, vals, rp, ui, us, sub) \
                  SDESC_PATH_ROW_DS(f, T, df2, sd, c, vals, rp, ui),
#define S_PATH_DS_NS(f, T, n, df2, sd, c, vals, rp, ui, us) \
                  SDESC_PATH_ROW_DS(f, T, df2, sd, c, vals, rp, ui),
#define S_PATH_NS(f, T, n, d, sd, c, vals, rp, ui, us) \
                  SDESC_PATH_ROW(f, T, d, sd, c, vals, rp, ui),
#define S_STRING(f, T, n, d, sd, c, ok, rp, sta, sel, lf, rt, ui, us, sub) \
                  SDESC_STRING_ROW(f, T, d, sd, c, ok, rp, sta, sel, lf, rt, ui),
#define S_STRING_LV(f, T, TV, n, d, sd, c, ok, rp, sta, sel, lf, rt, ui, us, sub) \
                  SDESC_STRING_ROW_LV(f, T, TV, d, sd, c, ok, rp, sta, sel, lf, rt, ui),
#define S_STRING_LV_NS(f, T, TV, n, d, sd, c, ok, rp, sta, sel, lf, rt, ui, us) \
                  SDESC_STRING_ROW_LV(f, T, TV, d, sd, c, ok, rp, sta, sel, lf, rt, ui),
#define S_STRING_NS(f, T, n, d, sd, c, ok, rp, sta, sel, lf, rt, ui, us) \
                  SDESC_STRING_ROW(f, T, d, sd, c, ok, rp, sta, sel, lf, rt, ui),
#define S_STRING_P(f, T, n, d, sd, c, ok, rp, sta, sel, lf, rt, ui, us, sub) \
                  SDESC_STRING_ROW_P(f, T, d, sd, c, ok, rp, sta, sel, lf, rt, ui),
#define S_STRING_P_NS(f, T, n, d, sd, c, ok, rp, sta, sel, lf, rt, ui, us) \
                  SDESC_STRING_ROW_P(f, T, d, sd, c, ok, rp, sta, sel, lf, rt, ui),
#define S_UINT(f, T, n, d, sd, df, c, mn, mx, st, ob, ok, rp, us, sub) \
                  SDESC_UINT_ROW(f, T, d, sd, df, c, mn, mx, st, ob, ok, rp),
#define S_UINT_AT_EX(offs, T, n, d, sd, df, c, mn, mx, st, ob, ok, rp, sta, sel, lf, rt, ui, us, sub) \
                  SDESC_UINT_ROW_AT_EX(offs, T, d, sd, df, c, mn, mx, st, ob, ok, rp, sta, sel, lf, rt, ui),
#define S_UINT_AT_EX_NS(offs, T, n, d, sd, df, c, mn, mx, st, ob, ok, rp, sta, sel, lf, rt, ui, us) \
                  SDESC_UINT_ROW_AT_EX(offs, T, d, sd, df, c, mn, mx, st, ob, ok, rp, sta, sel, lf, rt, ui),
#define S_UINT_DF(f, T, n, resolver, sd, df, c, mn, mx, st, ob, ok, rp, ui, us, sub) \
                  SDESC_UINT_ROW_DF(f, T, resolver, sd, df, c, mn, mx, st, ob, ok, rp, ui),
#define S_UINT_EX(f, T, n, d, sd, df, c, mn, mx, st, ob, ok, rp, sta, sel, lf, rt, ui, us, sub) \
                  SDESC_UINT_ROW_EX(f, T, d, sd, df, c, mn, mx, st, ob, ok, rp, sta, sel, lf, rt, ui),
#define S_UINT_EX_NS(f, T, n, d, sd, df, c, mn, mx, st, ob, ok, rp, sta, sel, lf, rt, ui, us) \
                  SDESC_UINT_ROW_EX(f, T, d, sd, df, c, mn, mx, st, ob, ok, rp, sta, sel, lf, rt, ui),
#define S_UINT_NS(f, T, n, d, sd, df, c, mn, mx, st, ob, ok, rp, us) \
                  SDESC_UINT_ROW(f, T, d, sd, df, c, mn, mx, st, ob, ok, rp),
