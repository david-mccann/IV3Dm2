#pragma once

#include "src/duality/G3D.h"

#include "src/duality/Dataset.h"

#include "IVDA/GLMatrix.h"
#include "IVDA/Vectors.h"

class DataProvider;
class DatasetDispatcher;

class GeometryDataset : public Dataset {
public:
    GeometryDataset(std::unique_ptr<DataProvider> provider, Visibility visibility = Visibility::VisibleBoth, std::vector<IVDA::Mat4f> transforms = {});

    void accept(DatasetDispatcher& renderer) override;

    const G3D::GeometryInfo* geometryInfo() const noexcept;
    const std::vector<uint32_t>& getIndices() const noexcept;
    const float* getPositions() const noexcept;
    const float* getNormals() const noexcept;
    const float* getTangents() const noexcept;
    const float* getColors() const noexcept;
    const float* getTexCoords() const noexcept;
    const float* getAlphas() const noexcept;

private:
    void readData(const std::vector<uint8_t>& data) override;
    void assignShortcutPointers();
    void applyTransform(const IVDA::Mat4f& matrix);

private:
    std::unique_ptr<G3D::GeometrySoA> m_geometry;
    std::vector<IVDA::Mat4f> m_transforms;

    // vertex attributes
    float* m_positions;
    float* m_normals;
    float* m_tangents;
    float* m_colors;
    float* m_texcoords;
    float* m_alphas;
};