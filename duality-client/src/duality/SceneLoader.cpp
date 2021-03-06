#include "duality/SceneLoader.h"

#include "src/duality/Communication.h"
#include "src/duality/DataCache.h"
#include "src/duality/RenderDispatcher2D.h"
#include "src/duality/RenderDispatcher3D.h"
#include "src/duality/SceneController2DImpl.h"
#include "src/duality/SceneController3DImpl.h"
#include "src/duality/SceneParser.h"

#include "IVDA/Vectors.h"
#include "duality/Error.h"

#include "jsoncpp/json.h"

#include <algorithm>
#include <cassert>

using namespace IVDA;

class SceneLoaderImpl {
public:
    SceneLoaderImpl(const mocca::fs::Path& cacheDir, std::shared_ptr<Settings> settings);

    std::shared_ptr<Settings> settings();
    void updateEndpoint();
    void clearCache();

    std::vector<SceneMetadata> listMetadata() const;
    void loadScene(const std::string& name);
    void unloadScene();
    bool isSceneLoaded() const;
    SceneMetadata metadata() const;
    std::string webViewURL() const;

    std::shared_ptr<SceneController2D> sceneController2D(std::function<void(int, int, const std::string&)> updateDatasetCallback);
    std::shared_ptr<SceneController3D> sceneController3D(std::function<void(int, int, const std::string&)> updateDatasetCallback);

private:
    void createSceneController2D(std::function<void(int, int, const std::string&)> updateDatasetCallback);
    void createSceneController3D(std::function<void(int, int, const std::string&)> updateDatasetCallback);

private:
    std::shared_ptr<Settings> m_settings;
    std::shared_ptr<LazyRpcClient> m_rpc;
    std::shared_ptr<GLFrameBufferObject> m_resultFbo;
    std::shared_ptr<DataCache> m_dataCache;
    std::unique_ptr<Scene> m_scene;
    RenderParameters2D m_initialParameters2D;
    RenderParameters3D m_initialParameters3D;
    std::shared_ptr<SceneController2D> m_sceneController2D;
    std::shared_ptr<SceneController3D> m_sceneController3D;
};

SceneLoaderImpl::SceneLoaderImpl(const mocca::fs::Path& cacheDir, std::shared_ptr<Settings> settings)
    : m_settings(settings)
    , m_rpc(std::make_shared<LazyRpcClient>(mocca::net::Endpoint("tcp.prefixed", settings->serverIP(), settings->serverPort())))
    , m_resultFbo(std::make_shared<GLFrameBufferObject>())
    , m_dataCache(std::make_shared<DataCache>(cacheDir, m_settings)) {}

std::shared_ptr<Settings> SceneLoaderImpl::settings() {
    return m_settings;
}

void SceneLoaderImpl::updateEndpoint() {
    mocca::net::Endpoint ep("tcp.prefixed", m_settings->serverIP(), m_settings->serverPort());
    m_rpc = std::make_shared<LazyRpcClient>(ep);
}

void SceneLoaderImpl::clearCache() {
    m_dataCache->clear();
}

std::vector<SceneMetadata> SceneLoaderImpl::listMetadata() const {
    m_rpc->send("listScenes", JsonCpp::Value());
    auto root = m_rpc->receive().first;
    std::vector<SceneMetadata> result;
    for (auto it = root.begin(); it != root.end(); ++it) {
        result.push_back(SceneParser::parseMetadata(*it));
    }
    return result;
}

void SceneLoaderImpl::loadScene(const std::string& name) {
    m_rpc->send("listScenes", JsonCpp::Value());
    auto root = m_rpc->receive().first;
    m_dataCache->clearObservers();
    for (auto it = root.begin(); it != root.end(); ++it) {
        SceneParser parser(*it, m_rpc, m_dataCache);
        auto metadata = SceneParser::parseMetadata(*it);
        if (metadata.name() == name) {
            m_scene = parser.parseScene();
            RenderParameters3D default3D(Vec3f(0.0f, 0.0f, -3.0f), Mat4f());
            m_initialParameters3D = parser.initialParameters3D().getOr(default3D);
            m_initialParameters2D = parser.initialParameters2D().getOr(RenderParameters2D());
            m_sceneController2D = nullptr;
            m_sceneController3D = nullptr;
            return;
        }
    }
    throw Error("Scene named '" + name + "' does not exist", __FILE__, __LINE__);
}

