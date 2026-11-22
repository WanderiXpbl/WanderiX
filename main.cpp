// Console version of WanderiX. The website (server.cpp) uses the same Planner.
#include "Planner.h"

void chooseScoring(Planner& planner) {
    cout << "Scoring style:  1. Balanced  2. Budget saver  3. Top rated  4. Explorer" << endl;
    planner.setScorer(readNumber("Choose (1-4): ", 1, 4));
}

void printPlaces(vector<Place*> list) {
    cout << endl << " ID  Name                    Category  Rating  Cost      Time      Open hours" << endl;
    for (int i = 0; i < list.size(); i++)
        list[i]->print();
}

void askQuestions(Planner& planner) {
    cout << endl << "========== PLAN YOUR DAY ==========" << endl;
    int budget = readNumber("Budget (Rs.): ", 0, 100000);

    cout << "Day:  1. Mon  2. Tue  3. Wed  4. Thu  5. Fri  6. Sat  7. Sun" << endl;
    int day = readNumber("Choose (1-7): ", 1, 7) - 1;

    Time start = readTime("Start time (HH:MM, e.g. 10:30): ");
    Time end = readTime("End time (HH:MM, e.g. 17:45): ");
    while (end <= start) {
        cout << "  End time must be after " << start << "." << endl;
        end = readTime("End time (HH:MM): ");
    }

    cout << "Mood:  1. Relaxed  2. Foodie  3. Adventurous  4. Cultural  5. Shopping" << endl;
    int mood = readNumber("Choose (1-5): ", 1, 5);
    chooseScoring(planner);

    int startPlace = 0;   // 0 = Home
    int fromHome = readNumber("Are you starting from home? (1 = Yes, 2 = No): ", 1, 2);
    if (fromHome == 2) {
        cout << "Which place are you starting from?" << endl;
        vector<Place*> all = planner.getAllPlaces();
        for (int i = 0; i < all.size(); i++)
            cout << setw(4) << all[i]->getId() << ". " << all[i]->getName() << endl;
        startPlace = readNumber("Enter place ID: ", 1, 1000);
        while (!planner.hasPlace(startPlace))
            startPlace = readNumber("No such place, try again: ", 1, 1000);
    }
    bool returnHome = readNumber("Come back to the start at the end? (1 = Yes, 2 = No): ", 1, 2) == 1;
    double hours = readNumber("A food stop at least every how many hours? (2-6, e.g. 3): ", 2, 6);
    int mealGap = (int)(hours * 60 + 0.5);

    planner.setTrip(budget, start, end, mood, startPlace, day, returnHome, mealGap);
    planner.makePlan();
    planner.printPlan(cout);
}

void removeStop(Planner& planner) {
    vector<Place*> route = planner.getRoute();
    if (route.empty()) {
        cout << "Plan is empty." << endl;
        return;
    }
    for (int i = 0; i < route.size(); i++)
        cout << "  " << i + 1 << ". " << route[i]->getName() << endl;
    int n = readNumber("Which stop to remove? (0 = cancel): ", 0, route.size());
    if (n == 0) return;
    cout << planner.removePlace(route[n - 1]->getId()) << endl;
    planner.printPlan(cout);
}

void addStop(Planner& planner) {
    printPlaces(planner.getAllPlaces());
    int id = readNumber("Enter place ID to add (0 = cancel): ", 0, 1000);
    if (id == 0) return;
    string msg = planner.addPlace(id);
    cout << msg << endl;
    if (msg.find("Added") == 0) planner.printPlan(cout);
}

int main() {
    CityData city;
    try {
        city.load("data/places.txt", "data/roads.txt");
    } catch (DataFileException& e) {
        cout << e.what() << ". Run the program from the WanderiX folder." << endl;
        return 1;
    }
    Planner planner(&city);

    cout << endl << "*** Welcome to WanderiX ***" << endl;
    askQuestions(planner);

    int choice = -1;
    while (choice != 0) {
        cout << endl;
        cout << "+--------------------------------------+" << endl;
        cout << "|                 MENU                 |" << endl;
        cout << "+--------------------------------------+" << endl;
        cout << "|  1. Show plan                        |" << endl;
        cout << "|  2. Remove a place                   |" << endl;
        cout << "|  3. Add a place                      |" << endl;
        cout << "|  4. Undo                             |" << endl;
        cout << "|  5. Redo                             |" << endl;
        cout << "|  6. Search places by name            |" << endl;
        cout << "|  7. Browse places (sorted)           |" << endl;
        cout << "|  8. Change scoring style             |" << endl;
        cout << "|  9. Plan a new trip                  |" << endl;
        cout << "| 10. Save plan to file                |" << endl;
        cout << "|  0. Exit                             |" << endl;
        cout << "+--------------------------------------+" << endl;
        choice = readNumber("Enter your choice: ", 0, 10);

        if (choice == 1) planner.printPlan(cout);
        else if (choice == 2) removeStop(planner);
        else if (choice == 3) addStop(planner);
        else if (choice == 4) { cout << planner.undo() << endl; planner.printPlan(cout); }
        else if (choice == 5) { cout << planner.redo() << endl; planner.printPlan(cout); }
        else if (choice == 6) {
            string prefix;
            cout << "Type the start of a name: ";
            cin >> prefix;
            vector<int> ids = planner.search(prefix);
            if (ids.empty()) cout << "No place matches \"" << prefix << "\"." << endl;
            vector<Place*> found;
            for (int i = 0; i < ids.size(); i++) found.push_back(planner.getPlace(ids[i]));
            if (!found.empty()) printPlaces(found);
        }
        else if (choice == 7) {
            cout << "Sort by:  1. ID  2. Rating  3. Cost  4. Visit time" << endl;
            printPlaces(planner.sortedPlaces(readNumber("Choose (1-4): ", 1, 4)));
        }
        else if (choice == 8) {
            chooseScoring(planner);
            planner.makePlan();
            planner.printPlan(cout);
        }
        else if (choice == 9) askQuestions(planner);
        else if (choice == 10) {
            string fileName;
            cout << "File name (e.g. trip.txt): ";
            cin >> fileName;
            if (planner.savePlan(fileName)) cout << "Plan saved to " << fileName << endl;
            else cout << "Could not create " << fileName << endl;
        }
    }
    cout << "Bye! Enjoy your day." << endl;
    return 0;
}
