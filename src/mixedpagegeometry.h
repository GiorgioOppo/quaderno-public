#pragma once

#include <QImage>
#include <QVariantMap>

namespace MixedPageGeometry {

// Finds editable diagram shapes and directed connectors in a reviewed image
// region. Both diagram and node placement use full image coordinates in [0, 1].
// Returns an empty map if there is no reliable shape proposal.
QVariantMap diagram(const QImage &image);

}
