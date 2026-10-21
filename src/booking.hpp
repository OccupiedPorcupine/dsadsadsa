#pragma once

#include <vector>

// Small tolerance for comparing times, so rounding errors don't count as clashes.
const double EPS = 1e-9;

// A scheduled rally: `politician` has the town from `start` until `end` (in hours).
struct Booking {
    double start;
    double end;
    int politician;
};

// Keeps track of when each town is taken.
//
// Each town has a list of bookings sorted by start time. Two different politicians' bookings
// never overlap, so the list is also sorted by end time. With k politicians a town has at
// most k bookings, so going through the list is cheap.
class BookingTable {
public:
    explicit BookingTable(int numTowns);

    // Earliest time at or after `arriveTime` when `politician` can hold a rally lasting
    // `duration` hours in `town` without overlapping anyone else's rally.
    double earliestFreeTime(int town, double arriveTime, double duration, int politician) const;

    // When the next rally by another politician starts in `town`, at or after `time`.
    // Returns infinity if there isn't one.
    double nextOtherBooking(int town, double time, int politician) const;

    void book(int town, double start, double end, int politician);
    void cancel(int town, int politician);
    void rebook(int town, int politician, double newStart, double newEnd);

private:
    std::vector<std::vector<Booking>> bookingsByTown;
};
