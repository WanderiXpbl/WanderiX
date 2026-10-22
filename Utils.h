#ifndef UTILS_H
#define UTILS_H

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

// converts minutes from midnight to time like "10:15 AM"
string timeToString(int minutes) {
    int h = (minutes / 60) % 24;
    int m = minutes % 60;
    string ampm = "AM";
    if (h >= 12) ampm = "PM";
    if (h == 0) h = 12;
    if (h > 12) h = h - 12;

    string s = to_string(h) + ":";
    if (m < 10) s = s + "0";
    s = s + to_string(m) + " " + ampm;
    return s;
}

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

#endif
