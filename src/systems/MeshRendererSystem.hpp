#pragma once

#include "lazy.hpp"
#include "TextureManager.hpp"
#include "ecs/System.hpp"
#include "components/PlayerCameraComponent.hpp"
#include "components/MeshComponent.hpp"
#include "components/TransformComponent.hpp"
#include "components/SkyboxComponent.hpp"
#include "components/PointLightComponent.hpp"
#include "components/DirectionalLightComponent.hpp"
#include "components/ModelComponent.hpp"
#include "Engine.hpp"
#include "Framebuffer.hpp"
#include "ShaderManager.hpp"
#include <fmt/format.h>
#include <glm/gtx/projection.hpp>
#include "Engine.hpp"
#include <random>
#include "GBuffer.hpp"
#include "TextureAutoBind.hpp"

class MeshRendererSystem : public ecs::ComponentSystem
{
private:
	lazy::graphics::Shader _billboard;
	lazy::graphics::Shader _light;
	lazy::graphics::Shader _shadow;
	engine::Mesh _quad;

	GLuint _ssaoFb;
	GLuint _ssaoBlurFb;
	GLuint _ssaoColorBuf;
	GLuint _ssaoNoiseTex;
	GLuint _ssaoBlurTex;
	lazy::graphics::Shader _ssaoShader;
	lazy::graphics::Shader _ssaoBlurShader;

	GBuffer _gBuffer;

	GLuint _depthmapFb;
	GLuint _depthCubemap;
	glm::mat4 _shadowProjection;

	Callback<> buildShadowMap;

	static constexpr unsigned int ShadowWidth  = 2048;
	static constexpr unsigned int ShadowHeight = 2048;

	void InitDepthCubemap()
	{
		_shadow.addVertexShader("shaders/shadow.vs.glsl")
			.addFragmentShader("shaders/shadow.fs.glsl")
			.addGeometryShader("shaders/shadow.gs.glsl")
			.link();
		assert(_shadow.isValid());

		_shadow.bind();

		float aspect = static_cast<float>(ShadowWidth) / static_cast<float>(ShadowHeight);
		float near = 1.0f;
		float far = 1000.0f;
		_shadowProjection = glm::perspective(glm::radians(90.0f), aspect, near, far);

		glGenFramebuffers(1, &_depthmapFb);
		glBindFramebuffer(GL_FRAMEBUFFER, _depthmapFb);

		glGenTextures(1, &_depthCubemap);
		glBindTexture(GL_TEXTURE_CUBE_MAP, _depthCubemap);
		for (size_t face = 0; face < 6; face++) {
			glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, 0, GL_DEPTH_COMPONENT, ShadowWidth, ShadowHeight, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
		}
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

		glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, _depthCubemap, 0);
		glDrawBuffer(GL_NONE);
		glReadBuffer(GL_NONE);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

	void BindShadowMap()
	{
		glViewport(0, 0, ShadowWidth, ShadowHeight);
		glBindFramebuffer(GL_FRAMEBUFFER, _depthmapFb);
	}

	void UnbindShadowMap()
	{
		auto display = engine::Engine::Instance().GetDisplay();
		auto [ width, height ] = std::tuple(display->getWidth(), display->getHeight());

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glViewport(0, 0, width, height);
	}

	void InitQuad()
	{
		std::array<glm::vec3, 4> pos = {
			glm::vec3(-1.0f, -1.0f, 0.0f),
			glm::vec3( 1.0f, -1.0f, 0.0f),
			glm::vec3( 1.0f,  1.0f, 0.0f),
			glm::vec3(-1.0f,  1.0f, 0.0f),
		};
		std::array<glm::vec2, 4> tex = {
			glm::vec2(0.0f, 0.0f),
			glm::vec2(1.0f, 0.0f),
			glm::vec2(1.0f, 1.0f),
			glm::vec2(0.0f, 1.0f),
		};

		for (auto p : pos) {
			_quad.addPosition(p);
		}
		for (auto t : tex) {
			_quad.addUv(t);
		}
		_quad.addTriangle(glm::uvec3(0, 1, 2));
		_quad.addTriangle(glm::uvec3(0, 2, 3));
		_quad.build();

	}

