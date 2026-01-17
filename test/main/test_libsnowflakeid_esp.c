#include <inttypes.h>
#include <pthread.h>
#include <stdlib.h>
#include <sys/time.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "esp_log.h"
#include "unity.h"
#include "unity_fixture.h"
#include "memory_checks.h"

#include "snowflakeid.h"

#define TEST_DATACENTER_ID      5
#define TEST_WORKER_ID          1
#define TEST_OFFSET_TIME_MS     0
#define PERF_TEST_COUNT         10000

typedef struct thread_test_args {
    libsnowflakeid_handle_t handle;
    snowflakeid_t *ids;
    int count;
} thread_test_args_t;

static uint64_t get_current_time_ms(void);
static int set_current_time_ms(uint64_t time_ms);
static void *warmup_thread_func(void *arg);

static const char *TAG = "test_libsnowflakeid";

static void *warmup_thread_func(void *arg)
{
    (void)arg;
    vTaskDelay(1);
    return NULL;
}

static uint64_t get_current_time_ms(void)
{
    struct timeval tv = {0};
    gettimeofday(&tv, NULL);
    return ((tv.tv_sec * 1000) + (tv.tv_usec / 1000));
}

static int set_current_time_ms(const uint64_t time_ms)
{
    const struct timeval tv = {
        .tv_sec = (time_t)(time_ms / 1000),
        .tv_usec = (suseconds_t)((time_ms % 1000) * 1000)
    };

    return settimeofday(&tv, NULL);
}

static void *thread_generate_ids(void *arg)
{
    thread_test_args_t *args = (thread_test_args_t *)arg;

    for (int i = 0; i < args->count; i++) {
        args->ids[i] = snowflakeid_generate(args->handle, NULL);
    }

    return NULL;
}

TEST_GROUP(libsnowflakeid_esp);

TEST_SETUP(libsnowflakeid_esp)
{
    printf("\n");
    /*
     * Warmup: Force lazy initialization of subsystems before memory
     * tracking starts to avoid false positive memory leak reports.
     */
    static bool warmed_up = false;
    if (!warmed_up) {
        struct timeval tv;
        gettimeofday(&tv, NULL);  // force time subsystem init
        ESP_LOGI(TAG, "Initializing runtime (warming-up)");  // force log subsystem init
        vTaskDelay(1);  // force FreeRTOS task subsystem init
        pthread_t dummy_thread;
        pthread_create(&dummy_thread, NULL, warmup_thread_func, NULL);
        pthread_join(dummy_thread, NULL);  // force pthread subsystem init
        warmed_up = true;
    }
    test_utils_record_free_mem();
    TEST_ESP_OK(test_utils_set_leak_level(0, ESP_LEAK_TYPE_CRITICAL, ESP_COMP_LEAK_GENERAL));
}

TEST_TEAR_DOWN(libsnowflakeid_esp)
{
    test_utils_finish_and_evaluate_leaks(0, 0);
}

TEST(libsnowflakeid_esp, destroy_null_handle)
{
    snowflakeid_destroy(NULL);
    TEST_PASS();
}

TEST(libsnowflakeid_esp, generate_null_handle)
{
    esp_err_t err = ESP_OK;
    const snowflakeid_t id = snowflakeid_generate(NULL, &err);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);
    TEST_ASSERT_EQUAL(0, id);
}

TEST(libsnowflakeid_esp, getters_null_id)
{
    TEST_ASSERT_EQUAL(0, snowflakeid_get_timestamp(NULL));
    TEST_ASSERT_EQUAL(0, snowflakeid_get_datacenter_id(NULL));
    TEST_ASSERT_EQUAL(0, snowflakeid_get_worker_id(NULL));
    TEST_ASSERT_EQUAL(0, snowflakeid_get_sequence(NULL));
}

TEST(libsnowflakeid_esp, init_destroy)
{
    esp_err_t err = ESP_OK;
    libsnowflakeid_handle_t handle = snowflakeid_initialize(
        TEST_DATACENTER_ID, TEST_WORKER_ID, TEST_OFFSET_TIME_MS, &err);

    TEST_ESP_OK(err);
    TEST_ASSERT_NOT_NULL(handle);

    snowflakeid_destroy(handle);
}

