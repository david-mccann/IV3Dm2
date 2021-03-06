#include "src/duality/SceneParser.h"

#include "duality/Error.h"
#include "src/duality/DataCache.h"
#include "src/duality/DownloadProvider.h"
#include "src/duality/GeometryDataset.h"
#include "src/duality/GeometryNode.h"
#include "src/duality/PythonProvider.h"
#include "src/duality/VolumeDataset.h"
#include "src/duality/VolumeNode.h"

#include "mocca/base/Nullable.h"
#include "mocca/log/LogManager.h"

using namespace IVDA;

SceneParser::SceneParser(const JsonCpp::Value& root, std::shared_ptr<LazyRpcClient> rpc, std::shared_ptr<DataCache> dataCache)
    : m_root(root)
    , m_rpc(rpc)
    , m_dataCache(dataCache)
    , m_varIndex(0) {
    if (m_root.isMember("transforms")) {
        auto transformsJson = m_root["transforms"];
        for (auto it = transformsJson.begin(); it != transformsJson.end(); ++it) {
            m_transforms[it.key().asString()] = parseMatrix(*it);
        }
    }
}

SceneMetadata SceneParser::parseMetadata(const JsonCpp::Value& root) {
    std::string name = root["metadata"]["name"].asString();
    std::string description = root["metadata"]["description"].asString();
    return SceneMetadata(std::move(name), std::move(description));
}

mocca::Nullable<RenderParameters3D> SceneParser::initialParameters3D() {
    if (m_root.isMember("initialView") && m_root["initialView"].isMember("3d")) {
        auto node = m_root["initialView"]["3d"];
        Vec3f translation = parseVector3(node["translation"]);
        Mat4f rotation = parseTransform(node["rotation"]);
        return RenderParameters3D(translation, rotation);
    }
    return mocca::Nullable<RenderParameters3D>();
}

mocca::Nullable<RenderParameters2D> SceneParser::initialParameters2D() {
    if (m_root.isMember("initialView") && m_root["initialView"].isMember("2d")) {
        auto node = m_root["initialView"]["2d"];
        Vec2f translation = parseVector2(node["translation"]);
        float rotation = node["rotation"].asFloat();
        float zoom = node["zoom"].asFloat();
        CoordinateAxis axis = coordinateAxisMapper().getBySecond(node["axis"].asString());
        float depth = node["depth"].asFloat();
        SliderParameter sliderParameter(0, depth);
        return RenderParameters2D(translation, rotation, zoom, axis, sliderParameter);
    }
    return mocca::Nullable<RenderParameters2D>();
}

std::unique_ptr<Scene> SceneParser::parseScene() {
    auto metadata = parseMetadata(m_root);
    m_sceneName = metadata.name();

    std::string url;
    if (m_root.isMember("webViewURL")) {
        url = m_root["webViewURL"].asString();
    }

    auto sceneJson = m_root["scene"];
    std::vector<std::unique_ptr<SceneNode>> nodes;
    for (auto it = sceneJson.begin(); it != sceneJson.end(); ++it) {
        const auto& sceneNode = *it;
        m_varIndex = 0;
        auto type = sceneNode["type"].asString();
        if (type == "geometry") {
            nodes.push_back(parseGeometryNode(sceneNode));
        } else if (type == "volume") {
            nodes.push_back(parseVolumeNode(sceneNode));
        } else {
            throw Error("Invalid node type: " + type, __FILE__, __LINE__);
        }
    }
    return std::make_unique<Scene>(metadata, std::move(nodes), std::move(m_variables), url);
}

std::unique_ptr<SceneNode> SceneParser::parseGeometryNode(const JsonCpp::Value& node) {
    m_nodeName = node["name"].asString();
    Visibility visibility = parseVisibility(node);
    auto dataset = parseGeometryDataset(node["dataset"]);
    return std::make_unique<GeometryNode>(m_nodeName, visibility, std::move(dataset));
}

std::unique_ptr<GeometryDataset> SceneParser::parseGeometryDataset(const JsonCpp::Value& node) {
    auto provider = parseProvider(node["source"]);
    std::vector<Mat4f> transforms;
    if (node.isMember("transforms")) {
        for (const auto& transform : node["transforms"]) {
            transforms.push_back(parseTransform(transform));
        }
    }
    mocca::Nullable<Color> color;
    if (node.isMember("color")) {
        color = parseColor(node["color"]);
    }
    return std::make_unique<GeometryDataset>(std::move(provider), std::move(transforms), std::move(color));
}

std::unique_ptr<SceneNode> SceneParser::parseVolumeNode(const JsonCpp::Value& node) {
    m_nodeName = node["name"].asString();
    Visibility visibility = parseVisibility(node);
    auto dataset = parseVolumeDataset(node["dataset"]);
    std::unique_ptr<TransferFunction> tf;
    if (node.isMember("tf")) {
        tf = parseTransferFunction(node["tf"]);
    } else {
        tf = std::make_unique<TransferFunction>(nullptr);
    }
    return std::make_unique<VolumeNode>(m_nodeName, visibility, std::move(dataset), std::move(tf));
}

std::unique_ptr<VolumeDataset> SceneParser::parseVolumeDataset(const JsonCpp::Value& node) {
    auto provider = parseProvider(node["source"]);
    return std::make_unique<VolumeDataset>(std::move(provider));
}

