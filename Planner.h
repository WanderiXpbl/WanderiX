#ifndef PLANNER_H
#define PLANNER_H

#include "CityData.h"
#include "Route.h"
#include "Scorer.h"
#include "Itinerary.h"
#include <vector>
#include <stack>
#include <unordered_set>
using namespace std;

const int MAX_STOPS = 7;   // places picked automatically
const int MAX_TOTAL = 8;   // places the user can have in total

// saved change, used for undo / redo
struct Action {
    char type;   // 'A' = added, 'R' = removed
    Place* place;
};

// ---- the explanation of how a plan was made (shown on the website) ----

struct FilterNote {        // step 1: kept or dropped, and why
    Place* place;
    bool kept;
    string reason;
};

struct ScoreNote {         // step 2: the score card of each kept place
    Place* place;
    ScoreCard card;
};

// Makes a plan for one trip and lets the user edit it.
class Planner {
private:
    CityData* city;                        // places, roads (Dijkstra done), search: shared by every planner
    RouteBuilder builder;                  // picks AND orders the places, one step at a time
    Scorer scorer;                         // how places are scored

    // user answers
    int budget;
    Time startTime;
    Time endTime;
    int mood;
    int startPlace;                        // 0 = Home, otherwise a place id
    int day;                               // 0 = Monday ... 6 = Sunday
    bool returnHome;                       // must we travel back to the start?
    int mealGap;                           // eat at least every mealGap minutes

    vector<Place*> plan;                   // chosen places
    unordered_set<int> inPlan;             // ids of chosen places (fast check)
    vector<Place*> route;                  // chosen places in visiting order
    int routeTravel;                       // includes the trip back
    int backTravel;                        // just the trip back
    Time routeEnd;
    bool edited;                           // changed by the user after planning

    Itinerary itinerary;
    stack<Action> undoStack;
    stack<Action> redoStack;

    // explanation of the last plan
    vector<FilterNote> filterNotes;
    vector<ScoreNote> scoreNotes;
    vector<StepNote> stepNotes;

    int returnNode() { return returnHome ? startPlace : -1; }

    TripSettings settings() {
        return {startPlace, returnNode(), startTime, endTime, mood, budget, mealGap};
    }

public:
    Planner(CityData* city) : city(city), builder(&city->getGraph()), scorer(Scorer::preset(1)) {
        budget = 0;
        mood = RELAXED;
        startPlace = 0;
        day = 0;
        returnHome = false;
        mealGap = 180;
        routeTravel = 0;
        backTravel = 0;
        edited = false;
    }

    // ================= setting up =================

    void setTrip(int budget, Time startTime, Time endTime, int mood, int startPlace, int day, bool returnHome,
                 int mealGap) {
        this->budget = budget;
        this->startTime = startTime;
        this->endTime = endTime;
        this->mood = mood;
        this->startPlace = startPlace;
        this->day = day;
        this->returnHome = returnHome;
        this->mealGap = mealGap;
    }

    void setScorer(int style) { scorer = Scorer::preset(style); }

    void setCustomScorer(double wMood, double wRating, double wCost, double wTime, double wNear, double wRepeat) {
        scorer = Scorer("Custom", wMood, wRating, wCost, wTime, wNear, wRepeat, 0);
    }

    // ================= making a plan =================

    int totalCost() {
        int sum = 0;
        for (int i = 0; i < plan.size(); i++)
            sum += plan[i]->getCost();
        return sum;
    }

    // step 1: is this place worth considering? reason is empty if yes
    string whyNot(Place* p) {
        string moods[6] = {"", "Relaxed", "Foodie", "Adventurous", "Cultural", "Shopping"};
        if (p->getId() == startPlace) return "you start here";
        if (p->isClosedOn(day)) return "closed on " + DAY_NAMES[day];
        if (p->getCost() > budget) return "costs Rs." + to_string(p->getCost()) + ", more than the budget";
        if (p->moodMatch(mood) < 0.3) return "doesn't suit a " + moods[mood] + " mood";
        if (city->getGraph().getTime(startPlace, p->getId()) == INF) return "no road to it";
        return "";
    }

