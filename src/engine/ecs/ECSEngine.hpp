#pragma once

#include "Entity.hpp"

namespace ecs {

class EntityManager;

class EntityManager;
class SystemManager;

class ECSEngine
{
private:
	typedef std::unique_ptr<EntityManager> EntityManagerHandle;
	typedef std::unique_ptr<SystemManager> SystemManagerHandle;

public:
	struct Instance
	{
		unsigned int Uuid;
		EntityManagerHandle EntityManager;
		SystemManagerHandle SystemManager;
	};

private:
	std::unordered_map<unsigned int, Instance> _Instances;

	EntityManagerHandle _EntityManager;
	SystemManagerHandle _SystemManager;

	ECSEngine();

public:

	static ECSEngine &Get()
	{
		static ECSEngine engine;
		return engine;
	}

	EntityManager *GetEntityManager()
	{
		return _EntityManager.get();
	}

	SystemManager *GetSystemManager()
	{
		return _SystemManager.get();
	}

	auto CreateInstance() -> Instance*
	{
		unsigned int instanceUuid(g_NextUuid++);

		_Instances[instanceUuid].Uuid = instanceUuid;
		_Instances[instanceUuid].EntityManager = std::make_unique<EntityManager>();
		_Instances[instanceUuid].SystemManager = std::make_unique<SystemManager>(_Instances[instanceUuid].EntityManager.get());

		return &_Instances[instanceUuid];
	}

	auto RemoveInstance(unsigned int uuid)
	{
		auto instance = _Instances.find(uuid);
		if (instance != _Instances.end()) {
			_Instances.erase(instance);
		}
	}

	void Update(float deltaTime);
};

}
