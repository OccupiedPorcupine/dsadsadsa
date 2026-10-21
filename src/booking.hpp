#pragma once

#include <vector>

constexpr double kEps = 1e-9;

struct Booking {
    double start, end;  // the town is occupied during [start, end)
    int politician;
};

// For every town, the times politicians are holding a rally there, sorted by start time.
// Bookings by different politicians at one town never overlap, so each list is also sorted by end.
class BookingTable {
public:
    explicit BookingTable(int towns) : byTown_(towns) {}

    // Earliest s >= t such that [s, s + duration) overlaps no other politician's booking at `town`.
    double earliestFree(int town, double t, double duration, int self) const;

    // Start of the first booking by another politician at `town` that begins at or after `t`
    // (infinity if there is none). A rally starting at t may run until then without a clash.
    double nextOtherStart(int town, double t, int self) const;

    void add(int town, double start, double end, int politician);
    void remove(int town, int politician);
    void move(int town, int politician, double start, double end);

    const std::vector<Booking>& at(int town) const { return byTown_[town]; }

private:
    std::vector<std::vector<Booking>> byTown_;
};
