#include <immintrin.h>
#include <x86intrin.h>

#include <cstdint>

namespace base64 {

    namespace avx512 {

#define packed_dword(x) _mm512_set1_epi32(x)
#define packed_byte(x) packed_dword((x) | ((x) << 8) | ((x) << 16) | ((x) << 24))

        static const char* lookup = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        static uint32_t lookup_0[64];
        static uint32_t lookup_1[64];
        static uint32_t lookup_2[64];
        static uint32_t lookup_3[64];

        void initialize() {
            for (int i=0; i < 64; i++) {
                const uint32_t val = lookup[i];

                lookup_0[i] = val;
                lookup_1[i] = val << 8;
                lookup_2[i] = val << 16;
                lookup_3[i] = val << 24;
            }
        }

        void dump(__m512i v) {
            static uint8_t buf[64];

            _mm512_storeu_si512(reinterpret_cast<__m512i*>(buf), v);
        
            for (int i=0; i < 64; i++) {
                printf("%02x ", buf[i]);
            }

            putchar('\n');
        }

        __m512i lookup_gather(const __m512i in) {

            // split bytes into separate vectors
            const __m512i b0 = _mm512_and_si512(in, packed_dword(0x0000003f));
            const __m512i b1 = _mm512_and_si512(_mm512_srli_epi32(in, 1*6), packed_dword(0x0000003f));
            const __m512i b2 = _mm512_and_si512(_mm512_srli_epi32(in, 2*6), packed_dword(0x0000003f));
            const __m512i b3 = _mm512_and_si512(_mm512_srli_epi32(in, 3*6), packed_dword(0x0000003f));

            // do lookup
            const __m512i r0 = _mm512_i32gather_epi32(b0, (const int*)lookup_0, 4);
            const __m512i r1 = _mm512_i32gather_epi32(b1, (const int*)lookup_1, 4);
            const __m512i r2 = _mm512_i32gather_epi32(b2, (const int*)lookup_2, 4);
            const __m512i r3 = _mm512_i32gather_epi32(b3, (const int*)lookup_3, 4);

            return _mm512_or_si512(r0,
                   _mm512_or_si512(r1,
                   _mm512_or_si512(r2, r3)));

        }


        const uint8_t XOR_ALL = 0x96;


        __m512i lookup_incremental_logic(const __m512i in) {

            __m512i shift = packed_byte('A');
            __m512i c0, c1, c2, c3;
            const __m512i MSB = packed_byte(0x80);
            
            // shift ^= cmp(i >= 26) & 6;
            c0 = _mm512_and_si512(_mm512_add_epi32(in, packed_byte(0x80 - 26)), MSB);
            c0 = _mm512_sub_epi32(c0, _mm512_srli_epi32(c0, 7));
            c0 = _mm512_and_si512(c0, packed_byte(6));

            // shift ^= cmp(i >= 52) & 187;
            c1 = _mm512_and_si512(_mm512_add_epi32(in, packed_byte(0x80 - 52)), MSB);
            const __m512i c1msb = c1;
            c1 = _mm512_sub_epi32(c1, _mm512_srli_epi32(c1, 7));
            c1 = _mm512_and_si512(c1, packed_byte(187 & 0x7f));

            // shift ^= cmp(i >= 62) & 17;
            c2 = _mm512_and_si512(_mm512_add_epi32(in, packed_byte(0x80 - 62)), MSB);
            c2 = _mm512_sub_epi32(c2, _mm512_srli_epi32(c2, 7));
            c2 = _mm512_and_si512(c2, packed_byte(17));

            // shift ^= cmp(i >= 63) & 29;
            c3 = _mm512_and_si512(_mm512_add_epi32(in, packed_byte(0x80 - 63)), MSB);
            c3 = _mm512_sub_epi32(c3, _mm512_srli_epi32(c3, 7));
            c3 = _mm512_and_si512(c3, packed_byte(29));

            shift = _mm512_ternarylogic_epi32(shift, c0, c1, XOR_ALL);
            shift = _mm512_ternarylogic_epi32(shift, c2, c3, XOR_ALL);

            // produce the result
            return _mm512_xor_si512(_mm512_add_epi32(in, shift), c1msb);
        }


#undef packed_dword
#undef packed_byte

    } // namespace avx512

} // namespace base64