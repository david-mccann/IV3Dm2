#pragma once

#include "src/duality/DatasetDispatcher.h"
#include "src/duality/View.h"
#include "src/duality/DataProvider.h"

class Dataset {
public:
    Dataset(std::unique_ptr<DataProvider> provider, Visibility visibility);

    virtual void accept(DatasetDispatcher& renderer) = 0;
    
    void fetch();
    std::vector<FloatVariableInfo> floatVariableInfos() const;
    std::vector<EnumVariableInfo> enumVariableInfos() const;
    void setVariable(const std::string& variableName, float value);
    void setVariable(const std::string& variableName, const std::string& value);

private:
    virtual void readData(const std::vector<uint8_t>& data) = 0;

private:
    std::unique_ptr<DataProvider> m_provider;
};