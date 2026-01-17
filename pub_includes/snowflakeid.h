#ifndef LIBSNOWFLAKEID_H
#define LIBSNOWFLAKEID_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct libsnowflakeid_esp *libsnowflakeid_handle_t;
typedef uint64_t snowflakeid_t;

/**
 * Destroys the given Snowflake ID generator context.
 *
 * @param handle [IN] The Snowflake ID generator context to destroy
 */
void snowflakeid_destroy(libsnowflakeid_handle_t handle);

/**
 * Initialize Snowflake ID generator context.
 *
 * @param datacenter_id  [IN] The Datacenter ID (Max. value = 31)
 * @param worker_id      [IN] The Worker ID (Max. value = 31)
 * @param offset_time_ms [IN] Offset in milliseconds to be subtracted from the real "current time".
 *                            Must be less than or equal to the current system time at initialization.
 * @param status_out     [OUT] The operation status (i.e.: ESP_OK)
 * @return Initialized Snowflake ID generator context, or NULL in case of error
 */
libsnowflakeid_handle_t snowflakeid_initialize(uint8_t datacenter_id,
                                               uint8_t worker_id,
                                               uint64_t offset_time_ms,
                                               esp_err_t *status_out);

/**
 * Get the next Snowflake ID.
 *
 * @param handle     [IN] The Snowflake ID generator context
 * @param status_out [OUT] The operation status (i.e.: ESP_OK), can be NULL
 * @return The generated Snowflake ID, or 0 in case of error
 */
snowflakeid_t snowflakeid_generate(libsnowflakeid_handle_t handle, esp_err_t *status_out);
    
/**
 * Returns the timestamp of the given Snowflake ID.
 *
 * @param id [IN] The Snowflake ID
 * @return The timestamp of the given Snowflake ID
 */
uint64_t snowflakeid_get_timestamp(const snowflakeid_t* id);
    
/**
 * Returns the datacenter ID of the given Snowflake ID.
 *
 * @param id [IN] The Snowflake ID
 * @return The datacenter ID of the given Snowflake ID
 */
uint8_t snowflakeid_get_datacenter_id(const snowflakeid_t* id);

/**
 * Returns the worker ID of the given Snowflake ID.
 *
 * @param id [IN] The Snowflake ID
 * @return The worker ID of the given Snowflake ID
 */
uint8_t snowflakeid_get_worker_id(const snowflakeid_t* id);

/**
 * Returns the sequence number of the given Snowflake ID.
 *
 * @param id [IN] The Snowflake ID
 * @return The sequence number of the given Snowflake ID
 */
uint16_t snowflakeid_get_sequence(const snowflakeid_t* id);

/**
 * Returns the maximum valid datacenter ID.
 *
 * @return Maximum datacenter ID (31)
 */
uint8_t snowflakeid_get_max_datacenter_id(void);

/**
 * Returns the maximum valid worker ID.
 *
 * @return Maximum worker ID (31)
 */
uint8_t snowflakeid_get_max_worker_id(void);

/**
 * Returns the maximum sequence number.
 *
 * @return Maximum sequence number (4095)
 */
uint16_t snowflakeid_get_max_sequence(void);

# ifdef __cplusplus
}
# endif

#endif //LIBSNOWFLAKEID_H
