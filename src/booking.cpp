#include "booking.hpp"

#include <limits>

using namespace std;

BookingTable::BookingTable(int numTowns) : bookingsByTown(numTowns) {}

double BookingTable::earliestFreeTime(int town, double arriveTime, double duration, int politician) const {
    double start = arriveTime;
    for (const Booking& booking : bookingsByTown[town]) {
        if (booking.politician == politician) continue;  // can't clash with yourself
        if (booking.end <= start + EPS) continue;        // already over by the time we'd start
        if (booking.start >= start + duration - EPS) break;  // starts after we'd finish, and so do the rest

        // This booking overlaps, so wait until it ends and check again.
        start = booking.end;
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
