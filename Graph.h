#ifndef GRAPH_H
#define GRAPH_H

#include "Utils.h"
#include <vector>
#include <queue>
using namespace std;

// one two-way road (used to draw the map on the website)
struct Road {
    int a, b, minutes;
};

// Road network. Node 0 = Home, node i = place with id i.
// Answers: "What is the fastest way from A to B?"
class Graph {
private:
    int n;
    vector<vector<pair<int, int>>> adj;   // adj[u] = list of {v, minutes}
    vector<vector<int>> dist;             // dist[a][b] = shortest time from a to b
    vector<vector<int>> parent;           // parent[a][b] = node just before b on that fastest road

    // normal dijkstra from one node
    void dijkstra(int src) {
        dist[src].assign(n, INF);
        parent[src].assign(n, -1);
        priority_queue<pair<int, int>, vector<pair<int, int>>, greater<pair<int, int>>> pq;   // min heap

        dist[src][src] = 0;
        pq.push({0, src});

        while (!pq.empty()) {
            int d = pq.top().first;
            int u = pq.top().second;
            pq.pop();

            if (d > dist[src][u]) continue;   // old entry

            for (int i = 0; i < adj[u].size(); i++) {
                int v = adj[u][i].first;
                int w = adj[u][i].second;
                if (dist[src][u] + w < dist[src][v]) {
                    dist[src][v] = dist[src][u] + w;
                    parent[src][v] = u;          // remember how we got to v
                    pq.push({dist[src][v], v});
                }
            }
        }
    }

public:
    Graph() {
        n = 0;
    }

    void setSize(int nodes) {
        n = nodes;
        adj.resize(n);
    }

    int size() {
        return n;
    }

    void addEdge(int u, int v, int minutes) {
        if (u < 0 || v < 0 || u >= n || v >= n) {
            cout << "Bad road " << u << " - " << v << ", skipped" << endl;
            return;
        }
        adj[u].push_back({v, minutes});
        adj[v].push_back({u, minutes});   // two way road
    }

    // run dijkstra from every node once, after all roads are added
    void buildTable() {
        dist.resize(n);
        parent.resize(n);
        for (int i = 0; i < n; i++)
            dijkstra(i);
    }

    int getTime(int a, int b) {
        return dist[a][b];
    }

    // nodes on the fastest road from a to b, e.g. {0, 13, 10}. Empty if there is no road.
    vector<int> getPath(int a, int b) {
        vector<int> path;
        if (dist[a][b] == INF) return path;
        for (int v = b; v != -1; v = parent[a][v])   // walk back using parent, then flip
            path.push_back(v);
        reverse(path.begin(), path.end());
        return path;
    }

    // every road once (a < b)
    vector<Road> getRoads() {
        vector<Road> roads;
        for (int u = 0; u < n; u++)
            for (int i = 0; i < adj[u].size(); i++)
                if (u < adj[u][i].first)
                    roads.push_back({u, adj[u][i].first, adj[u][i].second});
        return roads;
    }
};

#endif