    // filter, then let the route builder pick and order the places
    void makePlan() {
        plan.clear();
        inPlan.clear();
        edited = false;
        while (!undoStack.empty()) undoStack.pop();
        while (!redoStack.empty()) redoStack.pop();
        filterNotes.clear();
        scoreNotes.clear();

        // step 1: which places are worth considering at all
        vector<Place*> candidates;
        vector<Place*> all = city->getAllPlaces();
        for (int i = 0; i < all.size(); i++) {
            Place* p = all[i];
            string reason = whyNot(p);
            filterNotes.push_back({p, reason == "", reason});
            if (reason != "") continue;
            candidates.push_back(p);
            scoreNotes.push_back({p, scorer.score(p, mood, budget, city->getGraph().getTime(startPlace, p->getId()), 0)});
        }

        // step 2: one loop picks the next best place again and again
        vector<Place*> order;
        int travel;
        Time finish;
        builder.build(candidates, false, MAX_STOPS, settings(), &scorer, order, travel, finish, &stepNotes);

        // a chosen place shows the score it had at the moment it was chosen
        for (int k = 0; k < stepNotes.size(); k++)
            for (int i = 0; i < scoreNotes.size(); i++)
                if (scoreNotes[i].place == stepNotes[k].chosen) scoreNotes[i].card = stepNotes[k].card;

        for (int i = 0; i < order.size(); i++)
            addToPlan(order[i]);
        route = order;
        routeTravel = travel;
        routeEnd = finish;
        buildItinerary();
    }

    // puts the places in 'plan' in order with the same step-by-step loop. Returns false if they don't all fit.
    bool updateRoute() {
        vector<Place*> order;
        int travel;
        Time finish;
        if (!builder.build(plan, true, 1000, settings(), &scorer, order, travel, finish, NULL))
            return false;

        route = order;
        routeTravel = travel;
        routeEnd = finish;
        buildItinerary();
        return true;
    }

    // puts the route into the linked list with the timings
    void buildItinerary() {
        itinerary.clear();
        Time time = startTime;
        int current = startPlace;

        for (int i = 0; i < route.size(); i++) {
            Place* p = route[i];
            int travel = city->getGraph().getTime(current, p->getId());
            time = time + travel;

            int wait = 0;
            if (time < p->getOpenTime()) {
                wait = p->getOpenTime() - time;
                time = p->getOpenTime();
            }
            itinerary.addStop(p, travel, wait, time);
            time = time + p->getDuration();
            current = p->getId();
        }
        backTravel = returnHome ? city->getGraph().getTime(current, startPlace) : 0;
    }

    // ================= editing (each returns a message) =================

    void removeFromPlan(Place* p) {
        plan.erase(find(plan.begin(), plan.end(), p));
        inPlan.erase(p->getId());
    }

    void addToPlan(Place* p) {
        plan.push_back(p);
        inPlan.insert(p->getId());
    }

    string addPlace(int id) {
        if (!city->hasPlace(id)) return "No place with this ID.";
        Place* p = city->getPlace(id);
        if (plan.size() >= MAX_TOTAL) return "Max " + to_string(MAX_TOTAL) + " places allowed, remove one first.";
        if (id == startPlace) return "You are starting at " + p->getName() + " already.";
        if (inPlan.count(id)) return p->getName() + " is already in the plan.";
        if (p->isClosedOn(day)) return p->getName() + " is closed on " + DAY_NAMES[day] + ".";
        if (totalCost() + p->getCost() > budget)
            return "Not enough budget! Only Rs." + to_string(budget - totalCost()) + " left.";

        addToPlan(p);
        if (!updateRoute()) {
            removeFromPlan(p);
            return "Can't fit " + p->getName() + ": no order fits your time, opening hours and the food rules.";
        }
        undoStack.push({'A', p});
        while (!redoStack.empty()) redoStack.pop();   // a new change clears redo
        edited = true;
        return "Added " + p->getName() + ".";
    }

    string removePlace(int id) {
        if (!city->hasPlace(id) || inPlan.count(id) == 0) return "That place is not in the plan.";
        Place* p = city->getPlace(id);
        removeFromPlan(p);
        if (!updateRoute()) {   // e.g. removing a meal leaves the other stops without food in time
            addToPlan(p);
            updateRoute();
            return "Can't remove " + p->getName() + ": the other places would break a rule without it.";
        }
        undoStack.push({'R', p});
        while (!redoStack.empty()) redoStack.pop();
        edited = true;
        return "Removed " + p->getName() + ".";
    }

    // the same set of places always gives the same route, so undo/redo restore plans exactly
    string undo() {
        if (undoStack.empty()) return "Nothing to undo.";
        Action last = undoStack.top();
        undoStack.pop();
        if (last.type == 'A') removeFromPlan(last.place);
        else addToPlan(last.place);
        updateRoute();
        redoStack.push(last);
        return string("Undo: ") + (last.type == 'A' ? "removed " : "added back ") + last.place->getName() + ".";
    }

    string redo() {
        if (redoStack.empty()) return "Nothing to redo.";
        Action next = redoStack.top();
        redoStack.pop();
        if (next.type == 'A') addToPlan(next.place);
        else removeFromPlan(next.place);
        updateRoute();
        undoStack.push(next);
        return string("Redo: ") + (next.type == 'A' ? "added " : "removed ") + next.place->getName() + ".";
    }

    // ================= getters (used by main.cpp and server.cpp) =================

