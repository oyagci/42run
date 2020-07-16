#pragma once

#include <memory>
#include "lazy.hpp"
#include "ui/UI.hpp"
#include "ecs/ecs.hpp"
#include "EngineObject.hpp"
#include "Mesh.hpp"
#include "Batch.hpp"
#include <unordered_map>
#include "TextureManager.hpp"
#include <fmt/format.h>
#include "Logger.hpp"
#include "Action.hpp"
#include "Scene.hpp"

namespace ecs
{
	class SystemManager;
	class EntityManager;
}

class ImguiSystem;

struct ModelComponent;

namespace engine
{

class Model;

class Engine
{
private:
	std::unique_ptr<lazy::graphics::Display> _display;
	std::unique_ptr<lazy::graphics::Camera> _camera;
	std::unique_ptr<UI> _ui;

	ecs::ECSEngine::Instance *_ecs;

	std::vector<std::unique_ptr<EngineObject>> _engineObjects;

	std::unordered_map<unsigned int, std::unique_ptr<engine::Model>> _models;
	std::unordered_map<unsigned int, std::unique_ptr<IDrawable>> _meshes;
	std::unordered_map<unsigned int, std::unique_ptr<Batch>> _batches;

	using MaterialContainer = std::pair<unsigned int, Material>;

	std::unordered_map<std::string, MaterialContainer> _materials;
	std::unordered_map<std::string, PbrMaterial> _pbrMaterials;

	static unsigned int _nextId;
	static unsigned int _nextMaterialId;

	enum class PlayState {
		Stopped,
		Playing,
		Paused,
	};
	PlayState _isPlaying;

	std::unique_ptr<Scene> _Scene;

private:
	Engine();
	~Engine();

	Callback<size_t> _selectItem;

public:
	/// Observer for building the lighting of the level
	/// Call this to rebuild the lighting of the scene
	Action<> OnBuildLighting;

	/// Select an item in the editor
	Action<size_t> OnSelectItem;

	/// Called when the level starts
	Action<> OnStartPlaying;
	/// Called when the level stops
	Action<> OnStopPlaying;

	// Editor
	void OnRebuildModel(ModelComponent &model);

public:
	Engine(Engine const &) = delete;
	void operator=(Engine const &) = delete;

	static Engine &Instance()
	{
		static Engine engine;
		return engine;
	}

	int Run();
	void Update();
	void UpdateObjects();
	void UpdateSubobjects(std::vector<EngineObject*> subobjects);

	lazy::graphics::Display *GetDisplay() { return _display.get(); }

	template <typename T, typename ... ArgTypes>
	[[nodiscard]] T *CreateEngineObject(ArgTypes... args)
	{
		static_assert(std::is_base_of<EngineObject, T>::value && !std::is_same<EngineObject, T>::value,
			"T must be derived from EngineObject");

		auto object = std::make_unique<T>(&_ecs, std::forward<ArgTypes>(args)...);
		auto ret = object.get();

		_engineObjects.push_back(std::move(object));

		return ret;
	}

	unsigned int AddMesh(Mesh mesh)
	{
		auto id = _nextId;
		auto to_ptr = std::make_unique<Mesh>(std::move(mesh));
		_meshes[id] = std::move(to_ptr);

		_nextId++;

		return id;
	}

	IDrawable *GetMesh(unsigned int id)
	{
		auto mesh = _meshes.find(id);

		if (mesh != _meshes.end()) {
			return mesh->second.get();
		}

		return nullptr;
	}

	unsigned int AddBatch(std::unique_ptr<Batch> batch)
	{
		_meshes[_nextId] = std::move(batch);

		return _nextId++;
	}

	void AddPbrMaterial(PbrMaterial material)
	{
		if (_pbrMaterials.find(material.Name) != _pbrMaterials.end()) {
			Logger::Warn("Material {} already exists\n", material.Name);
		}

		_pbrMaterials[material.Name] = material;
	}

	void BindPbrMaterial(std::string const &name)
	{
		if (name.size() == 0) {
			Logger::Warn("Material name not given\n");
			return ;
		}
		if (_pbrMaterials.find(name) == _pbrMaterials.end()) {
			Logger::Warn("Material not found ({})\n", name);
			return ;
		}

		auto const &material = _pbrMaterials[name];

		if (material.Albedo.has_value()) {
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, TextureManager::instance().get(material.Albedo.value()));
		}
		if (material.Normal.has_value()) {
			glActiveTexture(GL_TEXTURE2);
			glBindTexture(GL_TEXTURE_2D, TextureManager::instance().get(material.Normal.value()));
		}
	}

	void UnbindPbrMaterial()
	{
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, 0);

		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, 0);

		glActiveTexture(GL_TEXTURE2);
		glBindTexture(GL_TEXTURE_2D, 0);
	}

	std::optional<PbrMaterial const *> GetPbrMaterial(std::string const &name)
	{
		if (_pbrMaterials.find(name) != _pbrMaterials.end()) {
			return std::make_optional(&_pbrMaterials[name]);
		}
		return std::nullopt;
	}

	void AddMaterial(std::string const &name, Material mat)
	{
		if (name.size() == 0) {
			Logger::Warn("Warning: Creating a material without a name is illegal.\n");
			return ;
		}

		mat.name = name;
		_materials[name] = MaterialContainer(_nextMaterialId++, mat);
	}

	void BindMaterial(std::string const &name)
	{
		if (name.size() == 0) {
			Logger::Warn("Material name not given\n");
			return ;
		}
		if (_materials.find(name) == _materials.end()) {
			Logger::Warn("Material not found ({})\n", name);
			return ;
		}

		auto const &material = _materials[name].second;

		if (material.diffuse > 0) {
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, material.diffuse);
		}
		if (material.specular > 0) {
			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, material.specular);
		}
		if (material.normal > 0) {
			glActiveTexture(GL_TEXTURE2);
			glBindTexture(GL_TEXTURE_2D, material.normal);
		}
	}

	void UnbindMaterial()
	{
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, 0);

		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, 0);

		glActiveTexture(GL_TEXTURE2);
		glBindTexture(GL_TEXTURE_2D, 0);
	}

	unsigned int GetMaterialId(std::string const &name)
	{
		return _materials[name].first;
	}

	std::vector<std::string> GetMaterialList() const
	{
		std::vector<std::string> materials;

		materials.resize(_materials.size());
		std::transform(_materials.begin(), _materials.end(), materials.begin(),
			[] (auto mat) { return mat.first; });
		return materials;
	}

	void Close()
	{
		glfwSetWindowShouldClose(_display->getWindow(), GLFW_TRUE);
	}

	unsigned int RegisterModel(engine::Model model);
	std::optional<engine::Model const *> GetModel(unsigned int id) const;
	void RemoveModel(unsigned int id);

	void RebuildModel(ModelComponent &model);

	Scene &GetCurrentScene() { return *_Scene; }
};

}
