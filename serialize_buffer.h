#pragma once

#include <cstring>

#include <stdexcept>

class SerializeBuffer
{
private:
	static constexpr size_t MAX_SIZE = 16;

public:
	SerializeBuffer(size_t len = MAX_SIZE) : read_pos_(0), write_pos_(0)
	{
		capacity_ = len < MAX_SIZE ? len : MAX_SIZE;
		buff_ = new char[len];
	}

	~SerializeBuffer()
	{
		delete[] buff_;
	}

	SerializeBuffer(const SerializeBuffer& rhs) :
		read_pos_(rhs.read_pos_), write_pos_(rhs.write_pos_), capacity_(rhs.capacity_)
	{
		buff_ = new char[capacity_];
		std::memcpy(buff_, rhs.buff_, capacity_);
	}

	SerializeBuffer(SerializeBuffer&& rhs) noexcept :
		buff_(rhs.buff_), read_pos_(rhs.read_pos_), write_pos_(rhs.write_pos_), capacity_(rhs.capacity_)
	{
		rhs.buff_ = nullptr;
		rhs.read_pos_ = 0;
		rhs.write_pos_ = 0;
		rhs.capacity_ = 0;
	}

	SerializeBuffer& operator=(const SerializeBuffer& rhs)
	{
		if (this != &rhs)
		{
			if (capacity_ != rhs.capacity_)
			{
				delete[] buff_;
				capacity_ = rhs.capacity_;
				buff_ = new char[capacity_];
			}

			read_pos_ = rhs.read_pos_;
			write_pos_ = rhs.write_pos_;

			std::memcpy(buff_, rhs.buff_, capacity_);
		}

		return *this;
	}

	SerializeBuffer& operator=(SerializeBuffer&& rhs) noexcept
	{
		if (this != &rhs)
		{
			delete[] buff_;

			buff_ = rhs.buff_;
			read_pos_ = rhs.read_pos_;
			write_pos_ = rhs.write_pos_;
			capacity_ = rhs.capacity_;

			rhs.buff_ = nullptr;
			rhs.read_pos_ = 0;
			rhs.write_pos_ = 0;
			rhs.capacity_ = 0;
		}

		return *this;
	}

public:
	SerializeBuffer& operator<<(bool in)
	{
		if (write_pos_ + sizeof(bool) > capacity_)
		{
			throw std::runtime_error("insufficient buffer space (bool)");
		}

		std::memcpy(buff_ + write_pos_, &in, sizeof(bool));
		write_pos_ += sizeof(bool);

		return *this;
	}

	SerializeBuffer& operator<<(char in)
	{
		if (write_pos_ + sizeof(char) > capacity_)
		{
			throw std::runtime_error("insufficient buffer space (char)");
		}

		std::memcpy(buff_ + write_pos_, &in, sizeof(char));
		write_pos_ += sizeof(char);

		return *this;
	}

#if __cplusplus >= 202002L
	SerializeBuffer& operator<<(char8_t in)
	{
		if (write_pos_ + sizeof(char8_t) > capacity_)
		{
			throw std::runtime_error("insufficient buffer space (char8_t)");
		}

		std::memcpy(buff_ + write_pos_, &in, sizeof(char8_t));
		write_pos_ += sizeof(char8_t);

		return *this;
	}
#endif

	SerializeBuffer& operator<<(char16_t in)
	{
		if (write_pos_ + sizeof(char16_t) > capacity_)
		{
			throw std::runtime_error("insufficient buffer space (char16_t)");
		}

		std::memcpy(buff_ + write_pos_, &in, sizeof(char16_t));
		write_pos_ += sizeof(char16_t);

		return *this;
	}

	SerializeBuffer& operator<<(char32_t in)
	{
		if (write_pos_ + sizeof(char32_t) > capacity_)
		{
			throw std::runtime_error("insufficient buffer space (char32_t)");
		}

		std::memcpy(buff_ + write_pos_, &in, sizeof(char32_t));
		write_pos_ += sizeof(char32_t);

		return *this;
	}

	SerializeBuffer& operator<<(wchar_t in)
	{
		if (write_pos_ + sizeof(wchar_t) > capacity_)
		{
			throw std::runtime_error("insufficient buffer space (wchar_t)");
		}

		std::memcpy(buff_ + write_pos_, &in, sizeof(wchar_t));
		write_pos_ += sizeof(wchar_t);

		return *this;
	}

