#include <iostream>
#include <cstdio>
#include <math.h>
#include <algorithm>
#include <vector>

//Documentation: http://blog.thescorpius.com/index.php/2017/07/15/presentation-graphic-stream-sup-files-bluray-subtitle-format/

//using namespace std;

double const MS_TO_PTS_MULT = 90.0f;
size_t const HEADER_SIZE = 13;

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

struct t_header {
    uint16_t header;
    uint32_t pts1;
    uint32_t dts;
    uint16_t dataLength;
    uint8_t  segmentType;
};

struct t_window {
    uint8_t  windowID;
    uint16_t WindowsHorPos;
    uint16_t WindowsVerPos;
    uint16_t WindowsWidth;
    uint16_t WindowsHeight;
};

struct t_WDS {
    uint8_t  numberOfWindows;
    t_window windows[256];
};

struct t_compositionObject {
    uint16_t objectID;
    uint8_t  windowID;
    uint8_t  objectCroppedFlag;
    uint16_t objectHorPos;
    uint16_t objectVerPos;
    uint16_t objCropHorPos;
    uint16_t objCropVerPos;
    uint16_t objCropWidth;
    uint16_t objCropHeight;
};
struct t_PCS {
    uint16_t width;
    uint16_t height;
    uint8_t  frameRate;
    uint16_t compositionNumber;
    uint8_t  compositionState;
    uint8_t  paletteUpdFlag;
    uint8_t  paletteID;
    uint8_t  numCompositionObject;
    t_compositionObject compositionObject[256];
};

struct t_palette {
    uint8_t paletteEntryID;
    uint8_t paletteY;
    uint8_t paletteCr;
    uint8_t paletteCb;
    uint8_t paletteA;
};

struct t_PDS {
    uint8_t paletteID;
    uint8_t paletteVersionNumber;
    t_palette palette[255];
    uint8_t paletteNum; //not in format, only used internally
};

struct t_cmd {
    int32_t delay = 0;
    t_crop crop = {};
    double resync = 1;
    bool addZero = false;
    double tonemap = 1;
    t_cutMerge cutMerge = {};
};

uint16_t swapEndianness(uint16_t input) {
    uint16_t output;

    output = (input >> 8) | (input << 8);

    return output;
}

uint32_t swapEndianness(uint32_t input) {
    uint32_t output;

    output = ((input >> 24) & 0xff) |   // move byte 3 to byte 0
        ((input << 8) & 0xff0000) |   // move byte 1 to byte 2
        ((input >> 8) & 0xff00) |   // move byte 2 to byte 1
        ((input << 24) & 0xff000000); // byte 0 to byte 3;

    return output;
}

t_header ReadHeader(uint8_t* buffer) {
    t_header header = {};

    header.header = swapEndianness(*(uint16_t*)&buffer[0]);
    header.pts1 = swapEndianness(*(uint32_t*)&buffer[2]);
    header.dts = swapEndianness(*(uint32_t*)&buffer[6]);
    header.segmentType = *(uint8_t*)&buffer[10];
    header.dataLength = swapEndianness(*(uint16_t*)&buffer[11]);

    return header;
}

void WriteHeader(t_header header, uint8_t* buffer) {
    *((uint16_t*)(&buffer[0])) = swapEndianness(header.header);
    *((uint32_t*)(&buffer[2])) = swapEndianness(header.pts1);
    *((uint32_t*)(&buffer[6])) = swapEndianness(header.dts);
    *((uint8_t*)(&buffer[10])) = header.segmentType;
    *((uint16_t*)(&buffer[11])) = swapEndianness(header.dataLength);
}

t_WDS ReadWDS(uint8_t* buffer) {
    t_WDS wds;

    wds.numberOfWindows = *(uint8_t*)&buffer[0];
    for (int i = 0; i < wds.numberOfWindows; i++) {
        size_t bufferStartIdx = 1 + (size_t)i * 9;
        wds.windows[i].windowID = *(uint8_t*)&buffer[bufferStartIdx + 0];
        wds.windows[i].WindowsHorPos = swapEndianness(*(uint16_t*)&buffer[bufferStartIdx + 1]);
        wds.windows[i].WindowsVerPos = swapEndianness(*(uint16_t*)&buffer[bufferStartIdx + 3]);
        wds.windows[i].WindowsWidth = swapEndianness(*(uint16_t*)&buffer[bufferStartIdx + 5]);
        wds.windows[i].WindowsHeight = swapEndianness(*(uint16_t*)&buffer[bufferStartIdx + 7]);
    }

    return wds;
}

