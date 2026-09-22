#pragma once

#include <atomic>
#include <filesystem>
#include <random>
#include <string>

namespace mandelbrotter::test
{

/// A fresh directory under the system temp dir, removed on destruction.
class TempDir
{
public:
    TempDir()
    {
        // A random per-process token instead of the PID: it needs no OS header, and two test
        // binaries running at once still get distinct directories (each removes its own in the
        // destructor).
        static const unsigned   kProcessToken = std::random_device{}();
        static std::atomic<int> s_counter{0};
        const auto id = std::to_string(kProcessToken) + "-" + std::to_string(s_counter++);
        m_path        = std::filesystem::temp_directory_path() / ("mandelbrotter-test-" + id);
        std::filesystem::create_directories(m_path);
    }
    ~TempDir()
    {
        std::error_code ec;
        std::filesystem::remove_all(m_path, ec);
    }
    TempDir(const TempDir&)            = delete;
    TempDir& operator=(const TempDir&) = delete;
    TempDir(TempDir&&)                 = delete;
    TempDir& operator=(TempDir&&)      = delete;

    [[nodiscard]] const std::filesystem::path& path() const noexcept { return m_path; }
    [[nodiscard]] std::filesystem::path        operator/(const std::string& name) const
    {
        return m_path / name;
    }

private:
    std::filesystem::path m_path;
};

}  // namespace mandelbrotter::test
