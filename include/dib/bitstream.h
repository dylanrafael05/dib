#ifndef __DIB_BITSTREAM_H
#define __DIB_BITSTREAM_H

#include <stdint.h>
#include <stddef.h>
#include <algorithm>

namespace dib::bits
{
	template<std::integral T>
	void swap_endian(T &value)
	{
		std::reverse((uint8_t *)&value, (uint8_t *)&value + sizeof(T));
	}

	template<std::integral T>
	T flood_right(T value)
	{
		value |= value >> 1;
		value |= value >> 2;
		value |= value >> 4;

		if constexpr (sizeof(T) <= 1) return value;
		value |= value >> 8;

		if constexpr (sizeof(T) <= 2) return value;
		value |= value >> 16;
		if constexpr (sizeof(T) <= 4) return value;
		value |= value >> 32;

		return value;
	}

	constexpr uint8_t most_significant_bit(uint64_t value)
	{
		uint8_t result = 0;

		#define msb_handle(c) \
			if(value >= ((uint64_t)1 << c)) { value >>= c; result += c; }

		msb_handle(32);
		msb_handle(16);
		msb_handle(8);
		msb_handle(4);
		msb_handle(2);
		msb_handle(1);

		#undef msb_handle

		return result;
	}

	struct compressed_integer
	{
		uint64_t value;

		compressed_integer() : value(0) {}
		compressed_integer(uint64_t value) : value(value) {}
	};

	struct bit_integer
	{
		uint8_t size;
		uint64_t value;

		bit_integer() = delete;

		bit_integer(uint8_t size)
			: size(size), value(0)
		{}

		bit_integer(uint8_t size, uint64_t value)
			: size(size), value(value)
		{}
	};

	template<std::input_iterator Iterator>
	class bit_istream
	{
		Iterator _iter;
		uint8_t _byte;
		uint8_t _bit_mask = 0x1;

	public:
		bit_istream(const Iterator &iter)
			: _iter(iter), _byte(*_iter)
		{}

		auto &iterator() const { return _iter; }
		auto end_of_byte() const { return _bit_mask == 0x1; }

		friend bit_istream &operator>>(bit_istream &stream, bool &value)
		{
			value = stream._byte & stream._bit_mask;

			stream._bit_mask <<= 1;
			if (stream._bit_mask == 0)
			{
				++stream._iter;
				stream._bit_mask = 1;
			}

			return stream;
		}

		friend bit_istream &operator>>(bit_istream &stream, bit_integer &value)
		{
			for (uint64_t i = 0; i < value.size; i++)
			{
				bool bit;
				stream >> bit;

				value.value <<= 1;
				value.value |= (uint64_t)bit;
			}

			// if (std::endian::native != std::endian::big)
			//  	swap_endian(value.value);

			return stream;
		}

		friend bit_istream &operator>>(bit_istream &stream, compressed_integer &value)
		{
			bit_integer size(6);
			stream >> size;

			bit_integer bitvalue(size.value + 1);
			stream >> bitvalue;

			value.value = bitvalue.value;

			return stream;
		}
	};

	template<std::output_iterator<uint8_t> Iterator>
	struct bit_ostream
	{
		Iterator _iter;
		uint8_t _byte = 0;
		uint8_t _bit_count = 0;

	public:
		bit_ostream(const Iterator &iter)
			: _iter(iter)
		{}

		auto &iterator() const { return _iter; }
		auto end_of_byte() const { return _bit_count == 0; }

		friend bit_ostream &operator<<(bit_ostream &stream, bool value)
		{
			stream._byte |= (uint8_t)value << stream._bit_count;
			stream._bit_count++;

			if (stream._bit_count == 8)
			{
				stream.flush();
			}

			return stream;
		}

		friend bit_ostream &operator<<(bit_ostream &stream, bit_integer value)
		{
			// if (std::endian::native != std::endian::big)
			//  	swap_endian(value.value);

			for (uint64_t i = 0; i < value.size; i++)
			{
				stream << (bool)(value.value & 1);
				value.value >>= 1;
			}

			return stream;
		}

		friend bit_ostream &operator<<(bit_ostream &stream, compressed_integer value)
		{
			auto size = most_significant_bit(value.value);

			stream << bit_integer(6, size)
				   << bit_integer(size + 1, value.value);

			return stream;
		}

		void flush()
		{
			if (_bit_count > 0)
			{
				*_iter++ = _byte;
				_bit_count = 0;
				_byte = 0;
			}
		}
	};
}

#endif