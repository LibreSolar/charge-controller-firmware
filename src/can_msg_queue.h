
#ifndef CAN_MSG_QUEUE_H
#define CAN_MSG_QUEUE_H

#include "mbed.h"

#define CAN_QUEUE_SIZE 30

class CANMsgQueue {
public:
    bool full();
    bool empty();
    void enqueue(CANMessage msg);
    int dequeue(CANMessage& msg);
    int dequeue();
    int first(CANMessage& msg);

private:
    CANMessage _queue[CAN_QUEUE_SIZE];
    int _first;
    int _length;
};

#endif