#ifndef ITINERARY_H
#define ITINERARY_H

#include "Place.h"

// one stop in the trip (linked list node)
struct Stop {
    Place* place;
    int travel;   // minutes from previous stop
    int wait;     // minutes waited for the place to open
    int start;    // time the visit starts
    Stop* next;
};

// the final plan, stops are kept in visiting order
class Itinerary {
private:
    Stop* head;
    Stop* tail;
    int count;

public:
    Itinerary() {
        head = NULL;
        tail = NULL;
        count = 0;
    }

    ~Itinerary() {
        clear();
    }

    void addStop(Place* p, int travel, int wait, int start) {
        Stop* s = new Stop;
        s->place = p;
        s->travel = travel;
        s->wait = wait;
        s->start = start;
        s->next = NULL;

        if (head == NULL) {
            head = s;
            tail = s;
        } else {
            tail->next = s;
            tail = s;
        }
        count++;
    }

    void clear() {
        while (head != NULL) {
            Stop* temp = head;
            head = head->next;
            delete temp;
        }
        tail = NULL;
        count = 0;
    }

    Stop* getHead() {
        return head;
    }

    int size() {
        return count;
    }
};

#endif
