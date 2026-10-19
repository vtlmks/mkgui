// Copyright (c) 2026 Peter Fors
// SPDX-License-Identifier: MIT

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "mkguiicons.h"

struct mkguiicons_entry {
	uint32_t name;
	uint32_t first;
	uint32_t count;
	uint32_t topic;
	uint32_t keywords;
};

struct mkguiicons_variant {
	uint32_t size;
	uint32_t offset;
	uint32_t length;
};

extern uint8_t mkguiicons_data[];

#include "out/icons/catalogue.h"

// [=]===^=[ mkguiicons_count ]===================================[=]
MKGUIICONS_API uint32_t mkguiicons_count(void) {
	return sizeof(icon_entries) / sizeof(icon_entries[0]);

}

// [=]===^=[ mkguiicons_name ]====================================[=]
MKGUIICONS_API const char *mkguiicons_name(uint32_t index) {
	return index < mkguiicons_count() ? (char *)mkguiicons_data + icon_entries[index].name : NULL;

}

// [=]===^=[ mkguiicons_size_count ]=============================[=]
MKGUIICONS_API uint32_t mkguiicons_size_count(uint32_t index) {
	return index < mkguiicons_count() ? icon_entries[index].count : 0;

}

// [=]===^=[ mkguiicons_size ]===================================[=]
MKGUIICONS_API uint32_t mkguiicons_size(uint32_t index, uint32_t variant) {
	return variant < mkguiicons_size_count(index) ? icon_variants[icon_entries[index].first + variant].size : 0;

}

// [=]===^=[ mkguiicons_keywords ]================================[=]
MKGUIICONS_API const char *mkguiicons_keywords(uint32_t index) {
	return index < mkguiicons_count() ? (char *)mkguiicons_data + icon_entries[index].keywords : NULL;

}

// [=]===^=[ mkguiicons_topic ]===================================[=]
MKGUIICONS_API uint32_t mkguiicons_topic(uint32_t index) {
	return index < mkguiicons_count() ? icon_entries[index].topic : UINT32_MAX;

}

// [=]===^=[ mkguiicons_topic_count ]==============================[=]
MKGUIICONS_API uint32_t mkguiicons_topic_count(void) {
	return sizeof(icon_topics) / sizeof(icon_topics[0]);

}

// [=]===^=[ mkguiicons_topic_name ]===============================[=]
MKGUIICONS_API const char *mkguiicons_topic_name(uint32_t index) {
	return index < mkguiicons_topic_count() ? (char *)mkguiicons_data + icon_topics[index] : NULL;

}

// [=]===^=[ mkguiicons_release ]=================================[=]
MKGUIICONS_API const char *mkguiicons_release(void) {
	return icon_release;

}

// [=]===^=[ mkguiicons_svg ]=====================================[=]
MKGUIICONS_API const char *mkguiicons_svg(const char *name, uint32_t size, uint32_t *length) {
	uint32_t lower = 0;
	uint32_t upper = mkguiicons_count();
	uint32_t found = UINT32_MAX;
	*length = 0;

	if(name) {
		while(lower < upper) {
			uint32_t middle = lower + (upper - lower) / 2;
			int32_t order = strcmp(name, (char *)mkguiicons_data + icon_entries[middle].name);

			if(order == 0) {
				found = middle;
				break;

			} else if(order < 0) {
				upper = middle;

			} else {
				lower = middle + 1;

			}
		}

		if(found != UINT32_MAX) {
			struct mkguiicons_entry *entry = &icon_entries[found];
			uint32_t selected = entry->first;
			uint32_t distance = UINT32_MAX;
			size = size ? size : 24;

			for(uint32_t i = entry->first; i < entry->first + entry->count; ++i) {
				uint32_t candidate = icon_variants[i].size;
				uint32_t difference = candidate > size ? candidate - size : size - candidate;

				if(difference <= distance) {
					selected = i;
					distance = difference;

				}
			}

			*length = icon_variants[selected].length;
			return (char *)mkguiicons_data + icon_variants[selected].offset;

		}
	}

	return NULL;

}
