#pragma once

// The storage reader and wireless handshake must advertise the same ceiling.
namespace NotebookFormat {
inline constexpr int pageLinksVersion = 12;
inline constexpr int extendedDiagramShapesVersion = 13;
inline constexpr int umlDiagramShapesVersion = 14;
inline constexpr int maximumSupportedVersion = umlDiagramShapesVersion;
}
