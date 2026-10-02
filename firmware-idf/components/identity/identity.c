#include "identity.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_check.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "mbedtls/pem.h"
#include "mbedtls/pk.h"
#include "mbedtls/platform_util.h"
#include "mbedtls/x509_crt.h"
#include "mbedtls/x509_csr.h"
#include "nvs.h"
#include "psa/crypto.h"

#define NVS_NAMESPACE "identity"
#define KEY_PEM_MAX   512
#define CSR_PEM_MAX   1024
#define CERT_PEM_MAX  4000 // longest string NVS stores

#define PEM_BEGIN_CRT "-----BEGIN CERTIFICATE-----\n"
#define PEM_END_CRT   "-----END CERTIFICATE-----\n"

static const char *TAG = "identity";
static SemaphoreHandle_t s_lock;

#define LOCK()   xSemaphoreTake(s_lock, portMAX_DELAY)
#define UNLOCK() xSemaphoreGive(s_lock)

/* ---- storage ----------------------------------------------------------------------------------------------- */

static void free_secret(char *secret)
{
    if (secret) {
        mbedtls_platform_zeroize(secret, strlen(secret));
        free(secret);
    }
}

static char *nvs_read_str(nvs_handle_t nvs, const char *key)
{
    size_t len = 0;
    if (nvs_get_str(nvs, key, NULL, &len) != ESP_OK || len <= 1) {
        return NULL;
    }
    char *value = malloc(len);
    if (value && nvs_get_str(nvs, key, value, &len) != ESP_OK) {
        free(value);
        value = NULL;
    }
    return value;
}

// One value, NULL if it is not set (lock held)
static char *read_str(const char *key)
{
    nvs_handle_t nvs;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs) != ESP_OK) {
        return NULL; // nothing stored yet
    }
    char *value = nvs_read_str(nvs, key);
    nvs_close(nvs);
    return value;
}

// NULL or "" deletes the value (no commit)
static esp_err_t nvs_write_str(nvs_handle_t nvs, const char *key, const char *value)
{
    if (value == NULL || value[0] == '\0') {
        esp_err_t err = nvs_erase_key(nvs, key);
        return err == ESP_ERR_NVS_NOT_FOUND ? ESP_OK : err;
    }
    return nvs_set_str(nvs, key, value);
}

// (lock held)
static esp_err_t write_str(const char *key, const char *value)
{
    nvs_handle_t nvs;
    ESP_RETURN_ON_ERROR(nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs), TAG, "nvs open");
    esp_err_t err = nvs_write_str(nvs, key, value);
    if (err == ESP_OK) {
        err = nvs_commit(nvs);
    }
    nvs_close(nvs);
    return err;
}

esp_err_t identity_load(identity_t *out)
{
    memset(out, 0, sizeof(*out));
    nvs_handle_t nvs;
    LOCK();
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs);
    if (err == ESP_OK) {
        out->endpoint = nvs_read_str(nvs, "endpoint");
        out->thing = nvs_read_str(nvs, "thing");
        out->cert = nvs_read_str(nvs, "cert");
        out->key = nvs_read_str(nvs, "key");
        out->ap_password = nvs_read_str(nvs, "ap_pass");
        out->hostname = nvs_read_str(nvs, "hostname");
        nvs_close(nvs);
    }
    UNLOCK();
    return err == ESP_ERR_NVS_NOT_FOUND ? ESP_OK : err; // not found: nothing stored yet
}

void identity_free(identity_t *id)
{
    free(id->endpoint);
    free(id->thing);
    free(id->cert);
    free_secret(id->key);
    free_secret(id->ap_password);
    free(id->hostname);
    memset(id, 0, sizeof(*id));
}

bool identity_has_cloud(const identity_t *id)
{
    return id->endpoint && id->thing && id->cert && id->key;
}

