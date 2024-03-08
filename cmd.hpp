double const MS_TO_PTS_MULT = 90.0f;

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

bool compareCutMergeSection(t_cutMergeSection a, t_cutMergeSection b) {
    return (a.begin < b.begin);
}

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

int timestampToMs(char* timestamp) {
    int tokenRead;
    int hh, mm, ss, ms;

    tokenRead = std::sscanf(timestamp, "%d:%d:%d.%d", &hh, &mm, &ss, &ms);
    if (tokenRead != 4) {
        return -1;
    }

    return ((((hh * 60 * 60) + (mm * 60) + ss) * 1000) + ms);
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

        if (charRead + 1 < (int)list.length()) {
            list = list.substr(charRead + 1, list.length());
        }
        else {
            list.clear();
        }
    } while (list.length() > 0);

    std::sort(cutMerge->section.begin(), cutMerge->section.end(), compareCutMergeSection);

    int32_t runningDelay = 0;
    for (int i = 0; i < (int)cutMerge->section.size(); i++) {
        cutMerge->section[i].delay_until = runningDelay + cutMerge->section[i].begin;
        runningDelay += (cutMerge->section[i].begin - cutMerge->section[i].end);
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
