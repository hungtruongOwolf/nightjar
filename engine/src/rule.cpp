#include "nightjar/rule.h"

#include <cctype>

namespace nightjar {

bool TimeWindow::contains(int minute_of_day) const {
    if (start_min == end_min) return true;              // always active
    if (start_min < end_min) return minute_of_day >= start_min && minute_of_day < end_min;
    return minute_of_day >= start_min || minute_of_day < end_min;  // overnight wrap
}

int parse_hhmm(const std::string& s) {
    if (s.size() != 5 || s[2] != ':') return -1;
    for (int i : {0, 1, 3, 4}) {
        if (!std::isdigit(static_cast<unsigned char>(s[i]))) return -1;
    }
    const int hh = (s[0] - '0') * 10 + (s[1] - '0');
    const int mm = (s[3] - '0') * 10 + (s[4] - '0');
    if (hh > 23 || mm > 59) return -1;
    return hh * 60 + mm;
}

}  // namespace nightjar