esp_err_t identity_set_hostname(const char *hostname)
{
    ESP_RETURN_ON_FALSE(hostname == NULL || hostname[0] == '\0' || identity_hostname_valid(hostname),
                        ESP_ERR_INVALID_ARG, TAG, "invalid hostname");
    LOCK();
    esp_err_t err = write_str("hostname", hostname);
    UNLOCK();
    return err;
}

esp_err_t identity_set_ap_password(const char *password)
{
    ESP_RETURN_ON_FALSE(password == NULL || password[0] == '\0' || identity_ap_password_valid(password),
                        ESP_ERR_INVALID_ARG, TAG, "invalid hotspot password");
    LOCK();
    esp_err_t err = write_str("ap_pass", password);
    UNLOCK();
    return err;
}

/* ---- device key -------------------------------------------------------------------------------------------- */

// A new EC P-256 key pair as PEM (SEC1)
static esp_err_t generate_key(char **out)
{
    psa_key_attributes_t attr = PSA_KEY_ATTRIBUTES_INIT;
    psa_set_key_type(&attr, PSA_KEY_TYPE_ECC_KEY_PAIR(PSA_ECC_FAMILY_SECP_R1));
    psa_set_key_bits(&attr, 256);
    psa_set_key_usage_flags(&attr, PSA_KEY_USAGE_EXPORT); // only to store it, TLS and the CSR parse it again

    mbedtls_svc_key_id_t key_id = MBEDTLS_SVC_KEY_ID_INIT;
    psa_status_t status = psa_generate_key(&attr, &key_id);
    if (status != PSA_SUCCESS) {
        ESP_LOGE(TAG, "key generation failed (%d)", (int)status);
        return ESP_FAIL;
    }

    char *pem = calloc(1, KEY_PEM_MAX);
    mbedtls_pk_context pk;
    mbedtls_pk_init(&pk);
    int ret = pem ? mbedtls_pk_copy_from_psa(key_id, &pk) : PSA_ERROR_INSUFFICIENT_MEMORY;
    psa_destroy_key(key_id);
    if (ret == 0) {
        ret = mbedtls_pk_write_key_pem(&pk, (unsigned char *)pem, KEY_PEM_MAX);
    }
    mbedtls_pk_free(&pk);

    if (ret != 0) {
        ESP_LOGE(TAG, "writing the key failed (-0x%04x)", (unsigned)-ret);
        if (pem) {
            mbedtls_platform_zeroize(pem, KEY_PEM_MAX);
            free(pem);
        }
        return ESP_FAIL;
    }
    *out = pem;
    return ESP_OK;
}

// (lock held)
static esp_err_t replace_key(void)
{
    int64_t started = esp_timer_get_time();
    char *pem = NULL;
    ESP_RETURN_ON_ERROR(generate_key(&pem), TAG, "generate key");

    nvs_handle_t nvs;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs);
    if (err == ESP_OK) {
        // drop the certificate first, so a power failure in between never leaves it next to a key it doesn't match
        err = nvs_write_str(nvs, "cert", NULL);
        if (err == ESP_OK) {
            err = nvs_set_str(nvs, "key", pem);
        }
        if (err == ESP_OK) {
            err = nvs_commit(nvs);
        }
        nvs_close(nvs);
    }
    free_secret(pem);
    ESP_RETURN_ON_ERROR(err, TAG, "store key");

    ESP_LOGI(TAG, "new device key (EC P-256) generated in %" PRId64 " ms", (esp_timer_get_time() - started) / 1000);
    return ESP_OK;
}

// (lock held)
static bool has_key(void)
{
    nvs_handle_t nvs;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs) != ESP_OK) {
        return false;
    }
    size_t len = 0;
    bool found = nvs_get_str(nvs, "key", NULL, &len) == ESP_OK && len > 1;
    nvs_close(nvs);
    return found;
}

esp_err_t identity_ensure_key(void)
{
    LOCK();
    esp_err_t err = has_key() ? ESP_OK : replace_key();
    UNLOCK();
    return err;
}

