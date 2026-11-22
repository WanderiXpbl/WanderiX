#ifndef ROUTE_H
#define ROUTE_H

#include "Place.h"
#include "Graph.h"
#include "Scorer.h"
#include <vector>
#include <queue>
#include <unordered_map>
using namespace std;

// Everything the route builder needs to know about the trip.
struct TripSettings {
    int startPlace;     // 0 = Home
    int returnTo;       // -1 = trip ends at the last place, else the node we must get back to
    Time startTime;
    Time endTime;
    int mood;
    int budget;
    int mealGap;        // a food stop must start within this many minutes of the start / of the last meal
};

// One step of the loop, kept so the website can show how the plan was made.
struct StepNote {
    Place* chosen;
    Time at;                                   // when we were deciding
    Time start;                                // when the chosen visit starts
    double score;
    ScoreCard card;
    int considered;                            // places that followed every rule (went into the heap)
    vector<pair<Place*, double>> top;          // best places in the heap at this step
    vector<pair<Place*, string>> skipped;      // places that broke a rule, and why
};

// ONE algorithm picks the places AND puts them in order:
//
//   from where we are now, look at every place not used yet,
//   throw away the ones that break a rule, put the rest in a max-heap by score,
//   take the best one, move there, and repeat.
//
// Rules:  budget · open when we arrive · finish before it closes · get back in time (if returning) ·
//         no two Food places one after the other ·
//         eat within mealGap minutes of the start and of the last meal
//         (a non-food stop may not run past that deadline; a food stop must start by it) ·
//         when making a new plan: after a non-food stop, some food place must still be reachable in time
//         (so the trip never gets stuck far from food)
class RouteBuilder {
private:
    Graph* graph;

    // after visiting p (finishing at 'leave'), can we still start a meal at some food place by the deadline?
    bool foodReachable(Place* p, Time leave, int cost, Time mealDeadline, const TripSettings& t,
                       const vector<Place*>& candidates, const vector<bool>& used) {
        for (int i = 0; i < candidates.size(); i++) {
            Place* f = candidates[i];
            if (used[i] || f == p || f->getCategory() != "Food") continue;
            if (cost + p->getCost() + f->getCost() > t.budget) continue;
            int travel = graph->getTime(p->getId(), f->getId());
            if (travel == INF) continue;
            Time start = max(leave + travel, f->getOpenTime());
            Time end = start + f->getDuration();
            int back = (t.returnTo < 0) ? 0 : graph->getTime(f->getId(), t.returnTo);
            if (start <= mealDeadline && end <= f->getCloseTime() && end + back <= t.endTime) return true;
        }
        return false;
    }

    // why can't we go to p next? "" = we can
    string ruleBroken(Place* p, int at, Time now, int cost, string lastCategory, Time mealDeadline,
                      const TripSettings& t, bool lookAhead, const vector<Place*>& candidates,
                      const vector<bool>& used, Time& visitStart) {
        if (cost + p->getCost() > t.budget) return "over budget";

        int travel = graph->getTime(at, p->getId());
        if (travel == INF) return "no road";

        visitStart = max(now + travel, p->getOpenTime());   // wait if it is not open yet
        Time visitEnd = visitStart + p->getDuration();
        if (visitEnd > p->getCloseTime()) return "closes before you would finish";

        int back = (t.returnTo < 0) ? 0 : graph->getTime(p->getId(), t.returnTo);
        if (visitEnd + back > t.endTime) return "you could not finish (and get back) in time";

        bool food = p->getCategory() == "Food";
        if (food && lastCategory == "Food") return "Food right after Food";
        if (food && mealDeadline < visitStart) return "too late for the next meal";
        if (!food && mealDeadline < visitEnd) return "you need to eat by " + mealDeadline.toString() + " first";
        if (!food && lookAhead && mealDeadline < t.endTime &&
            !foodReachable(p, visitEnd, cost, mealDeadline, t, candidates, used))
            return "no food place reachable after it by " + mealDeadline.toString();
        return "";
    }

public:
    RouteBuilder(Graph* graph) {
        this->graph = graph;
    }

    // candidates: places allowed in the plan.
    // useAll = true  -> every candidate must be visited (used after add/remove/undo); false if impossible.
    // useAll = false -> take places while they fit, at most maxStops (used to make a new plan).
    // Fills order, travel (including the trip back) and finish. notes may be NULL.
    bool build(vector<Place*> candidates, bool useAll, int maxStops, const TripSettings& t, Scorer* scorer,
               vector<Place*>& order, int& travel, Time& finish, vector<StepNote>* notes) {
        order.clear();
        travel = 0;
        if (notes) notes->clear();

        vector<bool> used(candidates.size(), false);
        unordered_map<string, int> picked;          // category -> how many already in the plan
        int at = t.startPlace;
        Time now = t.startTime;
        Time mealDeadline = t.startTime + t.mealGap;
        string lastCategory = "";
        int cost = 0;

        while (order.size() < maxStops) {
            priority_queue<pair<double, int>> heap;  // {score, index in candidates}: best on top
            vector<Time> starts(candidates.size());
            StepNote note;
            note.at = now;

            for (int i = 0; i < candidates.size(); i++) {
                if (used[i]) continue;
                Place* p = candidates[i];
                string why = ruleBroken(p, at, now, cost, lastCategory, mealDeadline, t, !useAll, candidates, used,
                                        starts[i]);
                if (why != "") {
                    note.skipped.push_back({p, why});
                    continue;
                }
                int travelHere = graph->getTime(at, p->getId());
                double s = scorer->score(p, t.mood, t.budget, travelHere, picked[p->getCategory()]).total;
                heap.push({s, i});
            }

            if (heap.empty()) break;   // nothing else fits

            // keep the top 5 for the website, then take the best
            note.considered = heap.size();
            priority_queue<pair<double, int>> copy = heap;
            while (!copy.empty() && note.top.size() < 5) {
                note.top.push_back({candidates[copy.top().second], copy.top().first});
                copy.pop();
            }
            int best = heap.top().second;
            Place* p = candidates[best];
            int travelHere = graph->getTime(at, p->getId());

            note.chosen = p;
            note.start = starts[best];
            note.card = scorer->score(p, t.mood, t.budget, travelHere, picked[p->getCategory()]);
            note.score = note.card.total;
            if (notes) notes->push_back(note);

            // move there
            used[best] = true;
            order.push_back(p);
            travel += travelHere;
            now = starts[best] + p->getDuration();
            at = p->getId();
            cost += p->getCost();
            picked[p->getCategory()]++;
            lastCategory = p->getCategory();
            if (lastCategory == "Food") mealDeadline = now + t.mealGap;   // just ate: next meal due later
        }

        if (useAll && order.size() < candidates.size()) return false;

        int back = (t.returnTo < 0) ? 0 : graph->getTime(at, t.returnTo);
        travel += back;
        finish = now + back;
        return true;
    }
};

#endif
