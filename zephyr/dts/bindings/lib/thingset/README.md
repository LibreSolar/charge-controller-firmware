# C++ library for the ThingSet Protocol

![Travis CI build badge](https://travis-ci.com/LibreSolar/thingset-device-library.svg?branch=master)

The ThingSet protocol provides a consistent, standardized way to configure, monitor and control ressource-constrained devices via different communication interfaces.

Protocol specification: [libre.solar/thingset](https://libre.solar/thingset/)

The specification is developed in an open source way and still in progress. You can contribute ideas for improvement.

This library is used in Libre Solar devices and implements v0.3 of the specification.

The implementation is tested vs. newlib-nano and the default newlib provided by GNU C++ compiler.

## Usage of the library

An example program is implemented in `src/main.cpp`, which provides a shell to access the data via ThingSet protocol if run on a computer.

Most important is the setup of the data node tree in `test/test_data.h`.

Assuming the data is stored in a static array `data_nodes` as in the example, a ThingSet object is created by:

```C++
ThingSet ts(data_nodes, sizeof(data_nodes)/sizeof(DataNode));
```

Afterwards, it can be used with any communication interface using the `process` function:

```C++
uint8_t req_buf[500];        // buffer to store incoming data
uint8_t resp_buf[500];       // buffer to store ThingSet response

/*
 * Listen to a communication interface (e.g. UART) and store
 * incoming data in the request buffer.
 *
 * After receiving a new-line character, process the request.
 */

int req_len = strlen((char *)req_buf);  // only works for text mode

ts.process(req_buf, req_len, resp_buf, sizeof(resp_buf));

/*
 * The response including the requested data is now in the response_buffer
 * and can be sent back to the communication interface.
 */
```

## Implemented features

### Text mode

The following ThingSet functions are fully implemented:

- GET and FETCH requests (first byte '?')
- PATCH request (first byte '=')
- POST request (first byte '!' or '+')
- DELETE request (first byte '-')
- Execution of functions via callbacks to certain paths or via executable nodes
- Authentication via callback to 'auth' node
- Sending of publication messages (# {...})
- Setup of publication channels (enable/disable, configure data nodes to be published, change interval)

In order to reduce code size, verbose status messages can be turned off using the TS_VERBOSE_STATUS_MESSAGES = 0 in ts_config.h.

### Binary mode

The following functions are fully implemented:

- GET and FETCH requests (function codes 0x01 and 0x05)
- PATCH request (function code 0x07)
- Sending of publication messages (0x1F)

For an efficient implementation, only the most important CBOR data types will be supported:

- Unsigned int up to 64 bit
- Negative int up to 64 bit
- UTF8 strings of up to 2^16-1 bytes
- Binary data of up to 2^16-1 bytes
- Float 32 and 64 bit
- Simple values true and false
- Arrays of above types

Currently, following data types are still missing in the implementation.

- Binary data (only CBOR format)  of up to 2^16-1 bytes
- Float 64 (double)

It is possible to enable or disable 64 bit data types to decrease code size using the TS_64BIT_TYPES_SUPPORT flag in ts_config.h.

## Unit testing

The tests are implemented using the UNITY environment integrated in PlatformIO. The tests can be run on the device and in the native environment of the computer. For native (and more quick) tests run:

    pio test -e native-std

The test in native environment is also set as the default unit-test, so it is run if you push the test button in PlatformIO.

To run the unit tests on the device, execute the following command:

    pio test -e device-std -e device-newlib-nano

## Remarks

This implemntation uses the very lightweight JSON parser [JSMN](https://github.com/zserge/jsmn).
