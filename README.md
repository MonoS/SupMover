# SupMover
SupMover - Shift timings and Screen Area of PGS/Sup subtitle

# Usage
`SupMover (<input.sup> <output.sup>) [delay (ms)] [crop (<left> <top> <right> <bottom>)] [resync (<num>/<den> | multFactor)] [add_zero] [tonemap <perc>] [cut_merge <Cut&Merge option>]`

`SupMover (<input.sup> <output.sup> <ms>)` old syntax, kept for backward compatibility

# Modes
* delay
  * Apply a milliseconds delay, positive or negative, to all the subpic of the subtitle, it can be fractional as the SUP speficication have a precision of 1/90ms
* resync
  * Multiply all the timestamp by this factor, this can also be supplied as a fraction
* crop
  * Crop the windows area of all subpic by the inputed parameters.
  * This is done losslessly by only shifting the windows position (the image data is left untouched).
  * Crop functionality is not exstensivelly tested when multiple Composition Object or Windows are present or when the windows are is outside the new screen area, a warning is issued if that's the case and i strongly advise to check the resulting subtitle with a video player, also handling of the Object Cropped flag and windows area bigger than the new screen area is not implemented, a warning is issued if needed
* delay + resync
  * If both modes are selected the delay will be adjusted if it comes before the resync parameter, for example if the program is launched with `delay 1000 resync 1.001` it will be internally adjusted to 1001ms, instead if it's launched with `resync 1.001 delay 1000` it will not
* add_zero
  * Some media players (especially Plex) don't correctly sync `*.sup` subtitles.  They seem to ignore any delay before the first 'display set'.  This option adds a dummy 'display set' at time 0 so subsequent timestamps are correctly interpreted.
* tonemap
  * Change the brightness of the subtitle applying the specified percentage factor to all the palette's luminance value, similar to https://github.com/quietvoid/subtitle_tonemap , the percentage must be specified as a decimal value with 1 as 100%, it can be bigger than 1 to increase brightness
* cut_merge
  * allows to cut subtitle and optionally, if more sections are specified, to merge the cuts into a single subtitle file with the subsequent cuts shifted to have them begin at the end of the previous section. It is possible to personalize its functionality with some options
	* list: specifies the space-separated list of sections, `format` and `timemode` can be used to further configure the parsing of this list. The list must be contained inside double quotes
	* format: the format type of the list
	  * secut: uses the same format as SECut. eg `1000-2000;3000-4000`
	  * vapoursynt or vs: uses the same format as vapoursynth split sintax, additionally if `timemode` is set as `frame` the range will be treated inclusively at the start and exclusively at the end. eg `[1000:2001] [3000:4001]`
	  * avisynth or avs: uses the same format as avisynth trim sintax. Eg `(1000,2000) (3000,4000)`
	  * remap: uses the same format as [Vapoursynth-RemapFrames ReplaceFrameSimple](https://github.com/Irrational-Encoding-Wizardry/Vapoursynth-RemapFrames#replaceframessimple). Eg `[1000 2000] [3000 4000]`
    * timemode: allow to specifies how to read the values of the sections
	  * ms
	  * frame: if selected a framerate MUST be specified, as a fraction like `24000/1001` or as a number like `23.976`
	  * timestamp: if selected the sections MUST be in the format hh:mm:ss.ms and can't be used with `format vapoursynth`
    * fixmode: allow to specify how to treat subtitles which are not fully contained in a section
	  * del or delete: delete the subtitle if not fully contained inside a section
	  * cut: cut the subtitle so that it is fully contained in the section
  * if no further option is specified it will works like secut so like the following command line `format secut timemode ms fixmode delete`


# Build instruction
```
g++.exe -Wall -fexceptions -O2 -Wall -Wextra  -c main.cpp -o main.o
g++.exe -o SupMover.exe main.o -s -static
```
