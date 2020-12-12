#pragma once
#include <assert.h>
#include <memory>
#include <mutex>
#include <new>

// Based on https://thinkingeek.com/2017/11/19/simple-memory-pool/ with updates for thread safety

namespace msglib {

template <typename T> class Pool {
    union PoolItem {
    private:
        using StorageType = alignas(alignof(T)) char[sizeof(T)];

        PoolItem *m_next;

        StorageType m_datum;
    public:
        void setNextItem(PoolItem *n) { m_next = n; }
        PoolItem *getNextItem() const { return m_next; }

        T *getStorage() { return reinterpret_cast<T*>(m_datum); }

        static PoolItem *storageToItem(T *t) {
            PoolItem *currentItem = reinterpret_cast<PoolItem*>(t);
            return currentItem;
        }
    };

    struct PoolArena {
    private:
        std::unique_ptr<PoolItem[]> m_storage;
        std::unique_ptr<PoolArena> m_next;
        size_t m_size;
    public:
        PoolArena(size_t arena_size): m_storage(new PoolItem[arena_size]), m_size(arena_size) {
            for (size_t i = 1; i < m_size; i++) {
                m_storage[i-1].setNextItem(&m_storage[i]);
            }
            m_storage[m_size-1].setNextItem(nullptr);
        }

        PoolItem* getStorage() const { return m_storage.get(); }

        void setNextArena(std::unique_ptr<PoolArena> &&n) {
            assert(!m_next);
            m_next.reset(n.release());
        }

        bool contains(PoolItem *item) {
            if (item) {
                return (item >= &m_storage[0] && item <= &m_storage[m_size-1]);
            }
            return false;
        }
    };

    std::mutex m_mutex;
    size_t m_capacity;
    size_t m_size;
    std::unique_ptr<PoolArena> m_arena;
    PoolItem *m_freeList;

public:

    Pool(size_t arena_size) :
        m_capacity(arena_size),
        m_size(arena_size),
        m_arena(new PoolArena(arena_size)),
        m_freeList(m_arena->getStorage())
    {

    }

    Pool() : 
        m_capacity(256),
        m_size(256),
        m_arena(new PoolArena(256)),
        m_freeList(m_arena->getStorage())
    {

    }

    size_t capacity() {
        std::lock_guard<std::mutex> guard(m_mutex);
        return m_capacity;
    }

    size_t size() {
        std::lock_guard<std::mutex> guard(m_mutex);
        return m_size;
    }

    // Allocates an object in the current arena.
    template <typename... Args> T *alloc(Args &&... args) {
        std::lock_guard<std::mutex> guard(m_mutex);
        if (m_freeList == nullptr) {
            throw std::bad_alloc();
        }
        
        // Get the first free item.
        PoolItem *currentItem = m_freeList;
        // Update the free list to the next free item.
        m_freeList = currentItem->getNextItem();

        // Get the storage for T.
        T *result = currentItem->getStorage();
        // Construct the object in the obtained storage.
        new (result) T(std::forward<Args>(args)...);
        m_size--;

        return result;
    }

    void free(T *t) {
        // Convert this pointer to T to its enclosing pointer of an item of the
        // arena.
        PoolItem *currentItem = PoolItem::storageToItem(t);

        if (m_arena->contains(currentItem)) {

            // Destroy the object.
            t->T::~T();

            std::lock_guard<std::mutex> guard(m_mutex);

            // Add the item at the beginning of the free list.
            currentItem->setNextItem(m_freeList);
            m_freeList = currentItem;
            m_size++;
        }
    }
};

}   // Namespace msglib