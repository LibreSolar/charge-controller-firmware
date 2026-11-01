# Power Model Design (Migration Step 2)

Status: **draft for review**

This document describes how the power topology of a board (DC buses, power ports and converters)
is described in the devicetree and how measurements are mapped to it. It implements step 2 of
the [architecture migration plan](architecture-migration-plan.md).

## Goals

- A board's power topology is fully described in its devicetree. Adding a board with a different
  combination of converters, switches and outputs requires no changes in the application code.
- Measurements are assigned to buses, ports and converters in the devicetree instead of the
  hand-written `daq_update()`.
- Derived values (e.g. battery current as residual of all other currents at the bus) follow
  generic rules instead of board-specific code.
- Hardware limits move from the `/pcb` node to the components they belong to.

## Non-goals (later steps)

- Changing the control algorithms (step 4). The existing `Dcdc`, `PwmSwitch`, `LoadOutput` and
  `Charger` logic stays as it is.
- Port roles (solar, battery, grid) in the devicetree (step 5). The `HV_TERMINAL_*` /
  `LV_TERMINAL_*` Kconfig choices stay for now.
- Data layer and ThingSet split (step 3).

## Current state

The topology is implicit in `setup.cpp`, `daq.cpp` and `board.h`:

| Object         | Defined in    | Measurements (`daq_update()`)                                    |
|----------------|---------------|------------------------------------------------------------------|
| `lv_bus`       | `setup.cpp`   | `v_low`, filtered with τ = 10 s                                   |
| `hv_bus`       | `setup.cpp`   | `v_high`, filtered with τ = 10 s (only with DC/DC)                |
| `dcdc`         | `setup.cpp`   | `i_dcdc` as inductor current, `temp_fets`                         |
| `hv_terminal`  | `setup.cpp`   | derived: `-i_dcdc * V_lv / V_hv` (lossless converter)             |
| `load`         | `setup.cpp`   | `i_load`                                                          |
| `pwm_switch`   | `setup.cpp`   | `-duty * i_pwm`, filtered τ = 1.5 s; ext. voltage `v_low - v_pwm` |
| `lv_terminal`  | `setup.cpp`   | derived: `i_dcdc - i_load - i_pwm_port`, filtered τ = 10 s        |

Limits are taken from the `/pcb` node (`hs-voltage-max`, `ls-voltage-max`, `dcdc-current-max`,
`ls-fuse`, thermal model parameters).

## Concepts and sign conventions

- **DC bus:** Node with a single voltage. Ports and converters are connected to buses.
- **Power port:** Connection of a bus to an external device (battery, solar panel, load) or an
  internal sub-circuit. **Positive port current flows from the bus into the external device**,
  e.g. charging current of a battery, current into a load. A solar panel has negative current.
  This is the existing convention of `PowerPort`.
- **Converter:** Transfers power between two buses (DC/DC half bridge). The inductor current is
  positive in buck direction (from high side to low side).

### Current calculation rules

1. **Measured:** The port current is the value of a DAQ channel, optionally inverted.
2. **Switched sampling:** For PWM switches, the current is only sampled while the switch is
   conducting (DAQ sample filter), and the port current is the sampled value multiplied by the
   duty cycle.
3. **Converter injection:** A converter injects `+i_L` into its low bus and `-i_L * V_low / V_high`
   into its high bus (lossless approximation, same as today).
4. **Kirchhoff residual:** At most one port per bus may be marked `current-derived`. Its current is
   the sum of all converter injections into the bus minus the currents of all other ports.

Rule 4 replaces both special cases of today: the HV terminal is the residual of the HV bus with
only the converter injection, and the battery terminal is the residual of the LV bus.

### Filtering

Every bus voltage and port current gets a filtered value with a configurable time constant
(`filter-time-constant-ms`, default 10 s). The filter constant is calculated from the control
frequency (`c = dt / (tau + dt)`), so the existing constants are reproduced exactly:
10 s → 0.0099, 1.5 s → 0.0625. The filter is initialized with the first sample.

## Devicetree bindings

All bindings in `dts/bindings/power/`. DAQ channels are referenced via phandles to the labelled
child nodes of the DAQ device (same pattern as the NTC sensor driver).

### `libresolar,dc-bus`

