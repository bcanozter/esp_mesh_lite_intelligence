#ifndef __HTTPS_SERVER_H__
#define __HTTPS_SERVER_H__

#include <esp_err.h>
#include <esp_http_server.h>

esp_err_t long_async(httpd_req_t *);

void worker_task(void *p);
void start_workers(void);

esp_err_t long_handler(httpd_req_t *);
esp_err_t quick_handler(httpd_req_t *);
esp_err_t index_handler(httpd_req_t *);
httpd_handle_t start_webserver(void);

esp_err_t stop_webserver(httpd_handle_t);

void disconnect_handler(void *, esp_event_base_t,
                        int32_t, void *);

void connect_handler(void *, esp_event_base_t,
                     int32_t, void *);

#endif