void SceneLoaderImpl::unloadScene() {
    m_scene = nullptr;
    m_sceneController2D = nullptr;
    m_sceneController3D = nullptr;
}

bool SceneLoaderImpl::isSceneLoaded() const {
    return m_scene != nullptr;
}

SceneMetadata SceneLoaderImpl::metadata() const {
    if (m_scene != nullptr) {
        return m_scene->metadata();
    }
    throw Error("No scene loaded", __FILE__, __LINE__);
}

std::string SceneLoaderImpl::webViewURL() const {
    if (m_scene != nullptr) {
        return m_scene->webViewURL();
    }
    throw Error("No scene loaded", __FILE__, __LINE__);
}

std::shared_ptr<SceneController2D>
SceneLoaderImpl::sceneController2D(std::function<void(int, int, const std::string&)> updateDatasetCallback) {
    if (m_sceneController2D == nullptr) {
        createSceneController2D(updateDatasetCallback);
    }
    return m_sceneController2D;
}

std::shared_ptr<SceneController3D>
SceneLoaderImpl::sceneController3D(std::function<void(int, int, const std::string&)> updateDatasetCallback) {
    if (m_sceneController3D == nullptr) {
        createSceneController3D(updateDatasetCallback);
    }
    return m_sceneController3D;
}

void SceneLoaderImpl::createSceneController2D(std::function<void(int, int, const std::string&)> updateDatasetCallback) {
    auto impl = std::make_unique<SceneController2DImpl>(*m_scene, m_initialParameters2D, updateDatasetCallback, m_resultFbo, m_settings);
    m_sceneController2D = std::make_shared<SceneController2D>(std::move(impl));
}

void SceneLoaderImpl::createSceneController3D(std::function<void(int, int, const std::string&)> updateDatasetCallback) {
    auto impl = std::make_unique<SceneController3DImpl>(*m_scene, m_initialParameters3D, updateDatasetCallback, m_resultFbo, m_settings);
    m_sceneController3D = std::make_shared<SceneController3D>(std::move(impl));
}

SceneLoader::SceneLoader(const mocca::fs::Path& cacheDir, std::shared_ptr<Settings> settings)
    : m_impl(std::make_unique<SceneLoaderImpl>(cacheDir, settings)) {}

SceneLoader::~SceneLoader() = default;

std::shared_ptr<Settings> SceneLoader::settings() {
    return m_impl->settings();
}

void SceneLoader::updateEndpoint() {
    m_impl->updateEndpoint();
}

void SceneLoader::clearCache() {
    m_impl->clearCache();
}

std::vector<SceneMetadata> SceneLoader::listMetadata() const {
    return m_impl->listMetadata();
}

void SceneLoader::loadScene(const std::string& name) {
    m_impl->loadScene(name);
}

void SceneLoader::unloadScene() {
    m_impl->unloadScene();
}

bool SceneLoader::isSceneLoaded() const {
    return m_impl->isSceneLoaded();
}

SceneMetadata SceneLoader::metadata() const {
    return m_impl->metadata();
}

std::string SceneLoader::webViewURL() const {
    return m_impl->webViewURL();
}

std::shared_ptr<SceneController2D> SceneLoader::sceneController2D(std::function<void(int, int, const std::string&)> updateDatasetCallback) {
    return m_impl->sceneController2D(updateDatasetCallback);
}

std::shared_ptr<SceneController3D> SceneLoader::sceneController3D(std::function<void(int, int, const std::string&)> updateDatasetCallback) {
    return m_impl->sceneController3D(updateDatasetCallback);
}