	void InitFramebuffer()
	{
		InitQuad();
	}

	void InitBillboard()
	{
		_billboard.addVertexShader("shaders/billboard.vs.glsl")
			.addFragmentShader("shaders/billboard.fs.glsl")
			.link();
	}

	void InitSSAO()
	{
		auto display = engine::Engine::Instance().GetDisplay();
		auto [ width, height ] = std::tuple(display->getWidth(), display->getHeight());

		_ssaoShader.addVertexShader("shaders/ssao.vs.glsl")
			.addFragmentShader("shaders/ssao.fs.glsl")
			.link();
		assert(_ssaoShader.isValid());

		glGenFramebuffers(1, &_ssaoFb);
		glBindFramebuffer(GL_FRAMEBUFFER, _ssaoFb);

		glGenTextures(1, &_ssaoColorBuf);
		glBindTexture(GL_TEXTURE_2D, _ssaoColorBuf);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, width, height, 0, GL_RED, GL_FLOAT, nullptr);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, _ssaoColorBuf, 0);
		assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);

		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		GenSSAOKernel();

		_ssaoShader.bind();
		_ssaoShader.setUniform1i("gPosition", 0);
		_ssaoShader.setUniform1i("gNormal", 1);
		_ssaoShader.setUniform1i("texNoise", 2);
		_ssaoShader.unbind();

		_ssaoBlurShader.addVertexShader("shaders/ssao.vs.glsl")
			.addFragmentShader("shaders/ssaoblur.fs.glsl")
			.link();
		_ssaoBlurShader.bind();
		_ssaoBlurShader.setUniform1i("ssaoInput", 0);

		glGenFramebuffers(1, &_ssaoBlurFb);
		glBindFramebuffer(GL_FRAMEBUFFER, _ssaoBlurFb);

		glGenTextures(1, &_ssaoBlurFb);
		glBindTexture(GL_TEXTURE_2D, _ssaoBlurFb);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, width, height, 0, GL_RED, GL_FLOAT, nullptr);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, _ssaoColorBuf, 0);
	}

	void GenSSAOKernel()
	{
		std::uniform_real_distribution<float> randomFloats(0.0f, 1.0f);
		std::default_random_engine generator;
		std::vector<glm::vec3> ssaoKernel;

		auto lerp = [] (float a, float b, float f) -> float {
			return a + f * (b - a);
		};

		for (size_t i = 0; i < 64; i++) {
			glm::vec3 sample(randomFloats(generator) * 2.0f - 1.0f,
							 randomFloats(generator) * 2.0f - 1.0f,
							 randomFloats(generator));
			sample = glm::normalize(sample);
			sample *= randomFloats(generator);

			// Make distribution closer to origin
			float scale = static_cast<float>(i) / 64.0f;
			scale = lerp(0.1f, 1.0f, scale * scale);
			sample *= scale;

			ssaoKernel.push_back(sample);
		}

		std::array<glm::vec3, 64> ssaoNoise;
		for (size_t i = 0; i < 64; i++) {
			glm::vec3 noise(randomFloats(generator) * 2.0f - 1.0f,
							randomFloats(generator) * 2.0f - 1.0f,
							0.0f);
			ssaoNoise[i] = noise;
		}

		glGenTextures(1, &_ssaoNoiseTex);
		glBindTexture(GL_TEXTURE_2D, _ssaoNoiseTex);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, 4, 4, 0, GL_RGB, GL_FLOAT, &ssaoNoise[0]);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glBindTexture(GL_TEXTURE_2D, 0);

		_ssaoShader.bind();
