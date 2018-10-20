#pragma once
#include <intrin.h>
#include <cstdint>
#include <array>
#include <cstring>


namespace meowh
{

	namespace types
	{
		using _hash_32_underlying = uint32_t;
		using _hash_64_underlying = uint64_t;
		using _hash_128_underlying = __m128i;
		using _hash_256_underlying = __m256i;
		using _hash_512_underlying = __m512i;

		template <size_t N>
		struct hash_type { using type = void; };
		template <>
		struct hash_type<32> { using type = _hash_32_underlying; };
		template <>
		struct hash_type<64> { using type = _hash_64_underlying; };
		template <>
		struct hash_type<128> { using type = _hash_128_underlying; };
		template <>
		struct hash_type<256> { using type = _hash_256_underlying; };
		template <>
		struct hash_type<512> { using type = _hash_512_underlying; };		
	}

	template <size_t N>
	using hash_type_t = typename types::hash_type<N>::type;
	
	template <size_t N>
	struct alignas(64) hash_t
	{
	public:

		static_assert(N == 32 || N == 64 || N == 128 || N == 256 || N == 512, "meowh::hash_t can only be declared with 32, 64, 128, 256 and 512 element bit width.");
	 
		std::array<hash_type_t<N>, 512 / N> elem;

	

		template <size_t A>
		hash_t(const hash_t<A>& other)
		{
			std::memcpy(reinterpret_cast<uint8_t*>(elem.data()), reinterpret_cast<const uint8_t*>(other.elem.data()), 64);
		}

		template <size_t A>
		hash_t(hash_t<A> other)
		{
			std::memcpy(reinterpret_cast<void*>(elem.data()), reinterpret_cast<const void*>(other.elem.data()), 64);
		}

		hash_t() {};

		template <size_t A>
		hash_t<N>& operator= (const hash_t<A>& other)
		{
			std::memcpy(reinterpret_cast<void*>(elem.data()), reinterpret_cast<const void*>(other.elem.data()), 64);
			return *this;
		}

		template <size_t A>
		hash_t<N>& operator= (hash_t<A> other)
		{
			std::memcpy(reinterpret_cast<void*>(elem.data()), reinterpret_cast<const void*>(other.elem.data()), 64);
			return *this;
		}

		hash_type_t<N>& operator[](size_t N)
		{
			return elem[N];
		}

		const hash_type_t<N>& operator[](size_t N) const
		{
			return elem[N];
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
			static_assert(A == 32 || A == 64 || A == 128 || A == 256 || A == 512, "meowh::hash_t can only be declared with 32, 64, 128, 256 and 512 element bit width.");

			hash_type_t<A> transmuted;
			std::memcpy(reinterpret_cast<void*>(&transmuted), reinterpret_cast<const uint8_t*>(elem.data()) + ((A / 8) * index), A / 8);
			return transmuted;
		}

		
	};

	namespace helper
	{
		template <size_t N>
		inline static hash_type_t<N> unaligned_read(const void* src)
		{
			hash_type_t<N> val;
			std::memcpy(reinterpret_cast<void*>(&val), src, sizeof(val));
			return val;
		}
	}

	namespace impl
	{
		template <size_t N>
		inline static void aes_merge(hash_t<N>& a, const hash_t<N>& b)
		{
			if constexpr (N == 128)
			{
				a[0] = _mm_aesdec_si128(a[0] , b[0]);
				a[1] = _mm_aesdec_si128(a[1] , b[1]);
				a[2] = _mm_aesdec_si128(a[2] , b[2]);
				a[3] = _mm_aesdec_si128(a[3] , b[3]);
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
		inline static void aes_rotate(hash_t<N>& a, hash_t<N>& b)
		{
			aes_merge<N>(a, b);

			hash_t<128> rot_b = b;

			hash_type_t<128> tmp = rot_b[0];
			rot_b[0] = rot_b[1];
			rot_b[1] = rot_b[2];
			rot_b[2] = rot_b[3];
			rot_b[3] = tmp;

			b = rot_b;
		}

		template <size_t N>
		inline static void aes_load(hash_t<N>& a, const uint8_t* src)
		{
			if constexpr (N == 128)
			{
				a[0] = _mm_aesdec_si128(a[0], helper::unaligned_read<128>(src));
				a[1] = _mm_aesdec_si128(a[1], helper::unaligned_read<128>(src + 16));
				a[2] = _mm_aesdec_si128(a[2], helper::unaligned_read<128>(src + 32));
				a[3] = _mm_aesdec_si128(a[3], helper::unaligned_read<128>(src + 48));
			}
			else if constexpr (N == 256)
			{
				a[0] = _mm256_aesdec_epi128(a[0], helper::unaligned_read<256>(src));
				a[1] = _mm256_aesdec_epi128(a[1], helper::unaligned_read<256>(src + 32));
			}
			else if constexpr (N == 512)
			{
				a[0] = _mm512_aesdec_epi128(a[0], helper::unaligned_read<512>(src));
			}
		}

		template <size_t N, size_t R = N>
		inline static hash_t<R> meow_hash_impl(const uint8_t* src, uint64_t len, uint64_t seed)
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

			while (block_count-- > 0)
			{
				aes_load<N>(stream_0123, src);
				aes_load<N>(stream_4567, src + 64);
				aes_load<N>(stream_89AB, src + 128);
				aes_load<N>(stream_CDEF, src + 192);
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

	template <size_t N, size_t R = N>
	hash_t<R> meow_hash(const void* src, uint64_t len, uint64_t seed)
	{
		static_assert(N == 128 || N == 256 || N == 512, "meow_hash can only be called in 128, 256, or 512 bit mode.");
		return impl::meow_hash_impl<N, R>(reinterpret_cast<const uint8_t*>(src), len, seed);
	}

	constexpr int32_t meow_hash_version = 1;
	constexpr const char* meow_hash_version_name = "0.1 Alpha - cpp edition";
}