void WriteWDS(t_WDS wds, uint8_t* buffer) {
    *((uint8_t*)(&buffer[0])) = wds.numberOfWindows;
    for (int i = 0; i < wds.numberOfWindows; i++) {
        size_t bufferStartIdx = 1 + (size_t)i * 9;
        *((uint8_t*)(&buffer[bufferStartIdx + 0])) = wds.windows[i].windowID;
        *((uint16_t*)(&buffer[bufferStartIdx + 1])) = swapEndianness(wds.windows[i].WindowsHorPos);
        *((uint16_t*)(&buffer[bufferStartIdx + 3])) = swapEndianness(wds.windows[i].WindowsVerPos);
        *((uint16_t*)(&buffer[bufferStartIdx + 5])) = swapEndianness(wds.windows[i].WindowsWidth);
        *((uint16_t*)(&buffer[bufferStartIdx + 7])) = swapEndianness(wds.windows[i].WindowsHeight);
    }
}

t_PCS ReadPCS(uint8_t* buffer) {
    t_PCS pcs;

    pcs.width = swapEndianness(*(uint16_t*)&buffer[0]);
    pcs.height = swapEndianness(*(uint16_t*)&buffer[2]);
    pcs.frameRate = *(uint8_t*)&buffer[4];
    pcs.compositionNumber = swapEndianness(*(uint16_t*)&buffer[5]);
    pcs.compositionState = *(uint8_t*)&buffer[7];
    pcs.paletteUpdFlag = *(uint8_t*)&buffer[8];
    pcs.paletteID = *(uint8_t*)&buffer[9];
    pcs.numCompositionObject = *(uint8_t*)&buffer[10];

    for (int i = 0; i < pcs.numCompositionObject; i++) {
        size_t bufferStartIdx = 11 + (size_t)i * 8;

        pcs.compositionObject[i].objectID = swapEndianness(*(uint16_t*)&buffer[bufferStartIdx + 0]);
        pcs.compositionObject[i].windowID = *(uint8_t*)&buffer[bufferStartIdx + 2];
        pcs.compositionObject[i].objectCroppedFlag = *(uint8_t*)&buffer[bufferStartIdx + 3];
        pcs.compositionObject[i].objectHorPos = swapEndianness(*(uint16_t*)&buffer[bufferStartIdx + 4]);
        pcs.compositionObject[i].objectVerPos = swapEndianness(*(uint16_t*)&buffer[bufferStartIdx + 6]);
        /*
        pcs.compositionObject.objCropHorPos = swapEndianness(*(uint16_t*)&buffer[bufferStartIdx + 8]);
        pcs.compositionObject.objCropVerPos = swapEndianness(*(uint16_t*)&buffer[bufferStartIdx + 10]);
        pcs.compositionObject.objCropWidth = swapEndianness(*(uint16_t*)&buffer[bufferStartIdx + 12]);
        pcs.compositionObject.objCropHeight = swapEndianness(*(uint16_t*)&buffer[bufferStartIdx + 14]);
        */
    }

    return pcs;
}

