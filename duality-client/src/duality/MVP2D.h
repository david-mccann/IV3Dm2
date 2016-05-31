#pragma once

#include "IVDA/GLMatrix.h"
#include "duality/ScreenInfo.h"
#include "src/duality/BoundingBox.h"
#include "src/duality/RenderParameters2D.h"

class MVP2D {
public:
    MVP2D(const ScreenInfo& screenInfo, const BoundingBox& boundingBox);

    GLMatrix calculate(const RenderParameters2D& parameters) const;

    void updateScreenInfo(const ScreenInfo& screenInfo);
    void updateBoundingBox(const BoundingBox& boundingBox);

private:
    static IVDA::Mat3i getSliceViewMatrix(const CoordinateAxis axis);
    static IVDA::Mat3i getSliceViewerBasis(const Axis viewerUp, const Axis viewerFace);

private:
    float m_screenAspect;
    BoundingBox m_boundingBox;
};