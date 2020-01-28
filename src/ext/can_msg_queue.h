/*
 * Copyright (c) 2017 Martin Jäger / Libre Solar
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef CAN_MSG_QUEUE_H
#define CAN_MSG_QUEUE_H

#include "mbed.h"

#ifdef STM32F0  // STM32L0 does not have CAN

#define CAN_QUEUE_SIZE 30

class CANMsgQueue
{
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

#endif /* STM32F0 */

#endif /* CAN_MSG_QUEUE_H */
