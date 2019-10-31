/* LibreSolar charge controller firmware
 * Copyright (c) 2016-2019 Martin Jäger (www.libre.solar)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef UEXT_H
#define UEXT_H

#include <vector>

/** @file
 *
 * @brief
 * Communication interfaces connected to UEXT port
 */

class UExtInterface
{
    public:
        virtual void process_asap()  {};
        virtual void process_1s() {};
        virtual void enable() {};
    private:

};

class UExtInterfaceManager: public UExtInterface
{
    public:
        /** UEXT interface process function
         *
         * This function is called in each main loop, as soon as all other tasks finished.
         */
        virtual void process_asap();

        /** UEXT interface process function
         *
         * This function is called every second, if no other task was blocking for a longer time.
         * It should be used for state machines, etc.
         */
        virtual void process_1s();

        /* UEXT interface initialization
        *
        * This function is called only once at startup.
        */
        virtual void enable();

    private:
        static std::vector<UExtInterface*> interfaces;
};

extern UExtInterfaceManager uext;

#endif /* UEXT_H */
