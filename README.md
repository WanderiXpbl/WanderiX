# WanderiX – Itinerary Planner (C++)

Plans a day out in Dehradun: asks your budget, start and end time (with minutes), mood, scoring
style, starting point and how often you want food, then builds the trip one stop at a time so every
visit fits opening hours and your end time, two food places never come one after the other, and you
always get food stop in time.

## Run
**Website** (same planner, in the browser):
```
g++ -std=c++17 server.cpp -o wanderix-server
./wanderix-server
```
Then open http://localhost:8080. On Linux add `-pthread`; on Windows (MinGW) add `-lws2_32`.

**Console:**
```
g++ -std=c++17 main.cpp -o wanderix
./wanderix
```
Run both from this folder so they can find `data/` and `web/`.

## Put it online (Render, free)
The `Dockerfile` compiles `server.cpp` on Linux and runs it. On render.com: **New → Web Service** → connect
the GitHub repository → Render finds the Dockerfile → choose the **Free** instance → **Create Web Service**.
The server reads the `PORT` Render gives it and accepts outside visitors; on a laptop it stays on
`localhost:8080`. Free services sleep when unused, so the first visit after a while takes about 30 seconds.

## Website
`server.cpp` uses [cpp-httplib](https://github.com/yhirose/cpp-httplib) v0.18.1 (`httplib.h`, MIT licence)
and the same `Planner` as the console. It only reads the request, calls the planner and answers with JSON
built by `Json.h`. The page is plain HTML/CSS/JS in `web/`.

| Address | What it does |
|---|---|
| `/api/plan?budget=900&day=0&start=10:30&end=17:45&mood=1&style=1&startPlace=0&returnHome=1&mealGap=180` | make a plan; `mealGap` = food at least every N minutes (60–360). `style=5` also takes `w_mood`, `w_rating`, `w_cost`, `w_time`, `w_near`, `w_repeat` |
| `/api/add?id=3`, `/api/remove?id=3` | edit the plan |
| `/api/undo`, `/api/redo` | two stacks |
| `/api/places?sort=2` | all places, sorted (1 id, 2 rating, 3 cost, 4 visit) |
| `/api/search?q=ka` | trie prefix search |
| `/api/graph` | every place (with its map position) and every road |
| `/api/path?from=0&to=10` | fastest road between two places, from the Dijkstra table |
| `/api/distances?from=0` | fastest time from one place to all others (one row of the table) |

Bad values get a clear error (HTTP 400), for example `Invalid time '25:70'`. Every browser sends a random
visitor id (`sid`), and the server keeps a separate `Planner` for each visitor (up to 200; the one away longest
is forgotten), so many people can plan at once. The city data is loaded once and shared by all of them.

Page tabs: **Your plan** (timeline, "Why this place?" score card, remove, add with search, undo/redo,
download, and an "Inside the C++" panel with the real numbers for the plan: Dijkstra runs, loop steps,
places compared in the max-heap, rule skips, linked-list length, undo/redo stack sizes) ·
**Map** (the road graph; your route in teal; click one place to see its Dijkstra distances, a second to
see the fastest road) · **How it was planned** (filter reasons, score cards, and every step of the loop:
the heap, the chosen stop and which places broke a rule) · **All places** (sortable).

## Files
| File | What's inside |
|---|---|
| `TimeOfDay.h` | `Time` class: "10:30" parsing, `+`, `-`, `<`, `<<` operators, `InvalidTimeException` |
| `Utils.h` | moods, `readNumber()`, `readTime()` |
| `Place.h` | abstract `Place` + `FoodPlace`, `NaturePlace`, `CulturePlace`, `ShoppingPlace` |
| `Graph.h` | roads + Dijkstra → fastest time (and the road itself, via a `parent` table) between every pair of places |
| `Scorer.h` | one `Scorer` class; the styles (Balanced, Budget saver, Top rated, Explorer, Custom) are just different weights |
| `Route.h` | `RouteBuilder`: the one loop that picks and orders the places, with the rules |
| `Itinerary.h` | final plan as a linked list |
| `Trie.h` | prefix search over place names |
| `CityData.h` | loads and holds the city: places, road graph (Dijkstra run once), name search, map positions |
| `Planner.h` | one trip: filter, call the route builder, edit with undo/redo, print/save |
| `main.cpp` | console version: questions and menu |
| `server.cpp` | web version: one `Planner` per visitor; reads requests, calls it, answers with JSON |
| `Json.h` | small `JsonObject` / `JsonArray` classes to build JSON text |
| `httplib.h` | cpp-httplib web server library (single file, MIT) |
| `web/` | `index.html`, `style.css`, `app.js`: the page |
| `Dockerfile`, `.dockerignore` | how a hosting service builds and runs the server |
| `data/places.txt` | `id name category rating cost duration(min) open(HH:MM) close(HH:MM)` |
| `data/roads.txt` | `from to minutes` (0 = Home) |
| `data/coords.txt` | `id x y`: where each place is drawn on the map (simplified: real directions kept, city centre spread out) |

## How a plan is made
1. **Start-up:** load places (skipping bad lines), build the road graph, run Dijkstra from every node.
2. **Filter:** drop places over budget, closed that day, unreachable, the start point, or that don't suit
   the mood at all (match below 0.3).
3. **Build the trip, one stop at a time** (`RouteBuilder` in `Route.h`, the only algorithm):
   ```
   from the start point at the start time, repeat (at most 7 stops):
       for every place not used yet:
           skip it if it breaks a rule (below)
           else push {score, place} into a max-heap
       heap empty -> stop.  Otherwise pop the best: it is the next stop. Move there.
   ```
   **Rules:** within budget · open when you arrive · finish before it closes · finish (and get back, if
   returning) before your end time · **no Food right after Food** · **a food stop must start within N hours**
   of the trip start and of the end of the last meal (a non-food stop may not run past that deadline) ·
   **look-ahead (new plans only):** after a non-food stop, some food place must still be reachable in time,
   so the trip never gets stuck somewhere far from food (e.g. Daat Kali Temple) and has to end early.
4. **Score** (from where you are now, so nearby places win ties):
   `wMood·mood + wRating·(rating−3)/2 + wCost·(1 − cost/budget) + wTime·(1 − visit/240) + wNear·(1 − travel/60) − wRepeat·(same category picked)`

   | Style | mood | rating | cost | time | near | repeat | extra |
   |---|---|---|---|---|---|---|---|
   | Balanced | 0.35 | 0.20 | 0.15 | 0.10 | 0.20 | 0.10 | |
   | Budget saver | 0.25 | 0.15 | 0.30 | 0.10 | 0.20 | 0.10 | |
   | Top rated | 0.25 | 0.40 | 0.05 | 0.10 | 0.20 | 0.10 | |
   | Explorer | 0.30 | 0.15 | 0.15 | 0.10 | 0.20 | 0.20 | +0.10 for a category not picked yet |
5. **Edit:** add / remove / undo / redo (two stacks) re-run the same loop on the chosen places; a change
   that would break a rule is refused with a reason. The same places always give the same order.
6. **Also:** search by name (trie), browse places sorted, save the plan to a file.

## Data structures
Graph (adjacency list) · min-heap (Dijkstra) · 2D travel-time table and parent table · max-heap (the route loop) ·
hash map · hash set · singly linked list (itinerary) · two stacks (undo/redo) · trie (search) ·
sorting with comparators

## OOP
Abstract class and inheritance (`Place` and its four categories) · runtime polymorphism (virtual `moodMatch()`) ·
static factory method (`Scorer::preset`) · operator overloading and a friend function (`Time`) · custom exceptions ·
smart pointers (`unique_ptr`) · composition · file streams

## Results (300 random trips, fixed seed, checked by a separate program)
| | |
|---|---|
| Plans breaking any rule (budget, hours, closed day, end time, Food after Food, late meal) | 0 / 300 |
| Average stops per plan | 5.1 |
| Average travel per plan | 75 min (Home is in Clement Town, about 25–30 min from the city centre) |
| Time to build a plan (47 places) | 0.07 ms (max 0.7 ms) |

"Nearby" goes below zero for places more than an hour away, so the planner does not drive to
Mussoorie unless the day is long enough to make it worth it.

## Data (Dehradun and Mussoorie, 47 places)
- **City:** Clock Tower, Paltan Bazaar, Gandhi Park, Tibetan Market, Astley Hall, Rajpur Road market, FRI,
  Wadia Institute Museum, Tapkeshwar, Sai Darbar, Tapovan, Santala Devi, Daat Kali, Mindrolling, Robber's Cave,
  Malsi Deer Park, Sahastradhara (+ ropeway), Shikhar Falls, Maldevta, Lacchiwala, Pacific Mall, Centrio Mall,
  Mall of Dehradun, and food places: Kalsang, Ellora's, Kumar Sweet Shop, Black Pepper, Town Table, Gaylord,
  Y Café, Café Marigold, Eddie's Patisserie, Rising Dough, Café De Piccolo, Sunrise Bakers.
- **Mussoorie:** Sunset Point, Mall Road, Gun Hill, Camel's Back Road, Lal Tibba, Company Garden, Kempty Falls,
  and food places: Lovely Omelette Centre, Chick Chocolate, Char Dukan, Café Ivy.
- **Estimates to check:** all ratings (3.6–4.7), costs, visit times and opening hours are estimates from travel
  guides and listings, not from the places themselves. Cost = typical spend per person: entry fee for sights
  (FRI Rs.50, Robber's Cave Rs.35, Malsi Rs.30, most temples free), about half the "cost for two" for cafes and
  restaurants (Rs.100–600), and a typical shopping spend for markets and malls.
- **Closed days (estimates):** Malsi Deer Park on Monday; FRI and Paltan Bazaar on Sunday; Wadia Institute Museum
  on Saturday and Sunday. Sources disagree on these, and all other places are treated as open every day.
- **Roads** (`roads.txt`): built from each place's approximate real position. Every place joins its 3 nearest
  places on the same side (city or Mussoorie); minutes = distance × 1.35 (winding roads) at 22 km/h in the city
  and 15 km/h in Mussoorie; the two hill roads (from Malsi and from Rajpur Road) take 65–72 min.
  These are estimates, not Google Maps times.
- **Positions** (`coords.txt`): only for drawing. A "fish-eye" drawing keeps each place's real direction from
  the Clock Tower but enlarges the busy centre; Mussoorie is drawn around Mall Road, north of the city.

## Next
- Check the estimated data against Google Maps (travel times) and the places themselves (hours, prices).
