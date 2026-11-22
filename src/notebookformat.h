#pragma once

// The storage reader and wireless handshake must advertise the same ceiling.
namespace NotebookFormat {
inline constexpr int pageLinksVersion = 12;
inline constexpr int maximumSupportedVersion = pageLinksVersion;
}
