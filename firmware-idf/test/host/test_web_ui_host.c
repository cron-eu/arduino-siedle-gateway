/*
 * Host unit tests for the Host header check of the setup endpoints.
 */
#include "unity.h"
#include "web_ui_host.h"

void setUp(void) {}
void tearDown(void) {}

#define IP "192.168.4.1"

static void test_ip(void)
{
    TEST_ASSERT_TRUE(web_ui_host_is_gateway("192.168.4.1", IP, "siedle"));
    TEST_ASSERT_TRUE(web_ui_host_is_gateway("192.168.4.1:80", IP, "siedle"));
    TEST_ASSERT_FALSE(web_ui_host_is_gateway("192.168.4.10", IP, "siedle"));
    TEST_ASSERT_FALSE(web_ui_host_is_gateway("192.168.4.1.example.com", IP, "siedle"));
    TEST_ASSERT_FALSE(web_ui_host_is_gateway("192.168.4", IP, "siedle"));
}

static void test_local_name(void)
{
    TEST_ASSERT_TRUE(web_ui_host_is_gateway("siedle.local", IP, "siedle"));
    TEST_ASSERT_TRUE(web_ui_host_is_gateway("Siedle.LOCAL:80", IP, "siedle"));
    TEST_ASSERT_TRUE(web_ui_host_is_gateway("siedle-office.local", IP, "siedle-office"));
    TEST_ASSERT_FALSE(web_ui_host_is_gateway("siedle", IP, "siedle"));
    TEST_ASSERT_FALSE(web_ui_host_is_gateway("siedle.local.example.com", IP, "siedle"));
    TEST_ASSERT_FALSE(web_ui_host_is_gateway("evil-siedle.local", IP, "siedle"));
    TEST_ASSERT_FALSE(web_ui_host_is_gateway("sied.local", IP, "siedle"));
    TEST_ASSERT_FALSE(web_ui_host_is_gateway(".local", IP, ""));
    TEST_ASSERT_FALSE(web_ui_host_is_gateway("siedle.local", IP, NULL));
}

// what a page using DNS rebinding sends
static void test_other_names(void)
{
    TEST_ASSERT_FALSE(web_ui_host_is_gateway("attacker.example", IP, "siedle"));
    TEST_ASSERT_FALSE(web_ui_host_is_gateway("[::ffff:c0a8:401]", IP, "siedle"));
    TEST_ASSERT_FALSE(web_ui_host_is_gateway("", IP, "siedle"));
    TEST_ASSERT_FALSE(web_ui_host_is_gateway(NULL, IP, "siedle"));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_ip);
    RUN_TEST(test_local_name);
    RUN_TEST(test_other_names);
    return UNITY_END();
}
