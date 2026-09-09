#ifndef NET_TASKS_H
#define NET_TASKS_H

#include "lwip/opt.h"

void low_level_init(void);
void tcpip_init_done_cb(void *arg);
void eth_rx_task(void *pvParameters);
void phy_monitor_task(void *pvParameters);

#endif /* NET_TASKS_H */
