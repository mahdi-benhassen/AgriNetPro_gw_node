#ifndef MESSAGING_H
#define MESSAGING_H

#include <stdint.h>
#include <stddef.h>

// ============= Message Types =============
#define MSG_TYPE_SENSOR_DATA 0x01
#define MSG_TYPE_CONFIG 0x02
#define MSG_TYPE_COMMAND 0x03
#define MSG_TYPE_ACK 0x04
#define MSG_TYPE_ERROR 0x05
#define MSG_TYPE_HEARTBEAT 0x06

// ============= Message Structure =============

typedef struct {
    uint8_t type;           // Message type
    uint8_t flags;          // Control flags
    uint16_t length;        // Payload length
    uint32_t sequence;      // Message ID
    uint64_t src_id;        // Source device ID
    uint64_t dst_id;        // Destination device ID
} MessageHeader_t;

typedef struct {
    MessageHeader_t header;
    uint8_t payload[256];
    uint8_t checksum;       // CRC8
} Message_t;

// ============= Message Queue =============

typedef struct {
    Message_t *buffer;
    uint16_t capacity;
    uint16_t head;
    uint16_t tail;
    uint16_t count;
} MessageQueue_t;

// ============= Function Prototypes =============

// Queue operations
void message_queue_init(MessageQueue_t *queue);
int message_queue_push(MessageQueue_t *queue, Message_t *msg);
int message_queue_pop(MessageQueue_t *queue, Message_t *msg);
int message_queue_is_empty(MessageQueue_t *queue);

// Message creation
uint16_t messaging_create_sensor_message(
    Message_t *msg,
    uint64_t src_id,
    uint8_t sensor_type,
    float value,
    uint64_t dst_id
);

uint16_t messaging_create_heartbeat_message(
    Message_t *msg,
    uint64_t src_id,
    uint64_t dst_id
);

// Message parsing
int messaging_parse_message(Message_t *msg, uint8_t *data, uint16_t length);

// Utilities
uint8_t messaging_calculate_checksum(uint8_t *data, uint16_t length);

#endif // MESSAGING_H