| Property                  | Type    | Description                                              |
|---------------------------|---------|----------------------------------------------------------|
| `voltage-channel`         | phandle | DAQ channel measuring the bus voltage (required)         |
| `voltage-max-mv`          | int     | Maximum voltage in mV, used for overvoltage alerts       |
| `filter-time-constant-ms` | int     | Low-pass filter time constant, default 10000             |

### `libresolar,power-port`

| Property                  | Type    | Description                                                    |
|---------------------------|---------|----------------------------------------------------------------|
| `bus`                     | phandle | Bus the port is connected to (required)                        |
| `current-channel`         | phandle | DAQ channel measuring the port current                         |
| `current-inverted`        | boolean | Channel measures current flowing into the bus                  |
| `current-derived`         | boolean | Current calculated as Kirchhoff residual (exclusive with above) |
| `switch`                  | phandle | Power switch device of the port                                |
| `current-switched`        | boolean | Current only sampled while switch conducts, scaled by duty     |
| `external-voltage-channel`| phandle | DAQ channel measuring the voltage across the switch            |
| `temperature`             | phandle | Temperature sensor (e.g. battery NTC)                          |
| `filter-time-constant-ms` | int     | Low-pass filter time constant, default 10000                   |

The external voltage of a switched port is `V_bus - V_channel` (voltage at the external terminal
while the switch is open, e.g. solar panel voltage of a PWM charger).

### `libresolar,dcdc`

| Property                   | Type    | Description                                               |
|----------------------------|---------|-----------------------------------------------------------|
| `half-bridge`              | phandle | Half-bridge device (required)                             |
| `high-bus`, `low-bus`      | phandle | Connected buses (required)                                |
| `inductor-current-channel` | phandle | DAQ channel measuring the inductor current (required)     |
| `current-max-ma`           | int     | Maximum inductor current in mA (replaces `dcdc-current-max`) |
| `temperature`              | phandle | MOSFET temperature sensor                                 |
| `ls-fuse`                  | boolean | Moved from `/pcb`                                         |

Thermal model parameters of the load switch (`internal-tref-max`, `mosfets-tj-max`,
`mosfets-tau-ja`) move to the power switch binding.

### Example: MPPT 2420 HC (DC/DC and load output)

```dts
adc-inputs {
    compatible = "libresolar,stm32-daq";
    daq_v_low: v-low { ... };
    daq_v_high: v-high { ... };
    daq_i_dcdc: i-dcdc { ... };
    daq_i_load: i-load { ... };
    daq_temp_fets: temp-fets { ... };
};

power {
    bus_lv: bus-lv {
        compatible = "libresolar,dc-bus";
        voltage-channel = <&daq_v_low>;
        voltage-max-mv = <32000>;
    };

    bus_hv: bus-hv {
        compatible = "libresolar,dc-bus";
        voltage-channel = <&daq_v_high>;
        voltage-max-mv = <90000>;
    };

    dcdc: dcdc {
        compatible = "libresolar,dcdc";
        half-bridge = <&halfbridge>;
        high-bus = <&bus_hv>;
        low-bus = <&bus_lv>;
        inductor-current-channel = <&daq_i_dcdc>;
        current-max-ma = <20000>;
        temperature = <&ntc_fets>;
        ls-fuse;
    };

    port_hv: port-hv {
        compatible = "libresolar,power-port";
        bus = <&bus_hv>;
        current-derived;
    };

    port_lv: port-lv {
        compatible = "libresolar,power-port";
        bus = <&bus_lv>;
        current-derived;
    };

    port_load: port-load {
        compatible = "libresolar,power-port";
        bus = <&bus_lv>;
        current-channel = <&daq_i_load>;
        switch = <&load_switch>;
    };
};
```

### Example: PWM 2420 LUS (PWM switch, load and USB output)

```dts
power {
    bus_lv: bus-lv {
        compatible = "libresolar,dc-bus";
        voltage-channel = <&daq_v_low>;
        voltage-max-mv = <32000>;
    };

    port_pwm: port-pwm {
        compatible = "libresolar,power-port";
        bus = <&bus_lv>;
        switch = <&pwm_switch>;
        current-channel = <&daq_i_pwm>;
        current-inverted;
        current-switched;
        external-voltage-channel = <&daq_v_pwm>;
        filter-time-constant-ms = <1500>;
    };

    port_lv: port-lv {
        compatible = "libresolar,power-port";
        bus = <&bus_lv>;
        current-derived;
        temperature = <&ntc_bat>;
    };

    port_load: port-load {
        compatible = "libresolar,power-port";
        bus = <&bus_lv>;
        current-channel = <&daq_i_load>;
        switch = <&load_switch>;
    };

    port_usb: port-usb {
        compatible = "libresolar,power-port";
        bus = <&bus_lv>;
        switch = <&usb_switch>;  /* no current measurement */
    };
};
```

