#ifndef PLACE_H
#define PLACE_H

#include "Utils.h"
#include <iomanip>

// base class (abstract)
class Place {
protected:
    int id;
    string name;
    double rating;
    int cost;       // in Rs
    int duration;   // visit time in minutes
    Time openTime;
    Time closeTime;
    bool closedOn[7];   // closedOn[0] = closed on Monday

public:
    Place(int id, string name, double rating, int cost, int duration, Time openTime, Time closeTime) {
        this->id = id;
        this->name = name;
        this->rating = rating;
        this->cost = cost;
        this->duration = duration;
        this->openTime = openTime;
        this->closeTime = closeTime;
        for (int d = 0; d < 7; d++)
            closedOn[d] = false;
    }
    virtual ~Place() {}

    // text from the data file: "-" = open every day, "Mon" or "Mon,Tue" = closed those days
    void setClosedDays(string text) {
        for (int d = 0; d < 7; d++)
            closedOn[d] = (text.find(DAY_SHORT[d]) != string::npos);
    }

    bool isClosedOn(int day) { return closedOn[day]; }

    string closedDaysText() {
        string s = "";
        for (int d = 0; d < 7; d++)
            if (closedOn[d]) s += (s == "" ? "" : ",") + DAY_SHORT[d];
        return s == "" ? "-" : s;
    }

    int getId() { return id; }
    string getName() { return name; }
    double getRating() { return rating; }
    int getCost() { return cost; }
    int getDuration() { return duration; }
    Time getOpenTime() { return openTime; }
    Time getCloseTime() { return closeTime; }

    // every child class must write these two
    virtual string getCategory() = 0;
    virtual double moodMatch(int mood) = 0;   // 0 to 1, how much this place suits the mood

    void print() {
        cout << setw(3) << id << "  " << left << setw(24) << name << setw(10) << getCategory() << right
             << fixed << setprecision(1) << rating << "/5   Rs." << setw(4) << cost
             << "   " << setw(3) << duration << " min   "
             << openTime << " - " << closeTime;
        if (closedDaysText() != "-") cout << "  (closed " << closedDaysText() << ")";
        cout << endl;
    }
};

class FoodPlace : public Place {
public:
    FoodPlace(int id, string name, double rating, int cost, int duration, Time open, Time close)
        : Place(id, name, rating, cost, duration, open, close) {}

    string getCategory() { return "Food"; }

    double moodMatch(int mood) {
        if (mood == FOODIE) return 1.0;
        if (mood == RELAXED) return 0.7;
        return 0.4;
    }
};

class NaturePlace : public Place {
public:
    NaturePlace(int id, string name, double rating, int cost, int duration, Time open, Time close)
        : Place(id, name, rating, cost, duration, open, close) {}

    string getCategory() { return "Nature"; }

    double moodMatch(int mood) {
        if (mood == RELAXED || mood == ADVENTUROUS) return 1.0;
        return 0.3;
    }
};

class CulturePlace : public Place {
public:
    CulturePlace(int id, string name, double rating, int cost, int duration, Time open, Time close)
        : Place(id, name, rating, cost, duration, open, close) {}

    string getCategory() { return "Culture"; }

    double moodMatch(int mood) {
        if (mood == CULTURAL) return 1.0;
        if (mood == ADVENTUROUS) return 0.6;
        if (mood == RELAXED) return 0.5;
        return 0.2;
    }
};

class ShoppingPlace : public Place {
public:
    ShoppingPlace(int id, string name, double rating, int cost, int duration, Time open, Time close)
        : Place(id, name, rating, cost, duration, open, close) {}

    string getCategory() { return "Shopping"; }

    double moodMatch(int mood) {
        if (mood == SHOPPING) return 1.0;
        return 0.3;
    }
};

#endif
