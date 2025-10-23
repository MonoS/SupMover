#include <iostream>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <vector>
#include "pgs.hpp"
#include "cmd.hpp"

struct t_rect {
    uint16_t x;
    uint16_t y;
    uint16_t width;
    uint16_t height;
};

struct t_compositionNumberToSaveInfo {
    uint16_t compositionNumber;
    size_t sectionIdx;
};

bool rectIsContained(t_rect container, t_rect window) {
    if ((window.x + window.width) < (container.x + container.width)
        && (window.x) > (container.x)
        && (window.y) > (container.y)
        && (window.y + window.height) < (container.y + container.height)
        )
    {
        return true;
    }
    else
    {
        return false;
    }
}

int searchSectionByPTS(std::vector<t_cutMergeSection> section, uint32_t beginPTS, uint32_t endPTS, e_cutMergeFixMode fixMode) {
    for (int i = 0; i < (int)section.size(); i++) {
        t_cutMergeSection currSection = section[i];
        int found = 0;

        if (currSection.begin <= beginPTS
            && currSection.end >= beginPTS) {
            found++;
        }

        if (currSection.begin <= endPTS
            && currSection.end >= endPTS) {
            found++;
        }

        if (currSection.begin <= beginPTS
            && currSection.end >= endPTS) {
            found += 2;
        }

        if (   fixMode == e_cutMergeFixMode::cut
            && found >= 1) {
            return i;
        }
        if (   fixMode == e_cutMergeFixMode::del
            && found >= 2) {
            return i;
        }
    }

    return -1;
}


const char* usageHelp = R"(Usage:  SupMover <input.sup> [<output.sup>] [OPTIONS ...]

OPTIONS:
  --trace
  --delay <ms>
  --move <delta x> <delta y>
  --crop <left> <top> <right> <bottom>
  --resync (<num>/<den> | <multFactor>)
  --add_zero
  --tonemap <perc>
  --cut_merge [CUT&MERGE OPTIONS ...]

CUT&MERGE OPTIONS:
  --list <list of sections>
  --format (secut | (vapoursynth | vs) | (avisynth | avs) | remap)
  --timemode (ms | frame (<num>/<den> | <fps>) | timestamp)
  --fixmode (cut | (delete | del))

Delay and resync command are executed in the order supplied.
)";


