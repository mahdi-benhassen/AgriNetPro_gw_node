/**
 * @file app_rest_api.h
 * @brief HTTP REST API public interface for the ESP Thread Border Router.
 *
 * Endpoints:
 *   GET  /api/v1/nodes            → JSON array of all registered nodes
 *   GET  /api/v1/nodes/<eui64>    → JSON object for a single node
 *   POST /api/v1/nodes/<eui64>/cmd → Queue a downlink command
 *   GET  /api/v1/status           → BR uptime + network stats
 */

#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Start the HTTP REST API server.
 *
 * Binds to REST_API_PORT (default 8080) and registers URI handlers.
 *
 * @return ESP_OK on success, or an error from httpd_start().
 */
esp_err_t app_rest_api_start(void);

#ifdef __cplusplus
}
#endif