//		GLuint sampleLoc = _ssaoShader.getUniformLocation("samples");
		for (size_t i = 0; i < 64; i++) {
			_ssaoShader.setUniform3f("samples[" + std::to_string(i) + "]", ssaoKernel[i]);
		}
//		glUniform3fv(sampleLoc, 64, &ssaoKernel[0][0]);
		_ssaoShader.unbind();
	}

	void RenderSSAO(PlayerCameraComponent const &camera)
	{
		glBindFramebuffer(GL_FRAMEBUFFER, _ssaoFb);
		_ssaoShader.bind();
		_ssaoShader.setUniform1i("gPosition", 0);
		_ssaoShader.setUniform1i("gNormal", 1);
		_ssaoShader.setUniform1i("texNoise", 2);

		glClear(GL_COLOR_BUFFER_BIT);
		glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, _gBuffer.GetPositionTex());
		glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, _gBuffer.GetNormalTex());
		glActiveTexture(GL_TEXTURE2);
			glBindTexture(GL_TEXTURE_2D, _ssaoNoiseTex);

		_ssaoShader.setUniform4x4f("projectionMatrix", camera.projection);
		_ssaoShader.setUniform4x4f("viewMatrix", camera.view);
		_quad.Draw();

		glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, 0);
		glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, 0);
		glActiveTexture(GL_TEXTURE2);
			glBindTexture(GL_TEXTURE_2D, 0);
		_ssaoShader.unbind();

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
//		glBindFramebuffer(GL_FRAMEBUFFER, _ssaoBlurFb);
//
//		_ssaoBlurShader.bind();
//		_ssaoBlurShader.setUniform1i("ssaoInput", 0);
//		glActiveTexture(GL_TEXTURE0);
//		glBindTexture(GL_TEXTURE_2D, _ssaoColorBuf);
//		_quad.Draw();
//		glBindTexture(GL_TEXTURE_2D, 0);
//		_ssaoShader.unbind();

