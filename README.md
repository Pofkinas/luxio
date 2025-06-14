# Luxio

**Luxio** is a reaction and movement training device.  
The system uses individually programmable LED strips to light up specific sections and uses time-of-flight (TOF) sensors to detect user interactions based on hand distance.

---

## Key Features

- Devices used:
  - STM32F411
  - VL53L0X (Time-of-Flight distance sensor)
  - WS2812 (RGB LED strip)
  - PCF8574T (I2C GPIO expander)
  - LCD 2x16 display
- Peripherals used:
  - GPIO, DMA, EXTI, I2C, PWM, Timer
- Built using [PDF (Pofkinas Development Framework)](https://github.com/Pofkinas/pdf)
- Real-time sensor feedback with LED animation control
- Expandable hardware setup

## Software Dependencies

- STM32Cube LL drivers (STM32F4 series)
- STM VL53L0X API (Time-of-Flight sensor control)
- PDF (Pofkinas Development Framework)
- FreeRTOS

## License

This project is licensed under the GNU General Public License v3.0. See the [LICENSE](LICENSE) file for more details.