    bool hasPlace(int id) { return city->hasPlace(id); }
    Place* getPlace(int id) { return city->getPlace(id); }
    vector<Place*> getAllPlaces() { return city->getAllPlaces(); }
    vector<int> search(string prefix) { return city->search(prefix); }
    Graph& getGraph() { return city->getGraph(); }
    bool hasCoords(int id) { return city->hasCoords(id); }
    pair<double, double> getCoords(int id) { return city->getCoords(id); }
    string nameOf(int id) { return city->nameOf(id); }

    Itinerary& getItinerary() { return itinerary; }
    vector<Place*> getRoute() { return route; }
    bool inPlanNow(int id) { return inPlan.count(id) > 0; }
    int getTravel() { return routeTravel; }
    int getBackTravel() { return backTravel; }
    Time getFinish() { return routeEnd; }
    Time getStartTime() { return startTime; }
    Time getEndTime() { return endTime; }
    int getBudget() { return budget; }
    int getDay() { return day; }
    bool getReturnHome() { return returnHome; }
    int getStartPlace() { return startPlace; }
    string getStartName() { return city->nameOf(startPlace); }
    Scorer* getScorer() { return &scorer; }
    int getMealGap() { return mealGap; }
    bool isEdited() { return edited; }
    bool canUndo() { return !undoStack.empty(); }
    bool canRedo() { return !redoStack.empty(); }
    int getUndoSize() { return undoStack.size(); }
    int getRedoSize() { return redoStack.size(); }
    vector<FilterNote>& getFilterNotes() { return filterNotes; }
    vector<ScoreNote>& getScoreNotes() { return scoreNotes; }
    vector<StepNote>& getStepNotes() { return stepNotes; }

    // ================= printing and sorting =================

    // 'out' can be cout (screen) or an ofstream (file), both work the same way
    void printPlan(ostream& out) {
        string moods[6] = {"", "Relaxed", "Foodie", "Adventurous", "Cultural", "Shopping"};
        out << endl << "==================================================================" << endl;
        out << " YOUR ITINERARY (" << DAY_NAMES[day] << ", " << startTime << " - " << endTime
            << ")   Mood: " << moods[mood] << endl;
        out << " Scoring: " << scorer.getName() << "   Food: at least every " << mealGap / 60 << " h";
        if (mealGap % 60) out << " " << mealGap % 60 << " min";
        out << endl;
        out << "==================================================================" << endl;

        if (itinerary.size() == 0) {
            out << " No places found. Try more budget or more time." << endl;
            return;
        }

        out << setw(9) << startTime << "   Start - " << getStartName() << endl;

        int visitTime = 0, number = 1;
        for (Stop* s = itinerary.getHead(); s != NULL; s = s->next) {
            Place* p = s->place;
            out << setw(9) << s->start << "   " << number << ". " << p->getName()
                << " (" << p->getCategory() << ") " << fixed << setprecision(1) << p->getRating() << "/5"
                << " - Rs." << p->getCost() << " - " << s->travel << " min travel - "
                << p->getDuration() << " min visit";
            if (s->wait > 0) out << " (waited " << s->wait << " min)";
            out << endl;
            visitTime += p->getDuration();
            number++;
        }

        if (returnHome)
            out << setw(9) << routeEnd << "   Back at " << getStartName() << " (" << backTravel << " min travel)" << endl;
        else
            out << setw(9) << routeEnd << "   Trip ends" << endl;
        out << "            " << endTime - routeEnd << " min free before " << endTime << endl;
        out << "------------------------------------------------------------------" << endl;
        out << " Total Cost   : Rs." << totalCost() << " / Rs." << budget << endl;
        out << " Total Travel : " << routeTravel << " min" << endl;
        out << " Total Visit  : " << visitTime / 60 << " hr " << visitTime % 60 << " min" << endl;
        out << "==================================================================" << endl;
    }

    bool savePlan(string fileName) {
        ofstream fout(fileName);
        if (!fout) return false;
        printPlan(fout);
        return true;
    }

    static bool byRating(Place* a, Place* b) { return a->getRating() > b->getRating(); }
    static bool byCost(Place* a, Place* b) { return a->getCost() < b->getCost(); }
    static bool byDuration(Place* a, Place* b) { return a->getDuration() < b->getDuration(); }

    // 1 = by id, 2 = best rating first, 3 = cheapest first, 4 = shortest visit first
    vector<Place*> sortedPlaces(int sortBy) {
        vector<Place*> list = getAllPlaces();
        if (sortBy == 2) sort(list.begin(), list.end(), byRating);
        else if (sortBy == 3) sort(list.begin(), list.end(), byCost);
        else if (sortBy == 4) sort(list.begin(), list.end(), byDuration);
        return list;
    }
};

#endif
