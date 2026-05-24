#pragma once
#include <atomic>
#include <deque>
#include <chrono>
#include <mutex>
#include <utility>
#include <cstdint>

class Metrics {
public:
    void recordRequest(long long duration_ms, bool ok);
    // текущий RPS: запросов за последнюю секунду
    double currentRps() const;
    // средний RPS за последние 10 минут
    double avgRps10min() const;
    // максимальный RPS за последние 10 минут
    double maxRps10min() const;
    // среднее время обработки за последние 10 секунд
    double avgDuration10s() const;
    // количество ошибок за последнюю минуту
    long long errorCount1min() const;

private:
    static constexpr long long RPS_1S_WINDOW_S   = 1;
    static constexpr long long RPS_10MIN_WINDOW_S = 600;
    static constexpr long long DUR_10S_WINDOW_S  = 10;
    static constexpr long long ERR_1MIN_WINDOW_S = 60;

    struct RpsBucket {
        long long second_epoch;
        int count;
    };

    mutable std::mutex _mutex;

    mutable std::deque<std::chrono::steady_clock::time_point> _rps_1s;

    mutable std::deque<RpsBucket> _rps_buckets;

    mutable std::deque<std::pair<std::chrono::steady_clock::time_point, long long>> _dur_10s;

    mutable std::deque<std::chrono::steady_clock::time_point> _errors_1min;
};