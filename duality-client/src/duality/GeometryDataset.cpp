#include "src/duality/GeometryDataset.h"

#include "src/duality/AbstractIO.h"
#include "src/duality/NodeDispatcher.h"

#include "IVDA/Vectors.h"
#include "duality/Error.h"

using namespace IVDA;

GeometryDataset::GeometryDataset(std::unique_ptr<DataProvider> provider, std::vector<Mat4f> transforms)
    : Dataset(std::move(provider))
    , m_transforms(transforms)
    , m_geometry(nullptr) {}

const std::vector<uint32_t>& GeometryDataset::indicesOpaque() const {
    return m_indicesOpaque;
}

std::vector<uint32_t> GeometryDataset::indicesTransparentSorted(const Mat4f& mvp) const {
    Vec4f eyePos(0, 0, 0, 1);
    eyePos = eyePos * mvp.inverse();
    Vec3f transformedEyePos = eyePos.dehomo();

    auto sorter = [&](size_t index1, size_t index2) {
        float dist1 = (m_centroids[index1] - transformedEyePos).sqLength();
        float dist2 = (m_centroids[index2] - transformedEyePos).sqLength();
        return dist1 > dist2;
    };

    std::vector<int32_t> permutation(m_centroids.size());
    std::iota(begin(permutation), end(permutation), 0);
    std::sort(begin(permutation), end(permutation), sorter);

    return permuteIndicesTransparent(permutation);
}

std::vector<uint32_t> GeometryDataset::permuteIndicesTransparent(const std::vector<int32_t>& permutation) const {
    std::vector<uint32_t> result(m_indicesTransparent.size());
    if (m_geometry->info.primitiveType == G3D::Point) {
        applyPermutation<1>(permutation, m_indicesTransparent, result);
    }
    else if (m_geometry->info.primitiveType == G3D::Line) {
        applyPermutation<2>(permutation, m_indicesTransparent, result);
    }
    else if (m_geometry->info.primitiveType == G3D::Triangle) {
        applyPermutation<3>(permutation, m_indicesTransparent, result);
    }
    return result;
}

void GeometryDataset::readData(const std::vector<uint8_t>& data) {
    ReaderFromMemory reader(reinterpret_cast<const char*>(data.data()), data.size());
    m_geometry = std::make_unique<G3D::GeometrySoA>();
    G3D::readSoA(reader, *m_geometry);
    for (const auto& transform : m_transforms) {
        G3D::applyTransform(*m_geometry, transform);
    }
    presortIndices();
    computeCentroids();
}

void GeometryDataset::presortIndices() {
    m_indicesOpaque.clear();
    m_indicesTransparent.clear();

    if (m_geometry->info.primitiveType == G3D::Point) {
        presortIndices<1>();
    } else if (m_geometry->info.primitiveType == G3D::Line) {
        presortIndices<2>();
    } else if (m_geometry->info.primitiveType == G3D::Triangle) {
        presortIndices<3>();
    }
}

void GeometryDataset::computeCentroids() {
    m_centroids.clear();
    if (m_geometry->info.primitiveType == G3D::Point) {
        computeCentroids<1>();
    } else if (m_geometry->info.primitiveType == G3D::Line) {
        computeCentroids<2>();
    } else if (m_geometry->info.primitiveType == G3D::Triangle) {
        computeCentroids<3>();
    }
}

const G3D::GeometrySoA& GeometryDataset::geometry() const {
    return *m_geometry;
}