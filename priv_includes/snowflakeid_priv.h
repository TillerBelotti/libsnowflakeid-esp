#ifndef LIBSNOWFLAKEID_PRIV_H
#define LIBSNOWFLAKEID_PRIV_H

#include <pthread.h>

#define SNOWFLAKEID_WORKER_ID_BITS         5L
#define SNOWFLAKEID_DATACENTER_ID_BITS     5L
#define SNOWFLAKEID_SEQUENCE_BITS          12L
#define SNOWFLAKEID_WORKER_ID_SHIFT        SNOWFLAKEID_SEQUENCE_BITS
#define SNOWFLAKEID_DATACENTER_ID_SHIFT    (SNOWFLAKEID_SEQUENCE_BITS + SNOWFLAKEID_WORKER_ID_BITS)
#define SNOWFLAKEID_TIMESTAMP_SHIFT        (SNOWFLAKEID_SEQUENCE_BITS + SNOWFLAKEID_WORKER_ID_BITS + SNOWFLAKEID_DATACENTER_ID_BITS)
#define SNOWFLAKEID_SEQUENCE_MAX           ((1 << SNOWFLAKEID_SEQUENCE_BITS) - 1)
#define SNOWFLAKEID_DATACENTER_ID_MAX      ((1 << SNOWFLAKEID_DATACENTER_ID_BITS) - 1)
#define SNOWFLAKEID_WORKER_ID_MAX          ((1 << SNOWFLAKEID_WORKER_ID_BITS) - 1)

#define SNOWFLAKEID_LOCK_WAIT_FOR(lock_instance)  pthread_mutex_lock(&lock_instance)
#define SNOWFLAKEID_LOCK_RELEASE(lock_instance)   pthread_mutex_unlock(&lock_instance)
#define SNOWFLAKEID_LOCK_DESTROY(lock_instance)   pthread_mutex_destroy(&lock_instance)

#ifdef __cplusplus
extern "C"
{
#endif

typedef pthread_mutex_t snowflakeid_lock_t;

/**
 * Snowflake ID generator context.
 */
typedef struct libsnowflakeid_esp {
    uint64_t           last_time_ms;
    uint64_t           offset_time_ms;
    uint8_t            datacenter_id;
    uint8_t            worker_id;
    uint16_t           sequence_number;
    snowflakeid_lock_t internal_lock;
} libsnowflakeid_esp_t;

# ifdef __cplusplus
}
# endif

#endif //LIBSNOWFLAKEID_PRIV_H
