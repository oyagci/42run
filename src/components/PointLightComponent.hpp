#pragma once

#include "ecs/Component.hpp"
#include <glm/vec3.hpp>

struct PointLightComponent : ecs::IComponentBase
{
	glm::vec3 Dir;
	glm::vec3 Color = { 1.0f, 1.0f, 0.8f };

	float Intensity = 1000.0f;
};
