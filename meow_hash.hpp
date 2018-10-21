#pragma once
#include <cstdint>
#include <array>
#include <cstring>
#include <vector>
#include <initializer_list>
#include <string>
#include <type_traits>

#ifdef _MSC_VER
#include <intrin.h>
#else
#ifdef __GNUC__
#include <x86intrin.h>
#endif
#endif

/* ========================================================================
Meow - A Fast Non-cryptographic Hash for Large Data Sizes
(C) Copyright 2018 by Molly Rocket, Inc. (https://mollyrocket.com)

See https://mollyrocket.com/meowhash for details.

========================================================================

LICENSE

This software is provided 'as-is', without any express or implied
warranty. In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not
claim that you wrote the original software. If you use this software
in a product, an acknowledgement in the product documentation would be
appreciated but is not required.

2. Altered source versions must be plainly marked as such, and must not be
misrepresented as being the original software.

3. This notice may not be removed or altered from any source distribution.

========================================================================


FAQ

Q: What is it?
A: Meow is a 512-bit non-cryptographic hash that operates at high speeds
on x64 processors.  It is designed to be truncatable to 256, 128, 64,
and 32-bit hash values and still retain good collision resistance.

Q: What is it GOOD for?
A: Quickly hashing large amounts of data for comparison purposes such as
block deduplication or file verification.  As of its publication in
October of 2018, it was the fastest hash in the smhasher suite by
a factor of 3, but it still passes all smhasher tests and has not
yet produced any spurious collisions in practical deployment as compared
to a baseline of SHA-1.  It is also designed to get faster with age:
it already contains 256-wide and 512-wide hash-equivalent versions
that can be enabled for potentially 4x faster performance on future
VAES x64 chips when they are available.

Q: What is it BAD for?
A: Anything security-related.  It is not designed for security and has
not be analyzed for security.  It should be assumed that it offers
no security whatsoever.  It is also not designed for small input
sizes, so although it _can_ hash 1 byte, 4 bytes, 32 bytes, etc.,
it will end up wasting a lot of time on padding since its minimum
block size is 256 bytes.  Generally speaking, if you're not usually
hashing a kilobyte or more, this is probably not the hash you're
looking for.

Q: Who wrote it and why?
A: It was written by Casey Muratori (https://caseymuratori.com) for use
in processing large-footprint assets for the game 1935
(https://molly1935.com).  The original system used an SHA-1 hash (which
is not designed for speed), and so to eliminate hashing bottlenecks
in the pipeline, the Meow hash was designed to produce equivalent
quality 256-bit hash values as a drop-in replacement that would take
a fraction of the CPU time.
This rewrite was written by Piotr Pliszka. (https://github.com/RedSpah)

Q: Why is it called the "Meow hash"?
A: It was created while Meow the Infinite (https://meowtheinfinite.com)
was in development at Molly Rocket, so there were lots of Meow the
Infinite drawings happening at that time.

Q: How does it work?
A: It was designed to be the fastest possible hash that produces
collision-free hash values in practice and passes standard hash
quality checks.  It uses the built-in AES acceleration provided by
modern CPUs and computes sixteen hash streams in parallel to avoid
serial dependency stalls.  The sixteen separate hash results are
then hashed themselves to produce the final hash value.  While only
four hash streams would suffice for maximum performance on today's
machines, hypothetical future chips will likely want sixteen.
Meow was designed to be future-proof by using sixteen streams up
front, so in the 2020 time frame when such chips start appearing,
wider execution of Meow can be enabled without needing to change
any persistent hash values stored in codebases, databases, etc.

======================================================================== */

// A force inline macro to match the original's behaviour of having AESMerge and AESLoad as macros.
#ifdef _MSC_VER    /* Visual Studio */
#define MEOWH_FORCE_STATIC_INLINE static __forceinline
#else 
#ifdef __GNUC__ /* gcc, clang */
#define MEOWH_FORCE_STATIC_INLINE static inline __attribute__((always_inline))
#else
#define MEOWH_FORCE_STATIC_INLINE static inline
#endif
#endif



// Feature test macros, Visual Studio
#ifdef _MSC_VER


#if (_M_IX86_FP >= 1) || (defined(_M_AMD64) || defined(_M_X64))
#define _MEOWH_128 
#endif

// broken?
#if defined(__AVX__) || defined(__AVX2__)
#define _MEOWH_256 
#endif

/* because there isn't a macro I can check to test for AVX512F support,
 * Visual Studio won't be able to support it for now
 * unless _MEOWH_512 is defined before including the header. */

#else
#endif

#ifdef __GNUC__

#ifdef __SSE__
#define _MEOWH_128
#endif

#ifdef __AVX__
#define _MEOWH_256
#endif

#ifdef __AVX512F__
#define _MEOWH_512
#endif

