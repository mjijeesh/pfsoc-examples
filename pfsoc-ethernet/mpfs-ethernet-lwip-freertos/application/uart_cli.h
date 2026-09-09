#ifndef UART_CLI_H
#define UART_CLI_H

#include "lwip/apps/lwiperf.h"

void cli_task(void *pvParameters);
void print_status(void);
void print_help(void);

void my_iperf_report_cb(void *arg, enum lwiperf_report_type report_type,
                        const ip_addr_t *local_addr, u16_t local_port,
                        const ip_addr_t *remote_addr, u16_t remote_port,
                        u32_t bytes_transferred, u32_t ms_duration, u32_t bandwidth_kbitpsec);

#endif /* UART_CLI_H */
