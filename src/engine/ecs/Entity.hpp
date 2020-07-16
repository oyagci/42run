#pragma once

#include <type_traits>
#include <memory>
#include <typeindex>
#include <unordered_map>
#include <vector>
#include <algorithm>
#include "Component.hpp"
#include <iostream>
#include <tuple>

namespace ecs {

static unsigned int g_NextUuid = 0;

typedef std::type_index TypeIndex;

template <typename T>
TypeIndex const GetTypeIndex()
{
	return std::type_index(typeid(T));
}

class IEntityBase
{
protected:
	///
	/// Check if the class U is an interface of IComponentBase
	///
	template <typename U>
	static bool constexpr is_component_base()
	{
		return std::is_base_of<IComponentBase, U>::value;
	}

	template <typename U, typename V, typename ... UTypes>
	static bool constexpr is_component_base()
	{
		return std::is_base_of<IComponentBase, U>::value && is_component_base<V, UTypes...>();
	}

	///
	/// Create a component of type U and add it to the list of components for that make up the entity
	///
	template <typename U>
	void RegisterComponents()
	{
		Components[GetTypeIndex<U>()] = std::make_unique<U>();
	}

	template <typename U, typename V, typename ... UTypes>
	void RegisterComponents()
	{
		RegisterComponents<U>();
		RegisterComponents<V, UTypes...>();
	}

	///
	/// Get a reference to the component of type U
	///
	template <typename U>
	U &GetComponent()
	{
		static_assert(std::is_base_of<IComponentBase, U>::value, "typename U must de derived from IComponentBase");

		if (!HasComponents<U>()) { throw MissingComponentException(); }

		return *dynamic_cast<U*>(Components[GetTypeIndex<U>()].get());
	}

	///
	/// Remove a registered component from the entity
	///
	template <typename U>
	void RemoveComponents()
	{
		auto it = Components.find(GetTypeIndex<U>());
		if (it != Components.end()) {
			Components.erase(it);
		}
	}

	///
	/// Remove a list of registered components from the entity
	///
	template <typename U, typename V, typename ... UTypes>
	void RemoveComponents()
	{
		RemoveComponents<U>();
		RemoveComponents<V, UTypes...>();
	}

protected:
	/// The entity's components
	std::unordered_map<TypeIndex, std::unique_ptr<IComponentBase>> Components;
	std::string Name;
	unsigned int Id;
	unsigned int Uuid;

	static unsigned int NextEntityID;

public:
	IEntityBase() : Name("Unnamed Entity"), Id(NextEntityID++), Uuid(g_NextUuid++)
	{
	}

	virtual ~IEntityBase() = default;

	std::string const &GetName() const { return Name; }
	void SetName(std::string const &name) { Name = name; }

	///
	/// Check if the entity has the component U
	///
	template <typename U>
	bool HasComponents() const
	{
		static_assert(std::is_base_of<IComponentBase, U>::value, "typename U must de derived from IComponentBase");

		return Components.find(GetTypeIndex<U>()) != Components.end();
	}

	template <typename U, typename V, typename ... UTypes>
	bool HasComponents() const
	{
		return Components.find(GetTypeIndex<U>()) != Components.end() &&
			HasComponents<V, UTypes...>();
	}

	///
	/// Modify the data of the component U
	///
	template <typename U>
	void Set(U const &data)
	{
		static_assert(std::is_base_of<IComponentBase, U>::value, "typename U must de derived from IComponentBase");
		GetComponent<U>() = data;
	}

	///
	/// Modify the data of the component U
	///
	template <typename U>
	void Set(U const &&data)
	{
		static_assert(std::is_base_of<IComponentBase, U>::value, "typename U must de derived from IComponentBase");
		GetComponent<U>() = std::move(data);
	}

	///
	/// Get a const reference to the data of component U
	///
	template <typename U>
	U &Get() const
	{
		static_assert(std::is_base_of<IComponentBase, U>::value, "typename U must de derived from IComponentBase");

		if (!HasComponents<U>()) { throw MissingComponentException(); }

		auto component = Components.find(GetTypeIndex<U>());

		return *dynamic_cast<U*>(component->second.get());
	}

	template <typename U, typename ... UTypes>
	void AddComponents()
	{
		RegisterComponents<U, UTypes...>();
	}

	template <typename U, typename ... UTypes>
	void DeleteComponents()
	{
		RemoveComponents<U, UTypes...>();
	}

	unsigned int GetId() const
	{
		return Id;
	}

	unsigned int const &GetUuid() const
	{
		return Uuid;
	}

	class MissingComponentException : public std::exception
	{
		public:
			const char *what() const noexcept
			{
				return "Component does not exists";
			}
	};
};

template <typename T, typename ... Types>
class IEntity : public IEntityBase
{
public:
	typedef std::tuple<const T&, const Types&...> ComponentsType;

public:
	IEntity()
	{
		RegisterComponents<T, Types...>();
	}

	///
	/// Returns a tuple containing a const reference to every component
	///
	ComponentsType GetAll()
	{
		return std::tie(Get<T>(), Get<Types>()...);
	}

	template <typename U, typename R, typename ... OtherTypes>
	void DuplicateContent(IEntity<T, Types...> *dupl)
	{
		if (nullptr != dupl->Components[GetTypeIndex<U>()].get() &&
			nullptr != Components[GetTypeIndex<U>()].get()) {
			*dupl->Components[GetTypeIndex<U>()] = *Components[GetTypeIndex<U>()];
		}
		DuplicateContent<R, OtherTypes...>();
	}

	std::unique_ptr<IEntity<T, Types...>> Duplicate()
	{
		auto dupl = std::make_unique<IEntity<T, Types...>>();

		DuplicateContent<T, Types...>(dupl.get());

		return dupl;
	}

	static_assert(is_component_base<T, Types...>(),
		"InputTypes parameter pack must be all derived from IComponent");
};

}
