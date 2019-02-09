#include "mgos.h"
#include "mgos_gpio.h"
#include "pms7003.h"
#include "common/platform.h"

#define PMS7003_FRAME_LEN 32
#define PMS7003_FRAME_START1 0x42
#define PMS7003_FRAME_START2 0x4D

#define PMS7003_CF1_PM_1_0_H     4
#define PMS7003_CF1_PM_1_0_L     5
#define PMS7003_CF1_PM_2_5_H     6
#define PMS7003_CF1_PM_2_5_L     7
#define PMS7003_CF1_PM_10_0_H    8
#define PMS7003_CF1_PM_10_0_L    9
#define PMS7003_ATM_PM_1_0_H    10
#define PMS7003_ATM_PM_1_0_L    11
#define PMS7003_ATM_PM_2_5_H    12
#define PMS7003_ATM_PM_2_5_L    13
#define PMS7003_ATM_PM_10_0_H   14
#define PMS7003_ATM_PM_10_0_L   15
#define PMS7003_NUM_0_3_H       16
#define PMS7003_NUM_0_3_L       17
#define PMS7003_NUM_0_5_H       18
#define PMS7003_NUM_0_5_L       19
#define PMS7003_NUM_1_0_H       20
#define PMS7003_NUM_1_0_L       21
#define PMS7003_NUM_2_5_H       22
#define PMS7003_NUM_2_5_L       23
#define PMS7003_NUM_5_0_H       24
#define PMS7003_NUM_5_0_L       25
#define PMS7003_NUM_10_0_H      26
#define PMS7003_NUM_10_0_L      27

#define PMS7003_CHECKSUM_H      30
#define PMS7003_CHECKSUM_L      31


#define PMS7003_CMD_LEN 7

const unsigned char passive_mode_cmd[] =  { 0x42, 0x4D, 0xE1, 0x00, 0x00, 0x01, 0x70 };
const unsigned char active_mode_cmd[] =   { 0x42, 0x4D, 0xE1, 0x00, 0x01, 0x01, 0x71 };
const unsigned char request_read_cmd[] =  { 0x42, 0x4D, 0xE2, 0x00, 0x00, 0x01, 0x71 };
const unsigned char sleep_cmd[] =         { 0x42, 0x4D, 0xE4, 0x00, 0x00, 0x01, 0x73 };
const unsigned char wakeup_cmd[] =        { 0x42, 0x4D, 0xE4, 0x00, 0x01, 0x01, 0x74 };

pms7003_callback pms7003_cb;

struct mgos_pms7003 {
  int uart;
};

unsigned long get_value(unsigned char* frame, int high, int low) {
  return (frame[high] << 8) + frame[low];
}

unsigned char* pms7003_find_frame(unsigned char* frame, size_t frame_len) {
  size_t i;
  if (frame_len > 0) {
    LOG(LL_VERBOSE_DEBUG, ("PMS7003: frame[0] = 0x%02x\r\n", frame[0]));
  }
  for (i = 1; i < frame_len; i++) {
    LOG(LL_VERBOSE_DEBUG, ("PMS7003: frame[%d] = 0x%02x\r\n", i, frame[i]));
    if ((frame[i-1] == PMS7003_FRAME_START1) && (frame[i] == PMS7003_FRAME_START2)) {
      if ((frame_len - i + 1) >= PMS7003_FRAME_LEN) {
          return &frame[i-1];
      }
      LOG(LL_DEBUG, ("PMS7003: Too small frame length: %d\r\n", frame_len));  
    }
  }
  LOG(LL_DEBUG, ("PMS7003: frame not found\r\n"));
  return NULL;
}

bool pms7003_validate_checksum(unsigned char* frame) {
  size_t i;
  unsigned long valid_checksum = (frame[PMS7003_CHECKSUM_H] << 8) + frame[PMS7003_CHECKSUM_L];
  unsigned long checksum = 0;

  for (i = 0; i < PMS7003_CHECKSUM_H; i++) {
    checksum += frame[i];
  }
  LOG(LL_DEBUG, ("PMS7003: calculated checksum: 0x%04lx\r\n", checksum));
  LOG(LL_DEBUG, ("PMS7003:      valid checksum: 0x%04lx\r\n", valid_checksum));
  return checksum == valid_checksum;
}

