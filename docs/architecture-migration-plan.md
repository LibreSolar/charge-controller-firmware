# Architecture Migration Plan

This document describes the target architecture of the charge controller firmware and the steps
to migrate the current application towards it. The goal is a firmware that can be adapted to
different boards and use cases mainly via devicetree and Kconfig, that allows to plug in
different control algorithms and that is not tied to a single communication protocol.

## Current limitations

1. **Topology hard-coded in the application:** `setup.cpp` instantiates a fixed set of global
   objects (`lv_bus`, `hv_bus`, `dcdc`, `pwm_switch`, `load`, `usb_pwr`). `BOARD_HAS_*`
   preprocessor conditions are spread over `main.cpp`, `daq.cpp` and `data_objects.cpp`.
   Boards with two DC/DC converters, multiple load outputs or a second battery are not
   possible without touching many files.
2. **Use case selected via Kconfig choices:** Port roles (`HV_TERMINAL_SOLAR`,
   `LV_TERMINAL_BATTERY`, ...) are Kconfig choices mapped to reference aliases
   (`solar_terminal`, `bat_terminal`) and `#define`s in the data objects.
3. **Board-specific measurement mapping in one place:** `daq_update()` knows every ADC channel
   by name and how they are combined (e.g. `lv_terminal_current = i_dcdc - i_load - i_pwm`).
4. **Monolithic control algorithm:** `Dcdc::perturb_observe_buck()` is a priority-ordered
   if/else chain that mixes MPPT, CV, CC, derating and nanogrid droop. The duty cycle is changed
   by ±1 step at 10 Hz. Replacing the algorithm requires replacing the entire function.
5. **ThingSet is the data model:** `data_objects.cpp` points directly to fields of global C++
   objects. Other protocols would have to access the same globals. Writes from the ThingSet
   thread are not synchronized with the control thread.
6. **Implicit timing:** Energy calculation relies on "called exactly once per second", the
   control period on `k_sleep(K_MSEC(100))`.
7. **Hardware access in the application:** Register-level code for STM32 timers, comparators
   and the bootloader lives in `app/src` (`load_driver.c`, `pwm_switch_driver.c`,
   `hardware.cpp`, `mcu.h`).

## Target architecture

```
┌──────────────────────────────────────────────────────────────────────────┐
│ Protocol adapters    ThingSet │ CAN BMS (Pylontech/Victron) │ CANopen │ Modbus │ OLED
├──────────────────────────────────────────────────────────────────────────┤
│ Data model           zbus channels: measurements, status, events, commands; settings
├──────────────────────────────────────────────────────────────────────────┤
│ Application          charger profiles │ load mgmt │ energy/SoC │ supervisor/errors
├──────────────────────────────────────────────────────────────────────────┤
│ Control framework    setpoint sources ──► limit arbitration ──► regulators (PI)
├──────────────────────────────────────────────────────────────────────────┤
│ Power model (DT)     dc-bus │ power-port │ converter │ switch   (instances from DT)
├──────────────────────────────────────────────────────────────────────────┤
│ Drivers              daq │ half_bridge │ power_switch │ Zephyr sensor/gpio/can/...
└──────────────────────────────────────────────────────────────────────────┘
```

Each layer only depends on layers below. Protocols never access control internals directly.

### Drivers

Zephyr device drivers with a generic API, a devicetree binding and an emulated or native_sim
backend for tests:

- `daq`: continuous sampling, filtering and limit checking of analog inputs (done)
- `half_bridge`: complementary PWM for DC/DC converters (done)
- `power_switch`: on/off switches for load and USB outputs including power-good feedback,
  charge pumps for high-side gate drivers and hardware short-circuit protection. Later also the
  low-frequency PWM switch of PWM charge controllers.
- Zephyr sensor API for NTC thermistors instead of the custom `ntc_temp()` in the application.

### Power model described in devicetree

The power topology is described in devicetree and instantiated with `DT_FOREACH_STATUS_OKAY`:

```dts
power {
    bus_lv: dc-bus-lv {
        compatible = "power,dc-bus";
        voltage = <&daq V_LOW>;
    };
    bus_hv: dc-bus-hv {
        compatible = "power,dc-bus";
        voltage = <&daq V_HIGH>;
    };
    dcdc0: dcdc {
        compatible = "power,buck-boost";
        half-bridge = <&halfbridge>;
        high-bus = <&bus_hv>;
        low-bus = <&bus_lv>;
        inductor-current = <&daq I_DCDC>;
    };
    port_solar: solar {
        compatible = "power,port";
        bus = <&bus_hv>;
        role = "solar";          /* use case overlay may change to "grid", "battery" */
        current-from = <&dcdc0>; /* derived from inductor current */
    };
    port_bat: battery {
        compatible = "power,port";
        bus = <&bus_lv>;
        role = "battery";
        current-derived;         /* residual of all other ports at the bus (KCL) */
    };
    port_load: load {
        compatible = "power,port";
        bus = <&bus_lv>;
        role = "load";
        current = <&daq I_LOAD>;
        switch = <&load_switch>;
    };
};
```