void WritePCS(t_PCS pcs, uint8_t* buffer) {
    *((uint16_t*)(&buffer[0])) = swapEndianness(pcs.width);
    *((uint16_t*)(&buffer[2])) = swapEndianness(pcs.height);
    *((uint8_t*)(&buffer[4])) = pcs.frameRate;
    *((uint16_t*)(&buffer[5])) = swapEndianness(pcs.compositionNumber);
    *((uint8_t*)(&buffer[7])) = pcs.compositionState;
    *((uint8_t*)(&buffer[8])) = pcs.paletteUpdFlag;
    *((uint8_t*)(&buffer[9])) = pcs.paletteID;
    *((uint8_t*)(&buffer[10])) = pcs.numCompositionObject;

    for (int i = 0; i < pcs.numCompositionObject; i++) {
        size_t bufferStartIdx = 11 + (size_t)i * 8;

        *((uint16_t*)(&buffer[bufferStartIdx + 0])) = swapEndianness(pcs.compositionObject[i].objectID);
        *((uint8_t*)(&buffer[bufferStartIdx + 2])) = pcs.compositionObject[i].windowID;
        *((uint8_t*)(&buffer[bufferStartIdx + 3])) = pcs.compositionObject[i].objectCroppedFlag;
        *((uint16_t*)(&buffer[bufferStartIdx + 4])) = swapEndianness(pcs.compositionObject[i].objectHorPos);
        *((uint16_t*)(&buffer[bufferStartIdx + 6])) = swapEndianness(pcs.compositionObject[i].objectVerPos);
    }

    /*
    *((uint16_t*)(&buffer[bufferStartIdx + 8])) = swapEndianness(pcs.compositionObject.objCropHorPos);
    *((uint16_t*)(&buffer[bufferStartIdx + 10])) = swapEndianness(pcs.compositionObject.objCropVerPos);
    *((uint16_t*)(&buffer[bufferStartIdx + 12])) = swapEndianness(pcs.compositionObject.objCropWidth);
    *((uint16_t*)(&buffer[bufferStartIdx + 14])) = swapEndianness(pcs.compositionObject.objCropHeight);
    */
}

t_PDS ReadPDS(uint8_t* buffer, size_t segment_size) {
    t_PDS pds;

    pds.paletteID = *(uint8_t*)&buffer[0];
    pds.paletteVersionNumber = *(uint8_t*)&buffer[1];
    pds.paletteNum = (segment_size - 2) / 5;

    for (int i = 0; i < pds.paletteNum; i++) {
        size_t bufferStartIdx = 2 + (size_t)i * 5;

        pds.palette[i].paletteEntryID = *(uint8_t*)&buffer[bufferStartIdx + 0];
        pds.palette[i].paletteY = *(uint8_t*)&buffer[bufferStartIdx + 1];
        pds.palette[i].paletteCb = *(uint8_t*)&buffer[bufferStartIdx + 2];
        pds.palette[i].paletteCr = *(uint8_t*)&buffer[bufferStartIdx + 3];
        pds.palette[i].paletteA = *(uint8_t*)&buffer[bufferStartIdx + 4];
    }

    return pds;
}

void WritePDS(t_PDS pds, uint8_t* buffer) {

    *((uint8_t*)(&buffer[0])) = pds.paletteID;
    *((uint8_t*)(&buffer[1])) = pds.paletteVersionNumber;

    for (int i = 0; i < pds.paletteNum; i++) {
        size_t bufferStartIdx = 2 + (size_t)i * 5;

        *((uint8_t*)(&buffer[bufferStartIdx + 0])) = pds.palette[i].paletteEntryID;
        *((uint8_t*)(&buffer[bufferStartIdx + 1])) = pds.palette[i].paletteY;
        *((uint8_t*)(&buffer[bufferStartIdx + 2])) = pds.palette[i].paletteCb;
        *((uint8_t*)(&buffer[bufferStartIdx + 3])) = pds.palette[i].paletteCr;
        *((uint8_t*)(&buffer[bufferStartIdx + 4])) = pds.palette[i].paletteA;
    }
}

