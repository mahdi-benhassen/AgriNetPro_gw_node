#include "messaging.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "MESSAGING";

// ============= CRC8 Checksum =============

uint8_t messaging_calculate_checksum(uint8_t *data, uint16_t length)
{
    uint8_t crc = 0;
    for (uint16_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 0x80) {
                crc = (crc << 1) ^ 0x07;
            } else {
                crc = crc << 1;
            }
        }
    }
    return crc;
}

// ============= Message Queue Operations =============

void message_queue_init(MessageQueue_t *queue)
{
    queue->capacity = 20;
    queue->buffer = (Message_t *)malloc(sizeof(Message_t) * queue->capacity);
    queue->head = 0;
    queue->tail = 0;
    queue->count = 0;
    ESP_LOGI(TAG, "Message queue initialized (capacity: %d)", queue->capacity);
}

int message_queue_push(MessageQueue_t *queue, Message_t *msg)
{
    if (queue->count >= queue->capacity) {
        ESP_LOGW(TAG, "Queue full, dropping message");
        return -1;
    }
    
    memcpy(&queue->buffer[queue->tail], msg, sizeof(Message_t));
    queue->tail = (queue->tail + 1) % queue->capacity;
    queue->count++;
    
    return 0;
}

int message_queue_pop(MessageQueue_t *queue, Message_t *msg)
{
    if (queue->count == 0) {
        return -1;
    }
    
    memcpy(msg, &queue->buffer[queue->head], sizeof(Message_t));
    queue->head = (queue->head + 1) % queue->capacity;
    queue->count--;
    
    return 0;
}

int message_queue_is_empty(MessageQueue_t *queue)
{
    return queue->count == 0;
}

// ============= Sensor Message Creation =============

uint16_t messaging_create_sensor_message(
    Message_t *msg,
    uint64_t src_id,
    uint8_t sensor_type,
    float value,
    uint64_t dst_id)
{
    memset(msg, 0, sizeof(Message_t));
    
    // Header
    msg->header.type = MSG_TYPE_SENSOR_DATA;
    msg->header.flags = 0;
    msg->header.src_id = src_id;
    msg->header.dst_id = dst_id;
    static uint32_t seq_counter = 0;
    msg->header.sequence = seq_counter++;
    
    // Payload: sensor_type (1 byte) + value (4 bytes, float)
    msg->payload[0] = sensor_type;
    memcpy(&msg->payload[1], &value, sizeof(float));
    
    msg->header.length = 5;  // 1 byte type + 4 bytes value
    
    // Calculate checksum
    uint8_t checksum_data[sizeof(MessageHeader_t) + 5];
    memcpy(checksum_data, &msg->header, sizeof(MessageHeader_t));
    memcpy(checksum_data + sizeof(MessageHeader_t), msg->payload, 5);
    msg->checksum = messaging_calculate_checksum(checksum_data, sizeof(checksum_data));
    
    return sizeof(MessageHeader_t) + msg->header.length + 1;
}

// ============= Heartbeat Message Creation =============

uint16_t messaging_create_heartbeat_message(
    Message_t *msg,
    uint64_t src_id,
    uint64_t dst_id)
{
    memset(msg, 0, sizeof(Message_t));
    
    msg->header.type = MSG_TYPE_HEARTBEAT;
    msg->header.flags = 0;
    msg->header.length = 0;
    msg->header.src_id = src_id;
    msg->header.dst_id = dst_id;
    static uint32_t seq_counter = 0;
    msg->header.sequence = seq_counter++;
    
    // Calculate checksum
    msg->checksum = messaging_calculate_checksum((uint8_t *)&msg->header, sizeof(MessageHeader_t));
    
    return sizeof(MessageHeader_t) + 1;
}

// ============= Message Parsing =============

int messaging_parse_message(Message_t *msg, uint8_t *data, uint16_t length)
{
    if (length < sizeof(MessageHeader_t) + 1) {
        ESP_LOGW(TAG, "Message too short");
        return -1;
    }
    
    // Copy header
    memcpy(&msg->header, data, sizeof(MessageHeader_t));
    
    // Validate length
    if (msg->header.length > 256) {
        ESP_LOGW(TAG, "Payload too large");
        return -1;
    }
    
    // Copy payload
    if (msg->header.length > 0) {
        memcpy(msg->payload, data + sizeof(MessageHeader_t), msg->header.length);
    }
    
    // Get checksum
    msg->checksum = data[sizeof(MessageHeader_t) + msg->header.length];
    
    // Verify checksum
    uint8_t calc_checksum = messaging_calculate_checksum(
        data,
        sizeof(MessageHeader_t) + msg->header.length
    );
    
    if (calc_checksum != msg->checksum) {
        ESP_LOGW(TAG, "Checksum mismatch: expected 0x%02x, got 0x%02x", 
                 msg->checksum, calc_checksum);
        return -1;
    }
    
    return 0;
}
