#pragma once
#include "entity.h"
#include "component_pool.h"
#include <unordered_map>
#include <typeindex>
#include <memory>
#include <queue>
#include <vector>
#include <cassert>
#include <array>

class Registry {
public:
    // --- Entités ---

    Entity createEntity() {
        Entity entity;
        if (!m_freeEntities.empty()) {
            entity.index = m_freeEntities.front();
            m_freeEntities.pop();
            entity.generation = m_generations[entity.index];
        }
        else {
            entity.index = static_cast<uint32_t>(m_generations.size());
            m_generations.push_back(0);
        }
        return entity;
    }

    void destroyEntity(Entity entity) {
        if (!isValid(entity)) return;
        m_pendingDestroy.push_back(entity);
    }


    void flushPendingDestroys() {
        for (auto& entity : m_pendingDestroy) {
            if (!isValid(entity)) continue;
            for (auto& [_, pool] : m_pools)
                pool->onEntityDestroyed(entity.index);
            m_generations[entity.index]++;
            m_freeEntities.push(entity.index);
        }
        m_pendingDestroy.clear();
    }

    bool isValid(Entity entity) const {
        if (entity.index >= m_generations.size()) return false;
        return m_generations[entity.index] == entity.generation;
    }

    // --- Composants ---

    template<typename T, typename... Args>
    T& addComponent(Entity entity, Args&&... args) {
        assert(isValid(entity));
        return pool<T>().add(entity.index, std::forward<Args>(args)...);
    }

    template<typename T>
    T& getComponent(Entity entity) {
        assert(isValid(entity));
        return pool<T>().get(entity.index);
    }

    template<typename T>
    bool hasComponent(Entity entity) {
        if (!isValid(entity)) return false;
        return pool<T>().has(entity.index);
    }

    template<typename T>
    void removeComponent(Entity entity) 
    {
        assert(isValid(entity));
        pool<T>().remove(entity.index);
    }

    // --- View : itère sur les entités qui ont TOUS les composants demandés ---
    // Usage : registry.view<Transform2D, SpriteComponent, ...>([](Entity e, Transform2D& t, SpriteComponent& s , ...) { ... });

    template<typename... Ts, typename Func> 
    void view(Func&& func) 
    {
        std::array<IComponentPool*, sizeof...(Ts)> pools = { &pool<Ts>()... };

        size_t smallestIdx = 0;
		for (size_t i = 1; i < pools.size(); ++i) {
			if (pools[i]->size() < pools[smallestIdx]->size())
				smallestIdx = i;
		}

        IComponentPool* primary = pools[smallestIdx];

        for (size_t i = 0; i < primary->size(); ++i) {
            uint32_t idx = primary->denseToEntity()[i];
            Entity   e = { idx, m_generations[idx] };
			if (isValid(e) && (pool<Ts>().has(idx) && ...)) 
                func(e, pool<Ts>().get(idx)...);
        }
    }

    template<typename T>
    void onConstruct(std::function<void(Entity, T&)> callback) {
        pool<T>().onConstruct([this, callback](uint32_t entityIndex, T& component) {
            Entity e{ entityIndex, m_generations[entityIndex] };
            callback(e, component);
            });
    }

	template <typename T>
	void onDestroy(std::function<void(Entity, T&)> callback) {
		pool<T>().onDestroy([this, callback](uint32_t entityIndex, T& component) {
			Entity e{ entityIndex, m_generations[entityIndex] };
			callback(e, component);
			});
	}


private:

    std::vector<uint32_t> m_generations;
    std::queue<uint32_t>  m_freeEntities;
	std::vector<Entity> m_pendingDestroy;
    std::unordered_map<std::type_index, std::unique_ptr<IComponentPool>> m_pools;

    template<typename T>
    ComponentPool<T>& pool() {
        auto key = std::type_index(typeid(T));
        auto it = m_pools.find(key);
        if (it == m_pools.end()) {
            m_pools[key] = std::make_unique<ComponentPool<T>>();
            return *static_cast<ComponentPool<T>*>(m_pools[key].get());
        }
        return *static_cast<ComponentPool<T>*>(it->second.get());
    }
};