#include "World/ColumnWorkers.hpp"
#include "utils/Profiler.hpp"
#include <algorithm>
#include <iterator>
#include <utility>

#ifdef __APPLE__
#include <pthread.h>
#endif

void ColumnWorkers::start(unsigned threadCount) {
    m_stopping = false;
    for (unsigned i = 0; i < threadCount; ++i) m_threads.emplace_back([this] { run(); });
}

void ColumnWorkers::stop() {
    {
        std::lock_guard lock(m_mutex);
        m_stopping = true;
        m_queue.clear();
    }
    m_wake.notify_all();
    for (auto& t : m_threads) t.join();
    m_threads.clear();
    m_results.clear();
}

std::vector<ColumnWorkers::Job> ColumnWorkers::clearQueue() {
    std::lock_guard lock(m_mutex);
    return std::exchange(m_queue, {});
}

void ColumnWorkers::enqueue(const std::vector<Job>& jobs) {
    {
        std::lock_guard lock(m_mutex);
        m_queue.insert(m_queue.end(), jobs.begin(), jobs.end());
        std::sort(m_queue.begin(), m_queue.end(),
                  [](const Job& a, const Job& b) { return a.priority > b.priority; });
    }
    m_wake.notify_all();
}

void ColumnWorkers::collect(std::vector<Result>& out) {
    std::lock_guard lock(m_mutex);
    std::move(m_results.begin(), m_results.end(), std::back_inserter(out));
    m_results.clear();
}

size_t ColumnWorkers::queued() const {
    std::lock_guard lock(m_mutex);
    return m_queue.size();
}

void ColumnWorkers::run() {
    ProfilerSetThreadName("Column worker");
#ifdef __APPLE__
    // Keeps workers on performance cores, one step below the render thread.
    pthread_set_qos_class_self_np(QOS_CLASS_USER_INITIATED, 0);
#endif
    ColumnVolume volume;
    for (;;) {
        Job job;
        {
            std::unique_lock lock(m_mutex);
            m_wake.wait(lock, [this] { return m_stopping || !m_queue.empty(); });
            if (m_stopping) return;
            job = m_queue.back();
            m_queue.pop_back();
        }

        Result result{job.key, job.openSides, {}};
        {
            ZoneScopedN("Build column");
            generateColumn(job.key, job.openSides, volume);
            meshColumn(volume, result.mesh);
        }

        std::lock_guard lock(m_mutex);
        if (m_stopping) return;
        m_results.push_back(std::move(result));
    }
}
