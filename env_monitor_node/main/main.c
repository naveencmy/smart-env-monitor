#include <stdio.h>
#include "firmgen_telemetry.h"
#include "firmgen_proto_monitor.h"
void app_main(void) { printf("Smart Env Monitor Ready\n"); }
    firmgen_telemetry_start();
    firmgen_proto_monitor_start();
