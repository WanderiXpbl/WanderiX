// Web version of WanderiX.
// It uses the SAME Planner as the console version (main.cpp). This file only:
//   1. reads the values the web page sends (?budget=900&start=10:30 ...)
//   2. calls the Planner
//   3. answers with JSON that the page (web/app.js) shows
//
// Build:  g++ -std=c++17 server.cpp -o wanderix-server        (Linux: add -pthread, Windows: add -lws2_32)
// Run:    ./wanderix-server     then open http://localhost:8080
//
// Every visitor (browser) gets their own Planner, so many people can use the site at once.
// The city data (places, roads, Dijkstra table) is loaded once and shared by all of them.

#include "httplib.h"   // must come first: it includes system headers that Planner.h's "using namespace std" can upset
#include "Planner.h"
#include "Json.h"
#include <mutex>
#include <memory>
#include <unordered_map>
#include <cctype>
#include <cstdlib>

CityData city;          // loaded once, read by every visitor's planner

// one visitor = one browser, known by the random id the page sends as ?sid=...
struct Visitor {
    unique_ptr<Planner> planner;
    bool havePlan;          // editing makes no sense before the first plan
    long long lastUsed;     // to forget the visitor who has been away longest
};
unordered_map<string, Visitor> visitors;
const int MAX_VISITORS = 200;
long long requestCount = 0;

mutex serverLock;          // the server answers several requests at once; one at a time may touch the planners
Visitor* visitor = NULL;   // the visitor of the request being answered right now (set in handle)
Planner* planner = NULL;   // their planner

// thrown when the page sends a missing or wrong value
class BadRequest : public runtime_error {
public:
    BadRequest(string message) : runtime_error(message) {}
};

// ================= reading the request =================

int intParam(const httplib::Request& req, string name, int low, int high) {
    if (!req.has_param(name)) throw BadRequest("Missing value: " + name);
    string text = req.get_param_value(name);
    int value;
    try {
        size_t used;
        value = stoi(text, &used);
        if (used != text.size()) throw BadRequest(name + " must be a whole number");
    } catch (invalid_argument&) {
        throw BadRequest(name + " must be a whole number");
    } catch (out_of_range&) {
        throw BadRequest(name + " is too big");
    }
    if (value < low || value > high)
        throw BadRequest(name + " must be between " + to_string(low) + " and " + to_string(high));
    return value;
}

double weightParam(const httplib::Request& req, string name) {
    if (!req.has_param(name)) throw BadRequest("Missing value: " + name);
    double value;
    try {
        value = stod(req.get_param_value(name));
    } catch (exception&) {
        throw BadRequest(name + " must be a number");
    }
    if (value < 0 || value > 1) throw BadRequest(name + " must be between 0 and 1");
    return value;
}

Time timeParam(const httplib::Request& req, string name) {
    if (!req.has_param(name)) throw BadRequest("Missing value: " + name);
    return Time::parse(req.get_param_value(name));   // throws InvalidTimeException for "25:70"
}

// ================= building the answers =================

// [0,13,10]
string idListJson(vector<int> ids) {
    JsonArray a;
    for (int i = 0; i < ids.size(); i++)
        a.add(to_string(ids[i]));
    return a.str();
}

string placeJson(Place* p) {
    JsonObject o;
    o.addInt("id", p->getId())
     .addText("name", p->getName())
     .addText("category", p->getCategory())
     .addNumber("rating", p->getRating(), 1)
     .addInt("cost", p->getCost())
     .addInt("visit", p->getDuration())
     .addText("open", p->getOpenTime().toString())
     .addText("close", p->getCloseTime().toString())
     .addText("closed", p->closedDaysText());
    return o.str();
}

string scoreCardJson(Place* p, ScoreCard c) {
    JsonObject o;
    o.addInt("id", p->getId())
     .addText("name", p->getName())
     .addText("category", p->getCategory())
     .addNumber("mood", c.mood, 3)
     .addNumber("rating", c.rating, 3)
     .addNumber("cost", c.cost, 3)
     .addNumber("time", c.time, 3)
     .addNumber("near", c.near, 3)
     .addNumber("bonus", c.bonus, 3)
     .addNumber("total", c.total, 3);
    return o.str();
}