int main(int32_t argc, char** argv)
{
    size_t size, newSize;
    t_header header;

    if (argc < 3) {
        std::fprintf(stderr, "%s", usageHelp);
        return -1;
    }
    t_cmd cmd = {};

    if (!parseCMD(argc, argv, cmd)) {
        std::fprintf(stderr, "Error parsing input\n");
        return -1;
    }


    bool doDelay   = cmd.delay != 0;
    bool doMove    = cmd.move.deltaX != 0 || cmd.move.deltaY != 0;
    bool doCrop    = (cmd.crop.left + cmd.crop.top + cmd.crop.right + cmd.crop.bottom) > 0;
    bool doResync  = cmd.resync != 1;
    bool doTonemap = cmd.tonemap != 1;

    bool doModification = doDelay || doMove || doCrop || doResync || cmd.addZero || doTonemap || cmd.cutMerge.doCutMerge;
    bool doAnalysis = cmd.trace;

    FILE* input = std::fopen(cmd.inputFile, "rb");
    if (input == nullptr) {
        std::fprintf(stderr, "Unable to open input file!\n");
        return -1;
    }
    FILE* output = nullptr;
    if (doModification) {
        if (cmd.outputFile == nullptr) {
            std::fprintf(stderr, "Specified options require an output file!\n");
            return -1;
        }
        output = std::fopen(cmd.outputFile, "wb");
        if (output == nullptr) {
            std::fprintf(stderr, "Unable to open output file!\n");
            std::fclose(input);
            return -1;
        }
    }

    std::fseek(input, 0, SEEK_END);
    size = newSize = std::ftell(input);
    if (size != 0) {
        std::fseek(input, 0, SEEK_SET);

        uint8_t* buffer = (uint8_t*)std::calloc(1, size);
        uint8_t* newBuffer = buffer;
        if (buffer == nullptr) {
            return -1;
        }

        std::fread(buffer, size, 1, input);
        if (doModification || doAnalysis) {
            size_t start = 0;

            t_rect screenRect = {};

            t_WDS wds = {};
            t_PCS pcs = {};
            t_PDS pds = {};
            t_ODS ods = {};

            size_t offsetCurrPCS = 0;
            bool fixPCS = false;

            std::vector<t_compositionNumberToSaveInfo> cutMerge_compositionNumberToSave = {};
            t_cutMergeSection cutMerge_currentSection = {};
            size_t cutMerge_offsetBeginCopy = 0;
            size_t cutMerge_offsetEndCopy = 0;
            size_t cutMerge_currentNewBufferSize = 0;
            uint32_t cutMerge_currentBeginPTS = 0;
            uint32_t cutMerge_currentEndPTS = 0;
            uint16_t cutMerge_currentCompositionNumber = 0;
            uint16_t cutMerge_newCompositionNumber = 0;
            bool cutMerge_foundBegin = false;
            bool cutMerge_foundEnd = false;
            bool cutMerge_keepSection = false;
            uint32_t cutMerge_currentToSaveIdx = 0;


            for (start = 0; start < size; start = start + HEADER_SIZE + header.dataLength) {
                header = t_header::read(&buffer[start]);
                if (header.header != 0x5047) {
                    std::fprintf(stderr, "Correct header not found at position %zd, abort!\n", start);
                    std::fclose(input);
                    std::fclose(output);
                    return -1;
                }

                t_timestamp timestamp = ptsToTimestamp(header.pts);
                char timestampString[13]; // max 99:99:99.999
                std::snprintf(timestampString, 13, "%lu:%02lu:%02lu.%03lu", timestamp.hh, timestamp.mm, timestamp.ss, timestamp.ms);
                char offsetString[13];    // max 0xFFFFFFFFFF (1TB)
                std::snprintf(offsetString, 13, "%#zx", start);

                if (doResync) {
                    header.pts = (uint32_t)std::round((double)header.pts * cmd.resync);
                }
                if (doDelay) {
                    if (   cmd.delay < 0
                        && header.pts < abs(cmd.delay)) {
                        std::fprintf(stderr, "Object at timestamp %s starts before the full delay amount, it was set to start at 0!\n", timestampString);
                        header.pts = 0;
                    }
                    else {
                        header.pts = header.pts + cmd.delay;
                    }
                }

                if (doResync || doDelay) {
                    header.write(&buffer[start]);
                }

                switch (header.segmentType) {
                case e_segmentType::pds:
                    if (cmd.trace || doTonemap) {
                        pds = t_PDS::read(&buffer[start + HEADER_SIZE], header.dataLength);
                    }

                    if (cmd.trace) {
                        std::printf("  + PDS Segment: offset %s\n", offsetString);
                        std::printf("    + Palette ID: %u\n", pds.id);
                        std::printf("    + Version: %u\n", pds.versionNumber);
                        std::printf("    + Palette entries: %u\n", pds.numberOfPalettes);
                    }
                    if (doTonemap) {
                        for (int i = 0; i < pds.numberOfPalettes; i++) {
                            //convert Y from TV level (16-235) to full range
                            double expandedY   = ((((double)pds.palettes[i].valueY - 16.0) * (255.0 / (235.0 - 16.0))) / 255.0);
                            double tonemappedY = expandedY * cmd.tonemap;
                            double clampedY    = std::min(1.0, std::max(tonemappedY, 0.0));
                            double newY        = std::round((clampedY * (235.0 - 16.0)) + 16.0);

                            pds.palettes[i].valueY = (uint8_t)newY;
                        }

                        pds.write(&buffer[start + HEADER_SIZE]);
                    }
                    break;
                case e_segmentType::ods:
                    if (cmd.trace) {
                        ods = t_ODS::read(&buffer[start + HEADER_SIZE]);

                        std::printf("  + ODS Segment: offset %s\n", offsetString);
                        std::printf("    + Object ID: %u\n", ods.id);
                        std::printf("    + Version: %u\n", ods.versionNumber);
                        if (ods.sequenceFlag != e_sequenceFlag::firstAndLast) {
                            std::printf("    + Sequence flag: ");
                            switch (ods.sequenceFlag) {
                                case e_sequenceFlag::last:  std::printf("Last\n"); break;
                                case e_sequenceFlag::first: std::printf("First\n"); break;
                                default:   std::printf("%#x\n", ods.sequenceFlag); break;
                            }
                        }
                        std::printf("    + Size: %ux%u\n", ods.width, ods.height);
                    }
                    break;
                case e_segmentType::pcs:
                    if (cmd.trace) {
                        std::printf("+ DS\n");
                        std::printf("  + PTS: %s\n", timestampString);
                        std::printf("  + PCS Segment: offset %s\n", offsetString);
                    }
                    if (cmd.trace || doMove | doCrop || cmd.addZero || cmd.cutMerge.doCutMerge) {
                        pcs = t_PCS::read(&buffer[start + HEADER_SIZE]);
                        offsetCurrPCS = start;

                        if (cmd.trace) {
                            std::printf("    + Video size: %ux%u\n", pcs.width, pcs.height);
                            std::printf("    + Composition number: %u\n", pcs.compositionNumber);
                            std::printf("    + Composition state: ");
                            switch (pcs.compositionState) {
                                case e_compositionState::normal:           std::printf("Normal\n"); break;
                                case e_compositionState::acquisitionPoint: std::printf("Aquisition Point\n"); break;
                                case e_compositionState::epochStart:       std::printf("Epoch Start\n"); break;
                                default:                                   std::printf("%#x\n", pcs.compositionState); break;
                            }
                            if (pcs.paletteUpdateFlag == 0x80) {
                                std::printf("    + Palette update: True\n");
                            }
                            std::printf("    + Palette ID: %u\n", pcs.paletteID);
                            for (int i = 0; i < pcs.numberOfCompositionObjects; i++) {
                                std::printf("    + Composition object\n");
                                t_compositionObject object = pcs.compositionObjects[i];
                                std::printf("      + Object ID: %u\n", object.objectID);
                                std::printf("      + Window ID: %u\n", object.windowID);
                                std::printf("      + Position: %u,%u\n", object.horizontalPosition, object.verticalPosition);
                                if (object.croppedAndForcedFlag & e_objectFlags::forced) {
                                    std::printf("      + Forced display: True\n");
                                }
                                if (object.croppedAndForcedFlag & e_objectFlags::cropped) {
                                    std::printf("      + Cropped: True\n");
                                    std::printf("      + Cropped position: %u,%u\n", object.croppedHorizontalPosition, object.croppedVerticalPosition);
                                    std::printf("      + Cropped size: %ux%u\n", object.croppedWidth, object.croppedHeight);
                                }
                            }
                        }

                        if (doCrop) {
                            screenRect.x      = 0 + cmd.crop.left;
                            screenRect.y      = 0 + cmd.crop.top;
                            screenRect.width  = pcs.width  - (cmd.crop.left + cmd.crop.right);
                            screenRect.height = pcs.height - (cmd.crop.top  + cmd.crop.bottom);

                            pcs.width  = screenRect.width;
                            pcs.height = screenRect.height;

                            if (pcs.numberOfCompositionObjects > 1) {
                                std::fprintf(stderr, "Multiple composition object at timestamp %s! Please Check!\n", timestampString);
                            }

                            for (int i = 0; i < pcs.numberOfCompositionObjects; i++) {
                                if (pcs.compositionObjects[i].croppedAndForcedFlag & e_objectFlags::cropped) {
                                    std::fprintf(stderr, "Object Cropped Flag set at timestamp %s! Implement it!\n", timestampString);
                                }

                                if (cmd.crop.left > pcs.compositionObjects[i].horizontalPosition) {
                                    pcs.compositionObjects[i].horizontalPosition = 0;
                                }
                                else {
                                    pcs.compositionObjects[i].horizontalPosition -= cmd.crop.left;
                                }

                                if (cmd.crop.top > pcs.compositionObjects[i].verticalPosition) {
                                    pcs.compositionObjects[i].verticalPosition = 0;
                                }
                                else {
                                    pcs.compositionObjects[i].verticalPosition -= cmd.crop.top;
                                }
                            }
                        }

                        if (cmd.addZero) {
                            if (pcs.compositionNumber == 0) {
                                uint8_t zeroBuffer[60];
                                uint8_t pos = 0;
                                t_header zeroHeader(header);
                                zeroHeader.pts = 0;
                                zeroHeader.dataLength = 11; //Length of upcoming PCS
                                zeroHeader.write(&zeroBuffer[pos]);
                                pos += 13;
                                t_PCS zeroPcs(pcs);
                                zeroPcs.compositionState = 0;
                                zeroPcs.paletteUpdateFlag = 0;
                                zeroPcs.paletteID = 0;
                                zeroPcs.numberOfCompositionObjects = 0;
                                zeroPcs.write(&zeroBuffer[pos]);
                                pos += zeroHeader.dataLength;

                                zeroHeader.segmentType = e_segmentType::wds; // WDS
                                zeroHeader.dataLength = 10; //Length of upcoming WDS
                                zeroHeader.write(&zeroBuffer[pos]);
                                pos += 13;
                                t_WDS zeroWds;
                                zeroWds.numberOfWindows = 1;
                                zeroWds.windows[0].id = 0;
                                zeroWds.windows[0].horizontalPosition = 0;
                                zeroWds.windows[0].verticalPosition = 0;
                                zeroWds.windows[0].width = 0;
                                zeroWds.windows[0].height = 0;
                                zeroWds.write(&zeroBuffer[pos]);
                                pos += zeroHeader.dataLength;

                                zeroHeader.segmentType = e_segmentType::end; // END
                                zeroHeader.dataLength = 0; //Length of upcoming END
                                zeroHeader.write(&zeroBuffer[pos]);
                                pos += 13;

                                std::fprintf(stderr, "Writing %d bytes as first display set\n", pos);
                                std::fwrite(zeroBuffer, pos, 1, output);

                                //For Cut&Merge functionality we don't need to save the added segment as it
                                //is saved in the resulting file automatically
                            }
                            pcs.compositionNumber += 1;
                        }

                        if (cmd.cutMerge.doCutMerge) {
                            if (!cutMerge_foundBegin) {
                                cutMerge_foundBegin = true;
                                cutMerge_currentBeginPTS = header.pts;
                                cutMerge_currentCompositionNumber = pcs.compositionNumber;
                            }
                            else if (!cutMerge_foundEnd) {
                                cutMerge_foundEnd = true;
                                cutMerge_currentEndPTS = header.pts;
                            }
                        }

                        pcs.write(&buffer[start + HEADER_SIZE]);
                    }
                    break;
                case e_segmentType::wds:
                    if (cmd.trace) {
                        std::printf("  + WDS Segment: offset %s\n", offsetString);
                    }
                    fixPCS = false;
                    if (cmd.trace || doMove || doCrop) {
                        wds = t_WDS::read(&buffer[start + HEADER_SIZE]);

                        if (wds.numberOfWindows > 1 && doModification) {
                            std::fprintf(stderr, "Multiple windows at timestamp %s! Please Check!\n", timestampString);
                        }

                        if (cmd.trace) {
                            for (int i = 0; i < wds.numberOfWindows; i++) {
                                std::printf("    + Window\n");
                                t_window window = wds.windows[i];
                                std::printf("      + Window ID: %u\n", window.id);
                                std::printf("      + Position: %u,%u\n", window.horizontalPosition, window.verticalPosition);
                                std::printf("      + Size: %ux%u\n", window.width, window.height);
                            }
                        }

                        if (doMove) {
                            for (int i = 0; i < wds.numberOfWindows; i++) {
                                t_window *window = &wds.windows[i];
                                int16_t minDeltaX = -(int16_t)window->horizontalPosition;
                                int16_t minDeltaY = -(int16_t)window->verticalPosition;
                                int16_t maxDeltaX = pcs.width - (window->horizontalPosition + window->width);
                                int16_t maxDeltaY = pcs.height - (window->verticalPosition + window->height);
                                int16_t clampedDeltaX = std::min(std::max(cmd.move.deltaX, minDeltaX), maxDeltaX);
                                int16_t clampedDeltaY = std::min(std::max(cmd.move.deltaY, minDeltaY), maxDeltaY);

                                window->horizontalPosition += clampedDeltaX;
                                window->verticalPosition += clampedDeltaY;

                                for (int j = 0; j < pcs.numberOfCompositionObjects; j++) {
                                    t_compositionObject *object = &pcs.compositionObjects[j];
                                    if (object->windowID != window->id) continue;
                                    if (object->croppedAndForcedFlag & e_objectFlags::cropped) {
                                        object->croppedHorizontalPosition += clampedDeltaX;
                                        object->croppedVerticalPosition += clampedDeltaY;
                                    }
                                    object->horizontalPosition += clampedDeltaX;
                                    object->verticalPosition += clampedDeltaY;
                                    fixPCS = true;
                                }
                            }
                        }

                        if (doCrop) {
                            for (int i = 0; i < wds.numberOfWindows; i++) {
                                t_rect wndRect;
                                uint16_t corrHor = 0;
                                uint16_t corrVer = 0;

                                wndRect.x      = wds.windows[i].horizontalPosition;
                                wndRect.y      = wds.windows[i].verticalPosition;
                                wndRect.width  = wds.windows[i].width;
                                wndRect.height = wds.windows[i].height;

                                if (wndRect.width > screenRect.width
                                    || wndRect.height > screenRect.height) {
                                    std::fprintf(stderr, "Window is bigger than new screen area at timestamp %s\n", timestampString);
                                    std::fprintf(stderr, "Implement it!\n");
                                    /*
                                    pcs.width = wndRect.width;
                                    pcs.height = wndRect.height;
                                    fixPCS = true;
                                    */
                                }
                                else {
                                    if (!rectIsContained(screenRect, wndRect)) {
                                        std::fprintf(stderr, "Window is outside new screen area at timestamp %s\n", timestampString);

                                        uint16_t wndRightPoint    = wndRect.x    + wndRect.width;
                                        uint16_t screenRightPoint = screenRect.x + screenRect.width;
                                        if (wndRightPoint > screenRightPoint) {
                                            corrHor = wndRightPoint - screenRightPoint;
                                        }

                                        uint16_t wndBottomPoint    = wndRect.y    + wndRect.height;
                                        uint16_t screenBottomPoint = screenRect.y + screenRect.height;
                                        if (wndBottomPoint > screenBottomPoint) {
                                            corrVer = wndBottomPoint - screenBottomPoint;
                                        }

                                        if (corrHor + corrVer != 0) {
                                            std::fprintf(stderr, "Please check\n");
                                        }
                                    }
                                }

                                if (cmd.crop.left > wds.windows[i].horizontalPosition) {
                                    wds.windows[i].horizontalPosition = 0;
                                }
                                else {
                                    wds.windows[i].horizontalPosition -= (cmd.crop.left + corrHor);
                                }

                                if (cmd.crop.top > wds.windows[i].verticalPosition) {
                                    wds.windows[i].verticalPosition = 0;
                                }
                                else {
                                    wds.windows[i].verticalPosition -= (cmd.crop.top + corrVer);
                                }

                                if (corrVer != 0 || corrHor != 0) {
                                    for (int j = 0; j < pcs.numberOfCompositionObjects; j++) {
                                        if (pcs.compositionObjects[j].windowID != wds.windows[i].id) continue;
                                        pcs.compositionObjects[j].verticalPosition -= corrVer;
                                        pcs.compositionObjects[j].horizontalPosition -= corrHor;
                                    }
                                    fixPCS = true;
                                }
                            }
                        }

                        if (fixPCS) {
                            pcs.write(&buffer[offsetCurrPCS + HEADER_SIZE]);
                        }
                        wds.write(&buffer[start + HEADER_SIZE]);

                    }
                    break;
                case e_segmentType::end:
                    if (cmd.trace) {
                        std::printf("  + END Segment: offset %s\n", offsetString);
                    }

                    if (cmd.cutMerge.doCutMerge) {
                        if (cutMerge_foundEnd) {
                            cutMerge_foundBegin = false;
                            cutMerge_foundEnd = false;
                            int idxFound = searchSectionByPTS(cmd.cutMerge.section, cutMerge_currentBeginPTS, cutMerge_currentEndPTS, cmd.cutMerge.fixMode);
                            if (idxFound != -1) {
                                t_compositionNumberToSaveInfo compositionNumberToSaveInfo = {};
                                compositionNumberToSaveInfo.compositionNumber = cutMerge_currentCompositionNumber;
                                compositionNumberToSaveInfo.sectionIdx = idxFound;

                                cutMerge_compositionNumberToSave.push_back(compositionNumberToSaveInfo);
                            }
                        }
                    }

                    screenRect = {};
                    pcs = {};
                    wds = {};
                    break;
                }
            }

            //Cut&Merge functionality is done in two pass as we need the resulting timestamp to do it
            //and we need to fix all the segment with the sum of all the in-between section delay
            if (cmd.cutMerge.doCutMerge) {
                newBuffer = (uint8_t*)std::calloc(1, size);

                header = {};
                cutMerge_foundBegin = false;
                cutMerge_foundEnd = false;
                cutMerge_keepSection = false;
                cutMerge_currentToSaveIdx = 0;
                cutMerge_newCompositionNumber = 0;
                if (cmd.addZero) {
                    cutMerge_newCompositionNumber = 1;
                }
                for (start = 0; start < size; start = start + HEADER_SIZE + header.dataLength) {
                    if (cutMerge_currentToSaveIdx >= cutMerge_compositionNumberToSave.size()) {
                        break;
                    }
                    header = t_header::read(&buffer[start]);

                    //For Cut&Merge we only need to handle the header (for the PTS), the PCS (to
                    //get the compositionNumber) and the END segment (to know when it finished)
                    if (header.segmentType == e_segmentType::pcs)
                    {
                        pcs = t_PCS::read(&buffer[start + HEADER_SIZE]);
                        if (!cutMerge_foundBegin) {
                            cutMerge_foundBegin = true;
                            cutMerge_currentBeginPTS = header.pts;
                            cutMerge_currentCompositionNumber = pcs.compositionNumber;

                            if (cutMerge_compositionNumberToSave[cutMerge_currentToSaveIdx].compositionNumber == cutMerge_currentCompositionNumber) {
                                cutMerge_keepSection = true;
                                cutMerge_offsetBeginCopy = start;
                                cutMerge_currentSection = cmd.cutMerge.section[cutMerge_compositionNumberToSave[cutMerge_currentToSaveIdx].sectionIdx];
                                pcs.compositionNumber = cutMerge_newCompositionNumber;
                                pcs.write(&buffer[start + HEADER_SIZE]);
                            }
                        }
                        else if (!cutMerge_foundEnd) {
                            cutMerge_foundEnd = true;
                            cutMerge_currentEndPTS = header.pts;
                            if (cutMerge_keepSection) {
                                pcs.compositionNumber = cutMerge_newCompositionNumber;

                                pcs.write(&buffer[start + HEADER_SIZE]);
                            }
                        }
                    }

                    if (cutMerge_keepSection) {
                        if (!cutMerge_foundEnd) {
                            if (cutMerge_currentBeginPTS < cutMerge_currentSection.begin) {
                                header.pts = cutMerge_currentSection.begin;
                            }
                        }
                        else {
                            if (cutMerge_currentEndPTS > cutMerge_currentSection.end) {
                                header.pts = cutMerge_currentSection.end;
                            }
                        }

                        header.pts -= cutMerge_currentSection.delay_until;

                        header.write(&buffer[start]);
                    }

                    if (header.segmentType == e_segmentType::end)
                    {
                        if (cutMerge_foundEnd) {
                            if (cutMerge_keepSection) {
                                cutMerge_currentToSaveIdx++;

                                cutMerge_offsetEndCopy = start + HEADER_SIZE + header.dataLength;

                                std::memcpy(&newBuffer[cutMerge_currentNewBufferSize],
                                    &buffer[cutMerge_offsetBeginCopy],
                                    (cutMerge_offsetEndCopy - cutMerge_offsetBeginCopy));

                                cutMerge_currentNewBufferSize += (cutMerge_offsetEndCopy - cutMerge_offsetBeginCopy);
                                cutMerge_newCompositionNumber++;
                            }

                            cutMerge_foundBegin = false;
                            cutMerge_foundEnd = false;
                            cutMerge_keepSection = false;
                            cutMerge_offsetBeginCopy = -1;
                            cutMerge_offsetEndCopy = -1;

                        }
                    }
                }
                newSize = cutMerge_currentNewBufferSize;
            }
            else {
                newBuffer = buffer;
                newSize = size;
            }
        }

        if (doModification) {
            std::fwrite(newBuffer, newSize, 1, output);
        }
    }

    std::fclose(input);
    if (output != nullptr) {
        std::fclose(output);
    }

    return 0;
}
