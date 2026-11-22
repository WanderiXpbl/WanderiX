#ifndef SCORER_H
#define SCORER_H

#include "Place.h"

// The points a place earns, part by part (so we can show WHY it was chosen).
struct ScoreCard {
    double mood;     // suits your mood?
    double rating;   // well rated?
    double cost;     // cheap compared with your budget?
    double time;     // short visit, leaves room for more places?
    double near;     // close to where you are now?
    double repeat;   // minus points: same category already picked
    double bonus;    // extra points for a category not picked yet (Explorer style)
    double total;
};

// How places are scored. The styles only differ in their weights, so one class is enough:
//
// total = wMood   x moodMatch                              (0..1)
//       + wRating x (rating - 3) / 2                       3 stars -> 0, 5 stars -> 1
//       + wCost   x (1 - cost / budget)                    free -> 1, whole budget -> 0
//       + wTime   x (1 - visit / 240 min)                  short visit -> close to 1
//       + wNear   x (1 - travel from where you are / 60)   next door -> 1, 1 h -> 0, 2 h -> -1
//       - wRepeat x (places of the same category already picked)
//       + newCategoryBonus, if no place of this category is picked yet
class Scorer {
private:
    string name;
    double wMood, wRating, wCost, wTime, wNear, wRepeat, newCategoryBonus;

public:
    Scorer(string name, double wMood, double wRating, double wCost, double wTime, double wNear, double wRepeat,
           double newCategoryBonus) {
        this->name = name;
        this->wMood = wMood;
        this->wRating = wRating;
        this->wCost = wCost;
        this->wTime = wTime;
        this->wNear = wNear;
        this->wRepeat = wRepeat;
        this->newCategoryBonus = newCategoryBonus;
    }

    // the ready-made styles: 1 Balanced, 2 Budget saver, 3 Top rated, 4 Explorer
    static Scorer preset(int style) {
        //                         mood  rating cost  time  near  repeat bonus
        if (style == 2) return Scorer("Budget saver", 0.25, 0.15, 0.30, 0.10, 0.20, 0.10, 0.00);
        if (style == 3) return Scorer("Top rated",    0.25, 0.40, 0.05, 0.10, 0.20, 0.10, 0.00);
        if (style == 4) return Scorer("Explorer",     0.30, 0.15, 0.15, 0.10, 0.20, 0.20, 0.10);
        return Scorer("Balanced", 0.35, 0.20, 0.15, 0.10, 0.20, 0.10, 0.00);
    }

    string getName() { return name; }
    double getMoodWeight() { return wMood; }
    double getRatingWeight() { return wRating; }
    double getCostWeight() { return wCost; }
    double getTimeWeight() { return wTime; }
    double getNearWeight() { return wNear; }
    double getRepeatWeight() { return wRepeat; }

    ScoreCard score(Place* p, int mood, int budget, int travel, int sameCategoryPicked) {
        ScoreCard c;

        c.mood = wMood * p->moodMatch(mood);

        double r = (p->getRating() - 3) / 2;
        if (r < 0) r = 0;
        c.rating = wRating * r;

        double money = 1;
        if (budget > 0) money = 1 - (double)p->getCost() / budget;
        c.cost = wCost * money;

        double t = 1 - p->getDuration() / 240.0;
        if (t < 0) t = 0;
        c.time = wTime * t;

        c.near = wNear * (1 - travel / 60.0);   // more than an hour away -> minus points

        c.repeat = -wRepeat * sameCategoryPicked;
        c.bonus = (sameCategoryPicked == 0) ? newCategoryBonus : 0;
        c.total = c.mood + c.rating + c.cost + c.time + c.near + c.repeat + c.bonus;
        return c;
    }
};

#endif