// ---- the parts of a plan answer, one small function each ----

string tripJson() {
    JsonObject trip;
    trip.addText("day", DAY_NAMES[planner->getDay()])
        .addText("start", planner->getStartTime().toString())
        .addText("end", planner->getEndTime().toString())
        .addInt("budget", planner->getBudget())
        .addText("startName", planner->getStartName())
        .addBool("returnHome", planner->getReturnHome())
        .addText("style", planner->getScorer()->getName())
        .addInt("mealGap", planner->getMealGap())
        .addBool("edited", planner->isEdited());
    return trip.str();
}

// walks the linked list of stops; each stop also gets the roads Dijkstra found to reach it
string stopsJson() {
    JsonArray stops;
    int previous = planner->getStartPlace();
    for (Stop* s = planner->getItinerary().getHead(); s != NULL; s = s->next) {
        Place* p = s->place;
        JsonObject stop;
        stop.addInt("id", p->getId())
            .addText("name", p->getName())
            .addText("category", p->getCategory())
            .addNumber("rating", p->getRating(), 1)
            .addInt("cost", p->getCost())
            .addInt("visit", p->getDuration())
            .addInt("travel", s->travel)
            .addInt("wait", s->wait)
            .addText("start", s->start.toString())
            .addText("leave", (s->start + p->getDuration()).toString())
            .add("path", idListJson(planner->getGraph().getPath(previous, p->getId())));
        stops.add(stop.str());
        previous = p->getId();
    }
    return stops.str();
}

string totalsJson() {
    int visitTime = 0;
    int last = planner->getStartPlace();
    for (Stop* s = planner->getItinerary().getHead(); s != NULL; s = s->next) {
        visitTime += s->place->getDuration();
        last = s->place->getId();
    }
    vector<int> backPath;
    if (planner->getReturnHome()) backPath = planner->getGraph().getPath(last, planner->getStartPlace());

    JsonObject totals;
    totals.addInt("cost", planner->totalCost())
          .addInt("budget", planner->getBudget())
          .addInt("travel", planner->getTravel())
          .addInt("back", planner->getBackTravel())
          .addInt("visit", visitTime)
          .addText("finish", planner->getFinish().toString())
          .addInt("free", planner->getEndTime() - planner->getFinish())
          .add("backPath", idListJson(backPath));
    return totals.str();
}

string weightsJson() {
    Scorer* sc = planner->getScorer();
    JsonObject weights;
    weights.addNumber("mood", sc->getMoodWeight(), 2)
           .addNumber("rating", sc->getRatingWeight(), 2)
           .addNumber("cost", sc->getCostWeight(), 2)
           .addNumber("time", sc->getTimeWeight(), 2)
           .addNumber("near", sc->getNearWeight(), 2)
           .addNumber("repeat", sc->getRepeatWeight(), 2);
    return weights.str();
}

string stepJson(StepNote& step) {
    JsonArray top;
    for (int k = 0; k < step.top.size(); k++) {
        JsonObject t;
        t.addText("name", step.top[k].first->getName())
         .addText("category", step.top[k].first->getCategory())
         .addNumber("score", step.top[k].second, 3);
        top.add(t.str());
    }
    JsonArray skipped;
    for (int k = 0; k < step.skipped.size(); k++) {
        JsonObject sk;
        sk.addText("name", step.skipped[k].first->getName()).addText("reason", step.skipped[k].second);
        skipped.add(sk.str());
    }
    JsonObject st;
    st.addText("name", step.chosen->getName())
      .addText("category", step.chosen->getCategory())
      .addText("at", step.at.toString())
      .addText("start", step.start.toString())
      .addNumber("score", step.score, 3)
      .addInt("considered", step.considered)
      .add("top", top.str())
      .add("skipped", skipped.str());
    return st.str();
}

