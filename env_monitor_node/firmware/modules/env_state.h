/* Portable environmental state types and threshold-classifier API. */
#ifndef ENV_STATE_H
#define ENV_STATE_H

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    STATE_NORMAL = 0,
    STATE_WARNING,
    STATE_CRITICAL
} env_state_t;

/**
 * @brief Classifies temperature, ESI, and button state using PRD thresholds.
 * @param[in] temp_c Synthetic estimated temperature in degrees Celsius.
 * @param[in] esi Environmental Stress Index in the range 0 through 500.
 * @param[in] button_is_pressed True when the active-low button is held.
 * @return STATE_CRITICAL if a critical threshold or button override applies;
 *         otherwise STATE_WARNING if either warning band applies; otherwise STATE_NORMAL.
 */
env_state_t evaluate_env_state(float temp_c, uint16_t esi, bool button_is_pressed);

/**
 * @brief Returns the stable uppercase text representation of a state.
 * @param[in] state State value to convert.
 * @return Static NORMAL, WARNING, CRITICAL, or INVALID text.
 */
const char *env_state_name(env_state_t state);

#endif /* ENV_STATE_H */
