/****************************************************************************
 *
 * Field review: local east/north/up conversions, WGS84 flat-earth.
 *
 * Plain C++. No Qt and no QGroundControl types, so this can be unit tested on
 * its own and checked against the Python prototype it was ported from
 * (uavrt_postflight/python/geodesy.py, itself a port of geo2enu.m/enu2geo.m).
 *
 * Accuracy is better than 5 cm against a rigorous ECEF-based ENU transform
 * over a 1 km box, checked from -37 to +54 degrees latitude. It is a local
 * approximation and is not meant for spans of tens of kilometres.
 *
 * The transform is linear in east and north independently, which is what makes
 * a rectangular ENU grid map to an exactly rectangular latitude/longitude box.
 *
 ****************************************************************************/

#pragma once

namespace FieldReview {

constexpr double kWgs84SemiMajorAxisMeters = 6378137.0;
constexpr double kWgs84Flattening          = 1.0 / 298.257223563;

/// A local tangent-plane origin: the point east/north are measured from.
struct GeoOrigin {
    double latDeg   = 0.0;
    double lonDeg   = 0.0;
    double altMeters = 0.0;
};

/// A position in the local frame, metres east, north and up of the origin.
struct EnuPoint {
    double eastMeters  = 0.0;
    double northMeters = 0.0;
    double upMeters    = 0.0;
};

/// A geodetic position.
struct GeoPoint {
    double latDeg    = 0.0;
    double lonDeg    = 0.0;
    double altMeters = 0.0;
};

/// Meridional and normal radii of curvature at a reference latitude, metres.
void earthRadii(double refLatDeg, double& meridionalOut, double& normalOut);

/// Geodetic to local ENU.
EnuPoint geoToEnu(const GeoPoint& point, const GeoOrigin& origin);

/// Local ENU to geodetic. The exact inverse of geoToEnu.
GeoPoint enuToGeo(const EnuPoint& point, const GeoOrigin& origin);

} // namespace FieldReview
