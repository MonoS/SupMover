# SupMover
SupMover - Shift timings and Screen Area of PGS/Sup subtitle

# Usage
`SupMover (<input.sup> <output.sup>) [delay (ms)] [crop (<left> <top> <right> <bottom>)] [resync (<num>/<den> | multFactor)]`
`SupMover (<input.sup> <output.sup> <ms>)` old syntax, kept for backward compatibility

# Modes
* delay
  * Apply a milliseconds delay, positive or negative, to all the subpic of the subtitle, it can be fractional as the SUP speficication have a precision of 1/90ms
* resync
  * Multiply all the timestamp by this factor, this can also be supplied as a fraction
* crop
  * Crop the windows area of all subpic by the inputed parameters.
  * This is done losslessly by only shifting the windows position (the image data is left untouched).
  * Some functionality is not fully tested as i didn't found any subtitles that used those functionality (eg Object Cropped Flag, multiple Composition Object/Windows). If the subtitle windows would go outside the new screen size a warning is returned, i advise to check and see if those are correct.
* delay + resync
  * If both modes are selected the delay will be adjusted if it comes before the resync parameter, for example if the program is launched with `delay 1000 resync 1.001` it will be internally adjusted to 1001ms, instead if it's launched with `resync 1.001 delay 1000` it will not
 
# Build instruction
```
g++.exe -Wall -fexceptions -O2 -Wall -Wextra  -c main.cpp -o main.o
g++.exe -o SupMover.exe main.o -s -static
```
