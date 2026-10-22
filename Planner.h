#ifndef PLANNER_H
#define PLANNER_H

#include "Place.h"
#include "Graph.h"
#include "Route.h"
#include "Itinerary.h"
#include <fstream>
#include <sstream>
#include <vector>
#include <stack>
#include <unordered_map>
#include <unordered_set>
using namespace std;

const int MAX_STOPS = 7;   // max places picked automatically

// saved change, used for undo
struct Action {
    char type;   // 'A' = added, 'R' = removed
    Place* place;
};

class Planner {
private:
    vector<Place*> places;                 // all places
    unordered_map<int, Place*> placeById;  // id -> place
    Graph graph;

    // user answers
    int budget;
    int startTime;
    int endTime;
    int mood;
    int startPlace;                        // 0 = Home, otherwise a place id

    vector<Place*> plan;                   // chosen places
    unordered_set<int> inPlan;             // ids of chosen places (fast check)
    vector<Place*> route;                  // chosen places in visiting order
    int routeTravel;
    int routeEnd;

    Itinerary itinerary;
    stack<Action> history;

public:
    Planner() {
        budget = 0;
        startTime = 0;
        endTime = 0;
        mood = RELAXED;
        startPlace = 0;
        routeTravel = 0;
        routeEnd = 0;
    }

    ~Planner() {
        for (int i = 0; i < places.size(); i++)
            delete places[i];
    }

    // ================= loading =================

    bool loadData(string placesFile, string roadsFile) {
        ifstream fin(placesFile);
        if (!fin) {
            cout << "Cannot open " << placesFile << endl;
            return false;
        }

        int id, cost, duration, openHour, closeHour;
        string name, category;
        double rating;
        int maxId = 0;

        string line;
        while (getline(fin, line)) {
            if (line.empty()) continue;

            // read one line at a time, so one bad line doesn't break the rest
            stringstream ss(line);
            if (!(ss >> id >> name >> category >> rating >> cost >> duration >> openHour >> closeHour)) {
                cout << "Bad line in " << placesFile << ", skipped: " << line << endl;
                continue;
            }
            replace(name.begin(), name.end(), '_', ' ');   // Cafe_Mocha -> Cafe Mocha
            int open = openHour * 60, close = closeHour * 60;

            Place* p;
            if (category == "Food") p = new FoodPlace(id, name, rating, cost, duration, open, close);
            else if (category == "Nature") p = new NaturePlace(id, name, rating, cost, duration, open, close);
            else if (category == "Culture") p = new CulturePlace(id, name, rating, cost, duration, open, close);
            else if (category == "Shopping") p = new ShoppingPlace(id, name, rating, cost, duration, open, close);
            else {
                cout << "Unknown category " << category << ", skipped" << endl;
                continue;
            }

            places.push_back(p);
            placeById[id] = p;
            if (id > maxId) maxId = id;
        }
        fin.close();

        ifstream rin(roadsFile);
        if (!rin) {
            cout << "Cannot open " << roadsFile << endl;
            return false;
        }
        graph.setSize(maxId + 1);
        int u, v, minutes;
        while (rin >> u >> v >> minutes)
            graph.addEdge(u, v, minutes);
        rin.close();

        graph.buildTable();
        cout << "Loaded " << places.size() << " places." << endl;
        return true;
    }

    void setTrip(int budget, int startTime, int endTime, int mood, int startPlace) {
        this->budget = budget;
        this->startTime = startTime;
        this->endTime = endTime;
        this->mood = mood;
        this->startPlace = startPlace;
    }

    bool hasPlace(int id) {
        return placeById.count(id) > 0;
    }

    // ================= choosing places =================

    // score from 0 to 1, higher = better
    //   50% how well it suits the mood
    //   35% rating   (3 stars or less -> 0, 5 stars -> 1)
    //   15% cost     (free -> 1, costs whole budget -> 0)
    //   -0.1 for every place of the same category already picked (for variety)
    double score(Place* p, int sameCategoryPicked) {
        double moodPart = p->moodMatch(mood);

        double ratingPart = (p->getRating() - 3) / 2;
        if (ratingPart < 0) ratingPart = 0;

        double costPart = 1;
        if (budget > 0) costPart = 1 - (double)p->getCost() / budget;

        return 0.5 * moodPart + 0.35 * ratingPart + 0.15 * costPart - 0.1 * sameCategoryPicked;
    }

