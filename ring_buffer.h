#pragma once

#include <cstring>

class RingBuffer
{
private:
	static constexpr size_t MAX_SIZE = 20000;

public:
	RingBuffer(size_t len = MAX_SIZE)
	{
		capacity_ = len < MAX_SIZE ? len : MAX_SIZE;
		buff_ = new char[capacity_ + 1];
	}

	~RingBuffer()
	{
		delete[] buff_;
	}

public:
	// tail
	char* write_pos() const noexcept
	{
		return buff_ + write_pos_;
	}
	
	// head
	char* read_pos() const noexcept
	{
		return buff_ + read_pos_;
	}

	bool empty() const noexcept
	{
		return (read_pos_ == write_pos_);
	}

	size_t capacity() const noexcept
	{
		return capacity_;
	}

	size_t max_size() const noexcept
	{
		return MAX_SIZE;
	}

	// 사용량
	size_t size() const noexcept
	{
		return (capacity_ + 1 - read_pos_ + write_pos_) % (capacity_ + 1);
	}

	// 가용량
	size_t available() const noexcept
	{
		return (capacity_ + read_pos_ - write_pos_) % (capacity_ + 1);
	}

	// move_tail
	size_t move_write_pos(size_t len) noexcept
	{
		size_t remaining = available();
		size_t move_size = len < remaining ? len : remaining;

		write_pos_ = (write_pos_ + move_size) % (capacity_ + 1);

		return move_size;
	}

	// move_head
	size_t move_read_pos(size_t len) noexcept
	{
		size_t data_size = size();
		size_t move_size = len < data_size ? len : data_size;

		read_pos_ = (read_pos_ + move_size) % (capacity_ + 1);

		return move_size;
	}

	// direct_enqueue_size
	size_t direct_write_size() const noexcept
	{
		size_t true_read_pos = (capacity_ + read_pos_) % (capacity_ + 1);
		size_t limit = true_read_pos < write_pos_ ? (capacity_ + 1) : true_read_pos;

		return limit - write_pos_;
	}

	// direct_dequeue_size
	size_t direct_read_size() const noexcept
	{
		size_t wp = write_pos_;

		return wp >= read_pos_ ? wp - read_pos_ : capacity_ + 1 - read_pos_;
	}

	// enqueue
	size_t write(const char* src, size_t len)
	{
		if (available() < len)
		{
			return 0;
		}

		size_t direct_size = capacity_ + 1 - write_pos_;
		if (len > direct_size)
		{
			std::memcpy(buff_ + write_pos_, src, direct_size);
			std::memcpy(buff_, src + direct_size, len - direct_size);
		}
		else
		{
			std::memcpy(buff_ + write_pos_, src, len);
		}

		write_pos_ = (write_pos_ + len) % (capacity_ + 1);

		return len;
	}

	// dequeue
	size_t read(char* dest, size_t len)
	{
		if (size() < len)
		{
			return 0;
		}

		size_t direct_size = capacity_ + 1 - read_pos_;
		if (len > direct_size)
		{
			std::memcpy(dest, buff_ + read_pos_, direct_size);
			std::memcpy(dest + direct_size, buff_, len - direct_size);
		}
		else
		{
			std::memcpy(dest, buff_ + read_pos_, len);
		}

		read_pos_ = (read_pos_ + len) % (capacity_ + 1);

		return len;
	}

	size_t peek(char* dest, size_t len) const
	{
		if (size() < len)
		{
			return 0;
		}

		size_t direct_size = capacity_ + 1 - read_pos_;
		if (len > direct_size)
		{
			std::memcpy(dest, buff_ + read_pos_, direct_size);
			std::memcpy(dest + direct_size, buff_, len - direct_size);
		}
		else
		{
			std::memcpy(dest, buff_ + read_pos_, len);
		}

		return len;
	}

	void clear() noexcept
	{
		read_pos_ = 0;
		write_pos_ = 0;
	}

private:
	char* buff_;
	size_t read_pos_ = 0; // head
	size_t write_pos_ = 0; // tail
	size_t capacity_; // 정확한 크기는 capacity_ + 1
};