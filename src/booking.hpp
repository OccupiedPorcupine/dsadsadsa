#pragma once

#include "instance.hpp"

#include <vector>

// A scheduled rally: `politician` has the town from `start` until `end` (in active hours).
struct Booking {
    double start;
    double end;
    int politician;
};

// Keeps track of when each town is taken.
//
// Each town has a list of bookings sorted by start time. Under the soft clash rule two
// politicians' bookings may overlap, so the list is not necessarily sorted by end time.
// With k politicians a town has at most k bookings, so going through the list is cheap.
class BookingTable {
public:
    BookingTable(int numTowns, const Clock& clock);

    // Earliest time at or after `arriveTime` when `politician` can hold a rally lasting
    // `duration` hours in `town` inside one day's window, without overlapping anyone else's rally.
    double earliestFreeTime(int town, double arriveTime, double duration, int politician) const;

    // When the next rally by another politician starts in `town`, at or after `time`.
    // Returns infinity if there isn't one.
    double nextOtherBooking(int town, double time, int politician) const;

    // Hours of [start, end) that overlap other politicians' rallies in `town`.
    double overlapHours(int town, double start, double end, int politician) const;

    const std::vector<Booking>& bookingsIn(int town) const { return bookingsByTown[town]; }

    void book(int town, double start, double end, int politician);
    void cancel(int town, int politician);
    void rebook(int town, int politician, double newStart, double newEnd);

private:
    std::vector<std::vector<Booking>> bookingsByTown;
    Clock clock;
};