	SerializeBuffer& operator<<(signed char in)
	{
		if (write_pos_ + sizeof(signed char) > capacity_)
		{
			throw std::runtime_error("insufficient buffer space (signed char)");
		}

		std::memcpy(buff_ + write_pos_, &in, sizeof(signed char));
		write_pos_ += sizeof(signed char);

		return *this;
	}

	SerializeBuffer& operator<<(short in)
	{
		if (write_pos_ + sizeof(short) > capacity_)
		{
			throw std::runtime_error("insufficient buffer space (short)");
		}

		std::memcpy(buff_ + write_pos_, &in, sizeof(short));
		write_pos_ += sizeof(short);

		return *this;
	}

	SerializeBuffer& operator<<(int in)
	{
		if (write_pos_ + sizeof(int) > capacity_)
		{
			throw std::runtime_error("insufficient buffer space (int)");
		}

		std::memcpy(buff_ + write_pos_, &in, sizeof(int));
		write_pos_ += sizeof(int);

		return *this;
	}

	SerializeBuffer& operator<<(long in)
	{
		if (write_pos_ + sizeof(long) > capacity_)
		{
			throw std::runtime_error("insufficient buffer space (long)");
		}

		std::memcpy(buff_ + write_pos_, &in, sizeof(long));
		write_pos_ += sizeof(long);

		return *this;
	}

	SerializeBuffer& operator<<(long long in)
	{
		if (write_pos_ + sizeof(long long) > capacity_)
		{
			throw std::runtime_error("insufficient buffer space (long long)");
		}

		std::memcpy(buff_ + write_pos_, &in, sizeof(long long));
		write_pos_ += sizeof(long long);

		return *this;
	}

	SerializeBuffer& operator<<(unsigned char in)
	{
		if (write_pos_ + sizeof(unsigned char) > capacity_)
		{
			throw std::runtime_error("insufficient buffer space (unsigned char)");
		}

		std::memcpy(buff_ + write_pos_, &in, sizeof(unsigned char));
		write_pos_ += sizeof(unsigned char);

		return *this;
	}

	SerializeBuffer& operator<<(unsigned short in)
	{
		if (write_pos_ + sizeof(unsigned short) > capacity_)
		{
			throw std::runtime_error("insufficient buffer space (unsigned short)");
		}

		std::memcpy(buff_ + write_pos_, &in, sizeof(unsigned short));
		write_pos_ += sizeof(unsigned short);

		return *this;
	}

	SerializeBuffer& operator<<(unsigned int in)
	{
		if (write_pos_ + sizeof(unsigned int) > capacity_)
		{
			throw std::runtime_error("insufficient buffer space (unsigned int)");
		}

		std::memcpy(buff_ + write_pos_, &in, sizeof(unsigned int));
		write_pos_ += sizeof(unsigned int);

		return *this;
	}

	SerializeBuffer& operator<<(unsigned long in)
	{
		if (write_pos_ + sizeof(unsigned long) > capacity_)
		{
			throw std::runtime_error("insufficient buffer space (unsigned long)");
		}

		std::memcpy(buff_ + write_pos_, &in, sizeof(unsigned long));
		write_pos_ += sizeof(unsigned long);

		return *this;
	}


	SerializeBuffer& operator<<(unsigned long long in)
	{
		if (write_pos_ + sizeof(unsigned long long) > capacity_)
		{
			throw std::runtime_error("insufficient buffer space (unsigned long long)");
		}

		std::memcpy(buff_ + write_pos_, &in, sizeof(unsigned long long));
		write_pos_ += sizeof(unsigned long long);

		return *this;
	}

	SerializeBuffer& operator<<(float in)
	{
		if (write_pos_ + sizeof(float) > capacity_)
		{
			throw std::runtime_error("insufficient buffer space (float)");
		}

		std::memcpy(buff_ + write_pos_, &in, sizeof(float));
		write_pos_ += sizeof(float);

		return *this;
	}

	SerializeBuffer& operator<<(double in)
	{
		if (write_pos_ + sizeof(double) > capacity_)
		{
			throw std::runtime_error("insufficient buffer space (double)");
		}

		std::memcpy(buff_ + write_pos_, &in, sizeof(double));
		write_pos_ += sizeof(double);

		return *this;
	}

