#ifndef GRAPH_H
#define GRAPH_H

#include "Utils.h"
#include <vector>
#include <queue>
using namespace std;

// Road network. Node 0 = Home, node i = place with id i.
// Answers: "What is the fastest way from A to B?"
class Graph {
private:
    int n;
    vector<vector<pair<int, int>>> adj;   // adj[u] = list of {v, minutes}
    vector<vector<int>> dist;             // dist[a][b] = shortest time from a to b

    // normal dijkstra from one node
    void dijkstra(int src) {
        dist[src].assign(n, INF);
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
        for (int i = 0; i < n; i++)
            dijkstra(i);
    }

    int getTime(int a, int b) {
        return dist[a][b];
    }
};

#endif
