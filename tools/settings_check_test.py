#!/usr/bin/env python3
"""Check key extraction independently of the compiler's current row layout."""
import unittest
import settings_check


LABELS = '\n'.join([
    'char m_LABEL_FIRST[sizeof("first")];',
    'char m_LABEL_SECOND[sizeof("second")];',
])
PACKED_FIRST = ('{ (uint32_t)__builtin_offsetof(settings_t, bools.first), '
                '(uint16_t)__builtin_offsetof(struct msg_hash_us_lbl_blob_s, '
                'm_LABEL_FIRST), (0) },')
PACKED_SECOND = ('{ (uint32_t)__builtin_offsetof(settings_t, bools.second), '
                 '(uint16_t)__builtin_offsetof(struct msg_hash_us_lbl_blob_s, '
                 'm_LABEL_SECOND), (1) },')
LITERAL = '{ "literal", (uint32_t)offsetof(settings_t, bools.literal), (2) },'


def table(*rows):
    return LABELS + '\nconfig_bool_rows[] = {\n' + '\n'.join(rows) + '\n{ 0 }};'


class SettingsRowsTest(unittest.TestCase):
    def test_packed_labels(self):
        self.assertEqual(settings_check.table_rows(table(PACKED_FIRST, PACKED_SECOND), 'bool'),
                         [('bool', 'first', '1', '0'), ('bool', 'second', '1', '1')])

    def test_literal_keys(self):
        self.assertEqual(settings_check.table_rows(table(LITERAL), 'bool'),
                         [('bool', 'literal', '1', '2')])

    def test_mixed_order(self):
        self.assertEqual(settings_check.table_rows(table(PACKED_FIRST, LITERAL, PACKED_SECOND), 'bool'),
                         [('bool', 'first', '1', '0'), ('bool', 'literal', '1', '2'),
                          ('bool', 'second', '1', '1')])

    def test_missing_label(self):
        with self.assertRaisesRegex(ValueError, 'no packed label'):
            settings_check.table_rows(table(PACKED_FIRST).replace(LABELS, ''), 'bool')

    def test_extract_keeps_call_order(self):
        text = table(PACKED_FIRST) + """
struct config_bool_setting *populate_settings_bool(void) {
 tmp[count].ident = "before"; tmp[count].ptr = &settings->bools.before;
 if (1) { tmp[count].flags = 1; tmp[count].def = 3; }
 config_bool_rows_add(tmp, settings, count, cap);
 tmp[count].ident = "after"; tmp[count].ptr = &settings->bools.after;
 if (1) { tmp[count].flags = 1; tmp[count].def = 4; }
 *size = count;
}
"""
        self.assertEqual(settings_check.extract(text),
                         [('bool', 'before', '1', '3'), ('bool', 'first', '1', '0'),
                          ('bool', 'after', '1', '4')])


if __name__ == '__main__':
    unittest.main()