TEST(libsnowflakeid_esp, init_null_status_out)
{
    libsnowflakeid_handle_t handle = snowflakeid_initialize(
        TEST_DATACENTER_ID, TEST_WORKER_ID, TEST_OFFSET_TIME_MS, NULL);

    TEST_ASSERT_NOT_NULL(handle);

    snowflakeid_destroy(handle);
}

TEST(libsnowflakeid_esp, get_components)
{
    esp_err_t err = ESP_OK;
    libsnowflakeid_handle_t handle = snowflakeid_initialize(
        TEST_DATACENTER_ID, TEST_WORKER_ID, TEST_OFFSET_TIME_MS, &err);
    TEST_ESP_OK(err);
    TEST_ASSERT_NOT_NULL(handle);

    snowflakeid_t id = snowflakeid_generate(handle, NULL);

    const uint64_t timestamp = snowflakeid_get_timestamp(&id);
    const uint8_t datacenter_id = snowflakeid_get_datacenter_id(&id);
    const uint8_t worker_id = snowflakeid_get_worker_id(&id);
    const uint16_t sequence = snowflakeid_get_sequence(&id);

    TEST_ASSERT_NOT_EQUAL(0, timestamp);
    TEST_ASSERT_EQUAL(TEST_DATACENTER_ID, datacenter_id);
    TEST_ASSERT_EQUAL(TEST_WORKER_ID, worker_id);
    TEST_ASSERT_TRUE(sequence <= snowflakeid_get_max_sequence());

    snowflakeid_destroy(handle);
}

TEST(libsnowflakeid_esp, init_invalid_datacenter_id)
{
    esp_err_t err = ESP_OK;
    libsnowflakeid_handle_t handle = snowflakeid_initialize(
        snowflakeid_get_max_datacenter_id() + 1, TEST_WORKER_ID, TEST_OFFSET_TIME_MS, &err);

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);
    TEST_ASSERT_NULL(handle);
}

TEST(libsnowflakeid_esp, init_invalid_worker_id)
{
    esp_err_t err = ESP_OK;
    libsnowflakeid_handle_t handle = snowflakeid_initialize(
        TEST_DATACENTER_ID, snowflakeid_get_max_worker_id() + 1, TEST_OFFSET_TIME_MS, &err);

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);
    TEST_ASSERT_NULL(handle);
}

TEST(libsnowflakeid_esp, init_invalid_offset_future)
{
    esp_err_t err = ESP_OK;
    const uint64_t original_time_ms = get_current_time_ms();
    const uint64_t future_time_ms = original_time_ms + 2000000;

    TEST_ASSERT_EQUAL(0, set_current_time_ms(future_time_ms));

    const uint64_t invalid_offset = future_time_ms + 60000;
    libsnowflakeid_handle_t handle = snowflakeid_initialize(
        TEST_DATACENTER_ID, TEST_WORKER_ID, invalid_offset, &err);

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);
    TEST_ASSERT_NULL(handle);
    TEST_ASSERT_EQUAL(0, set_current_time_ms(original_time_ms));
}

TEST(libsnowflakeid_esp, init_offset_equals_current_time)
{
    esp_err_t err = ESP_OK;
    const uint64_t current_time_ms = get_current_time_ms();

    libsnowflakeid_handle_t handle = snowflakeid_initialize(
        TEST_DATACENTER_ID, TEST_WORKER_ID, current_time_ms, &err);
    TEST_ESP_OK(err);
    TEST_ASSERT_NOT_NULL(handle);

    const snowflakeid_t id = snowflakeid_generate(handle, &err);
    TEST_ESP_OK(err);
    TEST_ASSERT_NOT_EQUAL(0, id);

    // With offset equal to current time, timestamp should be very small (near 0)
    const uint64_t timestamp = snowflakeid_get_timestamp(&id);
    TEST_ASSERT_TRUE(timestamp <= 1);

    snowflakeid_destroy(handle);
}

