#pragma once

#include <vector>
#include <optional>
#include <utility>
#include "tinygltf/tiny_gltf.h"
#include "Mesh.hpp"
#include "TextureManager.hpp"

namespace engine {

class Model
{
private:
	std::vector<unsigned int> _meshes{};

	std::optional<std::vector<unsigned int>> TinyProcessNode(tinygltf::Node const &node, tinygltf::Model const &model, std::vector<std::string> const &materials)
	{
		std::vector<unsigned int> allMeshes;

		if (node.mesh >= 0) {

			auto const &mesh = model.meshes[node.mesh];
			engine::Mesh current;

			for (auto const &primitive : mesh.primitives) {

				// Helper Lambda
				// Returns the count and the attributes
				// TODO: Return the number of components per element (vec{2,3,4}, scalar, ...)
				auto getVertex = [&] (std::string const &attributeName) -> std::optional<std::pair<size_t, const float *>> {

					std::optional<std::pair<size_t, const float *>> ret;

					auto const &attribute = primitive.attributes.find(attributeName);

					if (attribute == primitive.attributes.end()) {
						fmt::print("{}: {} attribute not found\n", mesh.name, attributeName);
						return ret;
					}

					auto const [name, indice] = *attribute;

					tinygltf::Accessor const &accessor = model.accessors[indice];
					tinygltf::BufferView const &bufferView = model.bufferViews[accessor.bufferView];
					tinygltf::Buffer const &buffer = model.buffers[bufferView.buffer];

					ret = std::pair<size_t, const float *>(accessor.count,
						reinterpret_cast<const float *>(&buffer.data[bufferView.byteOffset + accessor.byteOffset]));
					return ret;
				};

				// Get data for indices
				tinygltf::Accessor const &indAccessor = model.accessors[primitive.indices];
				tinygltf::BufferView const &indBufferView = model.bufferViews[indAccessor.bufferView];
				tinygltf::Buffer const &indBuffer = model.buffers[indBufferView.buffer];

				auto positions = getVertex("POSITION");
				auto normals   = getVertex("NORMAL");
				auto tangents  = getVertex("TANGENT");
				auto texcoords = getVertex("TEXCOORD_0");
				auto indices   = reinterpret_cast<const GLushort*>(&indBuffer.data[indBufferView.byteOffset + indAccessor.byteOffset]);

				// Build the mesh
				// --------------

				if (positions.has_value()) {
					for (size_t i = 0; i < positions.value().first; i++) {
						current.addPosition({
							positions.value().second[i * 3 + 0],
							positions.value().second[i * 3 + 1],
							positions.value().second[i * 3 + 2]
						});
					}
				}

				if (normals.has_value()) {
					for (size_t i = 0; i < normals.value().first; i++) {
						current.addNormal({
							normals.value().second[i * 3 + 0],
							normals.value().second[i * 3 + 1],
							normals.value().second[i * 3 + 2]
						});
					}
				}

				if (tangents.has_value()) {
					for (size_t i = 0; i < tangents.value().first; i++) {
						current.addTangent({
							tangents.value().second[i * 4 + 0],
							tangents.value().second[i * 4 + 1],
							tangents.value().second[i * 4 + 2],
							tangents.value().second[i * 4 + 3]
						});
					}
				}

				if (texcoords.has_value()) {
					for (size_t i = 0; i < texcoords.value().first; i++) {
						current.addUv({
							texcoords.value().second[i * 2 + 0],
							texcoords.value().second[i * 2 + 1]
						});
					}
				}

				// Component type for indices is SCALAR, that means the count must be divided by 3 when building
				// triangles so that we do not overrun the buffer
				for (size_t i = 0; i < indAccessor.count / 3; i++) {
					current.addTriangle({
						static_cast<GLuint>(indices[i * 3 + 0]),
						static_cast<GLuint>(indices[i * 3 + 1]),
						static_cast<GLuint>(indices[i * 3 + 2])
					});
				}

				current.build();
				if (primitive.material >= 0) {
					if (primitive.material > materials.size()) abort();
					current.SetPbrMaterial(materials[primitive.material]);
				}

				// Register this mesh to the engine and save its index
				allMeshes.push_back(engine::Engine::Instance().AddMesh(std::move(current)));
			}
		}

		for (auto const &child : node.children) {
			auto meshes = TinyProcessNode(model.nodes[child], model, materials);
			if (meshes.has_value()) {
				allMeshes.insert(allMeshes.end(), meshes.value().begin(), meshes.value().end());
			}
		}

		if (allMeshes.size() > 0) {
			return allMeshes;
		}

		return std::nullopt;
	}

