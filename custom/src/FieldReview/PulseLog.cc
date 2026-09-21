/****************************************************************************
 *
 * Field review: reader for TagTracker / MavlinkTagController2 pulse logs.
 *
 ****************************************************************************/

#include "PulseLog.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <map>
#include <sstream>

namespace FieldReview {

namespace {

std::string trimmed(const std::string& s)
{
    const char* ws = " \t\r\n";
    const size_t first = s.find_first_not_of(ws);
    if (first == std::string::npos) {
        return std::string();
    }
    const size_t last = s.find_last_not_of(ws);
    return s.substr(first, last - first + 1);
}

std::string lowered(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(::tolower(c)); });
    return s;
}

bool isHeaderLine(const std::string& line)
{
    return !line.empty()
           && (line[0] == '#' || line.find("tag_id") != std::string::npos);
}

/// Split on commas and convert leading fields to numbers, stopping at the
/// first field that is not a number. Mirrors the Python reader: a line is a
/// pulse record only if it yields at least kPulseFieldCount numbers.
std::vector<double> leadingNumbers(const std::string& line)
{
    std::vector<double> values;
    size_t start = 0;
    while (start <= line.size()) {
        const size_t comma = line.find(',', start);
        const std::string field =
            trimmed(line.substr(start, comma == std::string::npos
                                           ? std::string::npos
                                           : comma - start));
        if (field.empty()) {
            break;
        }
        char* end = nullptr;
        const double value = std::strtod(field.c_str(), &end);
        if (end == field.c_str() || *end != '\0') {
            break;
        }
        values.push_back(value);
        if (comma == std::string::npos) {
            break;
        }
        start = comma + 1;
    }
    return values;
}

PulseRecord toRecord(const std::vector<double>& v)
{
    PulseRecord r;
    r.commandId               = v[0];
    r.tagId                   = v[1];
    r.frequencyHz             = v[2];
    r.startTimeSeconds        = v[3];
    r.predictNextStartSeconds = v[4];
    r.snr                     = v[5];
    r.stftScore               = v[6];
    r.groupSeqCounter         = v[7];
    r.groupInd                = v[8];
    r.groupSnr                = v[9];
    r.noisePsd                = v[10];
    r.detectionStatus         = v[11];
    r.confirmedStatus         = v[12];
    r.latDeg                  = v[13];
    r.lonDeg                  = v[14];
    r.altRelMeters            = v[15];
    return r;
}

void appendWarning(std::string& warning, const std::string& note)
{
    if (warning.empty()) {
        warning = note;
    } else {
        warning += "\n" + note;
    }
}

} // namespace

PulseLogResult parsePulseLog(const std::string& contents)
{
    PulseLogResult result;

    std::vector<std::string> headers;
    std::vector<std::string> body;
    {
        std::istringstream stream(contents);
        std::string line;
        while (std::getline(stream, line)) {
            const std::string t = trimmed(line);
            if (t.empty()) {
                continue;
            }
            if (isHeaderLine(t)) {
                headers.push_back(t);
            } else {
                body.push_back(t);
            }
        }
    }

    if (headers.empty() && body.empty()) {
        result.error = "The file contains no data.";
        return result;
    }
    if (body.empty()) {
        result.error = "The file contains no pulse records.";
        return result;
    }

    // Warn if a future format moves the position fields off 14, 15 and 16.
    if (!headers.empty()) {
        std::string header = headers.front();
        if (!header.empty() && header[0] == '#') {
            header.erase(0, 1);
        }
        std::vector<std::string> tokens;
        std::istringstream stream(header);
        std::string token;
        while (std::getline(stream, token, ',')) {
            tokens.push_back(trimmed(token));
        }
        if (static_cast<int>(tokens.size()) >= kPulseFieldCount) {
            const std::string latName = lowered(tokens[13]);
            if (latName.find("latitude") == std::string::npos
                && latName.find("position_x") == std::string::npos) {
                appendWarning(result.warning,
                              "Unrecognised pulse log header. Column 14 is \""
                                  + tokens[13]
                                  + "\", expected \"latitude\" or \"position_x\"."
                                    " Positions may be wrong.");
            }
        }
    }

    std::vector<PulseRecord> rows;
    rows.reserve(body.size());
    for (const std::string& line : body) {
        const std::vector<double> values = leadingNumbers(line);
        if (static_cast<int>(values.size()) >= kPulseFieldCount) {
            rows.push_back(toRecord(values));
        }
    }
    if (rows.empty()) {
        result.error =
            "No complete pulse records found (rotation markers only?).";
        return result;
    }

    // Keep only the dominant record type, by command id.
    {
        std::map<double, size_t> counts;
        for (const PulseRecord& r : rows) {
            counts[r.commandId]++;
        }
        double dominant      = rows.front().commandId;
        size_t dominantCount = 0;
        for (const auto& entry : counts) {
            if (entry.second > dominantCount) {
                dominantCount = entry.second;
                dominant      = entry.first;
            }
        }
        rows.erase(std::remove_if(rows.begin(), rows.end(),
                                  [dominant](const PulseRecord& r) {
                                      return r.commandId != dominant;
                                  }),
                   rows.end());
    }

    // Then only rows with a usable time and a plausible fix.
    const size_t beforeFix = rows.size();
    rows.erase(std::remove_if(rows.begin(), rows.end(),
                              [](const PulseRecord& r) {
                                  const bool finite =
                                      std::isfinite(r.startTimeSeconds)
                                      && std::isfinite(r.tagId)
                                      && std::isfinite(r.latDeg)
                                      && std::isfinite(r.lonDeg);
                                  const bool inRange =
                                      std::fabs(r.latDeg) <= 90.0
                                      && std::fabs(r.lonDeg) <= 180.0;
                                  const bool nullIsland =
                                      r.latDeg == 0.0 && r.lonDeg == 0.0;
                                  return !(finite && inRange && !nullIsland);
                              }),
               rows.end());

    if (rows.empty()) {
        result.error = "No pulse records with a valid time and position.";
        return result;
    }
    const size_t dropped = beforeFix - rows.size();
    if (dropped > 0) {
        appendWarning(result.warning,
                      std::to_string(dropped)
                          + " record(s) dropped for a missing time or position.");
    }

    // Altitude is optional.
    for (PulseRecord& r : rows) {
        if (!std::isfinite(r.altRelMeters)) {
            r.altRelMeters = 0.0;
        }
    }

    // Pulses are logged as they arrive; a stable sort lets callers assume order.
    std::stable_sort(rows.begin(), rows.end(),
                     [](const PulseRecord& a, const PulseRecord& b) {
                         return a.startTimeSeconds < b.startTimeSeconds;
                     });

    result.pulses = std::move(rows);
    return result;
}

PulseLogResult readPulseLog(const std::string& path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        PulseLogResult result;
        result.error = "Could not open " + path;
        return result;
    }
    std::ostringstream buffer;
    buffer << file.rdbuf();
    return parsePulseLog(buffer.str());
}

} // namespace FieldReview
