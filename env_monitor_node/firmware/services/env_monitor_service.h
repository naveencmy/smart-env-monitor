/* Service contract for sensor sampling, state updates, and alerts. */
#ifndef ENV_MONITOR_SERVICE_H
#define ENV_MONITOR_SERVICE_H

#include <stdbool.h>

/**
 * @brief Initializes monitor tasks and onboard interfaces.
 * @return True when platform initialization and task creation succeed.
 */
bool env_monitor_service_start(void);

#endif /* ENV_MONITOR_SERVICE_H */
