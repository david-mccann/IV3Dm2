#pragma once

#include "IVDA/Vectors.h"
#include "duality/ScreenInfo.h"
#include "duality/Settings.h"
#include "src/duality/GeometryRenderer2D.h"
#include "src/duality/MVP2D.h"
#include "src/duality/SliderParameter.h"
#include "src/duality/VolumeRenderer2D.h"

class GLFrameBufferObject;
class GeometryRenderer2D;
class GeometryNode;
class VolumeNode;
class SceneNode;

class RenderDispatcher2D {
public:
    RenderDispatcher2D(std::shared_ptr<GLFrameBufferObject> fbo, const RenderParameters2D& initialParameters,
                       std::shared_ptr<Settings> settings);
    ~RenderDispatcher2D();

    void render(const std::vector<std::unique_ptr<SceneNode>>& nodes);

    void dispatch(GeometryNode& node);
    void dispatch(VolumeNode& node);

    void updateScreenInfo(const ScreenInfo& screenInfo);
    void updateBoundingBox(const BoundingBox& boundingBox);
    void setRedrawRequired();

    void addTranslation(const IVDA::Vec2f& translation);
    void addRotation(const float rotationAngle);
    void addZoom(const float zoom);
    void setSliderParameter(const SliderParameter& sliderParameter);
    void toggleAxis();

    CoordinateAxis currentAxis() const;

private:
    void startDraw();
    void finishDraw();

private:
    std::shared_ptr<GLFrameBufferObject> m_fbo;
    std::unique_ptr<GeometryRenderer2D> m_geoRenderer;
    std::unique_ptr<VolumeRenderer2D> m_volRenderer;
    RenderParameters2D m_parameters;
    std::shared_ptr<Settings> m_settings;
    ScreenInfo m_screenInfo;
    BoundingBox m_boundingBox;
    MVP2D m_mvp;
    bool m_redraw;
};