#define __DIB_COMPRESSION_IMPL

#include "dib/compression.h"
#include "dib/bitstream.h"

#include <array>
#include <algorithm>
#include <execution>
#include <unordered_map>
#include <memory>
#include <numeric>
#include <bit>
#include <iostream>

using namespace dib::compression;

using uint = unsigned int;

auto div_round_up(std::integral auto x, std::integral auto y)
{
	return (x + (y - 1)) / y;
}

#define div_cond_round_up(c, x, y) ((c) ? div_round_up((x), (y)) : (x) / (y))

using EncodedValue = uint64_t;

struct ArithmeticEncodingTree
{
	EncodedValue min = 0;
	EncodedValue max = 0;

	std::unordered_map<uint8_t, std::unique_ptr<ArithmeticEncodingTree>> next;
	std::vector<ArithmeticEncodingTree *> next_by_value;

	uint8_t byte = 0;
	bool filled = false;

	ArithmeticEncodingTree *parent = nullptr;

	ArithmeticEncodingTree() {}
	ArithmeticEncodingTree(EncodedValue min, EncodedValue max, uint8_t byte, ArithmeticEncodingTree *parent)
		: min(min), max(max), byte(byte), parent(parent)
	{}
};

struct ArithmeticEncodingKey
{
	std::array<std::atomic<size_t>, 256> counts;
	std::array<size_t, 257> accumulative_counts;
	std::array<uint8_t, 256> sorted_counts;
	size_t total_size = 0;

	ArithmeticEncodingTree root;

	ArithmeticEncodingKey()
	{
		for (size_t i = 0; i <= 255; i++)
		{
			sorted_counts[i] = i;
		}

		root.min = std::numeric_limits<EncodedValue>::min();
		root.max = std::numeric_limits<EncodedValue>::max();
	}
};

void fill_encoding_tree(ArithmeticEncodingKey &key, ArithmeticEncodingTree &value, bool first_layer = false)
{
	EncodedValue cur = value.min;
	EncodedValue range = value.max - value.min - 1;

	auto element = key.sorted_counts.cbegin();

	while (cur != value.max && element != key.sorted_counts.cend() && key.counts[*element] != 0)
	{
		auto new_range = div_cond_round_up(!first_layer, range, key.total_size) * key.counts[*element];

		auto begin = cur;

		// If there is no more room left, clamp (assumes new_range is less than half of numeric_limits<uintmax_t>::max)
		if (cur > value.max - 1 - new_range || cur + new_range > value.max - 1)
		{
			cur = value.max - 1;
		}
		else
		{
			cur += new_range;
		}

		auto next = std::make_unique<ArithmeticEncodingTree>(begin, cur, *element, &value);

		// Since we are pushing back to next_by_value in ascending order of .min, 
		// it will be sorted by .min implicitly
		auto &next_ref = value.next[*element];
		next_ref = std::move(next);
		value.next_by_value.push_back(next_ref.get());

		cur++;
		element++;
	}

	value.filled = true;
}

template<std::integral T>
void put_buffer(std::vector<uint8_t> &buf, T val)
{
	using TArr = std::array<uint8_t, sizeof(T)>;
	TArr valarr = std::bit_cast<TArr>(val);

	if (std::endian::native == std::endian::big)
	{
		buf.insert(buf.cend(), valarr.begin(), valarr.end());
	}
	else
	{
		buf.insert(buf.cend(), valarr.rbegin(), valarr.rend());
	}
}

template<std::integral T>
T get_buffer(const uint8_t *buf, size_t &index)
{
	using TArr = std::array<uint8_t, sizeof(T)>;
	TArr valarr = std::bit_cast<TArr>(*(T*)buf);

	if (std::endian::native != std::endian::big)
	{
		std::reverse(valarr.begin(), valarr.end());
	}

	index += sizeof(T);
	return std::bit_cast<T>(valarr);
}

