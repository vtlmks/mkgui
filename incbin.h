#define STR2(x) #x
#define STR(x)  STR2(x)

#ifdef _WIN32
#  define INCBIN_SECTION ".rdata, \"dr\""
#else
#  define INCBIN_SECTION ".rodata, \"a\", @progbits"
#endif

/* Core raw incbin (unchanged semantics) */
#define INCBIN(name, file)															\
	__asm__(".section " INCBIN_SECTION "\n"										\
		".global " STR(name) "_data\n"											\
		".balign 64\n"																	\
		STR(name) "_data:\n"															\
		".incbin " STR(file) "\n"													\
		".zero 64\n"																	\
		".global " STR(name) "_end\n"												\
		STR(name) "_end:\n"															\
		".section .text\n");															\
	extern __attribute__((aligned(64))) unsigned char name##_data[];	\
	extern                              unsigned char name##_end[];

/* Size helper (no storage) */
#define INCBIN_SIZE(name) ((size_t)((name##_end) - (name##_data)))

/* Typed wrappers (create a typed pointer variable named exactly <name>) */
#define INCBIN_RAW(name, file, type)											\
	INCBIN(name, file)																\
	static type * name = (type*)(name##_data)

/* Ready-made flavors */
#define INCBIN_UGG(name, file)     INCBIN_RAW(name, file, struct ugg)
#define INCBIN_BYTES(name, file)   INCBIN_RAW(name, file, unsigned char)

// Common assembly template for shader variants
// Exports <name>_data and <name>_end (char[]); use <name>_data as source.
#define _INCBIN_SHADER_COMMON(name, version_str, middle_section)		\
	__asm__(".section " INCBIN_SECTION "\n"									\
		".global " STR(name) "_data\n"											\
		".balign 64\n"																	\
		STR(name) "_data:\n"															\
		".ascii \"" version_str "\\n\"\n"										\
		middle_section																	\
		".byte 0\n"																		\
		".global " STR(name) "_end\n"												\
		STR(name) "_end:\n");														\
	extern __attribute__((aligned(64))) char name##_data[];				\
	extern                              char name##_end[];

// Shader variant: prepends version, then incbins header & shader
#define INCBIN_SHADER(name, version_str, header_file, shader_file)	\
	_INCBIN_SHADER_COMMON(name, version_str,									\
		".incbin \"" header_file "\"\n"											\
		".incbin \"" shader_file "\"\n")

// Shader without header: prepends version, then incbins shader only
#define INCBIN_SHADER_NOHEADER(name, version_str, shader_file)			\
	_INCBIN_SHADER_COMMON(name, version_str,									\
		".incbin \"" shader_file "\"\n")