TEST(libsnowflakeid_esp, init_min_valid_values)
{
    esp_err_t err = ESP_OK;
    libsnowflakeid_handle_t handle = snowflakeid_initialize(0, 0, 0, &err);

    TEST_ESP_OK(err);
    TEST_ASSERT_NOT_NULL(handle);

    const snowflakeid_t id = snowflakeid_generate(handle, &err);
    TEST_ESP_OK(err);
    TEST_ASSERT_NOT_EQUAL(0, id);
    TEST_ASSERT_EQUAL(0, snowflakeid_get_datacenter_id(&id));
    TEST_ASSERT_EQUAL(0, snowflakeid_get_worker_id(&id));

    snowflakeid_destroy(handle);
}

TEST(libsnowflakeid_esp, init_max_valid_values)
{
    esp_err_t err = ESP_OK;
    libsnowflakeid_handle_t handle = snowflakeid_initialize(
        snowflakeid_get_max_datacenter_id(), snowflakeid_get_max_worker_id(), 0, &err);

    TEST_ESP_OK(err);
    TEST_ASSERT_NOT_NULL(handle);

    const snowflakeid_t id = snowflakeid_generate(handle, &err);
    TEST_ESP_OK(err);
    TEST_ASSERT_NOT_EQUAL(0, id);
    TEST_ASSERT_EQUAL(snowflakeid_get_max_datacenter_id(), snowflakeid_get_datacenter_id(&id));
    TEST_ASSERT_EQUAL(snowflakeid_get_max_worker_id(), snowflakeid_get_worker_id(&id));

    snowflakeid_destroy(handle);
}

TEST(libsnowflakeid_esp, generate_single_id)
{
    esp_err_t err = ESP_OK;
    libsnowflakeid_handle_t handle = snowflakeid_initialize(
        TEST_DATACENTER_ID, TEST_WORKER_ID, TEST_OFFSET_TIME_MS, &err);
    TEST_ESP_OK(err);
    TEST_ASSERT_NOT_NULL(handle);

    const snowflakeid_t id = snowflakeid_generate(handle, &err);
    TEST_ESP_OK(err);
    TEST_ASSERT_NOT_EQUAL(0, id);

    TEST_ASSERT_EQUAL(TEST_DATACENTER_ID, snowflakeid_get_datacenter_id(&id));
    TEST_ASSERT_EQUAL(TEST_WORKER_ID, snowflakeid_get_worker_id(&id));
    TEST_ASSERT_EQUAL(0, snowflakeid_get_sequence(&id));

    ESP_LOGI(TAG, "Generated ID: %" PRIX64 " (ts: %" PRIX64 ", dc: %u, wk: %u, seq: %u)",
             id,
             snowflakeid_get_timestamp(&id),
             snowflakeid_get_datacenter_id(&id),
             snowflakeid_get_worker_id(&id),
             snowflakeid_get_sequence(&id));

    snowflakeid_destroy(handle);
}

TEST(libsnowflakeid_esp, generate_null_status_out)
{
    libsnowflakeid_handle_t handle = snowflakeid_initialize(
        TEST_DATACENTER_ID, TEST_WORKER_ID, TEST_OFFSET_TIME_MS, NULL);
    TEST_ASSERT_NOT_NULL(handle);

    // Generate with NULL status_out - should work fine
    const snowflakeid_t id = snowflakeid_generate(handle, NULL);
    TEST_ASSERT_NOT_EQUAL(0, id);
    TEST_ASSERT_EQUAL(TEST_DATACENTER_ID, snowflakeid_get_datacenter_id(&id));
    TEST_ASSERT_EQUAL(TEST_WORKER_ID, snowflakeid_get_worker_id(&id));

    snowflakeid_destroy(handle);
}

