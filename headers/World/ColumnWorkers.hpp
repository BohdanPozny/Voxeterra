#pragma once

#include "World/Mesher.hpp"
#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>

// Background threads that generate and mesh columns; the main thread only uploads.
class ColumnWorkers {
public:
    struct Job {
        ColumnKey key;
        unsigned openSides = 0;
        float priority = 0.0f;  // lower runs first
    };
    struct Result {
        ColumnKey key;
        unsigned openSides = 0;
        ColumnMesh mesh;
    };

    ColumnWorkers() = default;
    ~ColumnWorkers() { stop(); }

    ColumnWorkers(const ColumnWorkers&) = delete;
    ColumnWorkers& operator=(const ColumnWorkers&) = delete;

    void start(unsigned threadCount);
    void stop();

    // Removes jobs that have not started yet and returns them.
    std::vector<Job> clearQueue();
    void enqueue(const std::vector<Job>& jobs);
    void collect(std::vector<Result>& out);
    size_t queued() const;

private:
    void run();

    std::vector<std::thread> m_threads;
    mutable std::mutex m_mutex;
    std::condition_variable m_wake;
    std::vector<Job> m_queue;  // sorted so the most urgent job is at the back
    std::vector<Result> m_results;
    bool m_stopping = false;
};
