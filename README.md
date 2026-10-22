<img width="2720" height="1280" alt="wanderix_system_architecture" src="https://github.com/user-attachments/assets/3c447f01-af81-478f-88e4-dd1ab0109cc0" />
# WanderiX – Hangout Itinerary Planner (C++)

Asks budget, time, mood and starting point, picks good places, and puts them
in a visiting order that fits opening hours and your end time.

## Run
```
g++ -std=c++17 main.cpp -o wanderix
./wanderix
```
(run from this folder so it can find `data/`)

## Files
| File | What's inside |
|---|---|
| `Utils.h` | `INF`, mood constants, `timeToString()`, `readNumber()` |
| `Place.h` | `Place` (abstract) + `FoodPlace`, `NaturePlace`, `CulturePlace`, `ShoppingPlace` |
| `Graph.h` | roads + Dijkstra → **fastest way from A to B** (`getTime`) |
| `Route.h` | `findRoute()` greedy function → **in what order to visit the places** |
| `Itinerary.h` | final plan as a linked list |
| `Planner.h` | load data, score, pick places, add/remove/undo, print |
| `main.cpp` | questions and menu |
| `data/places.txt` | `id name category rating cost duration(min) openHour closeHour` |
| `data/roads.txt` | `from to minutes` (0 = Home) |

## Steps
1. **Shortest times (Graph)**: at start-up Dijkstra runs from every node and fill a table
   `dist[a][b]` = fastest travel time from a to b.
2. **Filter**: remove places that cost more than the budget, don't match the mood, or are the start point.
3. **Score** (0 to 1):
   `0.5 × moodMatch + 0.35 × (rating − 3)/2 + 0.15 × (1 − cost/budget) − 0.1 × (same category already picked)`
4. **Pick (max-heap)**: take the best score first. If its score dropped because its category was
   already picked, push it back with the new score. Keep a place only if budget allows and
   `findRoute()` can still fit all chosen places in the day.
5. **Order (Route, greedy)**: from where you are, go to the place where you can *start the visit
   soonest* (travel + waiting for opening). Skip places that would be closed or go past the end time.
   Ties → smaller id, so the same places always give the same route (this makes undo safe).
6. **Edit**: remove, add, undo (stack).
7. **Save**: `printPlan(ostream& out)` prints to the screen (`cout`) or to a file (`ofstream`), so saving reuses the same code.

## Data structures
- Graph (adjacency list): roads
- Min-heap: Dijkstra algorithm
- 2D table `dist`: shortest times between all places
- Max-heap: picking best places
- Hash map (`unordered_map`): find place by id, count per category
- Hash set (`unordered_set`): check if a place is already in the plan
- Linked list: itinerary
- Stack: undo

## OOP
Abstract class, inheritance (4 child classes), polymorphism (virtual `moodMatch()` and `getCategory()`),
encapsulation, composition (`Planner` has a `Graph` and an `Itinerary`), constructor/destructor.

## Limits / Phase 3 plan
- Greedy ordering is fast (about k² steps) but **not always the shortest route**, and it can miss a valid
  order that exists. Phase 3: replace the `findRoute()` function in `Route.h` with a method that checks all orders
  (or Held-Karp DP), and add a greedy vs optimal comparison over many random trips.
- Trip ends at the last place (going back home is not counted).
- Places use real Dehradun names, but ratings, costs, timings and travel times (`roads.txt`) are
  approximate and still being verified.