static void uart_dispatcher(int uart_no, void *arg) {
  char data_str[PMS7003_FRAME_LEN * 2 + 1];
  struct mbuf data = {0};
  struct pms7003_measure measure;
  unsigned char *frame;
  int* pms7003_uart = arg;

  if (pms7003_uart == NULL) {
    LOG(LL_ERROR, ("NULL pms7003_uart argument in uart_dispatcher callback.\r\n"));
    return;
  }

  if (uart_no != *pms7003_uart) {
    LOG(LL_DEBUG, ("PMS7003 uart: %d, Triggered uart: %d\r\n", *pms7003_uart, uart_no));
    return;
  }

  // read UART data if available
  size_t available = mgos_uart_read_avail(uart_no);
  if (available == 0) return;

  mgos_uart_read_mbuf(uart_no, &data, available);

  // find PMS7003 frame 
  frame = pms7003_find_frame((unsigned char*)data.buf, data.len);

  if (frame == NULL) {
    mbuf_free(&data);
    return;
  }

  cs_to_hex(data_str, frame, PMS7003_FRAME_LEN); 
  LOG(LL_DEBUG, ("PMS7003 data: %s\r\n", data_str));

  if (!pms7003_validate_checksum(frame)) {
    LOG(LL_DEBUG, ("PMS7003: Invalid frame. Checksum mismatch\r\n"));
  }
  else if (pms7003_cb != NULL) {
    measure.pm1_0_atm = get_value(frame, PMS7003_ATM_PM_1_0_H, PMS7003_ATM_PM_1_0_L);
    measure.pm2_5_atm = get_value(frame, PMS7003_ATM_PM_2_5_H, PMS7003_ATM_PM_2_5_L);
    measure.pm10_0_atm = get_value(frame, PMS7003_ATM_PM_10_0_H, PMS7003_ATM_PM_10_0_L);
    measure.pm1_0_cf1 = get_value(frame, PMS7003_CF1_PM_1_0_H, PMS7003_CF1_PM_1_0_L);
    measure.pm2_5_cf1 = get_value(frame, PMS7003_CF1_PM_2_5_H, PMS7003_CF1_PM_2_5_L);
    measure.pm10_0_cf1 = get_value(frame, PMS7003_CF1_PM_10_0_H, PMS7003_CF1_PM_10_0_L);
    measure.num0_3um = get_value(frame, PMS7003_NUM_0_3_H, PMS7003_NUM_0_3_L);
    measure.num0_5um = get_value(frame, PMS7003_NUM_0_5_H, PMS7003_NUM_0_5_L);
    measure.num1_0um = get_value(frame, PMS7003_NUM_1_0_H, PMS7003_NUM_1_0_L);
    measure.num2_5um = get_value(frame, PMS7003_NUM_2_5_H, PMS7003_NUM_2_5_L);
    measure.num5_0um = get_value(frame, PMS7003_NUM_5_0_H, PMS7003_NUM_5_0_L);
    measure.num10_0um = get_value(frame, PMS7003_NUM_10_0_H, PMS7003_NUM_10_0_L);

    pms7003_cb(&measure);
  }

  mbuf_free(&data);
}

bool pms7003_set_mode(struct mgos_pms7003* pms7003, enum pms7003_mode mode) { 
  size_t written;
  if (pms7003 == NULL) {
    LOG(LL_ERROR, ("parameter pms7003 cannot be NULL.\r\n"));
    return false;
  }  
  if (mode == ACTIVE) {
      written = mgos_uart_write(pms7003->uart, active_mode_cmd, PMS7003_CMD_LEN);
  }
  else if (mode == PASSIVE) {
      written = mgos_uart_write(pms7003->uart, passive_mode_cmd, PMS7003_CMD_LEN);
  }
  else {
      LOG(LL_ERROR, ("Invalid PMS7003 mode. Valid values: (ACTIVE, PASSIVE)\r\n"));
      return false;
  }
  if (written == 0) {
    LOG(LL_ERROR, ("sending SET MODE command to PMS7003 has failed\r\n"));
    return false;
  }
  return true;
}

