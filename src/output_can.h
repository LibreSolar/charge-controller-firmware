#ifndef OUTPUT_CAN_H
#define OUTPUT_CAN_H

#include "mbed.h"
#include "config.h"
#include "data_objects.h"
#include "can_msg_queue.h"


#define PUB_MULTIFRAME_EN (0x1U << 7)
#define PUB_TIMESTAMP_EN (0x1U << 6)

// Protocol functions
#define TS_READ     0x00
#define TS_WRITE    0x01
#define TS_PUB_REQ  0x02
#define TS_SUB_REQ  0x03
#define TS_OBJ_NAME 0x04
#define TS_LIST     0x05


//static uint8_t can_node_id = CAN_NODE_ID;

void can_send_data();
void can_receive();
void can_process_outbox();
void can_process_inbox();
//void can_list_object_ids(int category = TS_C_ALL);
void can_send_object_name(int obj_id, uint8_t can_dest_id);

#endif
