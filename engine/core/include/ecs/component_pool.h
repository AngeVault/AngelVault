#pragma once
#include <vector>
#include <cassert>
#include <utility>
#include <cstdint>
#include <functional> 

struct IComponentPool {
    virtual ~IComponentPool() = default;
    virtual void onEntityDestroyed(uint32_t entityIndex) = 0;
    virtual size_t size() const = 0;
    virtual const std::vector<uint32_t>& denseToEntity() const = 0;
};

template<typename T>
class ComponentPool : public IComponentPool {
public:
    using ConstructCallback = std::function<void(uint32_t, T&)>;

    void onConstruct(ConstructCallback cb) {
        m_onConstruct.push_back(std::move(cb));
    }

	using DestroyCallback = std::function<void(uint32_t, T&)>;

	void onDestroy(DestroyCallback cb) {
		m_onDestroy.push_back(std::move(cb));
	}

    bool has(uint32_t entityIndex) const {
        if (m_sparse.size() <= entityIndex) return false;
        return m_sparse[entityIndex] != UINT32_MAX;
    }


    template<typename... Args>
    T& add(uint32_t entityIndex, Args&&... args) {
        assert(!has(entityIndex) && "Composant deja present");
        if (m_sparse.size() <= entityIndex)
            m_sparse.resize(entityIndex + 1, UINT32_MAX);
        m_sparse[entityIndex] = static_cast<uint32_t>(m_dense.size());
        m_dense.emplace_back(std::forward<Args>(args)...);
        m_denseToEntity.push_back(entityIndex);

        T& component = m_dense.back();
        for (auto& cb : m_onConstruct) cb(entityIndex, component);
        return component;
    }

    T& get(uint32_t entityIndex) {
        assert(has(entityIndex));
        return m_dense[m_sparse[entityIndex]];
    }

    void remove(uint32_t entityIndex) {
        assert(has(entityIndex));

        uint32_t removedDenseIdx = m_sparse[entityIndex];
        for (auto& cb : m_onDestroy) cb(entityIndex, m_dense[removedDenseIdx]);
        uint32_t lastEntityIndex = m_denseToEntity.back();
        m_dense[removedDenseIdx] = std::move(m_dense.back());
        m_denseToEntity[removedDenseIdx] = lastEntityIndex;
        m_sparse[lastEntityIndex] = removedDenseIdx;
        m_dense.pop_back();
        m_denseToEntity.pop_back();
        m_sparse[entityIndex] = UINT32_MAX;

    }

    void onEntityDestroyed(uint32_t entityIndex) override {
        if (has(entityIndex)) remove(entityIndex);
    }

    size_t size() const { return m_dense.size(); }
    const std::vector<uint32_t>& denseToEntity() const { return m_denseToEntity; }

private:
    std::vector<ConstructCallback> m_onConstruct;
	std::vector<DestroyCallback> m_onDestroy;
    std::vector<T>        m_dense;
    std::vector<uint32_t> m_denseToEntity;
    std::vector<uint32_t> m_sparse;
};