bool pms7003_sleep(struct mgos_pms7003* pms7003) {
  size_t written;
  if (pms7003 == NULL) {
    LOG(LL_ERROR, ("parameter pms7003 cannot be NULL.\r\n"));
    return false;
  }  
  written = mgos_uart_write(pms7003->uart, sleep_cmd, PMS7003_CMD_LEN);
  if (written == 0) {
    LOG(LL_ERROR, ("sending SLEEP command to PMS7003 has failed\r\n"));
    return false;
  }
  return true;
}

bool pms7003_wakeup(struct mgos_pms7003* pms7003) {
  size_t written;
  if (pms7003 == NULL) {
    LOG(LL_ERROR, ("parameter pms7003 cannot be NULL.\r\n"));
    return false;
  }  
  written = mgos_uart_write(pms7003->uart, wakeup_cmd, PMS7003_CMD_LEN);
  if (written == 0) {
    LOG(LL_ERROR, ("sending WAKEUP command to PMS7003 has failed\r\n"));
    return false;
  }
  return true;
}

bool pms7003_request_read(struct mgos_pms7003* pms7003) {
  size_t written;
  if (pms7003 == NULL) {
    LOG(LL_ERROR, ("parameter pms7003 cannot be NULL.\r\n"));
    return false;
  }  
  written = mgos_uart_write(pms7003->uart, request_read_cmd, PMS7003_CMD_LEN);
  if (written == 0) {
    LOG(LL_ERROR, ("sending REQUEST READ command to PMS7003 has failed\r\n"));
    return false;
  }
  return true;
}

struct mgos_pms7003* pms7003_init(int uart_no, pms7003_callback cb, enum pms7003_mode mode) {
#if CS_PLATFORM == CS_P_ESP32 
  return pms7003_init_dev(uart_no, cb, mode, NULL);
#else
  return pms7003_init_dev(uart_no, cb, mode);
#endif
}

#if CS_PLATFORM == CS_P_ESP32 
struct mgos_pms7003* pms7003_init_dev(int uart_no, pms7003_callback cb, enum pms7003_mode mode, struct mgos_uart_dev_config *dev) {
#else
struct mgos_pms7003* pms7003_init_dev(int uart_no, pms7003_callback cb, enum pms7003_mode mode) {
#endif
  struct mgos_uart_config ucfg;
  struct mgos_pms7003* pms7003;

  if (cb == NULL) {
    LOG(LL_ERROR, ("parameter pms7003_callback cannot be null\r\n"));
    return NULL;
  }

  pms7003 = calloc(1, sizeof(*pms7003));  

  if (pms7003 == NULL) {
    LOG(LL_ERROR, ("Cannot create mgos_pms7003 structure.\r\n"));
    return NULL;
  }

  // configure UART - PMS7003 uses 9600bps
  mgos_uart_config_set_defaults(uart_no, &ucfg);
  ucfg.baud_rate = 9600;

#if CS_PLATFORM == CS_P_ESP32 
  if (dev != NULL) {
    struct mgos_uart_dev_config *dcfg = &ucfg.dev;
    dcfg->rx_gpio = dev->rx_gpio;
    dcfg->tx_gpio = dev->tx_gpio;
    dcfg->cts_gpio = dev->cts_gpio;
    dcfg->rts_gpio = dev->rts_gpio;  
  }
#endif

  if (!mgos_uart_configure(uart_no, &ucfg)) {
    LOG(LL_ERROR, ("Cannot configure UART%d \r\n", uart_no));
    free(pms7003);
    return NULL;
  } 

  pms7003_cb = cb;
  pms7003->uart = uart_no;

  // set mode
  pms7003_set_mode(pms7003, mode); 

  // register UART handler (when data received) and enable receiver
  mgos_uart_set_dispatcher(uart_no, uart_dispatcher, &uart_no /* arg */);
  mgos_uart_set_rx_enabled(uart_no, true);

  return pms7003;
}