void finalize_key(ArithmeticEncodingKey &key)
{
	// Sort sorted_counts list //
	std::sort(key.sorted_counts.rbegin(), key.sorted_counts.rend(), [&](uint8_t x, uint8_t y) { return key.counts[x] < key.counts[y]; });

	// Calculate cumulative counts //
	key.accumulative_counts[0] = key.counts[0];
	for (size_t i = 1; i < 256; i++)
	{
		key.accumulative_counts[i] = key.counts[i] + key.accumulative_counts[i - 1];
	}

	key.accumulative_counts[256] = key.total_size;

	// Generate encoding tree //
	fill_encoding_tree(key, key.root, true);
}

Buffer buf_from_vec(const std::vector<uint8_t> &vec)
{
	uint8_t *outbuf_final = new uint8_t[vec.size()];
	std::copy_n(vec.data(), vec.size(), outbuf_final);

	return { outbuf_final, vec.size() };
}

// NOTES:
// to improve storage of the compression header, the best (smallest) of many 'strategies' can be employed:
//   
//   <bit-width type> [[dynamic]] or [[fixed]]
//		determines how many bits a symbol count is allocated; dynamically per instance or fixed for all counts
//		[[dynamic]] is better for files with a varied frequency for its symbols, [[fixed]] for those with uniform frequency
// 
//	 <skips> [[enabled]] or [[disabled]]
//		determines if symbols are to be 'skipped' by counting K values as 'inactive' and then V as 'active'
//		[[enabled]] is better for text files, [[disabled]] for other types of files
// 
// whatever combination of these choices yields the smallest header is chosen, and this choice can be encoded as
// the first two bits of the compressed output.
// 
// furthermore, the total symbol count need not be stored, as it can be calculated.

// FURTHER NOTE:
// if the entire data source was represented as ONE giant number, how would the compression compare?

Buffer dib::compression::compress_f(const Buffer buf)
{
	ArithmeticEncodingKey key;
	key.total_size = buf.size;

	// Calculate counts //
	std::for_each(std::execution::par_unseq, buf.begin(), buf.end(), [&](uint8_t byte)
	{
		key.counts[byte]++;
	});

	finalize_key(key);

	// Begin assembling output buffer //
	std::vector<uint8_t> outbuf;
	outbuf.reserve(buf.size);

	// First, encode the count list //
	bits::bit_ostream stream(std::back_inserter(outbuf));

	stream << bits::compressed_integer(key.total_size);
	for (size_t i = 0; i < 256; i++)
	{
		stream << bits::compressed_integer(key.counts[i]);
	}

	stream.flush();

	// Encode using magic //
	EncodedValue min = std::numeric_limits<EncodedValue>::min();
	EncodedValue max = std::numeric_limits<EncodedValue>::max();

	constexpr EncodedValue top_bit = (EncodedValue)1 << 63;

	for (uint16_t byte : buf)
	{
		EncodedValue nmin = min + (max - min)                      / key.total_size * key.accumulative_counts[byte];
		EncodedValue nmax = min + (max - min + key.total_size - 1) / key.total_size * key.accumulative_counts[byte + 1];

		min = nmin;
		max = nmax;

		while ((min & top_bit) == (max & top_bit))
		{
			stream << (bool)(min & top_bit);

			min <<= 1;
			max <<= 1;
			max |= 1;
		}
	}

	stream.flush();

	// Return as buffer //
	return buf_from_vec(outbuf);
}

