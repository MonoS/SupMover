# SupMover
SupMover - Shift timings and Screen Area of PGS/Sup subtitle

# Usage
```
Usage:  SupMover <input.sup> [<output.sup>] [OPTIONS ...]

OPTIONS:
  --trace
  --delay <ms>
  --move <delta x> <delta y>
  --symmetrical
  --move-list <list of sections>
  --crop <left> <top> <right> <bottom>
  --resync (<num>/<den> | <multFactor>)
  --add_zero
  --tonemap <perc>
  --cutmerge-list <list of sections> [--cutmerge-fixmode ({cut} | (del | delete))]
  --setforced
  --setforced-list <list of sections>
  --unsetforced
  --unsetforced-list <list of sections>
  [LIST FORMAT OPTION]

LIST FORMAT OPTION
  --list-format ({secut} | (vapoursynth | vs) | (avisynth | avs) | remap)
  --list-timemode ({timestamp} | ms | frame (<num>/<den> | <fps>))
```

# Options
* `--trace`
  * Print contents and structure of input file segments

* `--delay `
  * Apply a milliseconds delay, positive or negative, to all the subpic of the subtitle, it can be fractional as the SUP speficication have a precision of 1/90ms
* `--resync`
  * Multiply all the timestamp by this factor, this can also be supplied as a fraction like `25025/24000`
* `--delay` + `--resync`
  * If both modes are selected the delay will be adjusted if it comes before the resync parameter, for example if the program is launched with `--delay 1000 --resync 1.001` it will be internally adjusted to 1001ms, instead if it's launched with `--resync 1.001 --delay 1000` it will not

* `--move`
  * Shift the windows position of all subpic by the inputed parameters (the image data is left untouched).
  * Position is clamped to the screen edges so that windows are always fully contained within the screen area.
* `--symmetrical`
  * For the upper half of the video frame, this reverses vertical movement offsets (for the `--move` command described above). Similarly, this also reverses horizontal movement offsets for the left half of the video frame. This affects both vertical and horizontal at the same time, so if symmetrical movement is only desired on  one axis, you must execute supmover twice.
  * For example, `--symmetrical --move 0 15` will move all subtitles in the lower half down, and the upper half will be moved upwards, but they will not be moved sideways. `--move -20 0 --symmetrical` will result in all subtitles on the right half of the screen moving inwards in a left direction, and all subtitles on the left will move inwards in a right direction, without affecting their vertical positioning. 
* `--move-list`
  * Supply a list of section to apply the move command to

* `--crop`
  * Crop the windows area of all subtitle, this is done losslessly by only shifting the windows position (the image data is left untouched).
  * Crop functionality is not exstensivelly tested when multiple Composition Object or Windows are present or when the windows are is outside the new screen area, a warning is issued if that's the case and i strongly advise to check the resulting subtitle with a video player, also handling of the Object Cropped flag and windows area bigger than the new screen area is not implemented, a warning is issued if needed
  * If both `--move` and `--crop` are selected, the crop is performed after the move.

* `--add_zero`
  * Some media players (especially Plex) don't correctly sync `*.sup` subtitles.  They seem to ignore any delay before the first 'display set'. This option adds a dummy 'display set' at time 0 so subsequent timestamps are correctly interpreted.

* `--tonemap`
  * Change the brightness of the subtitle applying the specified percentage factor to all the palette's luminance value, similar to https://github.com/quietvoid/subtitle_tonemap , the percentage must be specified as a decimal value with 1 as 100%, it can be bigger than 1 to increase brightness, right now it only works on B/W subtitles, it will change the colors in unexpected ways on non B/W subtitles

* `--cutmerge-list`
  * allows to cut subtitle and optionally, if more sections are specified, to merge the cuts into a single subtitle file with the subsequent cuts shifted to have them begin at the end of the previous section. It is possible to personalize its functionality with some options
* `--cutmerge-fixmode`: allow to specify how to treat subtitles which are not fully contained in a section
  * `cut`: cut the subtitle so that it is fully contained in the section
  * `delete` or `del`: delete the subtitle if not fully contained inside a section

* `--setforced/--unsetforced`: set or unset all subtitles as forced
* `--setforced-list/--unsetforced-list`: set or unset the subtitle in the specified sections as forced

* `--list-format`: format of the list
  * `secut`: uses the same format as SECut. eg `1000-2000;3000-4000`
  * `vapoursynth` or `vs`: uses the same format as vapoursynth split sintax, additionally if `--timemode` is set as `frame` the range will be treated inclusively at the start and exclusively at the end. eg `[1000:2001] [3000:4001]`
  * `avisynth` or `avs`: uses the same format as avisynth trim sintax. Eg `(1000,2000) (3000,4000)`
  * `remap`: uses the same format as [Vapoursynth-RemapFrames ReplaceFrameSimple](https://github.com/Irrational-Encoding-Wizardry/Vapoursynth-RemapFrames#replaceframessimple). Eg `[1000 2000] [3000 4000]`
* `--list-timemode`: allow to specifies how to read the values of the sections
  * `ms`
  * `frame`: if selected a framerate MUST be specified, as a fraction like `24000/1001` or as a number like `23.976`
  * `timestamp`: if selected the sections MUST be in the format hh:mm:ss.ms and can't be used with `--list-format vapoursynth`

# Build instruction
```
g++.exe -Wall -fexceptions -O2 -Wall -Wextra -std=c++17 -c main.cpp -o main.o
g++.exe -o SupMover.exe main.o -s -static
```
or
```
clang-cl.exe main.cpp /std:c++17 /O2 -o SupMover.exe
```