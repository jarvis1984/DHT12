## DHT12
A simple Linux I2C driver specific for DHT12 sensor.<br>
The testing platform is **[nanopi m1](http://wiki.friendlyarm.com/wiki/index.php/NanoPi_M1)**,
with Debian OS installed.<br>

## Pin Assignment
- GPIOA7(pin#32) is assigned to be the clock pin(SCL)
- GPIOA8(pin#33) is assigned to be the data pin(SDA)

## Test Result
- Currently, the transferring rate could only be 25KHZ, more or less
![](https://github.com/jarvis1984/DHT12/blob/master/2017-02-20-234443_1600x900_scrot.png)

## License
This `DHT12 driver` is freely redistributable under the BSD License.
