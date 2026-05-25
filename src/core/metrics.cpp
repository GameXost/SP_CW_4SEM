#include "core/metrics.h"
#include <algorithm>

void Metrics::recordRequest(long long duration_ms, bool ok) {
    auto now = std::chrono::steady_clock::now();
    long long sec = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();

    std::lock_guard<std::mutex> lock(_mutex);

    _rps_1s.push_back(now);
    auto cutoff_1s = now - std::chrono::seconds(RPS_1S_WINDOW_S);
    while (!_rps_1s.empty() && _rps_1s.front() < cutoff_1s) {
        _rps_1s.pop_front();
    }

    if (!_rps_buckets.empty() && _rps_buckets.back().second_epoch == sec) {
        ++_rps_buckets.back().count;
    } else {
        _rps_buckets.push_back({sec, 1});
    }
    long long cutoff_bucket = sec - RPS_10MIN_WINDOW_S;
    while (!_rps_buckets.empty() && _rps_buckets.front().second_epoch < cutoff_bucket) {
        _rps_buckets.pop_front();
    }

    _dur_10s.push_back({now, duration_ms});
    auto cutoff_10s = now - std::chrono::seconds(DUR_10S_WINDOW_S);
    while (!_dur_10s.empty() && _dur_10s.front().first < cutoff_10s) {
        _dur_10s.pop_front();
    }

    if (!ok) {
        _errors_1min.push_back(now);
    }
    auto cutoff_1min = now - std::chrono::seconds(ERR_1MIN_WINDOW_S);
    while (!_errors_1min.empty() && _errors_1min.front() < cutoff_1min) {
        _errors_1min.pop_front();
    }
}

double Metrics::currentRps() const {
    auto now = std::chrono::steady_clock::now();
    auto cutoff = now - std::chrono::seconds(RPS_1S_WINDOW_S);
    std::lock_guard<std::mutex> lock(_mutex);
    while (!_rps_1s.empty() && _rps_1s.front() < cutoff) {
        _rps_1s.pop_front();
    }
    return static_cast<double>(_rps_1s.size());
}

double Metrics::avgRps10min() const {
    std::lock_guard<std::mutex> lock(_mutex);
    if (_rps_buckets.empty()) return 0.0;
    long long total = 0;
    for (const auto& b : _rps_buckets) total += b.count;
    long long span = _rps_buckets.back().second_epoch - _rps_buckets.front().second_epoch + 1;
    span = std::max(span, 1LL);
    return static_cast<double>(total) / static_cast<double>(span);
}

double Metrics::maxRps10min() const {
    std::lock_guard<std::mutex> lock(_mutex);
    if (_rps_buckets.empty()) return 0.0;
    int mx = 0;
    for (const auto& b : _rps_buckets) mx = std::max(mx, b.count);
    return static_cast<double>(mx);
}

double Metrics::avgDuration10s() const {
    auto now = std::chrono::steady_clock::now();
    auto cutoff = now - std::chrono::seconds(DUR_10S_WINDOW_S);
    std::lock_guard<std::mutex> lock(_mutex);
    while (!_dur_10s.empty() && _dur_10s.front().first < cutoff) {
        _dur_10s.pop_front();
    }
    if (_dur_10s.empty()) return 0.0;
    long long sum = 0;
    for (const auto& [tp, dur] : _dur_10s) sum += dur;
    return static_cast<double>(sum) / static_cast<double>(_dur_10s.size());
}

long long Metrics::errorCount1min() const {
    auto now = std::chrono::steady_clock::now();
    auto cutoff = now - std::chrono::seconds(ERR_1MIN_WINDOW_S);
    std::lock_guard<std::mutex> lock(_mutex);
    while (!_errors_1min.empty() && _errors_1min.front() < cutoff) {
        _errors_1min.pop_front();
    }
    return static_cast<long long>(_errors_1min.size());
}