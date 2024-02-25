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

double const MS_TO_PTS_MULT = 90.0f;

struct t_rect {
    uint16_t x;
    uint16_t y;
    uint16_t width;
    uint16_t height;
};

struct t_timestamp {
    unsigned long hh;
    unsigned long mm;
    unsigned long ss;
    unsigned long ms;
};

struct t_move {
    int16_t deltaX;
    int16_t deltaY;
};

struct t_crop {
    uint16_t left;
    uint16_t top;
    uint16_t right;
    uint16_t bottom;
};

enum e_cutMergeFormat : uint8_t {
    secut = 0,       //"[1000-2000] [3000-4000]" both inclusive
    vapoursynth = 1, //"[1000:2001] [3000:4001]" start inclusive, end exclusive
    avisynth = 2,    //"(1000,2000) (3000,4000)" both inclusive
    remap = 3        //"[1000 2000] [3000 4000]" both inclusive
};

enum e_cutMergeTimeMode : uint8_t {
    ms = 0,
    frame = 1,
    timestamp = 2
};

enum e_cutMergeFixMode : uint8_t {
    del = 0, //delete section if not fully contained
    cut = 0  //cut begin and/or end to match current section
};

struct t_cutMergeSection {
    uint32_t begin;
    uint32_t end;
    uint32_t delay_until;
};

bool CompareCutMergeSection(t_cutMergeSection a, t_cutMergeSection b) {
    return (a.begin < b.begin);
}

struct t_compositionNumberToSaveInfo {
    uint16_t compositionNumber;
    size_t sectionIdx;
};

struct t_cutMerge {
    bool doCutMerge = false;
    e_cutMergeFormat format;
    e_cutMergeTimeMode timeMode;
    e_cutMergeFixMode fixMode;
    double fps;
    std::string list;
    std::vector<t_cutMergeSection> section;
};

struct t_cmd {
    const char* inputFile = nullptr;
    const char* outputFile = nullptr;
    bool trace = false;
    int32_t delay = 0;
    t_move move = {};
    t_crop crop = {};
    double resync = 1;
    bool addZero = false;
    double tonemap = 1;
    t_cutMerge cutMerge = {};
};

void toLower(std::string& str) {
    std::transform(str.begin(), str.end(), str.begin(), [](int i) {
            return std::tolower(i);
    });
}

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

t_timestamp PTStoTimestamp(uint32_t pts) {
    t_timestamp res;

    res.ms = ((int)floor((float)pts / MS_TO_PTS_MULT));
    res.ss = res.ms / 1000;
    res.mm = res.ss / 60;
    res.hh = res.mm / 60;

    res.ms %= 1000;
    res.ss %= 60;
    res.mm %= 60;

    return res;
}

int timestampToMs(char* timestamp) {
    int tokenRead;
    int hh, mm, ss, ms;

    tokenRead = std::sscanf(timestamp, "%d:%d:%d.%d", &hh, &mm, &ss, &ms);
    if (tokenRead != 4) {
        return -1;
    }

    return ((((hh * 60 * 60) + (mm * 60) + ss) * 1000) + ms);

}