	std::optional<std::vector<unsigned int>> TinyLoader(std::string const &path)
	{
		std::optional<std::vector<unsigned int>> meshes_ret;

		tinygltf::Model model;
		tinygltf::TinyGLTF loader;
		std::string err;
		std::string warn;

		bool ret = loader.LoadASCIIFromFile(&model, &err, &warn, path);

		if (!warn.empty()) {
			Logger::Warn("{}\n", warn);
		}
		if (!err.empty()) {
			Logger::Error("{}\n", err);
		}
		if (ret == false) {
			return meshes_ret;
		}

		std::string modelName;
		if (path.find_last_of("/") != std::string::npos) {
			if (path.find_last_of(".") != std::string::npos) {
				modelName = path.substr(path.find_last_of("/") + 1, path.find_last_of("."));
			}
			else {
				modelName = path.substr(path.find_last_of("/") + 1);
			}
		}
		else if (path.find_last_of(".") != std::string::npos) {
			modelName = path.substr(0, path.find_last_of("."));
		}
		else {
			modelName = path;
		}

		//fmt::print("Model name is \"{}\"\n", modelName);

		// Load Materials
		// --------------
		std::vector<std::string> pbrMaterials; // Save the materials' index to be able to set the material of the mesh later
		std::string directory = path.substr(0, path.find_last_of('/')) + "/";

		size_t currentMaterialIndex = 0;
		for (auto const &material : model.materials) {

			PbrMaterial pbrMaterial;

			pbrMaterial.Name = modelName + "-" + std::to_string(currentMaterialIndex);
			
			float r = static_cast<float>(material.pbrMetallicRoughness.baseColorFactor[0]);
			float g = static_cast<float>(material.pbrMetallicRoughness.baseColorFactor[1]);
			float b = static_cast<float>(material.pbrMetallicRoughness.baseColorFactor[2]);
			float a = static_cast<float>(material.pbrMetallicRoughness.baseColorFactor[3]);
			pbrMaterial.BaseColor = { r, g, b, a };

			auto const baseColorTexture = material.pbrMetallicRoughness.baseColorTexture.index;
			if (baseColorTexture >= 0) {
				pbrMaterial.Albedo = model.images[model.textures[baseColorTexture].source].uri;

				TextureManager::instance().createTexture(pbrMaterial.Albedo.value(), directory + pbrMaterial.Albedo.value(), {
					{ GL_TEXTURE_MAG_FILTER, GL_LINEAR },
					{ GL_TEXTURE_MIN_FILTER, GL_LINEAR },
					{ GL_TEXTURE_WRAP_S, GL_REPEAT },
					{ GL_TEXTURE_WRAP_T, GL_REPEAT },
				}, GL_TEXTURE_2D, true);
			}

			auto const normalTexture = material.normalTexture.index;
			if (normalTexture >= 0) {
				pbrMaterial.Normal = model.images[model.textures[normalTexture].source].uri;
			}

			// Blue channel : metalness
			// Green channel : roughness
			auto const metallicRoughnessTexture = material.pbrMetallicRoughness.metallicRoughnessTexture.index;
			if (metallicRoughnessTexture >= 0) {
				pbrMaterial.MetallicRoughness = model.images[model.textures[metallicRoughnessTexture].source].uri;
			}

			auto const metallicFactor = static_cast<float>(material.pbrMetallicRoughness.metallicFactor);
			pbrMaterial.MetallicFactor = metallicFactor;

			auto const roughnessFactor = static_cast<float>(material.pbrMetallicRoughness.roughnessFactor);
			pbrMaterial.RoughnessFactor = roughnessFactor;

			//fmt::print("{}\n", pbrMaterial);

			pbrMaterials.push_back(pbrMaterial.Name);
			engine::Engine::Instance().AddPbrMaterial(pbrMaterial);

			currentMaterialIndex++;
		}

		// Load texture images
		// -------------------

		for (auto const &t : model.textures) {

			auto const &image = model.images[t.source];

			std::string name = image.uri;
			std::string path = directory + image.uri;

			if (t.sampler >= 0) {
				auto const &sampler = model.samplers[t.sampler];

				TextureManager::instance().createTexture(name, path, {
					{ GL_TEXTURE_MAG_FILTER, sampler.magFilter },
					{ GL_TEXTURE_MIN_FILTER, sampler.minFilter },
					{ GL_TEXTURE_WRAP_S, sampler.wrapS },
					{ GL_TEXTURE_WRAP_T, sampler.wrapT },
				}, GL_TEXTURE_2D);
			}
			else {
				TextureManager::instance().createTexture(name, path, {
					{ GL_TEXTURE_MAG_FILTER, GL_LINEAR },
					{ GL_TEXTURE_MIN_FILTER, GL_LINEAR },
					{ GL_TEXTURE_WRAP_S, GL_REPEAT },
					{ GL_TEXTURE_WRAP_T, GL_REPEAT },
				}, GL_TEXTURE_2D);
			}
		}

		std::vector<unsigned int> meshes;

		for (auto const &scene : model.scenes) {
			for (auto const &node : scene.nodes) {
				auto const newMeshes = TinyProcessNode(model.nodes[node], model, pbrMaterials);
				if (newMeshes.has_value()) {
					meshes.insert(meshes.end(), newMeshes.value().begin(), newMeshes.value().end());
				}
			}
		}

		if (meshes.size() > 0) {
			meshes_ret = meshes;
		}

		return meshes_ret;
	}

public:
	Model()
	{
	};

	bool LoadFromGLTF(std::string const &path)
	{
		auto const meshes = TinyLoader(path);

		if (meshes.has_value()) {
			for (auto const mesh : meshes.value()) {
				_meshes.push_back(mesh);
			}
		}

		return meshes.has_value();
	}

	auto const &GetMeshes() const
	{
		return _meshes;
	}
};

}