	SerializeBuffer& operator<<(long double in)
	{
		if (write_pos_ + sizeof(long double) > capacity_)
		{
			throw std::runtime_error("insufficient buffer space (long double)");
		}

		std::memcpy(buff_ + write_pos_, &in, sizeof(long double));
		write_pos_ += sizeof(long double);

		return *this;
	}

	SerializeBuffer& operator>>(bool& out)
	{
		if (write_pos_ - read_pos_ < sizeof(bool))
		{
			throw std::runtime_error("insufficient buffer data (bool)");
		}

		std::memcpy(&out, buff_ + read_pos_, sizeof(bool));
		read_pos_ += sizeof(bool);

		return *this;
	}

	SerializeBuffer& operator>>(char& out)
	{
		if (write_pos_ - read_pos_ < sizeof(char))
		{
			throw std::runtime_error("insufficient buffer data (char)");
		}

		std::memcpy(&out, buff_ + read_pos_, sizeof(char));
		read_pos_ += sizeof(char);

		return *this;
	}

#if __cplusplus >= 202002L
	SerializeBuffer& operator>>(char8_t& out)
	{
		if (write_pos_ - read_pos_ < sizeof(char8_t))
		{
			throw std::runtime_error("insufficient buffer data (char8_t)");
		}

		std::memcpy(&out, buff_ + read_pos_, sizeof(char8_t));
		read_pos_ += sizeof(char8_t);

		return *this;
	}
#endif

	SerializeBuffer& operator>>(char16_t& out)
	{
		if (write_pos_ - read_pos_ < sizeof(char16_t))
		{
			throw std::runtime_error("insufficient buffer data (char16_t)");
		}

		std::memcpy(&out, buff_ + read_pos_, sizeof(char16_t));
		read_pos_ += sizeof(char16_t);

		return *this;
	}

	SerializeBuffer& operator>>(char32_t& out)
	{
		if (write_pos_ - read_pos_ < sizeof(char32_t))
		{
			throw std::runtime_error("insufficient buffer data (char32_t)");
		}

		std::memcpy(&out, buff_ + read_pos_, sizeof(char32_t));
		read_pos_ += sizeof(char32_t);

		return *this;
	}

	SerializeBuffer& operator>>(wchar_t& out)
	{
		if (write_pos_ - read_pos_ < sizeof(wchar_t))
		{
			throw std::runtime_error("insufficient buffer data (wchar_t)");
		}

		std::memcpy(&out, buff_ + read_pos_, sizeof(wchar_t));
		read_pos_ += sizeof(wchar_t);

		return *this;
	}

	SerializeBuffer& operator>>(signed char& out)
	{
		if (write_pos_ - read_pos_ < sizeof(signed char))
		{
			throw std::runtime_error("insufficient buffer data (signed char)");
		}

		std::memcpy(&out, buff_ + read_pos_, sizeof(signed char));
		read_pos_ += sizeof(signed char);

		return *this;
	}

	SerializeBuffer& operator>>(short& out)
	{
		if (write_pos_ - read_pos_ < sizeof(short))
		{
			throw std::runtime_error("insufficient buffer data (short)");
		}

		std::memcpy(&out, buff_ + read_pos_, sizeof(short));
		read_pos_ += sizeof(short);

		return *this;
	}

	SerializeBuffer& operator>>(int& out)
	{
		if (write_pos_ - read_pos_ < sizeof(int))
		{
			throw std::runtime_error("insufficient buffer data (int)");
		}

		std::memcpy(&out, buff_ + read_pos_, sizeof(int));
		read_pos_ += sizeof(int);

		return *this;
	}

	SerializeBuffer& operator>>(long& out)
	{
		if (write_pos_ - read_pos_ < sizeof(long))
		{
			throw std::runtime_error("insufficient buffer data (long)");
		}

		std::memcpy(&out, buff_ + read_pos_, sizeof(long));
		read_pos_ += sizeof(long);

		return *this;
	}

	SerializeBuffer& operator>>(long long& out)
	{
		if (write_pos_ - read_pos_ < sizeof(long long))
		{
			throw std::runtime_error("insufficient buffer data (long long)");
		}

		std::memcpy(&out, buff_ + read_pos_, sizeof(long long));
		read_pos_ += sizeof(long long);

		return *this;
	}

	SerializeBuffer& operator>>(unsigned char& out)
	{
		if (write_pos_ - read_pos_ < sizeof(unsigned char))
		{
			throw std::runtime_error("insufficient buffer data (unsigned char)");
		}

		std::memcpy(&out, buff_ + read_pos_, sizeof(unsigned char));
		read_pos_ += sizeof(unsigned char);

		return *this;
	}

