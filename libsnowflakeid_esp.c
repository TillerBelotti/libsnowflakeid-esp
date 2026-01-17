#include <inttypes.h>

#include <esp_check.h>
#include <esp_heap_caps.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <pthread.h>

#include "snowflakeid.h"
#include "snowflakeid_priv.h"

static uint64_t get_current_time_ms(const uint64_t *offset_time_ms);
static uint64_t get_next_time_ms(const uint64_t *not_until_time_ms, const uint64_t *offset_time_ms);

static const char *TAG = "libsnowflakeid";

static uint64_t get_current_time_ms(const uint64_t *const offset_time_ms)
{
    struct timeval tv = {0};
    gettimeofday(&tv, NULL);

    return ((tv.tv_sec * 1000) + (tv.tv_usec / 1000) - *offset_time_ms);
}

static uint64_t get_next_time_ms(const uint64_t *const not_until_time_ms, const uint64_t *const offset_time_ms)
{
    uint64_t current_time_ms;

    do {
        current_time_ms = get_current_time_ms(offset_time_ms);
    } while (*not_until_time_ms == current_time_ms);

    return current_time_ms;
}

void snowflakeid_destroy(libsnowflakeid_handle_t handle)
{
    if (handle == NULL) return;

    if (handle->internal_lock != 0) SNOWFLAKEID_LOCK_DESTROY(handle->internal_lock);

    heap_caps_free(handle);
}

libsnowflakeid_handle_t snowflakeid_initialize(const uint8_t datacenter_id, const uint8_t worker_id,
                                               const uint64_t offset_time_ms, esp_err_t *status_out)
{
    esp_err_t ret = ESP_OK;
    libsnowflakeid_esp_t *handle = NULL;

    ESP_GOTO_ON_FALSE(datacenter_id < 32,   ESP_ERR_INVALID_ARG, end, TAG, "datacenter_id out of range");
    ESP_GOTO_ON_FALSE(worker_id < 32,       ESP_ERR_INVALID_ARG, end, TAG, "worker_id out of range");

    struct timeval tv = {0};
    ESP_GOTO_ON_FALSE(gettimeofday(&tv, NULL) == 0, ESP_ERR_INVALID_STATE, end, TAG, "gettimeofday failed");
    uint64_t now_ms = ((uint64_t)tv.tv_sec * 1000) + ((uint64_t)tv.tv_usec / 1000);
    ESP_GOTO_ON_FALSE(offset_time_ms <= now_ms, ESP_ERR_INVALID_ARG, end, TAG, "offset_time_ms in the future");

    // Allocate a new ctx instance
    handle = heap_caps_malloc(sizeof(libsnowflakeid_esp_t), MALLOC_CAP_DEFAULT);
    ESP_GOTO_ON_FALSE(handle != NULL, ESP_ERR_NO_MEM, end, TAG, "memory allocation failed");

    // Initialize internal lock
    const int ret_lock = pthread_mutex_init(&handle->internal_lock, NULL);
    ESP_GOTO_ON_FALSE(ret_lock == 0, ESP_ERR_INVALID_STATE, end, TAG, "lock initialization failed");

    // Configure ctx
    handle->last_time_ms    = 0;
    handle->offset_time_ms  = offset_time_ms;
    handle->datacenter_id   = datacenter_id;
    handle->worker_id       = worker_id;
    handle->sequence_number = 0;

end:

    if (status_out != NULL) *status_out = ret;

    if (ret != ESP_OK && handle != NULL)
    {
        if (handle->internal_lock != 0) SNOWFLAKEID_LOCK_DESTROY(handle->internal_lock);
        heap_caps_free(handle);
        handle = NULL;
    }

    return handle;
}

snowflakeid_t snowflakeid_generate(libsnowflakeid_handle_t handle, esp_err_t *status_out)
{
    esp_err_t ret = ESP_OK;
    snowflakeid_t new_id = 0;

    ESP_GOTO_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, end, TAG, "handle is NULL");

    uint64_t current_time_ms = get_current_time_ms(&handle->offset_time_ms);

    SNOWFLAKEID_LOCK_WAIT_FOR(handle->internal_lock);
    while (current_time_ms < handle->last_time_ms)
    {
        // Clock moved backwards - wait for it to catch up
        const uint64_t drift_ms = handle->last_time_ms - current_time_ms;
        ESP_LOGW(TAG, "Clock moved backwards by %" PRIu64 "ms, waiting", drift_ms);
        SNOWFLAKEID_LOCK_RELEASE(handle->internal_lock);
        vTaskDelay(pdMS_TO_TICKS(drift_ms) + 1);
        SNOWFLAKEID_LOCK_WAIT_FOR(handle->internal_lock);
        current_time_ms = get_current_time_ms(&handle->offset_time_ms);
    }

    if (handle->last_time_ms == current_time_ms)
    {
        handle->sequence_number = (handle->sequence_number + 1) % SNOWFLAKEID_SEQUENCE_MAX;
        if (handle->sequence_number == 0)
        {
            current_time_ms = get_next_time_ms(&handle->last_time_ms, &handle->offset_time_ms);
        }
    }
    else
    {
        handle->sequence_number = 0;
    }
    handle->last_time_ms = current_time_ms;
    const uint16_t sequence_number = handle->sequence_number;
    SNOWFLAKEID_LOCK_RELEASE(handle->internal_lock);

    new_id = ((current_time_ms << SNOWFLAKEID_TIMESTAMP_SHIFT)
            | (handle->datacenter_id << SNOWFLAKEID_DATACENTER_ID_SHIFT)
            | (handle->worker_id << SNOWFLAKEID_WORKER_ID_SHIFT)
            | sequence_number);

    ESP_LOGV(TAG, "New ts: %" PRIX64, current_time_ms);
    ESP_LOGV(TAG, "New dc: %02X", handle->datacenter_id);
    ESP_LOGV(TAG, "New work: %02X", handle->worker_id);
    ESP_LOGV(TAG, "New seq: %04X", sequence_number);
    ESP_LOGD(TAG, "New ID: %" PRIX64, new_id);

end:
    if (status_out != NULL) *status_out = ret;

    return new_id;
}

uint64_t snowflakeid_get_timestamp(const snowflakeid_t* id)
{
    if (id == NULL) return 0;
    return *id >> SNOWFLAKEID_TIMESTAMP_SHIFT;
}

uint8_t snowflakeid_get_datacenter_id(const snowflakeid_t* id)
{
    if (id == NULL) return 0;
    return (*id & 0x3E0000) >> SNOWFLAKEID_DATACENTER_ID_SHIFT;
}

uint8_t snowflakeid_get_worker_id(const snowflakeid_t* id)
{
    if (id == NULL) return 0;
    return (*id & 0x1F000) >> SNOWFLAKEID_WORKER_ID_SHIFT;
}

uint16_t snowflakeid_get_sequence(const snowflakeid_t* id)
{
    if (id == NULL) return 0;
    return *id & 0xFFF;
}

uint8_t snowflakeid_get_max_datacenter_id(void)
{
    return SNOWFLAKEID_DATACENTER_ID_MAX;
}

uint8_t snowflakeid_get_max_worker_id(void)
{
    return SNOWFLAKEID_WORKER_ID_MAX;
}

uint16_t snowflakeid_get_max_sequence(void)
{
    return SNOWFLAKEID_SEQUENCE_MAX;
}
