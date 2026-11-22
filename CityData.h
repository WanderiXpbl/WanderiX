#ifndef CITYDATA_H
#define CITYDATA_H

#include "Place.h"
#include "Graph.h"
#include "Trie.h"
#include <fstream>
#include <sstream>
#include <vector>
#include <memory>
#include <unordered_map>
using namespace std;

// thrown when a data file is missing
class DataFileException : public runtime_error {
public:
    DataFileException(string file) : runtime_error("Cannot open " + file) {}
};

// Everything about the city that does not change while planning:
// the places, the road graph (with Dijkstra already run), name search and map positions.
class CityData {
private:
    vector<unique_ptr<Place>> places;                  // owns every place (freed automatically)
    unordered_map<int, Place*> placeById;              // id -> place
    Graph graph;
    Trie trie;                                         // name search
    unordered_map<int, pair<double, double>> coords;   // node -> map position (website only)

    // one line of places.txt -> a new place of the right kind, or NULL if the line is bad
    Place* makePlace(string line, string fileName) {
        stringstream ss(line);
        int id, cost, duration;
        string name, category, openText, closeText, closedText;
        double rating;
        if (!(ss >> id >> name >> category >> rating >> cost >> duration >> openText >> closeText)) {
            cout << "Bad line in " << fileName << ", skipped: " << line << endl;
            return NULL;
        }
        if (!(ss >> closedText)) closedText = "-";   // closed days are optional

        Time open, close;
        try {
            open = Time::parse(openText);
            close = Time::parse(closeText);
        } catch (InvalidTimeException& e) {
            cout << e.what() << " in " << fileName << ", skipped: " << line << endl;
            return NULL;
        }
        replace(name.begin(), name.end(), '_', ' ');   // Kalsang_Corner -> Kalsang Corner

        Place* p;
        if (category == "Food") p = new FoodPlace(id, name, rating, cost, duration, open, close);
        else if (category == "Nature") p = new NaturePlace(id, name, rating, cost, duration, open, close);
        else if (category == "Culture") p = new CulturePlace(id, name, rating, cost, duration, open, close);
        else if (category == "Shopping") p = new ShoppingPlace(id, name, rating, cost, duration, open, close);
        else {
            cout << "Unknown category " << category << ", skipped" << endl;
            return NULL;
        }
        p->setClosedDays(closedText);
        return p;
    }

public:
    // places.txt: id name category rating cost duration open close [closed days]
    // roads.txt:  from to minutes   (0 = Home)
    void load(string placesFile, string roadsFile) {
        ifstream fin(placesFile);
        if (!fin) throw DataFileException(placesFile);

        int maxId = 0;
        string line;
        while (getline(fin, line)) {   // one line at a time, so one bad line doesn't break the rest
            if (line.empty()) continue;
            Place* p = makePlace(line, placesFile);
            if (p == NULL) continue;
            places.push_back(unique_ptr<Place>(p));
            placeById[p->getId()] = p;
            trie.insert(p->getName(), p->getId());
            if (p->getId() > maxId) maxId = p->getId();
        }

        ifstream rin(roadsFile);
        if (!rin) throw DataFileException(roadsFile);
        graph.setSize(maxId + 1);
        int u, v, minutes;
        while (rin >> u >> v >> minutes)
            graph.addEdge(u, v, minutes);

        graph.buildTable();   // Dijkstra from every node, once
        cout << "Loaded " << places.size() << " places." << endl;
    }

    // coords.txt: id x y   (where each place is drawn on the website map)
    bool loadCoords(string fileName) {
        ifstream fin(fileName);
        if (!fin) return false;
        int id;
        double x, y;
        while (fin >> id >> x >> y)
            coords[id] = {x, y};
        return true;
    }

    bool hasPlace(int id) { return placeById.count(id) > 0; }
    Place* getPlace(int id) { return placeById[id]; }
    string nameOf(int id) { return id == 0 ? "Home" : placeById[id]->getName(); }
    Graph& getGraph() { return graph; }
    vector<int> search(string prefix) { return trie.startsWith(prefix); }
    bool hasCoords(int id) { return coords.count(id) > 0; }
    pair<double, double> getCoords(int id) { return coords[id]; }

    vector<Place*> getAllPlaces() {
        vector<Place*> list;
        for (int i = 0; i < places.size(); i++)
            list.push_back(places[i].get());
        return list;
    }
};

#endif