TEST(libsnowflakeid_esp, generate_unique_ids)
{
    esp_err_t err = ESP_OK;
    libsnowflakeid_handle_t handle = snowflakeid_initialize(
        TEST_DATACENTER_ID, TEST_WORKER_ID, TEST_OFFSET_TIME_MS, &err);
    TEST_ESP_OK(err);
    TEST_ASSERT_NOT_NULL(handle);

    const snowflakeid_t id1 = snowflakeid_generate(handle, NULL);
    const snowflakeid_t id2 = snowflakeid_generate(handle, NULL);
    const snowflakeid_t id3 = snowflakeid_generate(handle, NULL);

    TEST_ASSERT_NOT_EQUAL(id1, id2);
    TEST_ASSERT_NOT_EQUAL(id2, id3);
    TEST_ASSERT_NOT_EQUAL(id1, id3);

    snowflakeid_destroy(handle);
}

TEST(libsnowflakeid_esp, ids_monotonic_increasing)
{
    esp_err_t err = ESP_OK;
    libsnowflakeid_handle_t handle = snowflakeid_initialize(
        TEST_DATACENTER_ID, TEST_WORKER_ID, TEST_OFFSET_TIME_MS, &err);
    TEST_ESP_OK(err);
    TEST_ASSERT_NOT_NULL(handle);

    snowflakeid_t prev_id = snowflakeid_generate(handle, NULL);
    for (int i = 0; i < 100; i++)
    {
        const snowflakeid_t curr_id = snowflakeid_generate(handle, NULL);
        TEST_ASSERT_TRUE(curr_id > prev_id);
        prev_id = curr_id;
    }

    snowflakeid_destroy(handle);
}

TEST(libsnowflakeid_esp, generate_with_offset)
{
    esp_err_t err = ESP_OK;
    const uint64_t offset = 1000000;
    const uint64_t original_time_ms = get_current_time_ms();
    const uint64_t future_time_ms = original_time_ms + (offset * 2);

    TEST_ASSERT_EQUAL(0, set_current_time_ms(future_time_ms));

    libsnowflakeid_handle_t handle_no_offset = snowflakeid_initialize(
        TEST_DATACENTER_ID, TEST_WORKER_ID, 0, &err);
    TEST_ESP_OK(err);

    libsnowflakeid_handle_t handle_with_offset = snowflakeid_initialize(
        TEST_DATACENTER_ID, TEST_WORKER_ID, offset, &err);
    TEST_ESP_OK(err);

    const snowflakeid_t id_no_offset = snowflakeid_generate(handle_no_offset, NULL);
    const snowflakeid_t id_with_offset = snowflakeid_generate(handle_with_offset, NULL);

    const uint64_t ts_no_offset = snowflakeid_get_timestamp(&id_no_offset);
    const uint64_t ts_with_offset = snowflakeid_get_timestamp(&id_with_offset);

    TEST_ASSERT_TRUE(ts_with_offset < ts_no_offset);

    snowflakeid_destroy(handle_no_offset);
    snowflakeid_destroy(handle_with_offset);
    TEST_ASSERT_EQUAL(0, set_current_time_ms(original_time_ms));
}

TEST(libsnowflakeid_esp, sequence_overflow)
{
    esp_err_t err = ESP_OK;
    libsnowflakeid_handle_t handle = snowflakeid_initialize(
        TEST_DATACENTER_ID, TEST_WORKER_ID, TEST_OFFSET_TIME_MS, &err);
    TEST_ESP_OK(err);
    TEST_ASSERT_NOT_NULL(handle);

    const uint16_t max_seq = snowflakeid_get_max_sequence();

    const snowflakeid_t first_id = snowflakeid_generate(handle, NULL);
    snowflakeid_t last_id = first_id;
    const uint64_t first_timestamp = snowflakeid_get_timestamp(&first_id);

    // Generate enough IDs to overflow the sequence in the same millisecond
    // This will force the generator to wait for the next millisecond
    for (int i = 0; i < max_seq + 10; i++)
    {
        last_id = snowflakeid_generate(handle, NULL);
    }

    const uint64_t last_timestamp = snowflakeid_get_timestamp(&last_id);

    // After overflow, timestamp should have advanced
    TEST_ASSERT_TRUE(last_timestamp > first_timestamp);

    snowflakeid_destroy(handle);
}

