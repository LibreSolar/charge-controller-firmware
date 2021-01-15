/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2017 Martin Jäger / Libre Solar
 */

#ifndef TEST_FUNCTIONS_H
#define TEST_FUNCTIONS_H

#include "thingset.h"

#include <string.h>
#include <stdio.h>

const char pass_exp[] = "expert123";
const char pass_mkr[] = "maker456";

extern ThingSet ts;

void reset_function()
{
    printf("Reset function called!\n");
}

void auth_function()
{
    if (strlen(pass_exp) == strlen(auth_password) &&
        strncmp(auth_password, pass_exp, strlen(pass_exp)) == 0)
    {
        ts.set_authentication(TS_EXP_MASK | TS_USR_MASK);
    }
    else if (strlen(pass_mkr) == strlen(auth_password) &&
        strncmp(auth_password, pass_mkr, strlen(pass_mkr)) == 0)
    {
        ts.set_authentication(TS_MKR_MASK | TS_USR_MASK);
    }
    else {
        ts.set_authentication(TS_USR_MASK);
    }

    printf("Auth function called, password: %s\n", auth_password);
}

#endif