The `power` node is a plain container (no compatible), so it can be extended by overlays.

## Software structure

### Power model module

New module `app/src/power_model.c` (moves to `subsys/power/` in a later step). It requires the
C conversion of step 1b.

- Defines one `struct dc_bus` per `libresolar,dc-bus` node and one `struct power_port` per
  `libresolar,power-port` node with static initializers using `DT_FOREACH_STATUS_OKAY`.
- Generates constant descriptor tables from the devicetree (DAQ channel indices, flags, filter
  constants, references between nodes as table indices).
- `power_model_init()`: calibrates current sensor offsets (all current channels, as all switches
  and converters are off at startup), installs the DAQ sample filter for switched ports.
- `power_model_update()`: replaces `daq_update()`, executed in the control thread. Order:
  bus voltages → measured/switched port currents → converter values and injections →
  Kirchhoff residuals → powers → filters → temperatures.
- Access macros: `POWER_BUS(node_id)`, `POWER_PORT(node_id)`, e.g.
  `POWER_PORT(DT_NODELABEL(port_load))`.

The existing global names (`lv_bus`, `hv_bus`, `lv_terminal`, `hv_terminal`) become pointers to
or macros for the model instances, so the remaining application code changes as little as
possible in this step.

### Application objects with behavior

After step 1b, `struct load_output` and `struct pwm_switch` reference their port via a
`struct power_port *port` pointer. In this step, the pointer is initialized with the model
instance and the switch device is taken from the port node.

`struct dcdc` keeps its own fields (`inductor_current`, `power`, `temp_mosfets`), which are written by
the power model, and gets its limits from the `libresolar,dcdc` node.

## Implementation steps

Each step keeps all boards building and the tests passing.

1. **Bindings and DAQ channel labels:** Add bindings, label all DAQ channels, add `power` nodes
   to all boards and the test overlay (not used by the code yet).
2. **Power model for buses and ports:** `power_model.cpp` with measured, switched and derived
   currents and filtering. Replaces `daq_update()` and the bus/port definitions in `setup.cpp`.
   Unit tests on native_sim with the emulated DAQ and power switches, including a check that the
   results are identical to the previous `daq_update()` for the same readings.
3. **Converter node:** `struct dcdc` instantiated from the `libresolar,dcdc` node, limits moved from
   `/pcb` (`hs-voltage-max`, `ls-voltage-max`, `dcdc-current-max`, `ls-fuse`), Kconfig symbol
   `HS_MOSFET_FAIL_SAFE_PROTECTION` reads `ls-fuse` from the new node.
4. **Cleanup:** Remove `BOARD_HAS_*` macros where the model makes them obsolete, update docs.

## Open questions

1. ~~C or C++ for the model?~~ *Decided:* The application is converted to C before this step
   (step 1b of the migration plan).

2. **PWM switch as port or as converter?** Physically, the PWM switch connects the solar terminal
   to the battery bus, similar to a converter between a "PV bus" and the LV bus. Modelling it as
   a converter would be more consistent with the DC/DC, but `PwmSwitch::control()` uses the
   battery bus voltage and control targets directly.
   *Recommendation:* Keep it as a switched port in this step (as shown above) and revisit when
   the PWM control moves to the control framework.

3. ~~Composition vs. inheritance for `LoadOutput` / `PwmSwitch`?~~ *Decided:* Composition via
   port pointer, introduced with the C conversion in step 1b.

4. **Units in the devicetree:** Existing `/pcb` properties use volts and amps. Integer mV / mA
   allow fractional values (e.g. 7.5 A).
   *Recommendation:* mV / mA with `-mv` / `-ma` suffixes in the property names (used in this
   document). The existing `current-max` property of the power switches (in A) would be renamed
   accordingly.

5. **Unused channels:** `hv-ext-sense` (HPX, RC) and `i-dump-load` (Spinvert) are sampled but not
   used by the application today. They stay as DAQ channels without power model node.