TEST(libsnowflakeid_esp, sequence_boundary)
{
    esp_err_t err = ESP_OK;
    libsnowflakeid_handle_t handle = snowflakeid_initialize(
        TEST_DATACENTER_ID, TEST_WORKER_ID, TEST_OFFSET_TIME_MS, &err);
    TEST_ESP_OK(err);
    TEST_ASSERT_NOT_NULL(handle);

    const uint16_t max_seq = snowflakeid_get_max_sequence();

    snowflakeid_t prev_id = snowflakeid_generate(handle, NULL);
    uint64_t prev_ts = snowflakeid_get_timestamp(&prev_id);
    uint16_t prev_seq = snowflakeid_get_sequence(&prev_id);
    bool overflow_detected = false;

    // Generate enough IDs to guarantee at least one overflow
    for (uint32_t i = 0; i < (uint32_t)(max_seq + 1) * 2; i++)
    {
        const snowflakeid_t id = snowflakeid_generate(handle, NULL);
        TEST_ASSERT_TRUE(id > prev_id);

        const uint64_t ts = snowflakeid_get_timestamp(&id);
        const uint16_t seq = snowflakeid_get_sequence(&id);

        if (ts > prev_ts)
        {
            // Timestamp advanced - sequence must have reset to 0
            TEST_ASSERT_EQUAL_UINT16(0, seq);
            overflow_detected = true;
        }
        else
        {
            // The same timestamp-sequence must have incremented
            TEST_ASSERT_EQUAL_UINT16(prev_seq + 1, seq);
        }

        prev_id = id;
        prev_ts = ts;
        prev_seq = seq;
    }

    TEST_ASSERT_TRUE(overflow_detected);

    snowflakeid_destroy(handle);
}

TEST(libsnowflakeid_esp, clock_backwards)
{
    esp_err_t err = ESP_OK;
    const uint64_t original_time_ms = get_current_time_ms();

    libsnowflakeid_handle_t handle = snowflakeid_initialize(
        TEST_DATACENTER_ID, TEST_WORKER_ID, TEST_OFFSET_TIME_MS, &err);
    TEST_ESP_OK(err);
    TEST_ASSERT_NOT_NULL(handle);

    // Generate first ID
    const snowflakeid_t id1 = snowflakeid_generate(handle, NULL);
    TEST_ASSERT_NOT_EQUAL(0, id1);

    // Move the clock backwards by 100ms
    const uint64_t past_time_ms = original_time_ms - 100;
    TEST_ASSERT_EQUAL(0, set_current_time_ms(past_time_ms));

    // Generate second ID - should still work and maintain monotonic property
    const snowflakeid_t id2 = snowflakeid_generate(handle, NULL);
    TEST_ASSERT_NOT_EQUAL(0, id2);

    // IDs should still be monotonically increasing despite the clock going backwards
    TEST_ASSERT_TRUE(id2 > id1);

    // Restore the original time
    TEST_ASSERT_EQUAL(0, set_current_time_ms(original_time_ms));

    snowflakeid_destroy(handle);
}

TEST(libsnowflakeid_esp, thread_safety_concurrent)
{
    esp_err_t err = ESP_OK;
    libsnowflakeid_handle_t handle = snowflakeid_initialize(
        TEST_DATACENTER_ID, TEST_WORKER_ID, TEST_OFFSET_TIME_MS, &err);
    TEST_ESP_OK(err);
    TEST_ASSERT_NOT_NULL(handle);

    const int num_threads = 3;
    const int ids_per_thread = 100;

    pthread_t threads[num_threads];
    thread_test_args_t args[num_threads];
    snowflakeid_t *all_ids[num_threads];

    // Allocate memory for IDs
    for (int i = 0; i < num_threads; i++) {
        all_ids[i] = malloc(ids_per_thread * sizeof(snowflakeid_t));
        TEST_ASSERT_NOT_NULL(all_ids[i]);

        args[i].handle = handle;
        args[i].ids = all_ids[i];
        args[i].count = ids_per_thread;
    }

    // Create threads
    for (int i = 0; i < num_threads; i++) {
        TEST_ASSERT_EQUAL(0, pthread_create(&threads[i], NULL, thread_generate_ids, &args[i]));
    }

    // Wait for all threads to complete
    for (int i = 0; i < num_threads; i++) {
        TEST_ASSERT_EQUAL(0, pthread_join(threads[i], NULL));
    }

    // Verify all IDs are unique
    for (int i = 0; i < num_threads; i++) {
        for (int j = 0; j < ids_per_thread; j++) {
            TEST_ASSERT_NOT_EQUAL(0, all_ids[i][j]);

            // Check against all other IDs
            for (int k = 0; k < num_threads; k++) {
                for (int l = 0; l < ids_per_thread; l++) {
                    if (i == k && j == l) continue;
                    TEST_ASSERT_NOT_EQUAL(all_ids[i][j], all_ids[k][l]);
                }
            }
        }
    }

    // Clean up
    for (int i = 0; i < num_threads; i++) {
        free(all_ids[i]);
    }

    snowflakeid_destroy(handle);
}

