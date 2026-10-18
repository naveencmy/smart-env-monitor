/* Pure threshold classifier for environmental monitor readings. */
#include "env_state.h"

#include "app_config.h"

/** Applies critical-before-warning precedence for overlapping threshold inputs. */
env_state_t evaluate_env_state(float temp_c, uint16_t esi, bool button_is_pressed)
{
    env_state_t state;

    if ((temp_c > APP_TEMP_CRITICAL_MIN_EXCLUSIVE_C) ||
        (esi > APP_ESI_CRITICAL_MIN_EXCLUSIVE) || button_is_pressed)
    {
        state = STATE_CRITICAL;
    }
    else if (((temp_c >= APP_TEMP_WARNING_MIN_C) &&
              (temp_c <= APP_TEMP_WARNING_MAX_C)) ||
             ((esi >= APP_ESI_WARNING_MIN) && (esi <= APP_ESI_WARNING_MAX)))
    {
        state = STATE_WARNING;
    }
    else
    {
        state = STATE_NORMAL;
    }

    return state;
}

/** Returns a stable uppercase label for each supported environmental state. */
const char *env_state_name(env_state_t state)
{
    const char *name = "INVALID";

    switch (state)
    {
        case STATE_NORMAL:
            name = "NORMAL";
            break;
        case STATE_WARNING:
            name = "WARNING";
            break;
        case STATE_CRITICAL:
            name = "CRITICAL";
            break;
        default:
            break;
    }

    return name;
}
