#pragma once

#include <cmath>

// Straight-line distance in km between two points on the Earth (haversine formula).
inline double haversineKm(double lat1, double lon1, double lat2, double lon2) {
    const double EARTH_RADIUS_KM = 6371.0;
    const double DEG_TO_RAD = 3.14159265358979323846 / 180.0;

    double dLat = (lat2 - lat1) * DEG_TO_RAD;
    double dLon = (lon2 - lon1) * DEG_TO_RAD;
    double a = std::sin(dLat / 2) * std::sin(dLat / 2) +
               std::cos(lat1 * DEG_TO_RAD) * std::cos(lat2 * DEG_TO_RAD) *
               std::sin(dLon / 2) * std::sin(dLon / 2);
    return 2 * EARTH_RADIUS_KM * std::asin(std::sqrt(a));
}
