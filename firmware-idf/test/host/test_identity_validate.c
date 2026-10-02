/*
 * Host unit tests for the identity setting checks used by the setup page.
 */
#include <string.h>

#include "identity_validate.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

#define ENDPOINT "a1b2c3d4e5f6g7-ats.iot.eu-central-1.amazonaws.com"

static void assert_normalized(const char *input, const char *expected)
{
    char buf[300];
    strcpy(buf, input);
    identity_endpoint_normalize(buf);
    TEST_ASSERT_EQUAL_STRING(expected, buf);
}

static void test_endpoint_normalize(void)
{
    assert_normalized(ENDPOINT, ENDPOINT);
    assert_normalized("  " ENDPOINT "\n", ENDPOINT);
    assert_normalized("https://" ENDPOINT "/", ENDPOINT);
    assert_normalized("mqtts://" ENDPOINT ":8883", ENDPOINT);
    assert_normalized("A1B2C3D4E5F6G7-ATS.iot.eu-central-1.amazonaws.com", ENDPOINT);
    // other ports are kept, so they fail validation instead of being dropped silently
    assert_normalized(ENDPOINT ":443", ENDPOINT ":443");
    assert_normalized("", "");
}

static void test_endpoint_valid(void)
{
    TEST_ASSERT_TRUE(identity_endpoint_valid(ENDPOINT));
    TEST_ASSERT_TRUE(identity_endpoint_valid("a.b"));

    TEST_ASSERT_FALSE(identity_endpoint_valid(NULL));
    TEST_ASSERT_FALSE(identity_endpoint_valid(""));
    TEST_ASSERT_FALSE(identity_endpoint_valid("SiedleGateway")); // the thing name in the wrong field
    TEST_ASSERT_FALSE(identity_endpoint_valid("a..b"));
    TEST_ASSERT_FALSE(identity_endpoint_valid(".a.b"));
    TEST_ASSERT_FALSE(identity_endpoint_valid("a.b."));
    TEST_ASSERT_FALSE(identity_endpoint_valid("-a.b"));
    TEST_ASSERT_FALSE(identity_endpoint_valid("a-.b"));
    TEST_ASSERT_FALSE(identity_endpoint_valid("a_b.com"));
    TEST_ASSERT_FALSE(identity_endpoint_valid("a b.com"));
    TEST_ASSERT_FALSE(identity_endpoint_valid(ENDPOINT ":443"));

    char label[70];
    memset(label, 'a', 63);
    strcpy(label + 63, ".b");
    TEST_ASSERT_TRUE(identity_endpoint_valid(label));
    memset(label, 'a', 64);
    strcpy(label + 64, ".b");
    TEST_ASSERT_FALSE(identity_endpoint_valid(label));

    char longest[IDENTITY_ENDPOINT_MAX + 2];
    memset(longest, 'a', sizeof(longest) - 1);
    longest[sizeof(longest) - 1] = '\0';
    for (size_t i = 50; i < sizeof(longest) - 1; i += 50) {
        longest[i] = '.';
    }
    TEST_ASSERT_FALSE(identity_endpoint_valid(longest));
    longest[IDENTITY_ENDPOINT_MAX] = '\0';
    TEST_ASSERT_TRUE(identity_endpoint_valid(longest));
}

static void test_thing_valid(void)
{
    TEST_ASSERT_TRUE(identity_thing_valid("SiedleGateway"));
    TEST_ASSERT_TRUE(identity_thing_valid("x"));
    TEST_ASSERT_TRUE(identity_thing_valid("Siedle_Gateway-02:office"));

    TEST_ASSERT_FALSE(identity_thing_valid(NULL));
    TEST_ASSERT_FALSE(identity_thing_valid(""));
    // MQTT topic syntax must not end up in siedle/<thing>/status
    TEST_ASSERT_FALSE(identity_thing_valid("a/b"));
    TEST_ASSERT_FALSE(identity_thing_valid("a+b"));
    TEST_ASSERT_FALSE(identity_thing_valid("a#"));
    TEST_ASSERT_FALSE(identity_thing_valid("a b"));
    TEST_ASSERT_FALSE(identity_thing_valid("T\xc3\xbcr")); // "Tür"

    char name[IDENTITY_THING_MAX + 2];
    memset(name, 'a', IDENTITY_THING_MAX);
    name[IDENTITY_THING_MAX] = '\0';
    TEST_ASSERT_TRUE(identity_thing_valid(name));
    strcat(name, "a");
    TEST_ASSERT_FALSE(identity_thing_valid(name));
}

static void test_hostname_valid(void)
{
    TEST_ASSERT_TRUE(identity_hostname_valid("siedle"));
    TEST_ASSERT_TRUE(identity_hostname_valid("Siedle-2"));
    TEST_ASSERT_TRUE(identity_hostname_valid("7"));

    TEST_ASSERT_FALSE(identity_hostname_valid(NULL));
    TEST_ASSERT_FALSE(identity_hostname_valid(""));
    TEST_ASSERT_FALSE(identity_hostname_valid("-siedle"));
    TEST_ASSERT_FALSE(identity_hostname_valid("siedle-"));
    TEST_ASSERT_FALSE(identity_hostname_valid("siedle.local"));
    TEST_ASSERT_FALSE(identity_hostname_valid("siedle_2"));

    char name[IDENTITY_HOSTNAME_MAX + 2];
    memset(name, 'a', IDENTITY_HOSTNAME_MAX);
    name[IDENTITY_HOSTNAME_MAX] = '\0';
    TEST_ASSERT_TRUE(identity_hostname_valid(name));
    strcat(name, "a");
    TEST_ASSERT_FALSE(identity_hostname_valid(name));
}

static void test_ap_password_valid(void)
{
    TEST_ASSERT_TRUE(identity_ap_password_valid("12345678"));
    TEST_ASSERT_TRUE(identity_ap_password_valid("correct horse battery staple!"));

    TEST_ASSERT_FALSE(identity_ap_password_valid(NULL));
    TEST_ASSERT_FALSE(identity_ap_password_valid(""));
    TEST_ASSERT_FALSE(identity_ap_password_valid("1234567"));
    TEST_ASSERT_FALSE(identity_ap_password_valid("tab\tseparated"));
    TEST_ASSERT_FALSE(identity_ap_password_valid("T\xc3\xbcrklingel")); // "Türklingel"

    char password[IDENTITY_AP_PASSWORD_MAX + 2];
    memset(password, 'a', IDENTITY_AP_PASSWORD_MAX);
    password[IDENTITY_AP_PASSWORD_MAX] = '\0';
    TEST_ASSERT_TRUE(identity_ap_password_valid(password));
    strcat(password, "a"); // 64 characters would be a raw key, not a passphrase
    TEST_ASSERT_FALSE(identity_ap_password_valid(password));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_endpoint_normalize);
    RUN_TEST(test_endpoint_valid);
    RUN_TEST(test_thing_valid);
    RUN_TEST(test_hostname_valid);
    RUN_TEST(test_ap_password_valid);
    return UNITY_END();
}