#endif // __GNUC__




namespace meowh
{
	namespace types
	{
		template <size_t N>
		struct hash_type { using type = void; };


		// common
		using hash_32_underlying = uint32_t;
		template <>
		struct hash_type<32> { using type = hash_32_underlying; };

		using hash_64_underlying = uint64_t;
		template <>
		struct hash_type<64> { using type = hash_64_underlying; };

		// xmm
#ifdef _MEOWH_128
		using hash_128_underlying = __m128i;
		template <>
		struct hash_type<128> { using type = hash_128_underlying; };
#endif

		// ymm
#ifdef _MEOWH_256
		using hash_256_underlying = __m256i;
		template <>
		struct hash_type<256> { using type = hash_256_underlying; };
#endif

		// zmm
#ifdef _MEOWH_512
		using hash_512_underlying = __m512i;
		template <>
		struct hash_type<512> { using type = hash_512_underlying; };
#endif
	}

	template <size_t N>
	using hash_type_t = typename types::hash_type<N>::type;


	template <size_t N>
	struct alignas(64) hash_t
	{
	public:

		static_assert(N == 32 || N == 64 ||
			(N == 128 && !std::is_same<hash_type_t<128>, void>::value) ||
			(N == 256 && !std::is_same<hash_type_t<256>, void>::value) ||
			(N == 512 && !std::is_same<hash_type_t<512>, void>::value),
			"meowh::hash_t can only be declared with 32, 64, 128, 256 and 512 element bit width,"
			"and for the latter three, only if your CPU supports SSE, AVX, and AVX512F respectively.");

		std::array<hash_type_t<N>, 512 / N> elem;

		hash_t() {};

		template <size_t A>
		hash_t<N>& operator= (const hash_t<A>& other)
		{
			if (this != &other)
			{
				std::memcpy(reinterpret_cast<void*>(elem.data()), reinterpret_cast<const void*>(other.elem.data()), 64);
			}
			return *this;
		}

		template <size_t A>
		hash_t<N>& operator= (hash_t<A> other)
		{
			if (this != &other)
			{
				std::memcpy(reinterpret_cast<void*>(elem.data()), reinterpret_cast<const void*>(other.elem.data()), 64);
			}
			return *this;
		}

		hash_type_t<N>& operator[](size_t n)
		{
			return elem[n];
		}

		const hash_type_t<N>& operator[](size_t n) const
		{
			return elem[n];
		}

		template <size_t A>
		operator hash_t<A>() const
		{
			hash_t<A> transmuted;
			std::memcpy(reinterpret_cast<void*>(transmuted.elem.data()), reinterpret_cast<const void*>(elem.data()), 64);
			return transmuted;
		}

		template <size_t A>
		hash_type_t<A> as(size_t index) const
		{
			static_assert(N == 32 || N == 64 ||
				(N == 128 && !std::is_same<hash_type_t<128>, void>::value) ||
				(N == 256 && !std::is_same<hash_type_t<256>, void>::value) ||
				(N == 512 && !std::is_same<hash_type_t<512>, void>::value),
				"meowh::hash_t can only be declared with 32, 64, 128, 256 and 512 element bit width,"
				"and for the latter three, only if your CPU supports SSE, AVX, and AVX512F respectively.");

			hash_type_t<A> transmuted;
			std::memcpy(reinterpret_cast<void*>(&transmuted), reinterpret_cast<const uint8_t*>(elem.data()) + ((A / 8) * index), A / 8);
			return transmuted;
		}

		bool operator!=(const hash_t<N>& other)
		{
			return !!(memcmp(elem.data(), other.elem.data(), 64));
		}


	};

	using hash32_t = hash_t<32>;
	using hash64_t = hash_t<64>;
	using hash128_t = hash_t<128>;
	using hash256_t = hash_t<256>;
	using hash512_t = hash_t<512>;

	namespace detail
	{
		template <size_t N>
		MEOWH_FORCE_STATIC_INLINE hash_type_t<N> unaligned_read(const void* src)
		{
			hash_type_t<N> val;
			std::memcpy(reinterpret_cast<void*>(&val), src, sizeof(val));
			return val;
		}

		template <size_t N>
		MEOWH_FORCE_STATIC_INLINE void aes_merge(hash_t<N>& a, const hash_t<N>& b)
		{
			if constexpr (N == 128)
			{
				a[0] = _mm_aesdec_si128(a[0], b[0]);
				a[1] = _mm_aesdec_si128(a[1], b[1]);
				a[2] = _mm_aesdec_si128(a[2], b[2]);
				a[3] = _mm_aesdec_si128(a[3], b[3]);
			}
			else if constexpr (N == 256)
			{
				a[0] = _mm256_aesdec_epi128(a[0], b[0]);
				a[1] = _mm256_aesdec_epi128(a[1], b[1]);
			}
			else if constexpr (N == 512)
			{
				a[0] = _mm512_aesdec_epi128(a[0], b[0]);
			}
		}