// how the plan was made: filter reasons, score cards, and every step of the route loop
string explainJson() {
    JsonArray filter;
    vector<FilterNote>& notes = planner->getFilterNotes();
    for (int i = 0; i < notes.size(); i++) {
        JsonObject f;
        f.addInt("id", notes[i].place->getId())
         .addText("name", notes[i].place->getName())
         .addText("category", notes[i].place->getCategory())
         .addBool("kept", notes[i].kept)
         .addText("reason", notes[i].reason);
        filter.add(f.str());
    }

    JsonArray scores;
    vector<ScoreNote>& cards = planner->getScoreNotes();
    for (int i = 0; i < cards.size(); i++)
        scores.add(scoreCardJson(cards[i].place, cards[i].card));

    JsonArray steps;
    vector<StepNote>& list = planner->getStepNotes();
    for (int i = 0; i < list.size(); i++)
        steps.add(stepJson(list[i]));

    JsonObject explain;
    explain.add("filter", filter.str()).add("scores", scores.str()).add("steps", steps.str());
    return explain.str();
}

// real numbers showing where each data structure was used
string statsJson() {
    int compared = 0;
    vector<StepNote>& steps = planner->getStepNotes();
    for (int i = 0; i < steps.size(); i++)
        compared += steps[i].considered;
    JsonObject stats;
    stats.addInt("dijkstraRuns", planner->getGraph().size())
         .addInt("steps", steps.size())
         .addInt("compared", compared)
         .addInt("listLength", planner->getItinerary().size())
         .addInt("undoSize", planner->getUndoSize())
         .addInt("redoSize", planner->getRedoSize());
    return stats.str();
}

// the whole current plan, plus how it was made
string planJson(string message) {
    JsonObject o;
    o.addBool("ok", true)
     .addText("message", message)
     .add("trip", tripJson())
     .add("stops", stopsJson())
     .add("totals", totalsJson())
     .addBool("canUndo", planner->canUndo())
     .addBool("canRedo", planner->canRedo())
     .add("weights", weightsJson())
     .add("explain", explainJson())
     .add("stats", statsJson());
    return o.str();
}

string errorJson(string message) {
    JsonObject o;
    o.addBool("ok", false).addText("error", message);
    return o.str();
}

// ================= one function per web address =================

// /api/plan?budget=900&day=0&start=10:30&end=17:45&mood=1&style=1&startPlace=0&returnHome=1&mealGap=180
// (style=5 also needs w_mood, w_rating, w_cost, w_time, w_near, w_repeat)
string planRequest(const httplib::Request& req) {
    int budget = intParam(req, "budget", 0, 100000);
    int day = intParam(req, "day", 0, 6);
    Time start = timeParam(req, "start");
    Time end = timeParam(req, "end");
    if (end <= start) throw BadRequest("End time must be after the start time.");
    int mood = intParam(req, "mood", 1, 5);
    int style = intParam(req, "style", 1, 5);
    int startPlace = intParam(req, "startPlace", 0, 1000);
    if (startPlace != 0 && !planner->hasPlace(startPlace)) throw BadRequest("Unknown starting place.");
    bool returnHome = intParam(req, "returnHome", 0, 1) == 1;
    int mealGap = intParam(req, "mealGap", 60, 360);   // minutes

    if (style == 5)
        planner->setCustomScorer(weightParam(req, "w_mood"), weightParam(req, "w_rating"), weightParam(req, "w_cost"),
                                weightParam(req, "w_time"), weightParam(req, "w_near"), weightParam(req, "w_repeat"));
    else
        planner->setScorer(style);

    planner->setTrip(budget, start, end, mood, startPlace, day, returnHome, mealGap);
    planner->makePlan();
    visitor->havePlan = true;

    if (planner->getItinerary().size() == 0) return planJson("No places fit. Try more budget or more time.");
    return planJson("Plan ready.");
}

void needPlan() {
    if (!visitor->havePlan) throw BadRequest("Make a plan first.");
}

string addRequest(const httplib::Request& req) {
    needPlan();
    return planJson(planner->addPlace(intParam(req, "id", 1, 1000)));
}

string removeRequest(const httplib::Request& req) {
    needPlan();
    return planJson(planner->removePlace(intParam(req, "id", 1, 1000)));
}

string undoRequest(const httplib::Request&) {
    needPlan();
    return planJson(planner->undo());
}

string redoRequest(const httplib::Request&) {
    needPlan();
    return planJson(planner->redo());
}

