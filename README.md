# libsnowflakeid-esp

Snowflake ID generator component for ESP-IDF.

Based on [thibaultmeyer/libsnowflakeid](https://github.com/thibaultmeyer/libsnowflakeid).

## Installation

Add to your project’s `idf_component.yml`:

```yaml
dependencies:
  libsnowflakeid-esp:
    git: https://github.com/TillerBelotti/libsnowflakeid-esp.git
```

Or clone directly into your `components/` directory.

## Usage

```c
#include <inttypes.h>
#include "snowflakeid.h"

void app_main(void)
{
    esp_err_t err;

    // Initialize generator (datacenter_id=1, worker_id=1, offset=0)
    // Offset must be <= current system time at initialization.
    libsnowflakeid_handle_t handle = snowflakeid_initialize(1, 1, 0, &err);
    if (err != ESP_OK) {
        // Handle error
        return;
    }

    // Generate ID
    snowflakeid_t id = snowflakeid_generate(handle, &err);
    if (err == ESP_OK) {
        printf("Generated ID: %" PRIu64 "\n", id);
        printf("  Timestamp: %" PRIu64 "\n", snowflakeid_get_timestamp(&id));
        printf("  Datacenter: %u\n", snowflakeid_get_datacenter_id(&id));
        printf("  Worker: %u\n", snowflakeid_get_worker_id(&id));
        printf("  Sequence: %u\n", snowflakeid_get_sequence(&id));
    }

    // Cleanup
    snowflakeid_destroy(handle);
}
```

## API

| Function                            | Description                                                             |
|-------------------------------------|-------------------------------------------------------------------------|
| `snowflakeid_initialize`            | Create generator (datacenter 0-31, worker 0-31, offset <= current time) |
| `snowflakeid_generate`              | Generate unique ID                                                      |
| `snowflakeid_destroy`               | Free resources                                                          |
| `snowflakeid_get_timestamp`         | Extract timestamp from ID                                               |
| `snowflakeid_get_datacenter_id`     | Extract datacenter ID                                                   |
| `snowflakeid_get_worker_id`         | Extract worker ID                                                       |
| `snowflakeid_get_sequence`          | Extract sequence number                                                 |
| `snowflakeid_get_max_datacenter_id` | Get max datacenter ID (31)                                              |
| `snowflakeid_get_max_worker_id`     | Get max worker ID (31)                                                  |
| `snowflakeid_get_max_sequence`      | Get max sequence number (4095)                                          |

## License

MIT License - See [LICENSE](LICENSE)
