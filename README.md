# PMS7003 dust sensor library v2 for Mongoose OS

## Overview

[PMS7003](https://botland.com.pl/index.php?controller=attachment&id_attachment=2182) (by Plantower) is a dust sensor which measures PM concentration (air polution).

<p align="center">
  <img src="https://github.com/adamniedzwiedz/pms7003-mongoose-os-lib2/blob/master/pms7003.jpg">
</p>

The sensor can work in two modes:
- **active** (the default after power on) `pms7003_init(.., ACTIVE)` , where measure is sent continuously with specified period of time (see the documentation for the details)
- **passive** `pms7003_init(.., PASSIVE)`, where measure is sent only when it was requested `pms7003_request_read`

Furthermore there is possible to put the sensor into *sleep mode* `pms7003_sleep` and then *wake it up* `pms7003_wakeup`. In a sleep mode a fan is disabled and the sensor consumes about 4 mA.

Measured values are passed to the *callback method* as a `pms7003_measure` structure. The *callback method* is provided as the second parameter of the `pms7003_init`.

```c
struct pms7003_measure {
    unsigned long pm1_0_atm;
    unsigned long pm2_5_atm;
    unsigned long pm10_0_atm;
    unsigned long pm1_0_cf1;
    unsigned long pm2_5_cf1;
    unsigned long pm10_0_cf1;
    unsigned long num0_3um;
    unsigned long num0_5um;
    unsigned long num1_0um;
    unsigned long num2_5um;
    unsigned long num5_0um;
    unsigned long num10_0um;
};
```

## Initialization

Since communication with the sensor is done via *UART0* the debug messages are sent through *UART1* (*TX1 => GPIO2* on ESP8266). The library sets `debug.stdout_uart` and `debug.stderr_uart` on *UART1*.

Typical connection of the sensor.

<p align="center">
  <img src="https://github.com/adamniedzwiedz/pms7003-mongoose-os-lib2/blob/master/pms7003_connection.png">
</p>

## Usage

```c
#include "mgos.h"
#include "pms7003.h"

#define UART_NO 0
struct mgos_pms7003* pms7003;

static void pms7003_ready(void *arg) {
  struct pms7003_measure *measure = arg;

  if (measure == NULL) {
    LOG(LL_ERROR, ("Reading measure has failed.\r\n"));
    return;
  }

  LOG(LL_INFO, ("PMS7003 PM_1_0: %ld\r\n", measure->pm1_0_atm));
  LOG(LL_INFO, ("PMS7003 PM_2_5: %ld\r\n", measure->pm2_5_atm));
  LOG(LL_INFO, ("PMS7003 PM_10_0: %ld\r\n", measure->pm10_0_atm)); 
}

static void init_cb(void *arg) {
  LOG(LL_INFO, ("PMS7003 init\r\n"));
  pms7003 = pms7003_init(UART_NO, pms7003_ready, ACTIVE);
  (void) arg;
}

enum mgos_app_init_result mgos_app_init(void) {
  // We need to wait on the sensor to run, so the first command must be send with at least 2s delay
  mgos_set_timer(2000 /* ms */, false /* repeat */, init_cb, NULL /* arg */);

  return MGOS_APP_INIT_SUCCESS;
}
```

## Debugging

In case of any issues increase the debug level and check debug logs.
For deep debugging set *debug_level* on **3** or **4** which shows also each value in a frame.
This can be done by adding the following lines to *mos.yml* file
```yaml
config_schema:
  - ["debug.level", 3]
```

The default baud rate of the debug port (UART1) is *115.2kbps*
