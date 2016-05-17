#pragma once

#include "IVDA/Vectors.h"
#include "Scene/DatasetDispatcher.h"

#include <memory>
#include <vector>

class Dataset {
public:
    Dataset(std::vector<IVDA::Mat4f> transforms = {});
    virtual ~Dataset();

    void load(std::shared_ptr<std::vector<uint8_t>> data);
    virtual void accept(DatasetDispatcher& dispatcher) = 0;

    std::vector<IVDA::Mat4f> transforms() const;

private:
    virtual void read(std::shared_ptr<std::vector<uint8_t>> data) = 0;
    virtual void applyTransform(const IVDA::Mat4f& matrix) = 0;

private:
    std::vector<IVDA::Mat4f> m_transforms;
};