//		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

	void RenderShadowMeshes()
	{
		auto entities = GetEntities<ModelComponent, TransformComponent>();

		for (auto const entity: entities) {

			auto [ model, transform ] = entity->GetAll();

			for (auto const meshId : model.Meshes) {

				auto const *mesh = engine::Engine::Instance().GetMesh(meshId);

				glm::mat4 model(1.0f);
				model = glm::translate(model, transform.position);
				model = glm::scale(model, transform.scale);
				_shadow.setUniform4x4f("modelMatrix", model);

				mesh->Draw();
			}

		}
	}

	void RenderMeshes(PlayerCameraComponent const &camera, TransformComponent const &playerTransform)
	{
		auto models = GetEntities<ModelComponent, TransformComponent>();

		for (auto const &entity : models) {

			auto const [ model, transform ] = entity->GetAll();

			for (auto const meshId : model.Meshes) {

				auto const mesh = engine::Engine::Instance().GetMesh(meshId);
				auto const shaderId = model.Shader;

				auto shaderOpt = ShaderManager::instance().Get(shaderId);

				if (!shaderOpt.has_value()) {
					Logger::Warn("Shader {} does not exist\n", shaderId);
					continue ;
				}

				auto &shader = shaderOpt.value();

				shader->bind();
				shader->setUniform4x4f("viewProjectionMatrix", camera.viewProjection);
				shader->setUniform4x4f("viewMatrix", camera.view);
				shader->setUniform4x4f("projectionMatrix", camera.projection);
				shader->setUniform3f("viewPos", playerTransform.position);

				UpdateLight(*shader);

				glm::mat4 modelMatrix(1.0f);
				modelMatrix = glm::translate(modelMatrix, transform.position);
				modelMatrix = glm::scale(modelMatrix, transform.scale);
				shader->setUniform4x4f("modelMatrix", modelMatrix);

				std::vector<TextureAutoBind> textureBindings;

				// Bind each texture
				auto meshCast = dynamic_cast<engine::Mesh*>(mesh);
				if (meshCast != nullptr) {
					if (meshCast->GetPbrMaterial().has_value()) {

						auto material = engine::Engine::Instance().GetPbrMaterial(
							meshCast->GetPbrMaterial().value());

						if (material.has_value()) {
							auto m = material.value();

							shader->setUniform4f("material.baseColor", m->BaseColor);
							shader->setUniform1f("material.metallicFactor", m->MetallicFactor);
							shader->setUniform1f("material.roughnessFactor", m->RoughnessFactor);

							if (m->Albedo.has_value()) {
								auto albedo = m->Albedo.value();
								auto texture = TextureManager::instance().get(albedo);

								shader->setUniform1i("material.hasAlbedo", 1);
								textureBindings.push_back(TextureAutoBind(GL_TEXTURE0, GL_TEXTURE_2D, texture));
							}
							else {
								shader->setUniform1i("material.hasAlbedo", 0);
							}

							if (m->MetallicRoughness.has_value()) {
								auto metallicRoughness = m->MetallicRoughness.value();
								auto texture = TextureManager::instance().get(metallicRoughness);

								shader->setUniform1i("material.hasMetallicRoughness", 1);
								textureBindings.push_back(TextureAutoBind(GL_TEXTURE1, GL_TEXTURE_2D, texture));
							}
							else {
								shader->setUniform1i("material.hasMetallicRoughness", 0);
							}

							if (m->Normal.has_value()) {
								auto normal = m->Normal.value();
								auto texture = TextureManager::instance().get(normal);

								textureBindings.push_back(TextureAutoBind(GL_TEXTURE2, GL_TEXTURE_2D, texture));
							}
							else {
								auto const defaultNormal = TextureManager::instance().get("default_normal");
								textureBindings.push_back(TextureAutoBind(GL_TEXTURE2, GL_TEXTURE_2D, defaultNormal));
							}
						}
					}
				}

				mesh->Draw();
				shader->unbind();
			}

		}
	}

	void RenderSkybox(PlayerCameraComponent const &camera)
	{
		auto skybox = GetEntities<MeshComponent, SkyboxComponent>();

		if (skybox.size() == 0) { return ; }

		auto [ meshComponent, _ ] = skybox[0]->GetAll();
		auto mesh = engine::Engine::Instance().GetMesh(meshComponent.Id);

		auto shader = ShaderManager::instance().Get(meshComponent.Shader).value();

		shader->bind();
		shader->setUniform4x4f("viewMatrix", glm::mat4(glm::mat3(camera.view)));
		shader->setUniform4x4f("projectionMatrix", camera.projection);

		glDepthMask(GL_FALSE);
		TextureManager::instance().bind("skybox-cubemap", 0);
		mesh->Draw();
		glDepthMask(GL_TRUE);

		shader->unbind();
	}

	void UpdateLight(lazy::graphics::Shader &shader)
	{
		auto lights = GetEntities<PointLightComponent, TransformComponent>();

		if (lights.size() == 0) { return ; }

		shader.setUniform1i("pointLightCount", lights.size());

		for (size_t i = 0; i < lights.size(); i++) {
			auto [ light, transform ] = lights[i]->GetAll();

			std::string pointLight = "pointLight[" + std::to_string(i) + "]";

			shader.setUniform3f(pointLight + ".position", transform.position);
			shader.setUniform3f(pointLight + ".color", light.Color);
			shader.setUniform1f(pointLight + ".intensity", light.Intensity);
		}

		auto dirLights = GetEntities<DirectionalLightComponent>();

		shader.setUniform1i("directionalLightCount", dirLights.size());

		for (size_t i = 0; i < dirLights.size(); i++) {
			auto [ light ] = dirLights[i]->GetAll();

			std::string directionalLight = "directionalLights[" + std::to_string(i) + "]";

			shader.setUniform3f(directionalLight + ".direction", light.Direction);
			shader.setUniform3f(directionalLight + ".color", light.Color);
			shader.setUniform1f(directionalLight + ".intensity", light.Intensity);
		}
	}

	void RenderLightBillboard(PlayerCameraComponent const &camera)
	{
		auto lights = GetEntities<PointLightComponent, TransformComponent>();

		if (lights.size() == 0 ) { return ; }

		for (auto const &lightEnt : lights) {
			auto [ light, transform ] = lightEnt->GetAll();

			_billboard.bind();
			_billboard.setUniform4x4f("viewMatrix", camera.view);
			_billboard.setUniform4x4f("viewProjectionMatrix", camera.viewProjection);
			_billboard.setUniform4x4f("projectionMatrix", camera.projection);
			_billboard.setUniform3f("particlePosition", transform.position);

			TextureManager::instance().bind("light_bulb_icon", 0);
			_quad.Draw();

			_billboard.unbind();
		}
	}

	void RenderLight(glm::vec3 const &viewPos, float const exposure)
	{
		// Lighting pass
		_light.bind();
			UpdateLight(_light);
			_light.setUniform1i("gPosition", 0);
			_light.setUniform1i("gNormal", 1);
			_light.setUniform1i("gAlbedoSpec", 2);
			_light.setUniform1i("gSSAO", 3);
			_light.setUniform1i("gMetallicRoughness", 5);
			_light.setUniform1i("depthMap", 4);
			_light.setUniform3f("viewPos", viewPos);
			_light.setUniform1f("exposure", exposure);

			// Bind GBuffer Textures
			glActiveTexture(GL_TEXTURE0);
				glBindTexture(GL_TEXTURE_2D, _gBuffer.GetPositionTex());
			glActiveTexture(GL_TEXTURE1);
				glBindTexture(GL_TEXTURE_2D, _gBuffer.GetNormalTex());
			glActiveTexture(GL_TEXTURE2);
				glBindTexture(GL_TEXTURE_2D, _gBuffer.GetAlbedoSpecTex());
			glActiveTexture(GL_TEXTURE3);
				//glBindTexture(GL_TEXTURE_2D, _ssaoBlurTex);
				glBindTexture(GL_TEXTURE_2D, _ssaoColorBuf);
			glActiveTexture(GL_TEXTURE4);
				glBindTexture(GL_TEXTURE_CUBE_MAP, _depthCubemap);
			glActiveTexture(GL_TEXTURE5);
				glBindTexture(GL_TEXTURE_2D, _gBuffer.GetMetallicRoughnessTex());

			_quad.Draw();

			// Unbind Textures
			glActiveTexture(GL_TEXTURE0);
				glBindTexture(GL_TEXTURE_2D, 0);
			glActiveTexture(GL_TEXTURE1);
				glBindTexture(GL_TEXTURE_2D, 0);
			glActiveTexture(GL_TEXTURE2);
				glBindTexture(GL_TEXTURE_2D, 0);
			glActiveTexture(GL_TEXTURE3);
				glBindTexture(GL_TEXTURE_2D, 0);
			glActiveTexture(GL_TEXTURE4);
				glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
			glActiveTexture(GL_TEXTURE5);
				glBindTexture(GL_TEXTURE_2D, 0);
		_light.unbind();
	}

	void BakeShadowMap()
	{
		//Logger::Info("Building shadow map\n");

		auto lights = GetEntities<PointLightComponent, TransformComponent>();
		auto players = GetEntities<PlayerCameraComponent, TransformComponent>();

		if (lights.size() == 0) return ;
		if (players.size() == 0) return ;

		auto playerTransform = players[0]->Get<TransformComponent>();
		auto lightPos = lights[0]->Get<TransformComponent>().position;

		std::array<glm::mat4, 6> shadowTransforms = {
			_shadowProjection * glm::lookAt(lightPos, lightPos + glm::vec3( 1.0, 0.0, 0.0), glm::vec3(0.0,-1.0, 0.0)),
			_shadowProjection * glm::lookAt(lightPos, lightPos + glm::vec3(-1.0, 0.0, 0.0), glm::vec3(0.0,-1.0, 0.0)),
			_shadowProjection * glm::lookAt(lightPos, lightPos + glm::vec3( 0.0, 1.0, 0.0), glm::vec3(0.0, 0.0, 1.0)),
			_shadowProjection * glm::lookAt(lightPos, lightPos + glm::vec3( 0.0,-1.0, 0.0), glm::vec3(0.0, 0.0,-1.0)),
			_shadowProjection * glm::lookAt(lightPos, lightPos + glm::vec3( 0.0, 0.0, 1.0), glm::vec3(0.0,-1.0, 0.0)),
			_shadowProjection * glm::lookAt(lightPos, lightPos + glm::vec3( 0.0, 0.0,-1.0), glm::vec3(0.0,-1.0, 0.0)),
		};

		BindShadowMap();

		glDisable(GL_BLEND);
		glEnable(GL_DEPTH_TEST);
		glClear(GL_DEPTH_BUFFER_BIT);

		_shadow.bind();
		_shadow.setUniform4x4f("model", glm::mat4(1.0f));
		glUniformMatrix4fv(_shadow.getUniformLocation("shadowMatrices"), 6, GL_FALSE,
			(float *)shadowTransforms.data());
		_shadow.setUniform1f("far_plane", 10000.0f);
		_shadow.setUniform3f("lightPos", lightPos);

			RenderShadowMeshes();

		_shadow.unbind();

		UnbindShadowMap();

		//Logger::Info("Done building shadow map\n");
	}