TEST(libsnowflakeid_esp, generate_performance)
{
    libsnowflakeid_handle_t handle = snowflakeid_initialize(
        TEST_DATACENTER_ID, TEST_WORKER_ID, TEST_OFFSET_TIME_MS, NULL);
    TEST_ASSERT_NOT_NULL(handle);

    const uint64_t time_start = get_current_time_ms();
    for (int i = 0; i < PERF_TEST_COUNT; i++) {
        snowflakeid_generate(handle, NULL);
    }
    const uint64_t time_end = get_current_time_ms();

    ESP_LOGI(TAG, "Performance: %d IDs generated in %" PRIu64 "ms",
             PERF_TEST_COUNT, time_end - time_start);

    snowflakeid_destroy(handle);
}

TEST_GROUP_RUNNER(libsnowflakeid_esp)
{
    // Null/error handling (no dependencies)
    RUN_TEST_CASE(libsnowflakeid_esp, destroy_null_handle);
    RUN_TEST_CASE(libsnowflakeid_esp, generate_null_handle);
    RUN_TEST_CASE(libsnowflakeid_esp, getters_null_id);

    // Basic initialization (no getters needed)
    RUN_TEST_CASE(libsnowflakeid_esp, init_destroy);
    RUN_TEST_CASE(libsnowflakeid_esp, init_null_status_out);

    // Getters (depends on init + generate, needed by tests below)
    RUN_TEST_CASE(libsnowflakeid_esp, get_components);

    // Initialization validation (uses getters)
    RUN_TEST_CASE(libsnowflakeid_esp, init_invalid_datacenter_id);
    RUN_TEST_CASE(libsnowflakeid_esp, init_invalid_worker_id);
    RUN_TEST_CASE(libsnowflakeid_esp, init_invalid_offset_future);
    RUN_TEST_CASE(libsnowflakeid_esp, init_offset_equals_current_time);
    RUN_TEST_CASE(libsnowflakeid_esp, init_min_valid_values);
    RUN_TEST_CASE(libsnowflakeid_esp, init_max_valid_values);

    // Basic generation (uses getters)
    RUN_TEST_CASE(libsnowflakeid_esp, generate_single_id);
    RUN_TEST_CASE(libsnowflakeid_esp, generate_null_status_out);

    // Multiple generation
    RUN_TEST_CASE(libsnowflakeid_esp, generate_unique_ids);
    RUN_TEST_CASE(libsnowflakeid_esp, ids_monotonic_increasing);
    RUN_TEST_CASE(libsnowflakeid_esp, generate_with_offset);

    // Sequence behavior
    RUN_TEST_CASE(libsnowflakeid_esp, sequence_overflow);
    RUN_TEST_CASE(libsnowflakeid_esp, sequence_boundary);

    // Edge cases
    RUN_TEST_CASE(libsnowflakeid_esp, clock_backwards);

    // Complex/stress tests
    RUN_TEST_CASE(libsnowflakeid_esp, thread_safety_concurrent);
    RUN_TEST_CASE(libsnowflakeid_esp, generate_performance);
}

void app_main(void)
{
    UNITY_MAIN(libsnowflakeid_esp);
}