std::unique_ptr<TransferFunction> SceneParser::parseTransferFunction(const JsonCpp::Value& node) {
    auto provider = parseProvider(node["source"]);
    return std::make_unique<TransferFunction>(std::move(provider));
}

std::unique_ptr<DataProvider> SceneParser::parseProvider(const JsonCpp::Value& node) {
    std::string type = node["type"].asString();
    m_variables[m_nodeName] = std::make_shared<Variables>();
    if (type == "download") {
        return parseDownload(node);
    } else if (type == "python") {
        return parsePython(node);
    }
    throw Error("Invalid data source type: " + type, __FILE__, __LINE__);
}

Visibility SceneParser::parseVisibility(const JsonCpp::Value& node) {
    Visibility visibility = Visibility::VisibleBoth;
    if (node.isMember("view2d") && node["view2d"] == false) {
        visibility = Visibility::Visible3D;
    }
    if (node.isMember("view3d") && node["view3d"] == false) {
        visibility = Visibility::Visible2D;
    }
    if (node.isMember("view2d") && node["view2d"] == false && node.isMember("view3d") && node["view3d"] == false) {
        visibility = Visibility::VisibleNone;
        LWARNING("Node is invisible in 2d-view and 3d-view");
    }
    return visibility;
}

std::unique_ptr<DataProvider> SceneParser::parseDownload(const JsonCpp::Value& node) {
    return std::make_unique<DownloadProvider>(m_sceneName, node["filename"].asString(), m_rpc, m_dataCache);
}

Vec3f SceneParser::parseVector3(const JsonCpp::Value& node) {
    if (node.size() != 3) {
        throw Error("Invalid 3D vector definition in JSON", __FILE__, __LINE__);
    }
    return Vec3f(node[0].asFloat(), node[1].asFloat(), node[2].asFloat());
}

IVDA::Vec2f SceneParser::parseVector2(const JsonCpp::Value& node) {
    if (node.size() != 2) {
        throw Error("Invalid 2D vector definition in JSON", __FILE__, __LINE__);
    }
    return Vec2f(node[0].asFloat(), node[1].asFloat());
}

Mat4f SceneParser::parseMatrix(const JsonCpp::Value& node) {
    if (node.size() != 16) {
        throw Error("Invalid matrix definition in JSON", __FILE__, __LINE__);
    }
    return Mat4f(node[0].asFloat(), node[1].asFloat(), node[2].asFloat(), node[3].asFloat(), node[4].asFloat(), node[5].asFloat(),
                 node[6].asFloat(), node[7].asFloat(), node[8].asFloat(), node[9].asFloat(), node[10].asFloat(), node[11].asFloat(),
                 node[12].asFloat(), node[13].asFloat(), node[14].asFloat(), node[15].asFloat());
}

Mat4f SceneParser::parseTransform(const JsonCpp::Value& node) {
    if (node.isArray()) {
        return parseMatrix(node);
    } else {
        std::string refName = node.asString();
        if (!m_transforms.count(refName)) {
            throw Error("Transform '" + refName + "' does not exist", __FILE__, __LINE__);
        }
        return m_transforms[refName];
    }
}

Color SceneParser::parseColor(const JsonCpp::Value& node) {
    if (!node.isArray() || !(node.size() == 4)) {
        throw Error("Invalid color", __FILE__, __LINE__);
    }
    return Color(node[0].asFloat(), node[1].asFloat(), node[2].asFloat(), node[3].asFloat());
}

std::unique_ptr<DataProvider> SceneParser::parsePython(const JsonCpp::Value& node) {
    std::string fileName = node["filename"].asString();
    parseParams(node["variables"]);
    return std::make_unique<PythonProvider>(m_sceneName, fileName, m_variables[m_nodeName], m_rpc, m_dataCache);
}

void SceneParser::parseParams(const JsonCpp::Value& node) {
    for (auto it = node.begin(); it != node.end(); ++it) {
        auto paramNode = *it;
        if (paramNode["type"] == "float") {
            parseFloatVariable(paramNode);
        } else if (paramNode["type"] == "enum") {
            parseEnumVariable(paramNode);
        }
        ++m_varIndex;
    }
}

void SceneParser::parseFloatVariable(const JsonCpp::Value& node) {
    std::string name = node["name"].asString();
    mocca::Nullable<std::string> label;
    if (node.isMember("label")) {
        label = node["label"].asString();
    }
    float lowerBound = node["lowerBound"].asFloat();
    float upperBound = node["upperBound"].asFloat();
    float stepSize = node["stepSize"].asFloat();
    float defaultValue = node["defaultValue"].asFloat();
    FloatVariableInfo info{m_varIndex, lowerBound, upperBound, stepSize};
    FloatVariable var{name, label, info, defaultValue};
    m_variables[m_nodeName]->floatVariables.push_back(var);
}

void SceneParser::parseEnumVariable(const JsonCpp::Value& node) {
    std::string name = node["name"].asString();
    mocca::Nullable<std::string> label;
    if (node.isMember("label")) {
        label = node["label"].asString();
    }
    auto valuesNode = node["values"];
    std::vector<std::string> values;
    for (auto valIt = valuesNode.begin(); valIt != valuesNode.end(); ++valIt) {
        values.push_back(valIt->asString());
    }
    std::string defaultValue = node["defaultValue"].asString();
    EnumVariableInfo info{m_varIndex, std::move(values)};
    EnumVariable var{name, label, info, defaultValue};
    m_variables[m_nodeName]->enumVariables.push_back(var);
}