// /api/places?sort=1   (1 = id, 2 = rating, 3 = cost, 4 = visit time)
string placesRequest(const httplib::Request& req) {
    int sortBy = req.has_param("sort") ? intParam(req, "sort", 1, 4) : 1;
    vector<Place*> list = planner->sortedPlaces(sortBy);
    JsonArray places;
    for (int i = 0; i < list.size(); i++)
        places.add(placeJson(list[i]));
    JsonObject o;
    o.addBool("ok", true).add("places", places.str());
    return o.str();
}

// /api/search?q=ka   (uses the trie)
string searchRequest(const httplib::Request& req) {
    string q = req.has_param("q") ? req.get_param_value("q") : "";
    if (q.size() > 40) throw BadRequest("Search text is too long.");
    JsonArray found;
    if (q != "") {
        vector<int> ids = planner->search(q);
        for (int i = 0; i < ids.size(); i++) {
            Place* p = planner->getPlace(ids[i]);
            JsonObject f;
            f.addInt("id", p->getId())
             .addText("name", p->getName())
             .addText("category", p->getCategory())
             .addInt("cost", p->getCost())
             .addBool("inPlan", visitor->havePlan && planner->inPlanNow(p->getId()));
            found.add(f.str());
        }
    }
    JsonObject o;
    o.addBool("ok", true).add("results", found.str());
    return o.str();
}

// ================= map (graph + Dijkstra) =================

string nodeName(int id) {
    return planner->nameOf(id);
}

int nodeParam(const httplib::Request& req, string name) {
    int id = intParam(req, name, 0, 1000);
    if (id != 0 && !planner->hasPlace(id)) throw BadRequest("Unknown place: " + to_string(id));
    return id;
}

// /api/graph   every node (with its map position) and every road
string graphRequest(const httplib::Request&) {
    JsonArray nodes;
    for (int id = 0; id < planner->getGraph().size(); id++) {
        if (id != 0 && !planner->hasPlace(id)) continue;
        if (!planner->hasCoords(id)) continue;
        pair<double, double> c = planner->getCoords(id);
        JsonObject n;
        n.addInt("id", id)
         .addText("name", nodeName(id))
         .addText("category", id == 0 ? "Home" : planner->getPlace(id)->getCategory())
         .addNumber("x", c.first, 2)
         .addNumber("y", c.second, 2);
        nodes.add(n.str());
    }
    JsonArray roads;
    vector<Road> list = planner->getGraph().getRoads();
    for (int i = 0; i < list.size(); i++) {
        JsonObject r;
        r.addInt("a", list[i].a).addInt("b", list[i].b).addInt("minutes", list[i].minutes);
        roads.add(r.str());
    }
    JsonObject o;
    o.addBool("ok", true).add("nodes", nodes.str()).add("roads", roads.str());
    return o.str();
}

// /api/path?from=0&to=10   fastest road between two nodes (from the Dijkstra table)
string pathRequest(const httplib::Request& req) {
    int from = nodeParam(req, "from");
    int to = nodeParam(req, "to");
    vector<int> path = planner->getGraph().getPath(from, to);
    JsonArray names;
    for (int i = 0; i < path.size(); i++)
        names.add(jsonText(nodeName(path[i])));
    JsonObject o;
    o.addBool("ok", true)
     .addBool("reachable", !path.empty())
     .addInt("minutes", path.empty() ? -1 : planner->getGraph().getTime(from, to))
     .add("path", idListJson(path))
     .add("names", names.str());
    return o.str();
}

// /api/distances?from=0   fastest time from one node to every other node
string distancesRequest(const httplib::Request& req) {
    int from = nodeParam(req, "from");
    JsonArray list;
    for (int id = 0; id < planner->getGraph().size(); id++) {
        if (id != 0 && !planner->hasPlace(id)) continue;
        int t = planner->getGraph().getTime(from, id);
        JsonObject d;
        d.addInt("id", id).addInt("minutes", t == INF ? -1 : t);
        list.add(d.str());
    }
    JsonObject o;
    o.addBool("ok", true).addInt("from", from).add("distances", list.str());
    return o.str();
}

