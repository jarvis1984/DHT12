## DHT12
A simple Linux I2C driver specific for DHT12 sensor.<br>
The testing platform is **[nanopi m1](http://wiki.friendlyarm.com/wiki/index.php/NanoPi_M1)**,
with Debian OS installed.<br>

## Pin Assignment
- GPIOA7(pin#32) is assigned to be the clock pin(SCL)
- GPIOA8(pin#33) is assigned to be the data pin(SDA)

## Test Result
- The transferring rate could reach about 50KHZ
![](https://github.com/jarvis1984/DHT12/blob/master/2017-02-26-210826_1600x900_scrot.png)

## HISTORY
- V0.1 SIMPLE LOGIC
  - Use kernel delay functions to control the timimg of signals
  - The transferring rate is about 25KHZ
  - Higher rates will be disturbed by clock stretching

- V0.2 SPEEDING UP
  - Detection of clock stretching is provided
  - Use spinlock to disable interrupts
  - The rate could reach 50KHZ
  - The overhead of gpio functions to detect clock is too high,
    so that the cycle time is too long

## License
This `DHT12 driver` is freely redistributable under the BSD License.
