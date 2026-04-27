double const MS_TO_PTS_MULT = 90.0f;

struct t_timestamp {
    unsigned long hh;
    unsigned long mm;
    unsigned long ss;
    unsigned long ms;
};

struct t_listSection {
    uint32_t begin;
    uint32_t end;
    uint32_t delay_until;
};

struct t_move {
    int16_t deltaX;
    int16_t deltaY;
    bool symmetrical = false; //setting false by default as a safety thing
std::string listMove;
    std::vector<t_listSection> sectionMove;
};

struct t_crop {
    uint16_t left;
    uint16_t top;
    uint16_t right;
    uint16_t bottom;
};

enum e_listFormat : uint8_t {
    secut       = 0, //"1000-2000;3000-4000" both inclusive
    vapoursynth = 1, //"[1000:2001] [3000:4001]" start inclusive, end exclusive
    avisynth    = 2, //"(1000,2000) (3000,4000)" both inclusive
    remap       = 3  //"[1000 2000] [3000 4000]" both inclusive
};

enum e_listTimeMode : uint8_t {
    ms = 0,
    frame = 1,
    timestamp = 2
};

enum e_cutMergeFixMode : uint8_t {
    del = 0, //delete section if not fully contained
    cut = 0  //cut begin and/or end to match current section
};

bool compareListSection(t_listSection a, t_listSection b) {
    return (a.begin < b.begin);
}

struct t_listOption {
    e_listFormat format     = e_listFormat::secut;
    e_listTimeMode timeMode = e_listTimeMode::timestamp;
    double fps;
};

struct t_cutMerge {
    bool doCutMerge = false;
    e_cutMergeFixMode fixMode = e_cutMergeFixMode::cut;
    std::string list;
    std::vector<t_listSection> section;
    std::vector<t_cutMergeSection> section;
};

struct t_cmd {
    std::string inputFile;
    std::string outputFile;
    bool trace = false;
    int32_t delay = 0;
    t_move move = {};
    t_crop crop = {};
    double resync = 1;
    bool addZero = false;
    double tonemap = 1;
    t_listOption listOption = {};
    t_cutMerge cutMerge = {};
    t_toggleForced toggleForced = {};
};


void toLower(std::string& str) {
    std::transform(str.begin(), str.end(), str.begin(), [](int i) {
        return std::tolower(i);
    });
}

