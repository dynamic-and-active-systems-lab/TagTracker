/****************************************************************************
 *
 * Field review: local east/north/up conversions, WGS84 flat-earth.
 *
 ****************************************************************************/

#include "Geodesy.h"

#include <cmath>

namespace FieldReview {

namespace {
constexpr double kPi = 3.14159265358979323846;
constexpr double kEccentricitySquared =
    kWgs84Flattening * (2.0 - kWgs84Flattening);

inline double toRadians(double degrees) { return degrees * kPi / 180.0; }
inline double toDegrees(double radians) { return radians * 180.0 / kPi; }
} // namespace

void earthRadii(double refLatDeg, double& meridionalOut, double& normalOut)
{
    const double sinLat = std::sin(toRadians(refLatDeg));
    const double d      = 1.0 - kEccentricitySquared * sinLat * sinLat;

    normalOut     = kWgs84SemiMajorAxisMeters / std::sqrt(d);
    meridionalOut = kWgs84SemiMajorAxisMeters * (1.0 - kEccentricitySquared)
                    / (d * std::sqrt(d));
}

EnuPoint geoToEnu(const GeoPoint& point, const GeoOrigin& origin)
{
    double meridional = 0.0;
    double normal     = 0.0;
    earthRadii(origin.latDeg, meridional, normal);

    EnuPoint out;
    out.eastMeters  = toRadians(point.lonDeg - origin.lonDeg) * normal
                      * std::cos(toRadians(origin.latDeg));
    out.northMeters = toRadians(point.latDeg - origin.latDeg) * meridional;
    out.upMeters    = point.altMeters - origin.altMeters;
    return out;
}

GeoPoint enuToGeo(const EnuPoint& point, const GeoOrigin& origin)
{
    double meridional = 0.0;
    double normal     = 0.0;
    earthRadii(origin.latDeg, meridional, normal);

    GeoPoint out;
    out.lonDeg    = origin.lonDeg
                    + toDegrees(point.eastMeters
                                / (normal * std::cos(toRadians(origin.latDeg))));
    out.latDeg    = origin.latDeg + toDegrees(point.northMeters / meridional);
    out.altMeters = point.upMeters + origin.altMeters;
    return out;
}

} // namespace FieldReview
