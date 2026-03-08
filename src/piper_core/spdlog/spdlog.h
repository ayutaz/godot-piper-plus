// spdlog compatibility shim for GDExtension
// Provides no-op implementations to minimize code changes from piper-plus
#pragma once

namespace spdlog {

namespace level {
enum level_enum { debug, info, warn, err };
} // namespace level

inline bool should_log(level::level_enum) { return false; }

template <typename... Args>
inline void debug(const char *, Args &&...) {}

template <typename... Args>
inline void info(const char *, Args &&...) {}

template <typename... Args>
inline void warn(const char *, Args &&...) {}

template <typename... Args>
inline void error(const char *, Args &&...) {}

} // namespace spdlog
