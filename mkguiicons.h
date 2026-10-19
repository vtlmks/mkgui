// Copyright (c) 2026 Peter Fors
// SPDX-License-Identifier: MIT

#ifndef MKGUIICONS_H
#define MKGUIICONS_H

#include <stdint.h>

#if defined(_WIN32) && defined(MKGUIICONS_BUILD)
#define MKGUIICONS_API __declspec(dllexport)
#elif defined(_WIN32)
#define MKGUIICONS_API __declspec(dllimport)
#else
#define MKGUIICONS_API __attribute__((visibility("default")))
#endif

// Names and SVG data belong to the library and remain valid until it is unloaded.
// Default names select the regular style where available. Other styles use
// :filled, :light, or :color suffixes. Size selects the closest SVG design;
// the consumer rasterizes that vector at its required pixel dimensions.
// Unknown names return NULL and set length to zero. size == 0 selects 24.
// length must be non-NULL. Locale/direction variants use @ar, @rtl, etc.
// Enumeration indices may change between releases; persist names instead.
// Invalid string indices return NULL, invalid topics UINT32_MAX, sizes zero.
MKGUIICONS_API const char *mkguiicons_svg(const char *name, uint32_t size, uint32_t *length);
MKGUIICONS_API uint32_t mkguiicons_count(void);
MKGUIICONS_API const char *mkguiicons_name(uint32_t index);
MKGUIICONS_API uint32_t mkguiicons_size_count(uint32_t index);
MKGUIICONS_API uint32_t mkguiicons_size(uint32_t index, uint32_t variant);
MKGUIICONS_API const char *mkguiicons_keywords(uint32_t index);
MKGUIICONS_API uint32_t mkguiicons_topic(uint32_t index);
MKGUIICONS_API uint32_t mkguiicons_topic_count(void);
MKGUIICONS_API const char *mkguiicons_topic_name(uint32_t index);
MKGUIICONS_API const char *mkguiicons_release(void);

#endif
