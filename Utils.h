#ifndef UTILS_H
#define UTILS_H

#include "TimeOfDay.h"
#include <iostream>
#include <string>
#include <cstdlib>
#include <algorithm>
using namespace std;

const int INF = 1000000000;   // means "no road"

// moods
const int RELAXED = 1;
const int FOODIE = 2;
const int ADVENTUROUS = 3;
const int CULTURAL = 4;
const int SHOPPING = 5;

// days of the week, 0 = Monday
const string DAY_NAMES[7] = {"Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Sunday"};
const string DAY_SHORT[7] = {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};

// reads a number between low and high, asks again on wrong input
double readNumber(string msg, double low, double high) {
    double x;
    cout << msg;
    while (!(cin >> x) || x < low || x > high) {
        if (cin.eof()) exit(0);
        cin.clear();
        cin.ignore(1000, '\n');
        cout << "Invalid! Enter a number from " << low << " to " << high << ": ";
    }
    return x;
}

// reads a time like 10:30, asks again if it is not a valid time
Time readTime(string msg) {
    while (true) {
        cout << msg;
        string text;
        if (!(cin >> text)) exit(0);
        try {
            return Time::parse(text);
        } catch (InvalidTimeException& e) {
            cout << "  " << e.what() << ". Use HH:MM, for example 10:30" << endl;
        }
    }
}

#endif