public:
	MeshRendererSystem()
	{
		buildShadowMap = [this] { BakeShadowMap(); };
		engine::Engine::Instance().OnBuildLighting += buildShadowMap;

		InitFramebuffer();
		InitBillboard();
		InitSSAO();
		InitDepthCubemap();

		TextureManager::instance().createTexture("light_bulb_icon", "./img/light_bulb_icon.png", {
			{ GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE },
			{ GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE },
			{ GL_TEXTURE_MIN_FILTER, GL_NEAREST },
			{ GL_TEXTURE_MAG_FILTER, GL_NEAREST },
		}, GL_TEXTURE_2D);

		_light.addVertexShader("shaders/light.vs.glsl")
			.addFragmentShader("shaders/light.fs.glsl")
			.link();
//		assert(_light.isValid());
	}

	~MeshRendererSystem()
	{
		engine::Engine::Instance().OnBuildLighting -= buildShadowMap;
	}

	void OnUpdate(float __unused deltaTime) override
	{
		auto player = GetEntities<PlayerCameraComponent, TransformComponent>();
		auto display = engine::Engine::Instance().GetDisplay();
		auto [ width, height ] = std::tuple(display->getWidth(), display->getHeight());

		if (player.size() == 0) { return ; }

		auto [ playerCamera, playerTransform ] = player[0]->GetAll();

		BakeShadowMap();

		_gBuffer.Bind();
			glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
			glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
			glEnable(GL_DEPTH_TEST);
			RenderMeshes(playerCamera, playerTransform);
			glDisable(GL_DEPTH_TEST);
		_gBuffer.Unbind();

		glClearColor(0.0f, 0.0, 0.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		RenderLight(playerTransform.position, playerCamera.exposure);

		// Copy depth buffer to default framebuffer to enable depth testing with billboard
		// and other shaders
		glBindFramebuffer(GL_READ_FRAMEBUFFER, _gBuffer.GetFramebufferId());
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
		glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_DEPTH_BUFFER_BIT, GL_NEAREST);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		glEnable(GL_BLEND);
		glEnable(GL_DEPTH_TEST);
			RenderSkybox(playerCamera);
//			RenderSSAO(playerCamera);
			RenderLightBillboard(playerCamera);
		glDisable(GL_DEPTH_TEST);
		glDisable(GL_BLEND);
	}
};