Buffer dib::compression::decompress_f(const Buffer buf)
{
	ArithmeticEncodingKey key;

	// Read in count and size information //
	size_t idx = 0;
	auto bdata = buf.data;

	bits::bit_istream stream(bdata);

	bits::compressed_integer total_size_comp;
	stream >> total_size_comp;

	key.total_size = total_size_comp.value;

	for (size_t i = 0; i < 256; i++)
	{
		bits::compressed_integer count_comp;
		stream >> count_comp;

		key.counts[i] = count_comp.value;
	}

	idx = stream.iterator() - bdata + (int)!stream.end_of_byte();

	finalize_key(key);

	// Begin constructing output buffer //
	std::vector<uint8_t> outbuf;

	EncodedValue min = std::numeric_limits<EncodedValue>::min();
	EncodedValue max = std::numeric_limits<EncodedValue>::max();

	EncodedValue encoded_value = 0;
	uint8_t max_bit = 63;

	constexpr EncodedValue top_bit = (EncodedValue)1 << 63;

	while (outbuf.size() < key.total_size)
	{
		bool bit;
		stream >> bit;

		encoded_value |= (EncodedValue)bit << max_bit;

		if(max_bit > 0) max_bit--;
		else
		{
			encoded_value = 0;
			max_bit = 63;

			min = std::numeric_limits<EncodedValue>::min();
			max = std::numeric_limits<EncodedValue>::max();
		}

		auto byte_iter = std::lower_bound(key.accumulative_counts.begin(), key.accumulative_counts.end(), encoded_value, 
			[&](EncodedValue count, EncodedValue val)
			{
				EncodedValue count_norm = (max - min) / key.total_size * count + min;
				return count_norm < val;
			});

		auto byte_idx = byte_iter - key.accumulative_counts.begin();

		size_t acmin = key.accumulative_counts[byte_idx];
		size_t acmax = key.accumulative_counts[byte_idx + 1];

		if (acmin < encoded_value && (encoded_value | bits::flood_right((uint64_t)1 << max_bit)) < acmax)
		{
			outbuf.push_back(byte_idx);
			min = acmin;
			max = acmax;

			while ((min & top_bit) == (max & top_bit))
			{
				min <<= 1;
				max <<= 1;
				max |= 1;

				encoded_value <<= 1;
				max_bit++;
			}
		}
	}

	return buf_from_vec(outbuf);
}

Buffer dib::compression::compress(const Buffer buf)
{
	ArithmeticEncodingKey key;
	key.total_size = buf.size;

	// Calculate counts //
	std::for_each(std::execution::par_unseq, buf.begin(), buf.end(), [&](uint8_t byte)
	{
		key.counts[byte]++;
	});

	finalize_key(key);

	// Begin assembling output buffer //
	std::vector<uint8_t> outbuf;
	outbuf.reserve(buf.size);

	// First, encode the count list //
	bits::bit_ostream size_input(std::back_inserter(outbuf));

	size_input << bits::compressed_integer(key.total_size);
	for (size_t i = 0; i < 256; i++)
	{
		size_input << bits::compressed_integer(key.counts[i]);
	}

	size_input.flush();

	// Then, encode the data //
	size_t idx = 0;
	while (idx < buf.size)
	{
		ArithmeticEncodingTree *node = &key.root;

		while (idx != buf.size && node->next.contains(buf[idx]))
		{
			node = node->next.at(buf[idx++]).get();
			if (!node->filled) fill_encoding_tree(key, *node);
		}

		put_buffer(outbuf, node->max);
	}

	// Finally, shrink and return the buffer //
	return buf_from_vec(outbuf);
}

Buffer dib::compression::decompress(const Buffer buf)
{
	ArithmeticEncodingKey key;

	// Read in count and size information //
	size_t idx = 0;
	auto bdata = buf.data;

	bits::bit_istream size_stream(bdata);

	bits::compressed_integer total_size_comp;
	size_stream >> total_size_comp;

	key.total_size = total_size_comp.value;

	for (size_t i = 0; i < 256; i++)
	{
		bits::compressed_integer count_comp;
		size_stream >> count_comp;

		key.counts[i] = count_comp.value;
	}

	idx = size_stream.iterator() - bdata + (int)!size_stream.end_of_byte();

	finalize_key(key);

	// Begin constructing output buffer //
	std::vector<uint8_t> outbuf;

	while (idx < buf.size)
	{
		EncodedValue next = get_buffer<EncodedValue>(bdata, idx);
		ArithmeticEncodingTree *node = &key.root;

		while (outbuf.size() != key.total_size)
		{
			if (!node->filled) fill_encoding_tree(key, *node);

			if (next == node->max) break;

			node = *std::lower_bound(node->next_by_value.begin(), node->next_by_value.end(), next, [&](ArithmeticEncodingTree *node, EncodedValue val) { return node->min < val; });
			outbuf.push_back(node->byte);
		}
	}

	return buf_from_vec(outbuf);
}