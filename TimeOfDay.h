#ifndef TIMEOFDAY_H
#define TIMEOFDAY_H

#include <iostream>
#include <string>
#include <sstream>
#include <stdexcept>
using namespace std;

// thrown when a time like "25:70" or "abc" is given
class InvalidTimeException : public runtime_error {
public:
    InvalidTimeException(string text) : runtime_error("Invalid time '" + text + "'") {}
};

// A time of day, stored as minutes from midnight (10:30 -> 630).
// Operators let the rest of the code write things like: arrive = now + travel;
class Time {
private:
    int mins;

public:
    Time() {
        mins = 0;
    }

    Time(int hours, int minutes) {
        if (hours < 0 || hours > 24 || minutes < 0 || minutes > 59 || (hours == 24 && minutes > 0))
            throw InvalidTimeException(to_string(hours) + ":" + to_string(minutes));
        mins = hours * 60 + minutes;
    }

    // accepts "10:30", "9:05" or just "10"
    static Time parse(string text) {
        stringstream ss(text);
        int h, m = 0;
        char colon;
        if (!(ss >> h)) throw InvalidTimeException(text);
        if (ss >> colon) {
            if (colon != ':' || !(ss >> m)) throw InvalidTimeException(text);
        }
        string extra;
        if (ss >> extra) throw InvalidTimeException(text);
        return Time(h, m);
    }

    Time operator+(int minutes) const {
        Time t;
        t.mins = mins + minutes;
        return t;
    }

    int operator-(const Time& other) const {   // difference in minutes
        return mins - other.mins;
    }

    bool operator<(const Time& other) const { return mins < other.mins; }
    bool operator>(const Time& other) const { return mins > other.mins; }
    bool operator<=(const Time& other) const { return mins <= other.mins; }

    // "10:30 AM"
    string toString() const {
        int h = (mins / 60) % 24;
        int m = mins % 60;
        string ampm = (h >= 12) ? "PM" : "AM";
        if (h == 0) h = 12;
        if (h > 12) h = h - 12;

        string s = to_string(h) + ":";
        if (m < 10) s += "0";
        return s + to_string(m) + " " + ampm;
    }

    friend ostream& operator<<(ostream& out, const Time& t) {
        out << t.toString();
        return out;
    }
};

#endif
