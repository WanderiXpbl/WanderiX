#ifndef ROUTE_H
#define ROUTE_H

#include "Place.h"
#include "Graph.h"
#include <vector>
using namespace std;

// Decides in what order to visit the chosen places.
//
// Greedy idea: from where we are now, go to the place where we can
// START the visit soonest (travel time + waiting if it's not open yet).
// Skip places that would be closed or make us late.
//
// Phase 3: replace this with a better method (try all orders).
//
// Fills order, travel, finish. Returns false if the places can't all fit.
bool findRoute(Graph& graph, vector<Place*> places, int startPlace, int startTime, int endTime,
               vector<Place*>& order, int& travel, int& finish) {
    order.clear();
    travel = 0;
    int time = startTime;
    int current = startPlace;

    while (!places.empty()) {
        int best = -1;
        int bestStart = INF;

        for (int i = 0; i < places.size(); i++) {
            Place* p = places[i];
            int t = graph.getTime(current, p->getId());
            if (t == INF) continue;   // no road

            int visitStart = max(time + t, p->getOpenTime());   // wait if not open yet
            int visitEnd = visitStart + p->getDuration();
            if (visitEnd > p->getCloseTime() || visitEnd > endTime) continue;   // doesn't fit

            // soonest start wins, if same time take smaller id
            if (visitStart < bestStart || (visitStart == bestStart && p->getId() < places[best]->getId())) {
                best = i;
                bestStart = visitStart;
            }
        }

        if (best == -1) return false;   // nothing left fits

        Place* p = places[best];
        travel += graph.getTime(current, p->getId());
        time = bestStart + p->getDuration();
        current = p->getId();
        order.push_back(p);
        places.erase(places.begin() + best);
    }

    finish = time;
    return true;
}

#endif