		template <size_t N>
		MEOWH_FORCE_STATIC_INLINE void aes_rotate(hash_t<N>& a, hash_t<N>& b)
		{
			aes_merge<N>(a, b);

			if constexpr (N == 128)
			{
				hash_type_t<128> tmp = b[0];
				b[0] = b[1];
				b[1] = b[2];
				b[2] = b[3];
				b[3] = tmp;
			}
			else
			{
				hash_t<128> rot_b = b;

				hash_type_t<128> tmp = rot_b[0];
				rot_b[0] = rot_b[1];
				rot_b[1] = rot_b[2];
				rot_b[2] = rot_b[3];
				rot_b[3] = tmp;

				b = rot_b;


			}

		}

		template <size_t N, bool Align, typename ptr_arg_t = typename std::conditional<Align == true, const hash_type_t<N>*, const uint8_t*>::type>
		MEOWH_FORCE_STATIC_INLINE void aes_load(hash_t<N>& a, ptr_arg_t src)
		{
			if constexpr (Align)
			{
				if constexpr (N == 128)
				{
					a[0] = _mm_aesdec_si128(a[0], *(src));
					a[1] = _mm_aesdec_si128(a[1], *(src + 1));
					a[2] = _mm_aesdec_si128(a[2], *(src + 2));
					a[3] = _mm_aesdec_si128(a[3], *(src + 3));
				}
				else if constexpr (N == 256)
				{
					a[0] = _mm256_aesdec_epi128(a[0], *(src));
					a[1] = _mm256_aesdec_epi128(a[1], *(src + 1));
				}
				else if constexpr (N == 512)
				{
					a[0] = _mm512_aesdec_epi128(a[0], *(src));
				}
			}
			else
			{
				if constexpr (N == 128)
				{
					a[0] = _mm_aesdec_si128(a[0], unaligned_read<128>(src));
					a[1] = _mm_aesdec_si128(a[1], unaligned_read<128>(src + 16));
					a[2] = _mm_aesdec_si128(a[2], unaligned_read<128>(src + 32));
					a[3] = _mm_aesdec_si128(a[3], unaligned_read<128>(src + 48));
				}
				else if constexpr (N == 256)
				{
					a[0] = _mm256_aesdec_epi128(a[0], unaligned_read<256>(src));
					a[1] = _mm256_aesdec_epi128(a[1], unaligned_read<256>(src + 32));
				}
				else if constexpr (N == 512)
				{
					a[0] = _mm512_aesdec_epi128(a[0], unaligned_read<512>(src));
				}
			}
		}

		template <size_t N, bool Align = false, size_t R = N>
		static hash_t<R> meow_hash_impl(const uint8_t* src, uint64_t len, uint64_t seed)
		{
			hash_t<64> init_vector;

			init_vector[0] = init_vector[2] = init_vector[4] = init_vector[6] = seed;
			init_vector[1] = init_vector[3] = init_vector[5] = init_vector[7] = seed + len + 1;

			hash_t<N> stream_0123 = init_vector;
			hash_t<N> stream_4567 = init_vector;
			hash_t<N> stream_89AB = init_vector;
			hash_t<N> stream_CDEF = init_vector;

			uint64_t block_count = len / 256;
			len -= block_count * 256;

			if constexpr (Align == true)
			{
				const hash_type_t<N>* aligned_src = reinterpret_cast<const hash_type_t<N>*>(src);

				constexpr int32_t size_div = sizeof(hash_type_t<N>);

				while (block_count-- > 0)
				{
					aes_load<N, true>(stream_0123, aligned_src);
					aes_load<N, true>(stream_4567, aligned_src + (64 / size_div));
					aes_load<N, true>(stream_89AB, aligned_src + (128 / size_div));
					aes_load<N, true>(stream_CDEF, aligned_src + (192 / size_div));

					aligned_src += (256 / size_div);
				}

				if (len > 0)
				{
					std::array<hash_t<N>, 4> partial = { init_vector, init_vector, init_vector, init_vector };
					std::memcpy(reinterpret_cast<void*>(partial.data()), reinterpret_cast<const void*>(aligned_src), static_cast<size_t>(len));

					aes_merge<N>(stream_0123, partial[0]);
					aes_merge<N>(stream_4567, partial[1]);
					aes_merge<N>(stream_89AB, partial[2]);
					aes_merge<N>(stream_CDEF, partial[3]);
				}

			}
			else
			{
				while (block_count-- > 0)
				{
					aes_load<N, false>(stream_0123, src);
					aes_load<N, false>(stream_4567, src + 64);
					aes_load<N, false>(stream_89AB, src + 128);
					aes_load<N, false>(stream_CDEF, src + 192);
					src += 256;
				}

				if (len > 0)
				{
					std::array<hash_t<N>, 4> partial = { init_vector, init_vector, init_vector, init_vector };
					std::memcpy(reinterpret_cast<void*>(partial.data()), src, static_cast<size_t>(len));

					aes_merge<N>(stream_0123, partial[0]);
					aes_merge<N>(stream_4567, partial[1]);
					aes_merge<N>(stream_89AB, partial[2]);
					aes_merge<N>(stream_CDEF, partial[3]);
				}
			}


			hash_t<N> ret = init_vector;

			aes_rotate<N>(ret, stream_0123);
			aes_rotate<N>(ret, stream_4567);
			aes_rotate<N>(ret, stream_89AB);
			aes_rotate<N>(ret, stream_CDEF);

			aes_rotate<N>(ret, stream_0123);
			aes_rotate<N>(ret, stream_4567);
			aes_rotate<N>(ret, stream_89AB);
			aes_rotate<N>(ret, stream_CDEF);

			aes_rotate<N>(ret, stream_0123);
			aes_rotate<N>(ret, stream_4567);
			aes_rotate<N>(ret, stream_89AB);
			aes_rotate<N>(ret, stream_CDEF);

			aes_rotate<N>(ret, stream_0123);
			aes_rotate<N>(ret, stream_4567);
			aes_rotate<N>(ret, stream_89AB);
			aes_rotate<N>(ret, stream_CDEF);

			aes_merge<N>(ret, init_vector);
			aes_merge<N>(ret, init_vector);
			aes_merge<N>(ret, init_vector);
			aes_merge<N>(ret, init_vector);
			aes_merge<N>(ret, init_vector);

			return ret;
		}
	}

