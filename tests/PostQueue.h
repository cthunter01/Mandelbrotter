#pragma once

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <functional>
#include <mutex>
#include <utility>

namespace mandelbrotter::test
{

/// Stands in for a toolkit's event queue: worker threads push closures (a controller's post hook),
/// the test thread runs them. Nothing runs until the test drains the queue, so the states in
/// between are observable deterministically. Waits are bounded: they return false on timeout
/// instead of hanging the test.
class PostQueue
{
public:
    static constexpr std::chrono::milliseconds kTimeout{10'000};

    /// The post hook: pushes from any thread.
    [[nodiscard]] std::function<void(std::function<void()>)> hook()
    {
        return [this](std::function<void()> work) { push(std::move(work)); };
    }

    void push(std::function<void()> work)
    {
        {
            const std::scoped_lock lock(m_mutex);
            m_queue.push_back(std::move(work));
        }
        m_pushed.notify_all();
    }

    [[nodiscard]] std::size_t size() const
    {
        const std::scoped_lock lock(m_mutex);
        return m_queue.size();
    }

    /// Runs the oldest closure, if there is one.
    bool drainOne()
    {
        std::function<void()> work;
        {
            const std::scoped_lock lock(m_mutex);
            if (m_queue.empty())
            {
                return false;
            }
            work = std::move(m_queue.front());
            m_queue.pop_front();
        }
        work();
        return true;
    }

    /// Runs closures until the queue is empty; returns how many ran.
    std::size_t drainAll()
    {
        std::size_t count = 0;
        while (drainOne())
        {
            ++count;
        }
        return count;
    }

    /// Waits until at least `count` closures are queued, running none.
    [[nodiscard]] bool waitForPending(std::size_t               count,
                                      std::chrono::milliseconds timeout = kTimeout)
    {
        std::unique_lock lock(m_mutex);
        return m_pushed.wait_for(lock, timeout, [&] { return m_queue.size() >= count; });
    }

    /// Waits for a closure and runs it.
    [[nodiscard]] bool waitAndDrainOne(std::chrono::milliseconds timeout = kTimeout)
    {
        return waitForPending(1, timeout) && drainOne();
    }

    /// Runs closures as they arrive, one at a time, until `done` holds.
    [[nodiscard]] bool waitAndDrainUntil(const std::function<bool()>& done,
                                         std::chrono::milliseconds    timeout = kTimeout)
    {
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        while (!done())
        {
            if (drainOne())
            {
                continue;
            }
            std::unique_lock lock(m_mutex);
            if (!m_pushed.wait_until(lock, deadline, [this] { return !m_queue.empty(); }))
            {
                return false;
            }
        }
        return true;
    }

private:
    mutable std::mutex                m_mutex;
    std::condition_variable           m_pushed;
    std::deque<std::function<void()>> m_queue;
};

}  // namespace mandelbrotter::test
