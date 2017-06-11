# led-matrix-timer
Analogue timer using a common cathode 8x8 RGB LED matrix

The timer has a fix 4 minute timing period, however it is easy to change the period in the code. 

Before start the timer shows a  full blue screen. At the push of the button the countdown starts. On a green background a red dot will appear for every started 3.75 seconds (or period / 64 if you change the original value). After 64 updates on the screen, the 4 minutes is up and it will start blinking red.

Pressing the button any time during or after the coount down will set the clock to pre-start condition.
