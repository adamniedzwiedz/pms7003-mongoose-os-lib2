#include <stdio.h>
#include <stdbool.h>
#include "common/platform.h"

#ifndef PMS7003_H
#define PMS7003_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* ds18b20 measure callback */
typedef void (*pms7003_callback)(void *param);

enum pms7003_mode {
    ACTIVE = 1,
    PASSIVE = 2
};

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

struct mgos_pms7003* pms7003_init(int uart_no, pms7003_callback cb, enum pms7003_mode mode);
#if CS_PLATFORM == CS_P_ESP32 
struct mgos_pms7003* pms7003_init_dev(int uart_no, pms7003_callback cb, enum pms7003_mode mode, struct mgos_uart_dev_config *dev);
#else
struct mgos_pms7003* pms7003_init_dev(int uart_no, pms7003_callback cb, enum pms7003_mode mode);
#endif
bool pms7003_set_mode(struct mgos_pms7003* pms7003, enum pms7003_mode mode);
bool pms7003_sleep(struct mgos_pms7003* pms7003);
bool pms7003_wakeup(struct mgos_pms7003* pms7003);
bool pms7003_request_read(struct mgos_pms7003* pms7003);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* PMS7003_H */