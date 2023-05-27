#pragma once

#include <cassert>
#include <cstddef>
#include <unordered_map>

#include <cubos/core/ecs/component_manager.hpp>
#include <cubos/core/ecs/entity_manager.hpp>
#include <cubos/core/ecs/resource_manager.hpp>
#include <cubos/core/log.hpp>

namespace cubos::core::ecs
{
    namespace impl
    {
        template <typename T>
        struct QueryFetcher;
    }

    /// @brief World is used as a container for all of the entity and component data.
    /// Components are stored in abstract containers called storages.
    /// @see Storage
    class World
    {
    public:
        /// Used to iterate over all entities of a world.
        using Iterator = EntityManager::Iterator;

        /// @param initialCapacity The initial capacity of the world.
        World(std::size_t initialCapacity = 1024);

        /// Registers a new resource type.
        /// Unsafe to call during any reads or writes, should be called at the start of the program.
        /// @tparam T The type of the resource.
        /// @tparam TArgs The types of the arguments of the constructor of the resource.
        /// @param args The arguments of the constructor of the resource.
        template <typename T, typename... TArgs>
        void registerResource(TArgs... args);

        /// @brief Registers a component type.
        /// @tparam T Component type.
        /// @param storage Storage for the component type.
        template <typename T>
        void registerComponent();

        /// Reads a resource, locking it for reading.
        /// @tparam T The type of the resource.
        /// @returns A lock referring to the resource.
        template <typename T>
        ReadResource<T> read() const;

        /// Writes a resource, locking it for writing.
        /// @tparam T The type of the resource.
        /// @returns A lock referring to the resource.
        template <typename T>
        WriteResource<T> write() const;

        /// @brief Creates a new entity.
        /// @tparam ComponentTypes The types of the components to be added when the entity is created.
        /// @param components The initial values for the components.
        template <typename... ComponentTypes>
        Entity create(ComponentTypes&&... components);

        /// @brief Removes an entity.
        /// @param entity Entity ID.
        void destroy(Entity entity);

        /// @brief Checks if an entity is still alive.
        /// @param entity Entity identifier.
        /// @returns True if the entity is alive, false otherwise.
        bool isAlive(Entity entity) const;

        /// @brief Adds components to an entity.
        /// @tparam ComponentTypes The types of the components to be added.
        /// @param components The initial values for the components.
        template <typename... ComponentTypes>
        void add(Entity entity, ComponentTypes&&... components);

        /// @brief Removes components from an entity.
        /// @tparam ComponentTypes Component types to be removed.
        /// @param entity Entity ID.
        template <typename... ComponentTypes>
        void remove(Entity entity);

        /// @brief Checks if an entity has a component.
        /// @tparam T Component type.
        /// @param entity Entity ID.
        template <typename T>
        bool has(Entity entity) const;

        /// @brief Creates a package from the components of an entity.
        /// @param entity Entity ID.
        /// @param context Optional context for serializing the components.
        /// @returns A package containing the components of the entity.
        data::Package pack(Entity entity, data::Context* context = nullptr) const;

        /// @brief Unpacks components specified in a package into an entity.
        /// Removes any components that are already present in the entity.
        /// @param entity Entity ID.
        /// @param package Package to unpack.
        /// @param context Optional context for deserializing the components.
        /// @returns True if the package was unpacked successfully, false otherwise.
        bool unpack(Entity entity, const data::Package& package, data::Context* context = nullptr);

        /// Returns an iterator which points to the first entity of the world.
        /// @return An iterator.
        Iterator begin() const;

        /// Returns an iterator which points to the end of the world.
        /// @return An iterator.
        Iterator end() const;

    private:
        template <typename... ComponentTypes>
        friend class Query;
        template <typename T>
        friend struct impl::QueryFetcher;
        friend class CommandBuffer;

        ResourceManager mResourceManager;
        EntityManager mEntityManager;
        ComponentManager mComponentManager;
    };

