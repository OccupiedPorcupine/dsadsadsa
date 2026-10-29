#include "booking.hpp"

#include <algorithm>
#include <limits>

using namespace std;

BookingTable::BookingTable(int numTowns, const Clock& clock) : bookingsByTown(numTowns), clock(clock) {}

double BookingTable::earliestFreeTime(int town, double arriveTime, double duration, int politician) const {
    double start = clock.fitRally(arriveTime, duration);

    // Bookings can overlap each other, so one pass in start order isn't enough: after waiting for
    // one rally to end, an earlier-starting but longer one may still be running. Keep going until
    // nothing overlaps. Each step moves `start` forward, and there are at most k bookings.
    bool moved = true;
    while (moved) {
        moved = false;
        for (const Booking& booking : bookingsByTown[town]) {
            if (booking.politician == politician) continue;  // can't clash with yourself
            bool overlaps = booking.start < start + duration - EPS && booking.end > start + EPS;
            if (!overlaps) continue;

            // Wait until it ends, then make sure the rally still fits in that day.
            start = clock.fitRally(booking.end, duration);
            moved = true;
        }
    }
    return start;
}

double BookingTable::nextOtherBooking(int town, double time, int politician) const {
    for (const Booking& booking : bookingsByTown[town]) {
        if (booking.politician != politician && booking.start >= time - EPS) {
            return booking.start;
        }
    }
    return numeric_limits<double>::infinity();
}

double BookingTable::overlapHours(int town, double start, double end, int politician) const {
    double total = 0;
    for (const Booking& booking : bookingsByTown[town]) {
        if (booking.politician == politician) continue;
        total += max(0.0, min(end, booking.end) - max(start, booking.start));
    }
    return total;
}

void BookingTable::book(int town, double start, double end, int politician) {
    vector<Booking>& bookings = bookingsByTown[town];

    // Insert in start-time order (after any booking with the same start).
    int position = 0;
    while (position < (int)bookings.size() && bookings[position].start <= start) {
        position++;
    }
    bookings.insert(bookings.begin() + position, Booking{start, end, politician});
}

void BookingTable::cancel(int town, int politician) {
    vector<Booking>& bookings = bookingsByTown[town];
    for (int i = 0; i < (int)bookings.size(); ++i) {
        if (bookings[i].politician == politician) {
            bookings.erase(bookings.begin() + i);
            return;  // a politician holds at most one rally per town
        }
    }
}

void BookingTable::rebook(int town, int politician, double newStart, double newEnd) {
    cancel(town, politician);
    book(town, newStart, newEnd, politician);
}