	template <size_t N, bool Align = false, size_t R = N>
	hash_t<R> meow_hash(const void* input, size_t len, uint64_t seed = 0)
	{
		static_assert(N == 128 || N == 256 || N == 512, "meow_hash can only be called in 128, 256, or 512 bit mode.");
		return detail::meow_hash_impl<N, Align, R>(reinterpret_cast<const uint8_t*>(input), len, seed);
	}

	template <size_t N, bool Align = false, size_t R = N, typename T, size_t AN>
	hash_t<R> meow_hash(const std::array<T, AN>& input, uint64_t seed = 0)
	{
		static_assert(N == 128 || N == 256 || N == 512, "meow_hash can only be called in 128, 256, or 512 bit mode.");
		return detail::meow_hash_impl<N, Align, R>(reinterpret_cast<const uint8_t*>(input.data()), input.size() * sizeof(T), seed);
	}

	template <size_t N, bool Align = false, size_t R = N, typename T>
	hash_t<R> meow_hash(const std::basic_string<T>& input, uint64_t seed = 0)
	{
		static_assert(N == 128 || N == 256 || N == 512, "meow_hash can only be called in 128, 256, or 512 bit mode.");
		return detail::meow_hash_impl<N, Align, R>(reinterpret_cast<const uint8_t*>(input.data()), input.length() * sizeof(T), seed);
	}

	template <size_t N, bool Align = false, size_t R = N, typename T>
	hash_t<R> meow_hash(const std::vector<T>& input, uint64_t seed = 0)
	{
		static_assert(N == 128 || N == 256 || N == 512, "meow_hash can only be called in 128, 256, or 512 bit mode.");
		return detail::meow_hash_impl<N, Align, R>(reinterpret_cast<const uint8_t*>(input.data()), input.size() * sizeof(T), seed);
	}

	template <size_t N, bool Align = false, size_t R = N, typename T>
	hash_t<R> meow_hash(const std::initializer_list<T>& input, uint64_t seed = 0)
	{
		static_assert(N == 128 || N == 256 || N == 512, "meow_hash can only be called in 128, 256, or 512 bit mode.");
		return detail::meow_hash_impl<N, Align, R>(reinterpret_cast<const uint8_t*>(input.begin()), input.size() * sizeof(T), seed);
	}

	template <size_t N, bool Align = false, size_t R = N, typename ContiguousIterator>
	hash_t<R> meow_hash(ContiguousIterator begin, ContiguousIterator end, uint64_t seed = 0)
	{
		static_assert(N == 128 || N == 256 || N == 512, "meow_hash can only be called in 128, 256, or 512 bit mode.");
		using T = std::decay_t<decltype(*end)>;
		return detail::meow_hash_impl<N, Align, R>(reinterpret_cast<const uint8_t*>(&*begin), (end - begin) * sizeof(T), seed);
	}


	constexpr int32_t meow_hash_version = 1;
	constexpr const char meow_hash_version_name[] = "0.1 Alpha - clean cpp edition";
}