esp_err_t identity_new_key(void)
{
    LOCK();
    esp_err_t err = replace_key();
    UNLOCK();
    return err;
}

// The device key, ESP_ERR_INVALID_STATE if there is none (lock held)
static esp_err_t load_key(mbedtls_pk_context *pk)
{
    char *pem = read_str("key");
    if (pem == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    int ret = mbedtls_pk_parse_key(pk, (const unsigned char *)pem, strlen(pem) + 1, NULL, 0);
    free_secret(pem);
    if (ret != 0) {
        ESP_LOGE(TAG, "device key unreadable (-0x%04x)", (unsigned)-ret);
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t identity_csr(const char *thing, char **out)
{
    *out = NULL;
    ESP_RETURN_ON_FALSE(identity_thing_valid(thing), ESP_ERR_INVALID_ARG, TAG, "invalid thing name");
    char subject[sizeof("CN=") + IDENTITY_THING_MAX];
    snprintf(subject, sizeof(subject), "CN=%s", thing);

    mbedtls_pk_context pk;
    mbedtls_pk_init(&pk);
    LOCK();
    esp_err_t err = load_key(&pk);
    UNLOCK();
    if (err != ESP_OK) {
        mbedtls_pk_free(&pk);
        return err;
    }

    char *pem = malloc(CSR_PEM_MAX);
    mbedtls_x509write_csr csr;
    mbedtls_x509write_csr_init(&csr);
    mbedtls_x509write_csr_set_key(&csr, &pk);
    mbedtls_x509write_csr_set_md_alg(&csr, MBEDTLS_MD_SHA256);
    int ret = pem ? mbedtls_x509write_csr_set_subject_name(&csr, subject) : PSA_ERROR_INSUFFICIENT_MEMORY;
    if (ret == 0) {
        ret = mbedtls_x509write_csr_pem(&csr, (unsigned char *)pem, CSR_PEM_MAX);
    }
    mbedtls_x509write_csr_free(&csr);
    mbedtls_pk_free(&pk);

    if (ret != 0) {
        ESP_LOGE(TAG, "creating the CSR failed (-0x%04x)", (unsigned)-ret);
        free(pem);
        return ESP_FAIL;
    }
    *out = pem;
    return ESP_OK;
}

/* ---- certificate ------------------------------------------------------------------------------------------- */

// Parse a PEM certificate (or chain) and check the first one against the device key (lock held)
static identity_cert_check_t check_cert(mbedtls_x509_crt *crt, const char *pem)
{
    if (mbedtls_x509_crt_parse(crt, (const unsigned char *)pem, strlen(pem) + 1) < 0 || crt->raw.len == 0) {
        return IDENTITY_CERT_INVALID;
    }
    mbedtls_pk_context key;
    mbedtls_pk_init(&key);
    identity_cert_check_t result = IDENTITY_CERT_NO_KEY;
    if (load_key(&key) == ESP_OK) {
        result = mbedtls_pk_check_pair(&crt->pk, &key) == 0 ? IDENTITY_CERT_OK : IDENTITY_CERT_WRONG_KEY;
    }
    mbedtls_pk_free(&key);
    return result;
}

identity_cert_check_t identity_check_cert(const char *pem)
{
    mbedtls_x509_crt crt;
    mbedtls_x509_crt_init(&crt);
    LOCK();
    identity_cert_check_t result = check_cert(&crt, pem);
    UNLOCK();
    mbedtls_x509_crt_free(&crt);
    return result;
}

// The first certificate of a chain as PEM, NULL if it is too large to store
static char *leaf_pem(const mbedtls_x509_crt *crt)
{
    size_t len = 0;
    mbedtls_pem_write_buffer(PEM_BEGIN_CRT, PEM_END_CRT, crt->raw.p, crt->raw.len, NULL, 0, &len);
    char *pem = len > 0 && len <= CERT_PEM_MAX ? malloc(len) : NULL;
    if (pem && mbedtls_pem_write_buffer(PEM_BEGIN_CRT, PEM_END_CRT, crt->raw.p, crt->raw.len, (unsigned char *)pem,
                                        len, &len) != 0) {
        free(pem);
        pem = NULL;
    }
    return pem;
}

esp_err_t identity_set_cloud(const char *endpoint, const char *thing, const char *cert)
{
    ESP_RETURN_ON_FALSE(identity_endpoint_valid(endpoint) && identity_thing_valid(thing), ESP_ERR_INVALID_ARG, TAG,
                        "invalid endpoint or thing name");
    esp_err_t err = ESP_OK;
    char *cert_pem = NULL;

    LOCK();
    if (cert) {
        mbedtls_x509_crt crt;
        mbedtls_x509_crt_init(&crt);
        if (check_cert(&crt, cert) == IDENTITY_CERT_OK) {
            cert_pem = leaf_pem(&crt);
        }
        mbedtls_x509_crt_free(&crt);
        err = cert_pem ? ESP_OK : ESP_ERR_INVALID_ARG;
    }
    nvs_handle_t nvs;
    if (err == ESP_OK) {
        err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs);
    }
    if (err == ESP_OK) {
        err = nvs_set_str(nvs, "endpoint", endpoint);
        if (err == ESP_OK) {
            err = nvs_set_str(nvs, "thing", thing);
        }
        if (err == ESP_OK && cert_pem) {
            err = nvs_set_str(nvs, "cert", cert_pem);
        }
        if (err == ESP_OK) {
            err = nvs_commit(nvs);
        }
        nvs_close(nvs);
    }
    UNLOCK();

    free(cert_pem);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "AWS IoT settings for '%s' stored%s", thing, cert ? ", with a new certificate" : "");
    }
    return err;
}

