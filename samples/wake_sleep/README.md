

#### Example output:

    I: Waiting 6 secs for modem to boot...
    I: Max allowed PM mode: dh0
    I: Sleep mode: enable
    I: HIFC mode: A
    I: Setting APN to CATM.T-MOBILE.COM and IPV4
    I: Setting bands to 2, 4, 12
    I: Setting boot delay to 0
    I: rsi_wlan_get after rsi_wireless_init returned 0, mac: 90:35:ea:3d:0e:c8
    I: RS9116W WiFi driver initialized
    I: lps22hh@5c: int on gpio@4000a0f0.15
    *** Booting Zephyr OS build 0da53d0c285c ***
    I: Setting up GPIO and ISR structures
    I: Set up button at gpio@4000a030 pin 13



                            Welcome to T-Mobile DevEdge!

    This application aims to demonstrate the Gecko's Energy Mode 2 (EM2) (Deep
    Sleep Mode) and Wake capabilities in conjunction with the SW0 interrupt pin,
    which is connected to the user pushbutton switch of the DevEdge module.

    Press and release the user pushbutton to advance from one power management
    mode to the next.

    pm_thread(): awake
    Turn on white LED
    device_action[0].value: 0, device_action[0].name: PM_DEVICE_ACTION_SUSPEND
    I: Suspend successful
    I: pm_thread(): murata_1sc call status: 0 (Success), device state: suspended
    I: pm_thread(): rs9116w@0 call status: 0 (Success), device state: suspended
    E: Cannot switch PWM 0x189f0 power state
    E: Cannot switch PWM 0x189f0 power state
    E: Cannot switch PWM 0x189f0 power state
    E: Cannot switch PWM 0x189f0 power state
    I: pm_thread(): pwmleds call status: 0 (Success), device state: suspended
    I: pm_thread(): sonycxd5605@24 call status: 0 (Success), device state: suspended
    I: pm_thread(): tsl2540@39 call status: 0 (Success), device state: suspended
    Turn off LEDs
    pm_thread(): asleep

    pm_thread(): awake
    Turn on blue LED
    device_action[1].value: 2, device_action[1].name: PM_DEVICE_ACTION_TURN_OFF
    W: pm_thread(): murata_1sc call status: -134 (Unsupported value), device state: off
    I: pm_thread(): rs9116w@0 call status: 0 (Success), device state: off
    E: Cannot switch PWM 0x189f0 power state
    E: Cannot switch PWM 0x189f0 power state
    E: Cannot switch PWM 0x189f0 power state
    E: Cannot switch PWM 0x189f0 power state
    I: pm_thread(): pwmleds call status: 0 (Success), device state: off
    I: pm_thread(): sonycxd5605@24 call status: 0 (Success), device state: off
    W: pm_thread(): tsl2540@39 call status: -134 (Unsupported value), device state: off
    Turn off LEDs
    pm_thread(): asleep

    pm_thread(): awake
    Turn on green LED
    device_action[2].value: 3, device_action[2].name: PM_DEVICE_ACTION_TURN_ON
    W: pm_thread(): murata_1sc call status: -134 (Unsupported value), device state: suspended
    W: pm_thread(): rs9116w@0 call status: -1 (Not owner), device state: off
    E: Cannot switch PWM 0x189f0 power state
    E: Cannot switch PWM 0x189f0 power state
    E: Cannot switch PWM 0x189f0 power state
    E: Cannot switch PWM 0x189f0 power state
    I: pm_thread(): pwmleds call status: 0 (Success), device state: suspended
    I: pm_thread(): sonycxd5605@24 call status: 0 (Success), device state: suspended
    W: pm_thread(): tsl2540@39 call status: -134 (Unsupported value), device state: suspended
    Turn off LEDs
    pm_thread(): asleep

    pm_thread(): awake
    Turn on red LED
    device_action[3].value: 1, device_action[3].name: PM_DEVICE_ACTION_RESUME
    I: Resume successful
    I: pm_thread(): murata_1sc call status: 0 (Success), device state: active
    W: pm_thread(): rs9116w@0 call status: -1 (Not owner), device state: off
    E: Cannot switch PWM 0x189f0 power state
    E: Cannot switch PWM 0x189f0 power state
    E: Cannot switch PWM 0x189f0 power state
    E: Cannot switch PWM 0x189f0 power state
    I: pm_thread(): pwmleds call status: 0 (Success), device state: active
    I: pm_thread(): sonycxd5605@24 call status: 0 (Success), device state: active
    I: pm_thread(): tsl2540@39 call status: 0 (Success), device state: active
    Turn off LEDs
    pm_thread(): asleep

