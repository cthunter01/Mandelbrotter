#pragma once

#include <unistd.h>

#include <atomic>
#include <filesystem>
#include <string>

namespace mandelbrotter::test
{

/// A fresh directory under the system temp dir, removed on destruction.
class TempDir
{
public:
    TempDir()
    {
        static std::atomic<int> s_counter{0};
        const auto              id = std::to_string(::getpid()) + "-" + std::to_string(s_counter++);
        m_path = std::filesystem::temp_directory_path() / ("mandelbrotter-test-" + id);
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
