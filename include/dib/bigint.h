#ifndef __DIB_BIGINT_H
#define __DIB_BIGINT_H

#include <stdint.h>
#include <concepts>

#ifndef __DIB_BIGINT_IMPL
static_assert(false, "bigint.h is unfinished.");
#endif

namespace dib::ints
{
	using i8 = int8_t;
	using i16 = int16_t;
	using i32 = int32_t;
	using i64 = int64_t;

	using u8 = uint8_t;
	using u16 = uint16_t;
	using u32 = uint32_t;
	using u64 = uint64_t;

	template<size_t N>
	struct intbytes
	{
		// big endian //
		u8 bytes[N];

		constexpr intbytes &operator<<=(u32 bits)
		{
			u32 bycnt = bits >> 3;
			u32 bicnt = bits & 0x7;

			u8 leftover = 0;

			for (int i = N - 1; i >= 0; i--)
			{
				if (i < bycnt) bytes[i] = 0;
				else
				{
					u8 n = bytes[i - bycnt] << bicnt | leftover;
					leftover = bytes[i - bycnt] >> (8 - bicnt);

					bytes[i] = n;
				}
			}

			return *this;
		}

		constexpr intbytes operator<<(u32 bits) const { intbytes c(*this); c <<= bits; return c; }

		constexpr intbytes &operator>>=(u32 bits)
		{
			u32 bycnt = bits >> 3;
			u32 bicnt = bits & 0x7;

			u8 leftover = 0;

			for (int i = 0; i < N; i++)
			{
				if (i >= N - bycnt) bytes[i] = 0;
				else
				{
					u8 n = bytes[i + bycnt] >> bicnt | leftover;
					leftover = bytes[i + bycnt] << (8 - bicnt);

					bytes[i] = n;
				}
			}

			return *this;
		}

		constexpr intbytes operator>>(u32 bits) const { intbytes c(*this); c >>= bits; return c; }

		template<std::unsigned_integral T> 
		constexpr intbytes(T value)
		{
			for (int i = 0; i < N; i++)
			{
				bytes[i] = value & 0xFF;
				value >>= 8;
			}
		}

		template<std::unsigned_integral T>
		constexpr operator T() const
		{
			T value = 0;
			for (int i = 0; i < N && i < sizeof(T); i++)
			{
				value |= ((T)bytes[i] << (i << 3));
			}
			return value;
		}
	};
}

#endif