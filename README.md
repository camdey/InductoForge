# InductoForge
PID-controlled induction furnace for forging/melting metal

Originally created in 2016 using two Arduino Nanos, this project provided a precise temperature controlled induction furnace with a PID control closed-loop feedback system.
These days it would be possible to have a single Arduino-compatible board (e.g. Due, Teensy, Grand Central) to operate both programs and run everything much faster!

The DisplayMaster file provided the UI for the user and either sent requests for data or commands for control to the ControlSlave device via I2C communication.
The user would set a temperature setting on the DisplayMaster device and send that command to the ControlSlave, which would also feed back the temperature reading as well as other sensor readings.


Uploaded here for reference and so I don't lose the original code.
