#pragma once

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <cstring>

#include <vector>

// #define OBJECT_POOL_DEBUG_MODE

template<class T>
class ObjectPool
{
private:
	static constexpr size_t MAX_CAPACITY = 20000;

#ifdef OBJECT_POOL_DEBUG_MODE
	static constexpr unsigned char GUARD = 0xff;
#endif // OBJECT_POOL_DEBUG_MODE

	struct Node_
	{
#ifdef OBJECT_POOL_DEBUG_MODE
		unsigned char pre_guard = 0;
#endif // OBJECT_POOL_DEBUG_MODE
		T data;
#ifdef OBJECT_POOL_DEBUG_MODE
		unsigned char post_guard = 0;
#endif // OBJECT_POOL_DEBUG_MODE
		Node_* next = nullptr;
		unsigned long long id = 0;
		unsigned int in_use = 0;
	};

public:
	ObjectPool(size_t capacity = MAX_CAPACITY) : id_(unique_id()), size_(0)
	{
		capacity_ = capacity < MAX_CAPACITY ? capacity : MAX_CAPACITY;

		pool_.reserve(capacity);

#ifdef OBJECT_POOL_DEBUG_MODE
		pre_guard_size = static_cast<int>(reinterpret_cast<char*>(&head_.data) -
			reinterpret_cast<char*>(&head_.pre_guard));
		post_guard_size = static_cast<int>(reinterpret_cast<char*>(&head_.next) -
			reinterpret_cast<char*>(&head_.post_guard));
#endif // OBJECT_POOL_DEBUG_MODE

		Node_* node;
		Node_* prev = &head_;
		for (size_t i = 0; i < capacity; ++i)
		{
			node = new Node_;

#ifdef OBJECT_POOL_DEBUG_MODE
			std::memset(&(node->pre_guard), GUARD, pre_guard_size);
			std::memset(&(node->post_guard), GUARD, post_guard_size);
#endif // OBJECT_POOL_DEBUG_MODE

			node->next = prev;
			node->id = id_;
			node->in_use = 0;

			pool_.push_back(node);

			head_.next = node;
			prev = node;
		}
	}

	~ObjectPool()
	{
		size_t capacity = pool_.capacity();

		for (size_t i = 0; i < capacity; ++i)
		{
			delete pool_[i];
		}

		// option
		if (size_ > 0)
		{
			// log
		}
	}

public:
	size_t capacity() const noexcept
	{
		return capacity_;
	}

	size_t max_capacity() const noexcept
	{
		return MAX_CAPACITY;
	}

	size_t size() const noexcept
	{
		return size_;
	}

	size_t available() const noexcept
	{
		return capacity_ - size_;
	}

	bool empty() const noexcept
	{
		return (capacity_ == size_);
	}

public:
	T* alloc()
	{
		Node_* node;
		if (capacity_ > size_)
		{
			node = head_.next;
			head_.next = node->next;
		}
		else if (capacity_ < MAX_CAPACITY)
		{
			node = new Node_;

#ifdef OBJECT_POOL_DEBUG_MODE
			std::memset(&(node->pre_guard), GUARD, pre_guard_size);
			std::memset(&(node->post_guard), GUARD, post_guard_size);
#endif // OBJECT_POOL_DEBUG_MODE

			node->id = id_;
			pool_.push_back(node);
			++capacity_;
		}
		else // 전부 사용
		{
			return nullptr;
		}

		node->next = nullptr;
		node->in_use = 1;

		++size_;

		return &(node->data);
	}

	bool dealloc(T* data)
	{
		do
		{
			if (data == nullptr)
			{
				break;
			}

#ifdef OBJECT_POOL_DEBUG_MODE
			Node_* node = reinterpret_cast<Node_*>(reinterpret_cast<char*>(data) - pre_guard_size);

			if (!check_guard(node))
			{
				break;
			}
#else
			Node_* node = reinterpret_cast<Node_*>(data);
#endif // OBJECT_POOL_DEBUG_MODE

			if (node->id != id_)
			{
				break;
			}

			if (node->in_use == 0)
			{
				break;
			}

			if (node->next)
			{
				break;
			}

			node->next = head_.next;
			node->in_use = 0;
			head_.next = node;

			--size_;

			return true;
		} while (false);
		return false;
	}

private:
	unsigned long long unique_id()
	{
		static unsigned long long id = 1;
		return _InterlockedIncrement(&id);
	}

	bool check_guard(Node_* node)
	{
#ifdef OBJECT_POOL_DEBUG_MODE
		unsigned char* guard = &(node->pre_guard);
		for (int i = 0; i < pre_guard_size; ++i)
		{
			if (*guard++ != GUARD)
			{
				return false;
			}
		}

		guard = &(node->post_guard);
		for (int i = 0; i < post_guard_size; ++i)
		{
			if (*guard++ != GUARD)
			{
				return false;
			}
		}
#endif // OBJECT_POOL_DEBUG_MODE

		return true;
	}

private:
	Node_ head_;
	unsigned long long id_;
	size_t capacity_; // 전체 개수
	size_t size_; // 사용중인 개수
	std::vector<Node_*> pool_;
#ifdef OBJECT_POOL_DEBUG_MODE
	int pre_guard_size;
	int post_guard_size;
#endif // OBJECT_POOL_DEBUG_MODE
};