    // Implementation.

    template <typename T, typename... TArgs>
    void World::registerResource(TArgs... args)
    {
        CUBOS_TRACE("Registered resource '{}'", typeid(T).name());
        mResourceManager.add<T>(args...);
    }

    template <typename T>
    void World::registerComponent()
    {
        CUBOS_TRACE("Registered component '{}'", getComponentName<T>().value());
        mComponentManager.registerComponent<T>();
    }

    template <typename T>
    ReadResource<T> World::read() const
    {
        return mResourceManager.read<T>();
    }

    template <typename T>
    WriteResource<T> World::write() const
    {
        return mResourceManager.write<T>();
    }

    template <typename... ComponentTypes>
    Entity World::create(ComponentTypes&&... components)
    {
        std::size_t ids[] = {
            0, (mComponentManager
                    .getID<std::remove_const_t<std::remove_reference_t<std::remove_pointer_t<ComponentTypes>>>>())...};

        Entity::Mask mask{};
        for (auto id : ids)
        {
            mask.set(id);
        }

        auto entity = mEntityManager.create(mask);
        ([&](auto& component) { mComponentManager.add(entity.index, component); }(components), ...);

#if CUBOS_LOG_LEVEL <= CUBOS_LOG_LEVEL_DEBUG
        // Get the number of components being added.
        constexpr std::size_t componentCount = sizeof...(ComponentTypes);

        std::string componentNames[] = {"", "'" + std::string{getComponentName<ComponentTypes>().value()} + "'" ...};
        CUBOS_DEBUG("Created entity {} with components {}", entity.index,
                    fmt::join(componentNames + 1, componentNames + componentCount + 1, ", "));
#endif
        return entity;
    }

    template <typename... ComponentTypes>
    void World::add(Entity entity, ComponentTypes&&... components)
    {
        if (!mEntityManager.isValid(entity))
        {
            CUBOS_ERROR("Entity {} doesn't exist!", entity.index);
            return;
        }

        auto mask = mEntityManager.getMask(entity);

        (
            [&]() {
                std::size_t componentId = mComponentManager.getID<ComponentTypes>();
                mask.set(componentId);
                mComponentManager.add(entity.index, std::move(components));
            }(),
            ...);

        mEntityManager.setMask(entity, mask);

#if CUBOS_LOG_LEVEL <= CUBOS_LOG_LEVEL_DEBUG
        std::string componentNames[] = {"'" + std::string{getComponentName<ComponentTypes>().value()} + "'" ...};
        CUBOS_DEBUG("Added components {} to entity {}", fmt::join(componentNames, ", "), entity.index);
#endif
    }

    template <typename... ComponentTypes>
    void World::remove(Entity entity)
    {
        if (!mEntityManager.isValid(entity))
        {
            CUBOS_ERROR("Entity {} doesn't exist!", entity.index);
            return;
        }

        auto mask = mEntityManager.getMask(entity);

        (
            [&]() {
                std::size_t componentId = mComponentManager.getID<ComponentTypes>();
                mask.set(componentId, false);
                mComponentManager.remove<ComponentTypes>(entity.index);
            }(),
            ...);

        mEntityManager.setMask(entity, mask);

#if CUBOS_LOG_LEVEL <= CUBOS_LOG_LEVEL_DEBUG
        std::string componentNames[] = {"'" + std::string{getComponentName<ComponentTypes>().value()} + "'" ...};
        CUBOS_DEBUG("Removed components {} from entity {}", fmt::join(componentNames, ", "), entity.index);
#endif
    }

    template <typename T>
    bool World::has(Entity entity) const
    {
        if (!mEntityManager.isValid(entity))
        {
            CUBOS_ERROR("Entity {} doesn't exist!", entity.index);
            return false;
        }

        std::size_t componentId = mComponentManager.getID<T>();
        return mEntityManager.getMask(entity).test(componentId);
    }
} // namespace cubos::core::ecs
