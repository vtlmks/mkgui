// SPDX-License-Identifier: MIT

#define INCBIN_STR2(x) #x
#define INCBIN_STR(x)  INCBIN_STR2(x)

#ifdef _WIN32
#  define INCBIN_SECTION ".data, \"aw\""
#else
#  define INCBIN_SECTION ".data"
#endif

#define INCBIN(name, file)                                    \
	__asm__(".section " INCBIN_SECTION "\n"                    \
		".global " INCBIN_STR(name) "_data\n"                   \
		".balign 64\n"                                          \
		INCBIN_STR(name) "_data:\n"                             \
		".incbin " INCBIN_STR(file) "\n"                        \
		".global " INCBIN_STR(name) "_end\n"                    \
		INCBIN_STR(name) "_end:\n"                              \
		".section .text\n");                                    \
	extern __attribute__((aligned(64))) unsigned char name##_data[]; \
	extern                              unsigned char name##_end[]

#define INCBIN_SIZE(name) ((size_t)((name##_end) - (name##_data)))