- Port roles move from Kconfig to devicetree. Use cases become overlays.
- Derived measurements are declared instead of hand-coded in `daq_update()`.
- The existing `DcBus` / `PowerPort` concepts (limits, energy counters, droop) are kept.

### Control framework

- **Setpoint sources** publish limits: charger (CV/CC), MPPT (input voltage target), thermal
  derating, nanogrid droop, external commands (e.g. BMS via CAN).
- **Limit arbitration** selects the most restrictive constraint per converter and reports which
  one is active (replaces the `DCDC_CONTROL_*` states).
- **Regulators** (min-selected PI loops for output voltage, inductor current and input voltage)
  run in the DAQ sample callback at kHz rate.
- MPPT becomes a setpoint source adjusting the input voltage at ~10 Hz, so different algorithms
  (P&O, incremental conductance, fractional Voc) can be exchanged without touching the
  regulators.
- Sources are registered via iterable sections (`CTRL_SOURCE_DEFINE(...)`), so custom
  algorithms can live in out-of-tree modules. This replaces `CONFIG_CUSTOMIZATION`.
- Fixed and explicit rates: regulators in ISR, setpoint sources and protection at 10 Hz, charger
  state machine and energy/SoC at 1 Hz with measured time deltas.

### Application modules

- **charger:** state machine as today, battery chemistries as constant profile tables
- **load manager:** LVD/LVR, overcurrent recovery, load shedding for N load ports
- **energy/SoC:** coulomb counting plus OCV correction behind an exchangeable interface
- **supervisor:** error/event registry instead of a single global 32-bit `error_flags` field,
  owner of fail-safe actions (e.g. fuse destruction)

### Data model and protocols

- State is published on zbus channels (measurement snapshots, status, events), commands and
  configuration changes are received via zbus. This also solves thread synchronization.
- Persistent configuration via Zephyr settings (or ThingSet storage behind an adapter) with
  validation callbacks.
- Each protocol is an adapter:
  - ThingSet (data items split per module, compiled only if the module exists)
  - CAN BMS protocols (BMS sends charge voltage/current limits, which become a setpoint source)
  - CANopen, J1939, DroneCAN, Modbus RTU, OLED display

### Code organization

- Core written in C following Zephyr conventions (devicetree macros, iterable sections, device
  model). C++ remains possible in leaf modules.
- Repository layout as a Zephyr module:
  - `drivers/`, `include/libresolar/drivers/`, `dts/bindings/`
  - `subsys/power/` (power model and control framework)
  - `subsys/charger/`
  - `subsys/comm/{thingset,can_bms,canopen}/`
  - `app/` only containing `main.c` and use-case overlays/configs
- Closed-loop simulation on native_sim: the emulated DAQ and half-bridge drivers are connected
  to a plant model (PV curve, battery RC model, inductor), so MPPT, CC/CV transitions and fault
  cases can be tested with ztest in CI.

## Migration steps

The migration is done incrementally. Each step keeps the firmware working on all supported
boards and passes the existing tests.

### Step 1: Drivers

Move all remaining hardware access from `app/src` into Zephyr drivers.

- [x] `daq` driver (replaces ADC/DMA code in the application)
- [x] `half_bridge` driver (replaces timer code for DC/DC converters)
- [x] `power_switch` driver API
  - [x] GPIO backend (`libresolar,gpio-power-switch`) with optional latching power-good and
        charge pump for high-side gate drivers
  - [x] STM32 LPTIM backend with comparator-based short-circuit protection
        (`libresolar,stm32-lptim-power-switch`, PWM 2420 LUS), needs hardware test
  - [x] Load and USB outputs use the driver, `load_driver.c` removed
- [x] PWM support in the `power_switch` API
  - [x] STM32 timer backend (`libresolar,stm32-pwm-power-switch`) for PWM charge controllers
        (replaces `pwm_switch_driver.c`), needs hardware test
  - [x] Emulated backend (`libresolar,power-switch-emul`) for tests on native_sim
- [x] NTC thermistors via Zephyr sensor API (replaces `ntc_temp()` in `daq.cpp`)
  - [x] `libresolar,daq-ntc-thermistor` sensor driver using the filtered DAQ readings, as
        Zephyr's NTC driver requires exclusive ADC access
- [ ] Bootloader entry and SoC-specific setup out of the application (`hardware.cpp`,
      `setup.cpp`, `mcu.h`)

### Step 2: Devicetree power model

- Bindings for `dc-bus`, `power-port` and converters
- Generic measurement update based on the devicetree description (replaces `daq.cpp` and
  `setup.cpp`), initially behind the existing `DcBus` / `PowerPort` API

### Step 3: Data layer

- Introduce zbus channels for measurements, status and commands
- Split ThingSet data objects per module

### Step 4: Control framework

- Port the existing perturb & observe algorithm as the first setpoint source without changing
  its behavior
- Add PI regulators behind a Kconfig option and compare them in simulation and on hardware
- Requires the closed-loop simulation to be in place first

### Step 5: Port roles in devicetree

- Remove the `HV_TERMINAL_*` / `LV_TERMINAL_*` / `PWM_TERMINAL_*` Kconfig choices

### Step 6: Additional protocol

- Add a second CAN protocol (e.g. BMS protocol) to validate the adapter boundary