// the visitor who sent this request; a new visitor gets a new planner
void findVisitor(const httplib::Request& req) {
    string id = req.has_param("sid") ? req.get_param_value("sid") : "";
    bool ok = id.size() >= 8 && id.size() <= 64;
    for (int i = 0; i < id.size(); i++)
        if (!isalnum((unsigned char)id[i]) && id[i] != '-') ok = false;
    if (!ok) throw BadRequest("Missing visitor id. Please reload the page.");

    if (visitors.count(id) == 0) {
        if (visitors.size() >= MAX_VISITORS) {   // full: forget the visitor who has been away longest
            auto oldest = visitors.begin();
            for (auto it = visitors.begin(); it != visitors.end(); it++)
                if (it->second.lastUsed < oldest->second.lastUsed) oldest = it;
            visitors.erase(oldest);
        }
        visitors[id] = Visitor{make_unique<Planner>(&city), false, 0};
    }
    visitor = &visitors[id];
    visitor->lastUsed = ++requestCount;
    planner = visitor->planner.get();
}

// runs one request: finds the visitor, turns bad input into a clear message
void handle(const httplib::Request& req, httplib::Response& res, string (*work)(const httplib::Request&)) {
    lock_guard<mutex> guard(serverLock);
    try {
        findVisitor(req);
        res.set_content(work(req), "application/json");
    } catch (BadRequest& e) {
        res.status = 400;
        res.set_content(errorJson(e.what()), "application/json");
    } catch (InvalidTimeException& e) {
        res.status = 400;
        res.set_content(errorJson(string(e.what()) + ". Use HH:MM, for example 10:30."), "application/json");
    } catch (exception& e) {
        res.status = 500;
        res.set_content(errorJson(string("Server error: ") + e.what()), "application/json");
    }
}

int main() {
    try {
        city.load("data/places.txt", "data/roads.txt");
    } catch (DataFileException& e) {
        cout << e.what() << ". Run the server from the WanderiX folder." << endl;
        return 1;
    }

    if (!city.loadCoords("data/coords.txt"))
        cout << "data/coords.txt not found: the map tab will be empty." << endl;

    httplib::Server server;
    if (!server.set_mount_point("/", "./web")) {
        cout << "Cannot find the web folder. Run the server from the WanderiX folder." << endl;
        return 1;
    }

    server.Get("/api/plan", [](const httplib::Request& req, httplib::Response& res) { handle(req, res, planRequest); });
    server.Get("/api/add", [](const httplib::Request& req, httplib::Response& res) { handle(req, res, addRequest); });
    server.Get("/api/remove", [](const httplib::Request& req, httplib::Response& res) { handle(req, res, removeRequest); });
    server.Get("/api/undo", [](const httplib::Request& req, httplib::Response& res) { handle(req, res, undoRequest); });
    server.Get("/api/redo", [](const httplib::Request& req, httplib::Response& res) { handle(req, res, redoRequest); });
    server.Get("/api/places", [](const httplib::Request& req, httplib::Response& res) { handle(req, res, placesRequest); });
    server.Get("/api/search", [](const httplib::Request& req, httplib::Response& res) { handle(req, res, searchRequest); });
    server.Get("/api/graph", [](const httplib::Request& req, httplib::Response& res) { handle(req, res, graphRequest); });
    server.Get("/api/path", [](const httplib::Request& req, httplib::Response& res) { handle(req, res, pathRequest); });
    server.Get("/api/distances", [](const httplib::Request& req, httplib::Response& res) { handle(req, res, distancesRequest); });

    // on a hosting service (e.g. Render) the PORT is given to us and we must accept outside visitors;
    // on your laptop we use port 8080 and only accept this computer
    const char* hostPort = getenv("PORT");
    int port = hostPort ? atoi(hostPort) : 8080;
    string host = hostPort ? "0.0.0.0" : "localhost";

    cout << "WanderiX is running. Open http://localhost:" << port << " in your browser." << endl;
    cout << "Press Ctrl+C to stop." << endl;
    if (!server.listen(host, port)) {
        cout << "Could not start on port " << port << " (is it already in use?)." << endl;
        return 1;
    }
    return 0;
}
