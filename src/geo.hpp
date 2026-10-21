#pragma once

#include <cmath>

// Great-circle distance between two WGS84 points, in kilometres.
inline double haversineKm(double lat1, double lon1, double lat2, double lon2) {
    constexpr double kEarthRadiusKm = 6371.0;
    constexpr double kDegToRad = 3.14159265358979323846 / 180.0;
    double dLat = (lat2 - lat1) * kDegToRad;
    double dLon = (lon2 - lon1) * kDegToRad;
    double a = std::sin(dLat / 2) * std::sin(dLat / 2) +
               std::cos(lat1 * kDegToRad) * std::cos(lat2 * kDegToRad) *
               std::sin(dLon / 2) * std::sin(dLon / 2);
    return 2 * kEarthRadiusKm * std::asin(std::sqrt(a));
}