    int totalCost() {
        int sum = 0;
        for (int i = 0; i < plan.size(); i++)
            sum += plan[i]->getCost();
        return sum;
    }

    bool isGoodPlace(Place* p) {
        if (p->getCost() > budget) return false;
        if (p->moodMatch(mood) < 0.4) return false;
        if (p->getId() == startPlace) return false;                       // already there
        if (graph.getTime(startPlace, p->getId()) == INF) return false;   // no road
        return true;
    }

    // asks the route planner to order 'plan'. Returns false if it doesn't fit.
    bool updateRoute() {
        vector<Place*> order;
        int travel, finish;
        if (!findRoute(graph, plan, startPlace, startTime, endTime, order, travel, finish))
            return false;

        route = order;
        routeTravel = travel;
        routeEnd = finish;
        buildItinerary();
        return true;
    }

    // best score first (max heap). Keep a place only if budget and time still work.
    void makePlan() {
        plan.clear();
        inPlan.clear();
        while (!history.empty()) history.pop();
        updateRoute();   // empty plan

        unordered_map<string, int> picked;        // category -> how many picked
        priority_queue<pair<double, int>> heap;   // {score, id}
        for (int i = 0; i < places.size(); i++)
            if (isGoodPlace(places[i]))
                heap.push({score(places[i], 0), places[i]->getId()});

        while (!heap.empty() && plan.size() < MAX_STOPS) {
            double oldScore = heap.top().first;
            Place* p = placeById[heap.top().second];
            heap.pop();

            // score drops if we already picked this category -> put it back with new score
            double newScore = score(p, picked[p->getCategory()]);
            if (newScore < oldScore) {
                heap.push({newScore, p->getId()});
                continue;
            }

            if (totalCost() + p->getCost() > budget) continue;

            plan.push_back(p);
            if (updateRoute()) {
                inPlan.insert(p->getId());
                picked[p->getCategory()]++;
            } else {
                plan.pop_back();   // no time for this place
            }
        }
    }

    // puts the route into the linked list with the timings
    void buildItinerary() {
        itinerary.clear();
        int time = startTime;
        int current = startPlace;

        for (int i = 0; i < route.size(); i++) {
            Place* p = route[i];
            int travel = graph.getTime(current, p->getId());
            time += travel;

            int wait = 0;
            if (time < p->getOpenTime()) {
                wait = p->getOpenTime() - time;
                time = p->getOpenTime();
            }
            itinerary.addStop(p, travel, wait, time);
            time += p->getDuration();
            current = p->getId();
        }
    }

    // ================= printing =================

    void showPlan() {
        printPlan(cout);
    }

    void savePlan() {
        if (itinerary.size() == 0) {
            cout << "Nothing to save, the plan is empty." << endl;
            return;
        }
        string fileName;
        cout << "File name (e.g. trip.txt): ";
        cin >> fileName;

        ofstream fout(fileName);
        if (!fout) {
            cout << "Could not create " << fileName << endl;
            return;
        }
        printPlan(fout);
        fout.close();
        cout << "Plan saved to " << fileName << endl;
    }

