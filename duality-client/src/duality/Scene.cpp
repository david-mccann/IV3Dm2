#include "src/duality/Scene.h"

#include "IVDA/Vectors.h"
#include "src/duality/SceneParser.h"
#include "src/duality/View.h"

#include "mocca/base/ContainerTools.h"

using namespace IVDA;

Scene::Scene(SceneMetadata metadata, std::vector<std::unique_ptr<SceneNode>> nodes,
             std::map<std::string, std::shared_ptr<Variables>> variables)
    : m_metadata(std::move(metadata))
    , m_nodes(std::move(nodes))
    , m_variables(std::move(variables)) {}

SceneMetadata Scene::metadata() const {
    return m_metadata;
}

const std::vector<std::unique_ptr<SceneNode>>& Scene::nodes() const {
    return m_nodes;
}

void Scene::dispatch(NodeDispatcher& dispatcher, View view) const {
    for (auto& node : m_nodes) {
        if (node->isVisibleInView(view)) {
            node->accept(dispatcher);
        }
    }
}

void Scene::updateDatasets() {
    for (auto& node : m_nodes) {
        node->updateDataset();
    }
}

VariableMap Scene::variableMap(View view) {
    VariableMap result;
    for (auto& node : m_nodes) {
        if (node->isVisibleInView(view)) {
            auto name = node->name();
            for (const auto& var : m_variables[name]->floatVariables) {
                result[name].floatVariables.push_back(var);
            }
            for (const auto& var : m_variables[name]->enumVariables) {
                result[name].enumVariables.push_back(var);
            }
        }
    }
    return result;
}

void Scene::setVariable(const std::string& objectName, const std::string& variableName, float value) {
    assert(m_variables.count(objectName) != 0);
    auto& vars = m_variables[objectName]->floatVariables;
    auto it = mocca::findMemberEqual(begin(vars), end(vars), &FloatVariable::name, variableName);
    assert(it != end(vars));
    it->value = value;
}

void Scene::setVariable(const std::string& objectName, const std::string& variableName, const std::string& value) {
    assert(m_variables.count(objectName) != 0);
    auto& vars = m_variables[objectName]->enumVariables;
    auto it = mocca::findMemberEqual(begin(vars), end(vars), &EnumVariable::name, variableName);
    assert(it != end(vars));
    it->value = value;
}

BoundingBox duality::calculateSceneBoundingBox(const Scene& scene, View view) {
    BoundingBoxCalculator bbCalc;
    scene.dispatch(bbCalc, view);
    return bbCalc.boundingBox();
}
