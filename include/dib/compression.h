#ifndef __DIB_COMPRESSION_H
#define __DIB_COMPRESSION_H

#include <stddef.h>
#include <stdint.h>

#ifndef __DIB_COMPRESSION_IMPL
static_assert(false, "compression.h is unfinished.");
#endif

namespace dib::compression
{
	struct Buffer
	{
		uint8_t *data;
		size_t size;

		Buffer() : data(nullptr), size(0) {}
		Buffer(uint8_t *data, size_t size) : data(data), size(size) {}

		uint8_t *begin() { return data; }
		const uint8_t *begin() const { return data; }
		const uint8_t *cbegin() const { return data; }

		uint8_t *end() { return data + size; }
		const uint8_t *end() const { return data + size; }
		const uint8_t *cend() const { return data + size; }

		uint8_t &operator[](size_t index) { return data[index]; }
		uint8_t operator[](size_t index) const { return data[index]; }

		void free() { delete[] data; }
	};

	Buffer compress(const Buffer buf);
	Buffer decompress(const Buffer buf);

	Buffer compress_f(const Buffer buf);
	Buffer decompress_f(const Buffer buf);
}

#endif