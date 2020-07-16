#pragma once

#include "lazy.hpp"
#include "Engine.hpp"

class GBuffer
{
private:
	GLuint _gBuffer;
	GLuint _gPosition;
	GLuint _gNormal;
	GLuint _gAlbedoSpec;
	GLuint _gDepth;
	GLuint _gMetallicRoughness;

	void Init()
	{
		auto display = engine::Engine::Instance().GetDisplay();
		auto [ width, height ] = std::tuple(display->getWidth(), display->getHeight());

		glGenFramebuffers(1, &_gBuffer);
		glBindFramebuffer(GL_FRAMEBUFFER, _gBuffer);

		glGenTextures(1, &_gPosition);
		glBindTexture(GL_TEXTURE_2D, _gPosition);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, nullptr);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, _gPosition, 0);

		glGenTextures(1, &_gNormal);
		glBindTexture(GL_TEXTURE_2D, _gNormal);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, nullptr);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, _gNormal, 0);

		glGenTextures(1, &_gAlbedoSpec);
		glBindTexture(GL_TEXTURE_2D, _gAlbedoSpec);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, nullptr);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, _gAlbedoSpec, 0);

		glGenTextures(1, &_gMetallicRoughness);
		glBindTexture(GL_TEXTURE_2D, _gMetallicRoughness);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, nullptr);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT3, GL_TEXTURE_2D, _gMetallicRoughness, 0);

		glBindTexture(GL_TEXTURE_2D, 0);

		glGenRenderbuffers(1, &_gDepth);
		glBindRenderbuffer(GL_RENDERBUFFER, _gDepth);
		glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, width, height);
		glBindRenderbuffer(GL_RENDERBUFFER, 0);
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, _gDepth);

		GLuint st = glCheckFramebufferStatus(GL_FRAMEBUFFER);
		assert(st == GL_FRAMEBUFFER_COMPLETE);

		std::array<GLuint, 4> attachments = {
			GL_COLOR_ATTACHMENT0,
			GL_COLOR_ATTACHMENT1,
			GL_COLOR_ATTACHMENT2,
			GL_COLOR_ATTACHMENT3,
		};
		glDrawBuffers(attachments.size(), attachments.data());

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

public:
	GBuffer()
	{
		Init();
	}

	~GBuffer()
	{
		if (_gBuffer) { glDeleteFramebuffers(1, &_gBuffer); }
		if (_gPosition) { glDeleteTextures(1, &_gPosition); }
		if (_gNormal) { glDeleteTextures(1, &_gNormal); }
		if (_gAlbedoSpec) { glDeleteTextures(1, &_gAlbedoSpec); }
		if (_gDepth) { glDeleteTextures(1, &_gDepth); }
		if (_gMetallicRoughness) { glDeleteTextures(1, &_gMetallicRoughness); }
	}

	void Bind()
	{
		glBindFramebuffer(GL_FRAMEBUFFER, _gBuffer);
	}

	void Unbind()
	{
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

	GLuint GetFramebufferId() const { return _gBuffer; }
	GLuint GetPositionTex() const { return _gPosition; }
	GLuint GetNormalTex() const { return _gNormal; }
	GLuint GetAlbedoSpecTex() const { return _gAlbedoSpec; }
	GLuint GetDepthTex() const { return _gDepth; }
	GLuint GetMetallicRoughnessTex() const { return _gMetallicRoughness; }
};