void toLower(std::string& str) {
    std::transform(str.begin(), str.end(), str.begin(), ::tolower);
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

        if (fixMode == e_cutMergeFixMode::cut
            && found >= 1) {
            return i;
        }
        if (fixMode == e_cutMergeFixMode::del
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
        pattern = "[%[0123456789:.]-%[0123456789:.]]%n";
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
            beg = atoi(strBeg);
            end = atoi(strEnd);

            section.begin = (uint32_t)round(beg * MS_TO_PTS_MULT);
            section.end = (uint32_t)round(end * MS_TO_PTS_MULT);
            break;
        }
        case e_cutMergeTimeMode::frame:
        {
            beg = atoi(strBeg);
            end = atoi(strEnd);

            if (cutMerge->format == e_cutMergeFormat::vapoursynth) {
                end--;
            }

            section.begin = (uint32_t)round(((double)beg) / cutMerge->fps * MS_TO_PTS_MULT);
            section.end = (uint32_t)round(((double)end) / cutMerge->fps * MS_TO_PTS_MULT);
            break;
        }
        case e_cutMergeTimeMode::timestamp:
        {
            beg = timestampToMs(strBeg);
            end = timestampToMs(strEnd);
            if (beg == -1) {
                printf("Timestamp %s is invalid\n", strBeg);
                return false;
            }
            if (beg == -1) {
                printf("Timestamp %s is invalid\n", strEnd);
                return false;
            }

            section.begin = (uint32_t)round(beg * MS_TO_PTS_MULT);
            section.end = (uint32_t)round(end * MS_TO_PTS_MULT);
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

    sort(cutMerge->section.begin(), cutMerge->section.end(), CompareCutMergeSection);

    int32_t runningDelay = 0;
    t_cutMergeSection prec = {};
    for (int i = 0; i < cutMerge->section.size(); i++) {
        cutMerge->section[i].delay_until = runningDelay + cutMerge->section[i].begin;
        runningDelay += (cutMerge->section[i].begin - cutMerge->section[i].end);
    }

    return true;

}

bool ParseCMD(int32_t argc, char** argv, t_cmd& cmd) {
    cmd.delay = 0;
    cmd.crop = {};
    cmd.resync = 1;
    cmd.addZero = false;
    cmd.tonemap = 1;

    int i = 3;

    bool doSubcommand = false;
    if (argc == 4) {
        //backward compatibility
        cmd.delay = (int32_t)round(atof(argv[3]) * MS_TO_PTS_MULT);
        if (cmd.delay != 0) {
            printf("Running in backwards-compatibility mode\n");
            return true;
        }
    }

    while (i < argc) {
        std::string command = argv[i];
        toLower(command);

        if (command == "delay") {
            cmd.delay = (int32_t)round(atof(argv[i + 1]) * MS_TO_PTS_MULT);
            i += 2;

            if (cmd.cutMerge.doCutMerge) {
                printf("Delay parameter will NOT be applied to Cut&Merge\n");
                /*
                for (int i = 0; i < cmd.cutMerge.section.size(); i++) {
                    cmd.cutMerge.section[i].begin += cmd.delay;
                    cmd.cutMerge.section[i].end   += cmd.delay;
                }
                */
            }
        }
        else if (command == "crop") {
            cmd.crop.left = atoi(argv[i + 1]);
            cmd.crop.top = atoi(argv[i + 2]);
            cmd.crop.right = atoi(argv[i + 3]);
            cmd.crop.bottom = atoi(argv[i + 4]);
            i += 5;
        }
        else if (command == "resync") {
            std::string strFactor = argv[i + 1];
            size_t idx = strFactor.find("/");
            if (idx != SIZE_MAX) {
                double num = atof(strFactor.substr(0, idx).c_str());
                double den = atof(strFactor.substr(idx + 1, strFactor.length()).c_str());

                cmd.resync = num / den;
            }
            else {
                cmd.resync = atof(argv[i + 1]);
            }

            cmd.delay = (int32_t)round(((double)cmd.delay * cmd.resync));

            if (cmd.cutMerge.doCutMerge) {
                printf("Resync parameter will NOT be applied to Cut&Merge\n");
                /*
                for (int i = 0; i < cmd.cutMerge.section.size(); i++) {
                    cmd.cutMerge.section[i].begin *= cmd.resync;
                    cmd.cutMerge.section[i].end   *= cmd.resync;
                }
                */
            }

            i += 2;
        }
        else if (command == "add_zero") {
            cmd.addZero = true;
            i += 1;
        }
        else if (command == "tonemap") {
            cmd.tonemap = atof(argv[i + 1]);
            i += 2;
        }
        else if (command == "cut_merge") {
            cmd.cutMerge.doCutMerge = true;
            i++;
        }
        else if (command == "format") {
            std::string formatMode = argv[i + 1];
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
            i += 2;
        }
        else if (command == "list") {
            std::string list = argv[i + 1];
            toLower(list);

            cmd.cutMerge.list = list;

            i += 2;
        }
        else if (command == "timemode") {
            std::string timemode = argv[i + 1];
            toLower(timemode);

            if (timemode == "ms") {
                cmd.cutMerge.timeMode = e_cutMergeTimeMode::ms;
            }
            else if (timemode == "frame") {
                cmd.cutMerge.timeMode = e_cutMergeTimeMode::frame;
                std::string strFactor = argv[i + 2];

                size_t idx = strFactor.find("/");
                if (idx != SIZE_MAX) {
                    double num = atof(strFactor.substr(0, idx).c_str());
                    double den = atof(strFactor.substr(idx + 1, strFactor.length()).c_str());

                    cmd.cutMerge.fps = num / den;
                }
                else {
                    cmd.cutMerge.fps = atof(argv[i + 2]);
                }
                i++;
            }
            else if (timemode == "timestamp") {
                cmd.cutMerge.timeMode = e_cutMergeTimeMode::timestamp;
            }
            else {
                return false;
            }

            i += 2;
        }
        else if (command == "fixmode") {
            std::string fixmode = argv[i + 1];
            toLower(fixmode);

            if (fixmode == "cut") {
                cmd.cutMerge.fixMode = e_cutMergeFixMode::cut;
            }
            else if (fixmode == "delete" || fixmode == "del") {
                cmd.cutMerge.fixMode = e_cutMergeFixMode::del;
            }

            i += 2;
        }
        else {
            return false;
        }
    }

    if (cmd.cutMerge.doCutMerge) {
        if (cmd.cutMerge.format == e_cutMergeFormat::vapoursynth
            && cmd.cutMerge.timeMode == e_cutMergeTimeMode::timestamp) {
            printf("Compat mode VapourSynth cannot be used alongside timestamp time mode\n");

            return false;
        }

        if (!parseCutMerge(&cmd.cutMerge)) {
            return false;
        }
    }

    return true;
}

int main(int32_t argc, char** argv)
{
    size_t size, newSize;
    t_header header;


    if (argc < 4) {
        printf("Usage: SupMover (<input.sup> <output.sup>) [delay (ms)] [crop (<left> <top> <right> <bottom>)] [resync (<num>/<den> | multFactor)] [add_zero] [tonemap <perc>]\r\n");
        printf("delay and resync command are executed in the order supplied\r\n");
        return 0;
    }
    t_cmd cmd = {};

    if (!ParseCMD(argc, argv, cmd)) {
        printf("Error parsing input\r\n");
        return -1;
    }


    bool doDelay = cmd.delay != 0;
    bool doCrop = (cmd.crop.left + cmd.crop.top + cmd.crop.right + cmd.crop.bottom) > 0;
    bool doResync = cmd.resync != 1;
    bool doTonemap = cmd.tonemap != 1;

    bool doSomething = doDelay || doCrop || doResync || cmd.addZero || doTonemap || cmd.cutMerge.doCutMerge;

    FILE* input = fopen(argv[1], "rb");
    if (input == NULL) {
        printf("Unable to open input file!");
        return -1;
    }
    FILE* output = fopen(argv[2], "wb");
    if (output == NULL) {
        printf("Unable to open output file!");
        fclose(input);
        return -1;
    }

    fseek(input, 0, SEEK_END);
    size = newSize = ftell(input);
    if (size != 0) {
        fseek(input, 0, SEEK_SET);

        uint8_t* buffer = (uint8_t*)calloc(1, size);
        uint8_t* newBuffer = buffer;
        if (buffer == NULL) {
            return -1;
        }

        fread(buffer, size, 1, input);
        if (doSomething) {
            size_t start = 0;

            t_rect screenRect = {};

            t_WDS wds = {};
            t_PCS pcs = {};
            t_PDS pds = {};

            size_t offesetCurrPCS = 0;
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
                header = ReadHeader(&buffer[start]);
                if (header.header != 0x5047) {
                    printf("Correct header not found at position %zd, abort!", start);
                    fclose(input);
                    fclose(output);
                    return -1;
                }

                if (doResync) {
                    header.pts1 = (uint32_t)round((double)header.pts1 * cmd.resync);
                }
                if (doDelay) {
                    header.pts1 = header.pts1 + cmd.delay;
                }

                if (doResync || doDelay) {
                    WriteHeader(header, &buffer[start]);
                }

                switch (header.segmentType) {
                case 0x14:
                    //printf("PDS\r\n");
                    if (doTonemap) {
                        pds = ReadPDS(&buffer[start + HEADER_SIZE], header.dataLength);

                        for (int i = 0; i < pds.paletteNum; i++) {
                            //convert Y from TV level (16-235) to full range
                            double expandedY = ((((double)pds.palette[i].paletteY - 16.0) * (255.0 / (235.0 - 16.0))) / 255.0);
                            double tonemappedY = expandedY * cmd.tonemap;
                            double clampedY = std::min(1.0, std::max(tonemappedY, 0.0));
                            double newY = round((clampedY * (235.0 - 16.0)) + 16.0);
                            pds.palette[i].paletteY = (uint8_t)newY;
                        }

                        WritePDS(pds, &buffer[start + HEADER_SIZE]);
                    }
                    break;
                case 0x15:
                    //printf("ODS\r\n");
                    break;
                case 0x16:
                    //printf("PCS\r\n");
                    if (doCrop || cmd.addZero || cmd.cutMerge.doCutMerge) {
                        pcs = ReadPCS(&buffer[start + HEADER_SIZE]);
                        offesetCurrPCS = start;

                        if (doCrop) {
                            screenRect.x = 0 + cmd.crop.left;
                            screenRect.y = 0 + cmd.crop.top;
                            screenRect.width = pcs.width - (cmd.crop.left + cmd.crop.right);
                            screenRect.height = pcs.height - (cmd.crop.top + cmd.crop.bottom);

                            pcs.width = screenRect.width;
                            pcs.height = screenRect.height;

                            if (pcs.numCompositionObject > 1) {
                                t_timestamp timestamp = PTStoTimestamp(header.pts1);
                                printf("Multiple composition object at timestamp %lu:%02lu:%02lu.%03lu! Please Check!\r\n", timestamp.hh, timestamp.mm, timestamp.ss, timestamp.ms);
                            }

                            for (int i = 0; i < pcs.numCompositionObject; i++) {
                                if (pcs.compositionObject[i].objectCroppedFlag == 0x40) {
                                    t_timestamp timestamp = PTStoTimestamp(header.pts1);
                                    printf("Object Cropped Flag set at timestamp %lu:%02lu:%02lu.%03lu! Implement it!\r\n", timestamp.hh, timestamp.mm, timestamp.ss, timestamp.ms);
                                }

                                if (cmd.crop.left > pcs.compositionObject[i].objectHorPos) {
                                    pcs.compositionObject[i].objectHorPos = 0;
                                }
                                else {
                                    pcs.compositionObject[i].objectHorPos -= cmd.crop.left;
                                }

                                if (cmd.crop.top > pcs.compositionObject[i].objectVerPos) {
                                    pcs.compositionObject[i].objectVerPos = 0;
                                }
                                else {
                                    pcs.compositionObject[i].objectVerPos -= cmd.crop.top;
                                }
                            }
                        }

                        if (cmd.addZero) {
                            if (pcs.compositionNumber == 0) {
                                uint8_t zeroBuffer[60];
                                uint8_t pos = 0;
                                t_header zeroHeader(header);
                                zeroHeader.pts1 = 0;
                                zeroHeader.dataLength = 11; //Length of upcoming PCS
                                WriteHeader(zeroHeader, &zeroBuffer[pos]);
                                pos += 13;
                                t_PCS zeroPcs(pcs);
                                zeroPcs.compositionState = 0;
                                zeroPcs.paletteUpdFlag = 0;
                                zeroPcs.paletteID = 0;
                                zeroPcs.numCompositionObject = 0;
                                WritePCS(zeroPcs, &zeroBuffer[pos]);
                                pos += zeroHeader.dataLength;

                                zeroHeader.segmentType = 0x17; // WDS
                                zeroHeader.dataLength = 10; //Length of upcoming WDS
                                WriteHeader(zeroHeader, &zeroBuffer[pos]);
                                pos += 13;
                                t_WDS zeroWds;
                                zeroWds.numberOfWindows = 1;
                                zeroWds.windows[0].windowID = 0;
                                zeroWds.windows[0].WindowsHorPos = 0;
                                zeroWds.windows[0].WindowsVerPos = 0;
                                zeroWds.windows[0].WindowsWidth = 0;
                                zeroWds.windows[0].WindowsHeight = 0;
                                WriteWDS(zeroWds, &zeroBuffer[pos]);
                                pos += zeroHeader.dataLength;

                                zeroHeader.segmentType = 0x80; // END
                                zeroHeader.dataLength = 0; //Length of upcoming END
                                WriteHeader(zeroHeader, &zeroBuffer[pos]);
                                pos += 13;

                                printf("Writing %d bytes as first display set\n", pos);
                                fwrite(zeroBuffer, pos, 1, output);

                                //For Cut&Merge functionality we don't need to save the added segment as it
                                //is saved in the resulting file automatically
                            }
                            pcs.compositionNumber += 1;
                        }

                        if (cmd.cutMerge.doCutMerge) {
                            if (!cutMerge_foundBegin) {
                                cutMerge_foundBegin = true;
                                cutMerge_currentBeginPTS = header.pts1;
                                cutMerge_currentCompositionNumber = pcs.compositionNumber;
                            }
                            else if (!cutMerge_foundEnd) {
                                cutMerge_foundEnd = true;
                                cutMerge_currentEndPTS = header.pts1;
                            }
                        }

                        WritePCS(pcs, &buffer[start + HEADER_SIZE]);
                    }
                    break;
                case 0x17:
                    //printf("WDS\r\n");
                    fixPCS = false;
                    if (doCrop) {
                        wds = ReadWDS(&buffer[start + HEADER_SIZE]);

                        if (wds.numberOfWindows > 1) {
                            t_timestamp timestamp = PTStoTimestamp(header.pts1);
                            printf("Multiple windows at timestamp %lu:%02lu:%02lu.%03lu! Please Check!\r\n", timestamp.hh, timestamp.mm, timestamp.ss, timestamp.ms);
                        }

                        for (int i = 0; i < wds.numberOfWindows; i++) {
                            t_rect wndRect;
                            uint16_t corrHor = 0;
                            uint16_t corrVer = 0;

                            wndRect.x = wds.windows[i].WindowsHorPos;
                            wndRect.y = wds.windows[i].WindowsVerPos;
                            wndRect.width = wds.windows[i].WindowsWidth;
                            wndRect.height = wds.windows[i].WindowsHeight;

                            if (wndRect.width > screenRect.width
                                || wndRect.height > screenRect.height) {
                                t_timestamp timestamp = PTStoTimestamp(header.pts1);
                                printf("Window is bigger then new screen area at timestamp %lu:%02lu:%02lu.%03lu\r\n", timestamp.hh, timestamp.mm, timestamp.ss, timestamp.ms);
                                printf("Implement it!\r\n");
                                /*
                                pcs.width = wndRect.width;
                                pcs.height = wndRect.height;
                                fixPCS = true;
                                */
                            }
                            else {
                                if (!rectIsContained(screenRect, wndRect)) {
                                    t_timestamp timestamp = PTStoTimestamp(header.pts1);
                                    printf("Window is outside new screen area at timestamp %lu:%02lu:%02lu.%03lu\r\n", timestamp.hh, timestamp.mm, timestamp.ss, timestamp.ms);

                                    uint16_t wndRightPoint = wndRect.x + wndRect.width;
                                    uint16_t screenRightPoint = screenRect.x + screenRect.width;
                                    if (wndRightPoint > screenRightPoint) {
                                        corrHor = wndRightPoint - screenRightPoint;
                                    }

                                    uint16_t wndBottomPoint = wndRect.y + wndRect.height;
                                    uint16_t screenBottomPoint = screenRect.y + screenRect.height;
                                    if (wndBottomPoint > screenBottomPoint) {
                                        corrVer = wndBottomPoint - screenBottomPoint;
                                    }

                                    if (corrHor + corrVer != 0) {
                                        printf("Please check\r\n");
                                    }
                                }
                            }

                            if (cmd.crop.left > wds.windows[i].WindowsHorPos) {
                                wds.windows[i].WindowsHorPos = 0;
                            }
                            else {
                                wds.windows[i].WindowsHorPos -= (cmd.crop.left + corrHor);
                            }

                            if (cmd.crop.top > wds.windows[i].WindowsVerPos) {
                                wds.windows[i].WindowsVerPos = 0;
                            }
                            else {
                                wds.windows[i].WindowsVerPos -= (cmd.crop.top + corrVer);
                            }

                            if (corrVer != 0) {
                                pcs.compositionObject[i].objectVerPos -= corrVer;
                                fixPCS = true;
                            }
                            if (corrHor != 0) {
                                pcs.compositionObject[i].objectHorPos -= corrHor;
                                fixPCS = true;
                            }
                        }

                        if (fixPCS) {
                            WritePCS(pcs, &buffer[offesetCurrPCS + HEADER_SIZE]);
                        }
                        WriteWDS(wds, &buffer[start + HEADER_SIZE]);

                    }
                    break;
                case 0x80:
                    //printf("END\r\n");

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
                newBuffer = (uint8_t*)calloc(1, size);

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
                    header = ReadHeader(&buffer[start]);

                    //For Cut&Merge we only need to handle the header (for the PTS), the PCS (to
                    //get the compositionNumber) and the END segment (to know when it finished)
                    if (header.segmentType == 0x16)
                    {
                        pcs = ReadPCS(&buffer[start + HEADER_SIZE]);
                        if (!cutMerge_foundBegin) {
                            cutMerge_foundBegin = true;
                            cutMerge_currentBeginPTS = header.pts1;
                            cutMerge_currentCompositionNumber = pcs.compositionNumber;

                            if (cutMerge_compositionNumberToSave[cutMerge_currentToSaveIdx].compositionNumber == cutMerge_currentCompositionNumber) {
                                cutMerge_keepSection = true;
                                cutMerge_offsetBeginCopy = start;
                                cutMerge_currentSection = cmd.cutMerge.section[cutMerge_compositionNumberToSave[cutMerge_currentToSaveIdx].sectionIdx];
                                pcs.compositionNumber = cutMerge_newCompositionNumber;
                                WritePCS(pcs, &buffer[start + HEADER_SIZE]);
                            }
                        }
                        else if (!cutMerge_foundEnd) {
                            cutMerge_foundEnd = true;
                            cutMerge_currentEndPTS = header.pts1;
                            if (cutMerge_keepSection) {
                                pcs.compositionNumber = cutMerge_newCompositionNumber;

                                WritePCS(pcs, &buffer[start + HEADER_SIZE]);
                            }
                        }
                    }

                    if (cutMerge_keepSection) {
                        if (!cutMerge_foundEnd) {
                            if (cutMerge_currentBeginPTS < cutMerge_currentSection.begin) {
                                header.pts1 = cutMerge_currentSection.begin;
                            }
                        }
                        else {
                            if (cutMerge_currentEndPTS > cutMerge_currentSection.end) {
                                header.pts1 = cutMerge_currentSection.end;
                            }
                        }

                        header.pts1 -= cutMerge_currentSection.delay_until;

                        WriteHeader(header, &buffer[start]);
                    }

                    if (header.segmentType == 0x80)
                    {
                        if (cutMerge_foundEnd) {
                            if (cutMerge_keepSection) {
                                cutMerge_currentToSaveIdx++;

                                cutMerge_offsetEndCopy = start + HEADER_SIZE + header.dataLength;

                                memcpy(&newBuffer[cutMerge_currentNewBufferSize],
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

        fwrite(newBuffer, newSize, 1, output);
    }

    fclose(input);
    fclose(output);

    return 0;
}