esp_err_t identity_cert_info(identity_cert_info_t *out)
{
    memset(out, 0, sizeof(*out));
    LOCK();
    char *pem = read_str("cert");
    UNLOCK();
    if (pem == NULL) {
        return ESP_ERR_NOT_FOUND;
    }

    mbedtls_x509_crt crt;
    mbedtls_x509_crt_init(&crt);
    esp_err_t err = ESP_FAIL;
    if (mbedtls_x509_crt_parse(&crt, (const unsigned char *)pem, strlen(pem) + 1) == 0) {
        mbedtls_x509_dn_gets(out->subject, sizeof(out->subject), &crt.subject);
        mbedtls_x509_dn_gets(out->issuer, sizeof(out->issuer), &crt.issuer);
        const mbedtls_x509_time *t = &crt.valid_to;
        snprintf(out->not_after, sizeof(out->not_after), "%04d-%02d-%02dT%02d:%02d:%02dZ", t->year, t->mon, t->day,
                 t->hour, t->min, t->sec);
        uint8_t hash[32];
        size_t hash_len = 0;
        if (psa_hash_compute(PSA_ALG_SHA_256, crt.raw.p, crt.raw.len, hash, sizeof(hash), &hash_len) == PSA_SUCCESS) {
            for (size_t i = 0; i < hash_len; i++) {
                snprintf(out->fingerprint + 2 * i, 3, "%02x", hash[i]);
            }
        }
        err = ESP_OK;
    }
    mbedtls_x509_crt_free(&crt);
    free(pem);
    return err;
}

/* ---- init -------------------------------------------------------------------------------------------------- */

esp_err_t identity_init(void)
{
    s_lock = xSemaphoreCreateMutex();
    ESP_RETURN_ON_FALSE(s_lock, ESP_ERR_NO_MEM, TAG, "mutex");

    identity_t id;
    ESP_RETURN_ON_ERROR(identity_load(&id), TAG, "load");
    ESP_LOGI(TAG, "device key: %s, AWS IoT: %s", id.key ? "present" : "none yet",
             identity_has_cloud(&id) ? id.thing : "not set up");
    identity_free(&id);
    return ESP_OK;
}
