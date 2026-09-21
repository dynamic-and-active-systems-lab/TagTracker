/****************************************************************************
 *
 * Field review: reader for TagTracker / MavlinkTagController2 pulse logs.
 *
 * Plain C++. No Qt and no QGroundControl types. Ported from
 * uavrt_postflight/python/readpulsetable.py, itself a port of
 * readpulsetable.m, and it must keep parsing the same way they do.
 *
 * TagTracker has shipped four header variants of the same file:
 *
 *   2023 builds              no header at all,                     20 columns
 *   2024 to 2026-04          "# 7, tag_id, ... position_x, _y, _z,
 *                             orientation_x, _y, _z, _w,
 *                             antenna_offset"                      21 columns
 *   master since 2026-04-10  "# 1, tag_id, ... latitude, longitude,
 *                             altitude_rel, roll_deg, pitch_deg,
 *                             yaw_deg, antenna_offset"             20 columns
 *
 * In every one of them the first sixteen fields of a pulse record are in the
 * same order and columns 14, 15 and 16 are latitude, longitude and altitude.
 * So this parses positionally and ignores what the header calls things.
 *
 * The full pulse log also interleaves four-field rotation start and stop
 * records. A naive CSV reader turns those into "pulses" carrying a latitude
 * in the tag id column and no position, which corrupts the tag list and any
 * local frame anchored on the first row. They are dropped here.
 *
 * The reader takes any path and makes no assumption about which file it was
 * handed. Rotation logs are the same format as the whole-flight log, so
 * keeping it indifferent is all phase 2 needs from phase 1 (FIELD_REVIEW.md
 * section 4a). Do not narrow it to one timestamped Pulse-*.csv per session.
 *
 ****************************************************************************/

#pragma once

#include <string>
#include <vector>

namespace FieldReview {

/// The sixteen fields every pulse log variant shares, in file order.
struct PulseRecord {
    double commandId               = 0.0;
    double tagId                   = 0.0;
    double frequencyHz             = 0.0;
    double startTimeSeconds        = 0.0;
    double predictNextStartSeconds = 0.0;
    double snr                     = 0.0;
    double stftScore               = 0.0;
    double groupSeqCounter         = 0.0;
    double groupInd                = 0.0;
    double groupSnr                = 0.0;
    double noisePsd                = 0.0;
    double detectionStatus         = 0.0;
    double confirmedStatus         = 0.0;
    double latDeg                  = 0.0;
    double lonDeg                  = 0.0;
    double altRelMeters            = 0.0;
};

/// Number of leading fields a line must have to be a pulse record.
constexpr int kPulseFieldCount = 16;

struct PulseLogResult {
    /// Records sorted by startTimeSeconds, so callers can assume order.
    std::vector<PulseRecord> pulses;

    /// Empty when nothing was unusual. Worth showing the operator: it reports
    /// an unrecognised header or records dropped for a missing fix.
    std::string warning;

    /// Empty on success. Set when the file holds no usable pulse records, in
    /// which case pulses is empty.
    std::string error;

    bool ok() const { return error.empty(); }
};

/// Read a pulse log from disk.
PulseLogResult readPulseLog(const std::string& path);

/// Read a pulse log already in memory. Exposed for tests and for a caller
/// that has the bytes from somewhere other than a file.
PulseLogResult parsePulseLog(const std::string& contents);

} // namespace FieldReview