	SerializeBuffer& operator>>(unsigned short& out)
	{
		if (write_pos_ - read_pos_ < sizeof(unsigned short))
		{
			throw std::runtime_error("insufficient buffer data (unsigned short)");
		}

		std::memcpy(&out, buff_ + read_pos_, sizeof(unsigned short));
		read_pos_ += sizeof(unsigned short);

		return *this;
	}

	SerializeBuffer& operator>>(unsigned int& out)
	{
		if (write_pos_ - read_pos_ < sizeof(unsigned int))
		{
			throw std::runtime_error("insufficient buffer data (unsigned int)");
		}

		std::memcpy(&out, buff_ + read_pos_, sizeof(unsigned int));
		read_pos_ += sizeof(unsigned int);

		return *this;
	}

	SerializeBuffer& operator>>(unsigned long& out)
	{
		if (write_pos_ - read_pos_ < sizeof(unsigned long))
		{
			throw std::runtime_error("insufficient buffer data (unsigned long)");
		}

		std::memcpy(&out, buff_ + read_pos_, sizeof(unsigned long));
		read_pos_ += sizeof(unsigned long);

		return *this;
	}

	SerializeBuffer& operator>>(unsigned long long& out)
	{
		if (write_pos_ - read_pos_ < sizeof(unsigned long long))
		{
			throw std::runtime_error("insufficient buffer data (unsigned long long)");
		}

		std::memcpy(&out, buff_ + read_pos_, sizeof(unsigned long long));
		read_pos_ += sizeof(unsigned long long);

		return *this;
	}

	SerializeBuffer& operator>>(float& out)
	{
		if (write_pos_ - read_pos_ < sizeof(float))
		{
			throw std::runtime_error("insufficient buffer data (float)");
		}

		std::memcpy(&out, buff_ + read_pos_, sizeof(float));
		read_pos_ += sizeof(float);

		return *this;
	}

	SerializeBuffer& operator>>(double& out)
	{
		if (write_pos_ - read_pos_ < sizeof(double))
		{
			throw std::runtime_error("insufficient buffer data (double)");
		}

		std::memcpy(&out, buff_ + read_pos_, sizeof(double));
		read_pos_ += sizeof(double);

		return *this;
	}

	SerializeBuffer& operator>>(long double& out)
	{
		if (write_pos_ - read_pos_ < sizeof(long double))
		{
			throw std::runtime_error("insufficient buffer data (long double)");
		}

		std::memcpy(&out, buff_ + read_pos_, sizeof(long double));
		read_pos_ += sizeof(long double);

		return *this;
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

	// 사용량
	size_t size() const noexcept
	{
		return (write_pos_ - read_pos_);
	}

	// 가용량
	size_t available() const noexcept
	{
		return (capacity_ - write_pos_);
	}

	// move_tail
	size_t move_write_pos(size_t len) noexcept
	{
		size_t remaining = capacity_ - write_pos_;
		size_t move_size = len < remaining ? len : remaining;

		write_pos_ += move_size;

		return move_size;
	}

	// move_head
	size_t move_read_pos(size_t len) noexcept
	{
		size_t data_size = write_pos_ - read_pos_;
		size_t move_size = len < data_size ? len : data_size;

		read_pos_ += move_size;

		return move_size;
	}

	// enqueue
	size_t write(const char* src, size_t len) noexcept
	{
		if (capacity_ - write_pos_ < len)
		{
			return 0;
		}

		std::memcpy(buff_ + write_pos_, src, len);
		write_pos_ += len;

		return len;
	}

	// dequeue
	size_t read(char* dest, size_t len) noexcept
	{
		if (write_pos_ - read_pos_ < len)
		{
			return 0;
		}

		std::memcpy(dest, buff_ + read_pos_, len);
		read_pos_ += len;

		return len;
	}

	size_t peek(char* dest, size_t len) const noexcept
	{
		if (write_pos_ - read_pos_ < len)
		{
			return 0;
		}

		std::memcpy(dest, buff_ + read_pos_, len);

		return len;
	}

	void clear() noexcept
	{
		read_pos_ = 0;
		write_pos_ = 0;
	}

private:
	char* buff_;
	size_t read_pos_; // head
	size_t write_pos_; // tail
	size_t capacity_;
};