int SearchSectionByPTS(std::vector<t_cutMergeSection> section, uint32_t beginPTS, uint32_t endPTS, e_cutMergeFixMode fixMode) {
    for (int i = 0; i < section.size(); i++) {
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

bool parseCutMerge(t_cutMerge* cutMerge) {
    std::string pattern;

    switch (cutMerge->format)
    {
    case e_cutMergeFormat::secut:
    {
        pattern = "%[0123456789:.]-%[0123456789:.]%n";
        break;
    }
    case e_cutMergeFormat::vapoursynth:
    {
        pattern = "[%[0123456789]:%[0123456789]]%n";
        break;
    }
    case e_cutMergeFormat::avisynth:
    {
        pattern = "(%[0123456789:.],%[0123456789:.])%n";
        break;
    }
    case e_cutMergeFormat::remap:
    {
        pattern = "[%[0123456789:.] %[0123456789:.]]%n";
        break;
    }
    default:
        break;
    }

    char strBeg[1024], strEnd[1024];
    int beg, end;
    int charRead, tokenRead;

    std::string list = cutMerge->list;

    do {
        tokenRead = std::sscanf(list.c_str(), pattern.c_str(), strBeg, strEnd, &charRead);
        if (tokenRead != 2) {
            return false;
        }

        t_cutMergeSection section = {};
        switch (cutMerge->timeMode)
        {
        case e_cutMergeTimeMode::ms:
        {
            beg = std::atoi(strBeg);
            end = std::atoi(strEnd);

            section.begin = (uint32_t)std::round(beg * MS_TO_PTS_MULT);
            section.end = (uint32_t)std::round(end * MS_TO_PTS_MULT);
            break;
        }
        case e_cutMergeTimeMode::frame:
        {
            beg = std::atoi(strBeg);
            end = std::atoi(strEnd);

            if (cutMerge->format == e_cutMergeFormat::vapoursynth) {
                end--;
            }

            section.begin = (uint32_t)std::round(((double)beg) / cutMerge->fps * MS_TO_PTS_MULT);
            section.end = (uint32_t)std::round(((double)end) / cutMerge->fps * MS_TO_PTS_MULT);
            break;
        }
        case e_cutMergeTimeMode::timestamp:
        {
            beg = timestampToMs(strBeg);
            end = timestampToMs(strEnd);
            if (beg == -1) {
                std::fprintf(stderr, "Timestamp %s is invalid\n", strBeg);
                return false;
            }
            if (beg == -1) {
                std::fprintf(stderr, "Timestamp %s is invalid\n", strEnd);
                return false;
            }

            section.begin = (uint32_t)std::round(beg * MS_TO_PTS_MULT);
            section.end = (uint32_t)std::round(end * MS_TO_PTS_MULT);
            break;
        }
        default:
            break;
        }

        cutMerge->section.push_back(section);

        if (charRead + 1 < list.length()) {
            list = list.substr(charRead + 1, list.length());
        }
        else {
            list.clear();
        }
    } while (list.length() > 0);

    std::sort(cutMerge->section.begin(), cutMerge->section.end(), CompareCutMergeSection);

    int32_t runningDelay = 0;
    t_cutMergeSection prec = {};
    for (int i = 0; i < cutMerge->section.size(); i++) {
        cutMerge->section[i].delay_until = runningDelay + cutMerge->section[i].begin;
        runningDelay += (cutMerge->section[i].begin - cutMerge->section[i].end);
    }

    return true;

}

bool ParseCMD(int32_t argc, char** argv, t_cmd& cmd) {
    int i = 1;

    cmd.inputFile = argv[i++];
    bool foundStartOfOptions = false;

    while (i < argc) {
        const char* argCStr = argv[i++];
        std::string arg = argCStr;
        int remaining = argc - i;
        bool recognizedOption = true;

        if (arg == "trace" || arg == "--trace") {
            cmd.trace = true;
        }
        else if (arg == "delay" || arg == "--delay") {
            if (remaining < 1) return false;
            cmd.delay = (int32_t)round(atof(argv[i++]) * MS_TO_PTS_MULT);

            if (cmd.cutMerge.doCutMerge) {
                std::fprintf(stderr, "Delay parameter will NOT be applied to Cut&Merge\n");
                /*
                for (int i = 0; i < cmd.cutMerge.section.size(); i++) {
                    cmd.cutMerge.section[i].begin += cmd.delay;
                    cmd.cutMerge.section[i].end   += cmd.delay;
                }
                */
            }
        }
        else if (arg == "move" || arg == "--move") {
            if (remaining < 2) return false;
            cmd.move.deltaX = atoi(argv[i++]);
            cmd.move.deltaY = atoi(argv[i++]);
        }
        else if (arg == "crop" || arg == "--crop") {
            if (remaining < 4) return false;
            cmd.crop.left   = atoi(argv[i++]);
            cmd.crop.top    = atoi(argv[i++]);
            cmd.crop.right  = atoi(argv[i++]);
            cmd.crop.bottom = atoi(argv[i++]);
        }
        else if (arg == "resync" || arg == "--resync") {
            if (remaining < 1) return false;
            std::string strFactor = argv[i];
            size_t idx = strFactor.find("/");
            if (idx != SIZE_MAX) {
                double num = std::atof(strFactor.substr(0, idx).c_str());
                double den = std::atof(strFactor.substr(idx + 1, strFactor.length()).c_str());

                cmd.resync = num / den;
            }
            else {
                cmd.resync = std::atof(argv[i]);
            }
            i++;

            cmd.delay = (int32_t)std::round(((double)cmd.delay * cmd.resync));

            if (cmd.cutMerge.doCutMerge) {
                std::fprintf(stderr, "Resync parameter will NOT be applied to Cut&Merge\n");
                /*
                for (int i = 0; i < cmd.cutMerge.section.size(); i++) {
                    cmd.cutMerge.section[i].begin *= cmd.resync;
                    cmd.cutMerge.section[i].end   *= cmd.resync;
                }
                */
            }
        }
        else if (arg == "add_zero" || arg == "--add_zero") {
            cmd.addZero = true;
        }
        else if (arg == "tonemap" || arg == "--tonemap") {
            if (remaining < 1) return false;
            cmd.tonemap = std::atof(argv[i++]);
        }
        else if (arg == "cut_merge" || arg == "--cut_merge") {
            cmd.cutMerge.doCutMerge = true;
        }
        else if (arg == "format" || arg == "--format") {
            if (remaining < 1) return false;
            std::string formatMode = argv[i++];
            toLower(formatMode);

            if (formatMode == "secut") {
                cmd.cutMerge.format = e_cutMergeFormat::secut;
            }
            else if (formatMode == "vapoursynth" || formatMode == "vs") {
                cmd.cutMerge.format = e_cutMergeFormat::vapoursynth;
            }
            else if (formatMode == "avisynth" || formatMode == "avs") {
                cmd.cutMerge.format = e_cutMergeFormat::avisynth;
            }
            else if (formatMode == "remap") {
                cmd.cutMerge.format = e_cutMergeFormat::remap;
            }
            else {
                return false;
            }
        }
        else if (arg == "list" || arg == "--list") {
            if (remaining < 1) return false;
            std::string list = argv[i++];
            toLower(list);

            cmd.cutMerge.list = list;
        }
        else if (arg == "timemode" || arg == "--timemode") {
            if (remaining < 1) return false;
            std::string timemode = argv[i++];
            toLower(timemode);

            if (timemode == "ms") {
                cmd.cutMerge.timeMode = e_cutMergeTimeMode::ms;
            }
            else if (timemode == "frame") {
                if (remaining < 2) return false;
                cmd.cutMerge.timeMode = e_cutMergeTimeMode::frame;
                std::string strFactor = argv[i];

                size_t idx = strFactor.find("/");
                if (idx != SIZE_MAX) {
                    double num = std::atof(strFactor.substr(0, idx).c_str());
                    double den = std::atof(strFactor.substr(idx + 1, strFactor.length()).c_str());

                    cmd.cutMerge.fps = num / den;
                }
                else {
                    cmd.cutMerge.fps = std::atof(argv[i]);
                }
                i++;
            }
            else if (timemode == "timestamp") {
                cmd.cutMerge.timeMode = e_cutMergeTimeMode::timestamp;
            }
            else {
                return false;
            }
        }
        else if (arg == "fixmode" || arg == "--fixmode") {
            if (remaining < 1) return false;
            std::string fixmode = argv[i++];
            toLower(fixmode);

            if (fixmode == "cut") {
                cmd.cutMerge.fixMode = e_cutMergeFixMode::cut;
            }
            else if (fixmode == "delete" || fixmode == "del") {
                cmd.cutMerge.fixMode = e_cutMergeFixMode::del;
            }
        }
        else {
            recognizedOption = false;
        }

        if (recognizedOption) {
            foundStartOfOptions = true;
        }
        else if (!foundStartOfOptions) {
            cmd.outputFile = argCStr;
            foundStartOfOptions = true;
        }
        else {
            return false;
        }
    }

    if (cmd.cutMerge.doCutMerge) {
        if (   cmd.cutMerge.format   == e_cutMergeFormat::vapoursynth
            && cmd.cutMerge.timeMode == e_cutMergeTimeMode::timestamp) {
            std::fprintf(stderr, "Compat mode VapourSynth cannot be used alongside timestamp time mode\n");

            return false;
        }

        if (!parseCutMerge(&cmd.cutMerge)) {
            return false;
        }
    }

    return true;
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

    if (!ParseCMD(argc, argv, cmd)) {
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

                t_timestamp timestamp = PTStoTimestamp(header.pts);
                char timestampString[13]; // max 99:99:99.999
                std::snprintf(timestampString, 13, "%lu:%02lu:%02lu.%03lu", timestamp.hh, timestamp.mm, timestamp.ss, timestamp.ms);
                char offsetString[13];    // max 0xFFFFFFFFFF (1TB)
                std::snprintf(offsetString, 13, "%#zx", start);

                if (doResync) {
                    header.pts = (uint32_t)std::round((double)header.pts * cmd.resync);
                }
                if (doDelay) {
                    header.pts = header.pts + cmd.delay;
                }

                if (doResync || doDelay) {
                    header.write(&buffer[start]);
                }

                switch (header.segmentType) {
                case 0x14:
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
                case 0x15:
                    if (cmd.trace) {
                        ods = t_ODS::read(&buffer[start + HEADER_SIZE]);

                        std::printf("  + ODS Segment: offset %s\n", offsetString);
                        std::printf("    + Object ID: %u\n", ods.id);
                        std::printf("    + Version: %u\n", ods.versionNumber);
                        if (ods.sequenceFlag != 0xC0) {
                            std::printf("    + Sequence flag: ");
                            switch (ods.sequenceFlag) {
                                case 0x40: std::printf("Last\n"); break;
                                case 0x80: std::printf("First\n"); break;
                                default:   std::printf("%#x\n", ods.sequenceFlag); break;
                            }
                        }
                        std::printf("    + Size: %ux%u\n", ods.width, ods.height);
                    }
                    break;
                case 0x16:
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
                                case 0x00: std::printf("Normal\n"); break;
                                case 0x40: std::printf("Aquisition Point\n"); break;
                                case 0x80: std::printf("Epoch Start\n"); break;
                                default:   std::printf("%#x\n", pcs.compositionState); break;
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

                                zeroHeader.segmentType = 0x17; // WDS
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

                                zeroHeader.segmentType = 0x80; // END
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
                case 0x17:
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
                                    std::fprintf(stderr, "Window is bigger then new screen area at timestamp %s\n", timestampString);
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
                case 0x80:
                    if (cmd.trace) {
                        std::printf("  + END Segment: offset %s\n", offsetString);
                    }

                    if (cmd.cutMerge.doCutMerge) {
                        if (cutMerge_foundEnd) {
                            cutMerge_foundBegin = false;
                            cutMerge_foundEnd = false;
                            int idxFound = SearchSectionByPTS(cmd.cutMerge.section, cutMerge_currentBeginPTS, cutMerge_currentEndPTS, cmd.cutMerge.fixMode);
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
                    if (header.segmentType == 0x16)
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

                    if (header.segmentType == 0x80)
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
    std::fclose(output);

    return 0;
}
