# Coding Standards & Guidelines

## 1. Language and Compiler
*   Code shall be written in **C (C99)** or **C++ (C++17)**.
*   All code must compile under the current ESP-IDF toolchain without warnings (`-Wall -Wextra -Werror`).

## 2. Code Formatting
*   We use `clang-format` to enforce consistency.
*   **Indentation**: 4 spaces (no tabs).
*   **Braces**: K&R style. Braces are mandatory even for single-line `if`/`while` statements.
*   **Line Length**: Maximum 100 characters.

## 3. Naming Conventions

| Entity | Convention | Example |
| :--- | :--- | :--- |
| Variables (Local) | `snake_case` | `sensor_value` |
| Variables (Global) | `g_snake_case` | `g_system_state` |
| Constants/Macros | `UPPER_SNAKE_CASE` | `MAX_BUFFER_SIZE` |
| Functions | `module_action_object` | `mqtt_client_publish_msg()` |
| Structs/Enums/Typedefs | `snake_case_t` | `network_config_t` |

*Note: All functions and global variables must be prefixed with their module name to prevent namespace collisions.*

## 4. Documentation (Doxygen)
Every public API function in a header file must be documented using Doxygen syntax:

```c
/**
 * @brief Publishes a telemetry message to the broker.
 * 
 * @param topic The MQTT topic to publish to.
 * @param payload The JSON payload string.
 * @param qos Quality of Service level (0, 1, 2).
 * @return esp_err_t ESP_OK on success, ESP_FAIL otherwise.
 */
esp_err_t mqtt_client_publish(const char* topic, const char* payload, int qos);
```

## 5. Safety and Reliability (MISRA-inspired)
*   **Dynamic Memory**: Avoid `malloc()`/`free()` in the operational loop to prevent heap fragmentation. Use static allocation or FreeRTOS memory pools where possible.
*   **Error Handling**: Every function that can fail must return an `esp_err_t`. The caller **must** check the return value using the `ESP_ERROR_CHECK` macro or explicit error handling logic.
*   **Global Variables**: Usage of global variables is strictly discouraged. State should be encapsulated within modules and passed via context pointers.
*   **Magic Numbers**: Avoid magic numbers in code. Use `#define` or `enum`.
