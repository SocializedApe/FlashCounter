# Note

Design - button is on female wires, because it did not connect with the breadboard. Pins were used for interrupts instead of using analog readings, mainly for educational purposes.

Timing budget - the timer ticks every 10 ms. The ISRs just set the flags, and the main loop consists of a couple of if, else statements and pin readings, therefore everything fits very generously.

Test results - first flash did not register. Overall it works well as long as there is not too much ambient lighting.

Known issues - light bulb frequency automatically counts it up. Issues with ambient lighting, might be fixable by using the actual reading from analog A0 instead of using pin as RISING as the criteria. Or some more detailed timing ideas, for example it must be flashed for let's say 3 timer periods.
