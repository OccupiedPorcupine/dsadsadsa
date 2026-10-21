#include "booking.hpp"

#include <algorithm>
#include <limits>

double BookingTable::earliestFree(int town, double t, double duration, int self) const {
    double s = t;
    for (const Booking& b : byTown_[town]) {
        if (b.politician == self || b.end <= s + kEps) continue;
        if (b.start >= s + duration - kEps) break;
        s = b.end;  // overlaps: wait until this rally ends
    }
    return s;
}

double BookingTable::nextOtherStart(int town, double t, int self) const {
    for (const Booking& b : byTown_[town])
        if (b.politician != self && b.start >= t - kEps) return b.start;
    return std::numeric_limits<double>::infinity();
}

void BookingTable::add(int town, double start, double end, int politician) {
    auto& list = byTown_[town];
    auto pos = std::upper_bound(list.begin(), list.end(), start,
                                [](double s, const Booking& b) { return s < b.start; });
    list.insert(pos, Booking{start, end, politician});
}

void BookingTable::remove(int town, int politician) {
    auto& list = byTown_[town];
    list.erase(std::remove_if(list.begin(), list.end(),
                              [&](const Booking& b) { return b.politician == politician; }),
               list.end());
}

void BookingTable::move(int town, int politician, double start, double end) {
    remove(town, politician);
    add(town, start, end, politician);
}
