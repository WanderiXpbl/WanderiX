#include "Planner.h"

void askQuestions(Planner& planner) {
    cout << endl << "========== PLAN YOUR DAY ==========" << endl;
    int budget = readNumber("Budget (Rs.): ", 0, 100000);
    int start = readNumber("Start hour (0-23, e.g. 10 = 10 AM): ", 0, 23);
    int end = readNumber("End hour (e.g. 17 = 5 PM): ", start + 1, 24);

    cout << "Mood:  1. Relaxed  2. Foodie  3. Adventurous  4. Cultural  5. Shopping" << endl;
    int mood = readNumber("Choose (1-5): ", 1, 5);

    int startPlace = 0;   // 0 = Home
    int fromHome = readNumber("Are you starting from home? (1 = Yes, 2 = No): ", 1, 2);
    if (fromHome == 2) {
        planner.showStartOptions();
        startPlace = readNumber("Enter place ID: ", 1, 1000);
        while (!planner.hasPlace(startPlace))
            startPlace = readNumber("No such place, try again: ", 1, 1000);
    }

    planner.setTrip(budget, start * 60, end * 60, mood, startPlace);
    planner.makePlan();
    planner.showPlan();
}

int main() {
    Planner planner;
    if (!planner.loadData("data/places.txt", "data/roads.txt")) {
        cout << "Run the program from the WanderiX folder." << endl;
        return 1;
    }

    cout << endl << "*** Welcome to WanderiX ***" << endl;
    askQuestions(planner);

    int choice = -1;
    while (choice != 0) {
        cout << endl;
        cout << "+------------------------------+" << endl;
        cout << "|            MENU              |" << endl;
        cout << "+------------------------------+" << endl;
        cout << "|  1. Show plan                |" << endl;
        cout << "|  2. Remove a place           |" << endl;
        cout << "|  3. Add a place              |" << endl;
        cout << "|  4. Undo last change         |" << endl;
        cout << "|  5. Show all places          |" << endl;
        cout << "|  6. Plan a new trip          |" << endl;
        cout << "|  7. Save plan to file        |" << endl;
        cout << "|  0. Exit                     |" << endl;
        cout << "+------------------------------+" << endl;
        choice = readNumber("Enter your choice: ", 0, 7);

        if (choice == 1) planner.showPlan();
        else if (choice == 2) planner.removePlace();
        else if (choice == 3) planner.addPlace();
        else if (choice == 4) planner.undo();
        else if (choice == 5) planner.showAllPlaces();
        else if (choice == 6) askQuestions(planner);
        else if (choice == 7) planner.savePlan();
    }
    cout << "Bye! Enjoy your day." << endl;
    return 0;
}
