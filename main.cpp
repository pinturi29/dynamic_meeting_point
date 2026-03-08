#include <iostream>
#include <fstream>
#include <string>
#include <thread>
#include <chrono>
#include <cpr/cpr.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

static const std::string API_KEY = [] {
    const char* k = std::getenv("GOOGLE_API_KEY");
    if (!k) { std::cerr << "Error: GOOGLE_API_KEY not set\n"; std::exit(1); }
    return std::string(k);
}();
static const std::string ORIGIN    = "37.7749,-122.4194";  // San Francisco, CA
static const std::string DEST      = "37.3382,-121.8863";  // San Jose, CA
static const int         INTERVALS = 1;
static const int         SLEEP_SEC = 60;

struct LatLng {
    double lat;
    double lng;
};

// Returns the step end_location at the time midpoint, or {} on error.
std::optional<LatLng> findTimeMidpoint(const std::string& apiKey,
                                       const std::string& origin,
                                       const std::string& dest)
{
    std::string url = "https://maps.googleapis.com/maps/api/directions/json";

    auto r = cpr::Get(
        cpr::Url{url},
        cpr::Parameters{
            {"origin",          origin},
            {"destination",     dest},
            {"departure_time",  "now"},
            {"traffic_model",   "best_guess"},
            {"key",             apiKey}
        }
    );

    if (r.status_code != 200) {
        std::cerr << "HTTP error: " << r.status_code << "\n";
        return std::nullopt;
    }

    json body;
    try {
        body = json::parse(r.text);
    } catch (const json::exception& e) {
        std::cerr << "JSON parse error: " << e.what() << "\n";
        return std::nullopt;
    }

    if (body["status"] != "OK") {
        std::cerr << "API status: " << body["status"] << "\n";
        return std::nullopt;
    }

    auto& leg   = body["routes"][0]["legs"][0];
    auto& steps = leg["steps"];
    long long totalSec = leg["duration_in_traffic"]["value"].get<long long>();

    std::cout << "  Total duration in traffic: " << totalSec << "s\n";

    // Two-pointer: walk from A's end and B's end simultaneously.
    // Stop when the pointers cross — the meeting point is at that boundary,
    // meaning both users travel equal time to reach it.
    int lo = 0, hi = (int)steps.size() - 1;
    long long acc_A = 0, acc_B = 0;

    while (lo < hi) {
        long long dur_lo = steps[lo]["duration"]["value"].get<long long>();
        long long dur_hi = steps[hi]["duration"]["value"].get<long long>();

        if (acc_A + dur_lo <= acc_B + dur_hi) {
            acc_A += dur_lo;
            lo++;
        } else {
            acc_B += dur_hi;
            hi--;
        }
    }

    // Interpolate within the converging step so both users travel equal time.
    // Solve: acc_A + t = acc_B + (D - t)  =>  t = (acc_B + D - acc_A) / 2
    long long D  = steps[lo]["duration"]["value"].get<long long>();
    double fraction = 0.5;
    if (D > 0) {
        double t_A = (acc_B + D - acc_A) / 2.0;
        fraction = std::max(0.0, std::min(1.0, t_A / D));
    }

    double lat0 = steps[lo]["start_location"]["lat"].get<double>();
    double lng0 = steps[lo]["start_location"]["lng"].get<double>();
    double lat1 = steps[lo]["end_location"]["lat"].get<double>();
    double lng1 = steps[lo]["end_location"]["lng"].get<double>();

    long long each = (long long)(acc_A + fraction * D);
    std::cout << "  Time from A: " << each << "s  |  Time from B: " << each << "s\n";

    return LatLng{
        lat0 + fraction * (lat1 - lat0),
        lng0 + fraction * (lng1 - lng0)
    };
}

int main()
{
    std::cout << "=== Dynamic Meet Midpoint Finder ===\n";
    std::cout << "Origin : " << ORIGIN << "\n";
    std::cout << "Dest   : " << DEST   << "\n";
    std::cout << "Running " << INTERVALS << " iterations, "
              << SLEEP_SEC << "s apart.\n\n";

    for (int i = 1; i <= INTERVALS; ++i) {
        std::cout << "--- Iteration " << i << "/" << INTERVALS << " ---\n";

        auto result = findTimeMidpoint(API_KEY, ORIGIN, DEST);
        if (result) {
            std::cout << "  Meeting point: "
                      << result->lat << ", " << result->lng << "\n";

            // Write output.json for the map visualization
            json out;
            out["iteration"]   = i;
            out["timestamp"]   = std::chrono::duration_cast<std::chrono::seconds>(
                                     std::chrono::system_clock::now().time_since_epoch()).count();
            out["origin"]      = { {"lat", 37.7749}, {"lng", -122.4194} };
            out["destination"] = { {"lat", 37.3382}, {"lng", -121.8863} };
            out["midpoint"]    = { {"lat", result->lat}, {"lng", result->lng} };
            std::ofstream("output.json") << out.dump(2);
        } else {
            std::cout << "  Failed to compute meeting point.\n";
        }

        if (i < INTERVALS) {
            std::cout << "  Sleeping " << SLEEP_SEC << "s...\n\n";
            std::this_thread::sleep_for(std::chrono::seconds(SLEEP_SEC));
        }
    }

    std::cout << "\nDone.\n";
    return 0;
}