    // 'out' can be cout (screen) or an ofstream (file), both work the same way
    void printPlan(ostream& out) {
        string moods[6] = {"", "Relaxed", "Foodie", "Adventurous", "Cultural", "Shopping"};
        out << endl << "==================================================================" << endl;
        out << " YOUR ITINERARY (" << timeToString(startTime) << " - " << timeToString(endTime)
             << ")   Mood: " << moods[mood] << endl;
        out << "==================================================================" << endl;

        if (itinerary.size() == 0) {
            out << " No places found. Try more budget or more time." << endl;
            return;
        }

        out << setw(9) << timeToString(startTime) << "   Start - " << (startPlace == 0 ? "Home" : placeById[startPlace]->getName()) << endl;

        int visitTime = 0, number = 1;
        Stop* s = itinerary.getHead();

        while (s != NULL) {
            Place* p = s->place;
            out << setw(9) << timeToString(s->start) << "   " << number << ". " << p->getName()
                 << " (" << p->getCategory() << ") " << fixed << setprecision(1) << p->getRating() << "/5"
                 << " - Rs." << p->getCost() << " - " << s->travel << " min travel - "
                 << p->getDuration() << " min visit";
            if (s->wait > 0) out << " (waited " << s->wait << " min)";
            out << endl;

            visitTime += p->getDuration();
            number++;
            s = s->next;
        }

        out << setw(9) << timeToString(routeEnd) << "   Trip ends (" << endTime - routeEnd << " min free)" << endl;
        out << "------------------------------------------------------------------" << endl;
        out << " Total Cost   : Rs." << totalCost() << " / Rs." << budget << endl;
        out << " Total Travel : " << routeTravel << " min" << endl;
        out << " Total Visit  : " << visitTime / 60 << " hr " << visitTime % 60 << " min" << endl;
        out << "==================================================================" << endl;
    }

    void showAllPlaces() {
        cout << endl << " ID  Name                Category  Rating  Cost      Time      Open hours" << endl;
        for (int i = 0; i < places.size(); i++)
            places[i]->print();
    }

    void showStartOptions() {
        cout << "Which place are you starting from?" << endl;
        for (int i = 0; i < places.size(); i++)
            cout << setw(4) << places[i]->getId() << ". " << places[i]->getName() << endl;
    }

    // ================= editing =================

    void removePlace() {
        if (route.empty()) {
            cout << "Plan is empty." << endl;
            return;
        }
        for (int i = 0; i < route.size(); i++)
            cout << "  " << i + 1 << ". " << route[i]->getName() << endl;

        int n = readNumber("Which stop to remove? (0 = cancel): ", 0, route.size());
        if (n == 0) return;

        Place* p = route[n - 1];
        plan.erase(find(plan.begin(), plan.end(), p));

        if (!updateRoute()) {   // rare: greedy order of the rest doesn't fit
            plan.push_back(p);
            updateRoute();
            cout << "Can't remove " << p->getName() << ", the other places don't fit without it." << endl;
            return;
        }
        inPlan.erase(p->getId());
        history.push({'R', p});

        cout << "Removed " << p->getName() << endl;
        showPlan();
    }

    void addPlace() {
        showAllPlaces();
        int id = readNumber("Enter place ID to add (0 = cancel): ", 0, 1000);
        if (id == 0) return;

        if (!hasPlace(id)) {
            cout << "No place with this ID." << endl;
            return;
        }
        Place* p = placeById[id];

        if (id == startPlace) {
            cout << "You are starting at " << p->getName() << " already." << endl;
            return;
        }
        if (inPlan.count(id)) {
            cout << p->getName() << " is already in the plan." << endl;
            return;
        }
        if (totalCost() + p->getCost() > budget) {
            cout << "Not enough budget! Only Rs." << budget - totalCost() << " left." << endl;
            return;
        }

        plan.push_back(p);
        if (!updateRoute()) {
            plan.pop_back();
            cout << "Can't fit " << p->getName() << " in your time (or it is closed then)." << endl;
            return;
        }
        inPlan.insert(id);
        history.push({'A', p});

        cout << "Added " << p->getName() << endl;
        showPlan();
    }

    // undo works because the same set of places always gives the same route
    void undo() {
        if (history.empty()) {
            cout << "Nothing to undo." << endl;
            return;
        }
        Action last = history.top();
        history.pop();
        Place* p = last.place;

        if (last.type == 'A') {
            plan.erase(find(plan.begin(), plan.end(), p));
            inPlan.erase(p->getId());
            cout << "Undo: removed " << p->getName() << endl;
        } else {
            plan.push_back(p);
            inPlan.insert(p->getId());
            cout << "Undo: added back " << p->getName() << endl;
        }
        updateRoute();
        showPlan();
    }
};

#endif