t_timestamp ptsToTimestamp(uint32_t pts) {
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

uint32_t timestampToPTS(char* timestamp) {
    int tokenRead;
    int hh, mm, ss;
    double ms;
    char str_ms[11];
    int msDecimals = 0;
    char *errPtr = nullptr;

    tokenRead = std::sscanf(timestamp, "%d:%d:%d.%10s", &hh, &mm, &ss, str_ms);
    if (tokenRead != 4) {
        return -1;
    }

    //We want to obtain the milliseconds, so at least 3 decimals
    msDecimals = strnlen(str_ms, 10) - 3;

    ms = strtod(str_ms, &errPtr);
    if (*errPtr != '\0'){
        return -1;
    }
    ms = ms / std::pow(10, msDecimals);

    return (uint32_t)std::round(((( (hh * 60 * 60) + (mm * 60) + ss) * 1000) + ms) * MS_TO_PTS_MULT);
}

bool parseListSection(std::string list, std::vector<t_listSection>& sections, t_listOption listOption) {
    std::string pattern;

    switch (listOption.format)
    {
    case e_listFormat::secut:
    {
        pattern = "%[0123456789:.]-%[0123456789:.]%n";
        break;
    }
    case e_listFormat::vapoursynth:
    {
        pattern = "[%[0123456789]:%[0123456789]]%n";
        break;
    }
    case e_listFormat::avisynth:
    {
        pattern = "(%[0123456789:.],%[0123456789:.])%n";
        break;
    }
    case e_listFormat::remap:
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

    do {
        tokenRead = std::sscanf(list.c_str(), pattern.c_str(), strBeg, strEnd, &charRead);
        if (tokenRead != 2) {
            return false;
        }

        t_listSection section = {};
        switch (listOption.timeMode)
        {
        case e_listTimeMode::ms:
        {
            beg = std::atoi(strBeg);
            end = std::atoi(strEnd);

            section.begin = (uint32_t)std::round(beg * MS_TO_PTS_MULT);
            section.end   = (uint32_t)std::round(end * MS_TO_PTS_MULT);
            break;
        }
        case e_listTimeMode::frame:
        {
            beg = std::atoi(strBeg);
            end = std::atoi(strEnd);

            if (listOption.format == e_listFormat::vapoursynth) {
                end--;
            }

            section.begin = (uint32_t)std::round(((double)beg) / listOption.fps * MS_TO_PTS_MULT);
            section.end   = (uint32_t)std::round(((double)end) / listOption.fps * MS_TO_PTS_MULT);
            break;
        }
        case e_listTimeMode::timestamp:
        {
            beg = timestampToPTS(strBeg);
            end = timestampToPTS(strEnd);
            if (beg == -1) {
                std::fprintf(stderr, "Timestamp %s is invalid\n", strBeg);
                return false;
            }
            if (beg == -1) {
                std::fprintf(stderr, "Timestamp %s is invalid\n", strEnd);
                return false;
            }

            section.begin = beg;
            section.end   = end;
            break;
        }
        default:
            break;
        }

        sections.push_back(section);

        if (charRead + 1 < (int)list.length()) {
            list = list.substr(charRead + 1, list.length());
        }
        else {
            list.clear();
        }
    } while (list.length() > 0);

    std::sort(sections.begin(), sections.end(), compareListSection);

    int32_t runningDelay = 0;
    for (int i = 0; i < (int)sections.size(); i++) {
        sections[i].delay_until = runningDelay + sections[i].begin;
        runningDelay += (sections[i].begin - sections[i].end);
    }

    return true;
}

bool parseCMD(int32_t argc, char** argv, t_cmd& cmd) {
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
        else if (arg == "symmetrical" || arg == "--symmetrical") {
            cmd.move.symmetrical = true;
            i++;
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
        else if (arg == "list-format" || arg == "--list-format") {
            if (remaining < 1) return false;
            std::string formatMode = argv[i++];
            toLower(formatMode);

            if (formatMode == "secut") {
                cmd.listOption.format = e_listFormat::secut;
            }
            else if (formatMode == "vapoursynth" || formatMode == "vs") {
                cmd.listOption.format = e_listFormat::vapoursynth;
            }
            else if (formatMode == "avisynth" || formatMode == "avs") {
                cmd.listOption.format = e_listFormat::avisynth;
            }
            else if (formatMode == "remap") {
                cmd.listOption.format = e_listFormat::remap;
            }
            else {
                return false;
            }
        }
        else if (arg == "cutmerge-list" || arg == "--cutmerge-list") {
            if (remaining < 1) return false;
            std::string list = argv[i++];
            toLower(list);

            cmd.cutMerge.list = list;

            cmd.cutMerge.doCutMerge = true;
        }
        else if (arg == "list-timemode" || arg == "--list-timemode") {
            if (remaining < 1) return false;
            std::string timemode = argv[i++];
            toLower(timemode);

            if (timemode == "ms") {
                cmd.listOption.timeMode = e_listTimeMode::ms;
            }
            else if (timemode == "frame") {
                if (remaining < 2) return false;
                cmd.listOption.timeMode = e_listTimeMode::frame;
                std::string strFactor = argv[i];

                size_t idx = strFactor.find("/");
                if (idx != SIZE_MAX) {
                    double num = std::atof(strFactor.substr(0, idx).c_str());
                    double den = std::atof(strFactor.substr(idx + 1, strFactor.length()).c_str());

                    cmd.listOption.fps = num / den;
                }
                else {
                    cmd.listOption.fps = std::atof(argv[i]);
                }
                i++;
            }
            else if (timemode == "timestamp") {
                cmd.listOption.timeMode = e_listTimeMode::timestamp;
            }
            else {
                return false;
            }
        }
        else if (arg == "cutmerge-fixmode" || arg == "--cutmerge-fixmode") {
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
        else if (arg == "move-list" || arg == "--move-list"){
            if (remaining < 1) return false;
            std::string list = argv[i++];
            toLower(list);

            cmd.move.listMove = list;
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

    if (   cmd.listOption.format   == e_listFormat::vapoursynth
        && cmd.listOption.timeMode == e_listTimeMode::timestamp) {
        std::fprintf(stderr, "List format mode VapourSynth cannot be used alongside timestamp time mode\n");

        return false;
    }

    if (cmd.cutMerge.doCutMerge) {
        if (!parseListSection(cmd.cutMerge.list, cmd.cutMerge.section, cmd.listOption)) {
            return false;
        }
    }
    
    if (!cmd.move.listMove.empty()){
        if (!parseListSection(cmd.move.listMove, cmd.move.sectionMove, cmd.listOption)) {
            return false;
        }
    }

    return true;
}


const char* usageHelp =
R"(SupMover v2.5.0
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
    [LIST FORMAT OPTION]

LIST FORMAT OPTION
  --list-format ({secut} | (vapoursynth | vs) | (avisynth | avs) | remap)
  --list-timemode ({timestamp} | ms | frame (<num>/<den> | <fps>))

EXPLANATION
    trace: output the content of the SUP file
    delay: move all timestamp by the specified ms
    resync: speedup or speedown all timestamp by the specified amount
    move: move the position of all subpitcure by the specified amount (move is always done before crop)
    symmetrical: apply the move command in a symmetrical way towards the center
    move-list: apply the move command only to the specified sections
    crop: shrink the image area by the specified amount, if the image is outside the new area it will be moved (crop is always done after move)
    add_zero: add a dummy section at the beginning useful for some player
    tonemap: lower or increase the brightness of the image
    cutmerge-list: cut all the section and merge them to a new file (currently confirmed not working)
    cutmerge-fixmode: determine the way in which to handle subtitle which are partially contained inside a section
    set/unsetforced_list: set or unset the forced flag in the specified sections

Delay and resync command are executed in the order supplied.
)";