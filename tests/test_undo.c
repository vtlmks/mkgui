// Copyright (c) 2026 Peter Fors
// SPDX-License-Identifier: MIT

#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE
#define _GNU_SOURCE
#include <stdlib.h>
#include <stdint.h>

static uint32_t fail_allocation;

// [=]===^=[ undo_malloc ]======================================================================[=]
static void *undo_malloc(size_t size) {
	if(fail_allocation) {
		return NULL;

	}

	return malloc(size);

}

#define malloc undo_malloc
#include "../mkgui.c"
#undef malloc

static uint32_t checks;
static uint32_t failures;
static struct mkgui_input_data input;
static struct mkgui_textarea_data textarea;

// [=]===^=[ check ]============================================================================[=]
static void check(uint32_t condition, char *message) {
	++checks;

	if(!condition) {
		fprintf(stderr, "FAIL: %s\n", message);
		++failures;

	}
}

// [=]===^=[ input_value ]======================================================================[=]
static void input_value(uint32_t value) {
	snprintf(input.text, sizeof(input.text), "%04u", value);
	input.cursor = value % 5;
	input.sel_start = 0;
	input.sel_end = input.cursor;

}

// [=]===^=[ textarea_value ]===================================================================[=]
static void textarea_value(uint32_t value) {
	snprintf(textarea.text, textarea.text_cap, "%04u", value);
	textarea.text_len = 4;
	textarea.cursor = value % 5;
	textarea.sel_start = 0;
	textarea.sel_end = textarea.cursor;

}

// [=]===^=[ check_input ]======================================================================[=]
static void check_input(uint32_t value) {
	check(strtoul(input.text, NULL, 10) == value && input.cursor == value % 5 && input.sel_start == 0 && input.sel_end == value % 5, "input text/cursor/selection round trip");

}

// [=]===^=[ check_textarea ]===================================================================[=]
static void check_textarea(uint32_t value) {
	check(strtoul(textarea.text, NULL, 10) == value && textarea.text_len == 4 && textarea.cursor == value % 5 && textarea.sel_start == 0 && textarea.sel_end == value % 5, "textarea text/cursor/selection round trip");

}

// [=]===^=[ main ]=============================================================================[=]
int main(void) {
	check(!input_undo(&input) && !input_redo(&input), "empty input history");
	check(!textarea_undo(&textarea) && !textarea_redo(&textarea), "empty textarea history");
	textarea.text = malloc(5);

	if(textarea.text) {
		textarea.text_cap = 5;

		for(uint32_t i = 0; i < MKGUI_UNDO_MAX * 4; ++i) {
			input_value(i);
			textarea_value(i);
			input_undo_push_force(&input);
			check(textarea_undo_push_force(&textarea), "textarea snapshot allocation");

		}

		input_value(MKGUI_UNDO_MAX * 4);
		textarea_value(MKGUI_UNDO_MAX * 4);

		for(uint32_t i = 0; i < MKGUI_UNDO_MAX; ++i) {
			check(input_undo(&input), "input undo within retained history");
			check(textarea_undo(&textarea), "textarea undo within retained history");
			check_input(MKGUI_UNDO_MAX * 4 - i - 1);
			check_textarea(MKGUI_UNDO_MAX * 4 - i - 1);

		}

		check(!input_undo(&input) && !textarea_undo(&textarea), "undo stops at oldest retained state");

		for(uint32_t i = 0; i < MKGUI_UNDO_MAX; ++i) {
			check(input_redo(&input), "input redo through full history");
			check(textarea_redo(&textarea), "textarea redo through full history");
			check_input(MKGUI_UNDO_MAX * 3 + i + 1);
			check_textarea(MKGUI_UNDO_MAX * 3 + i + 1);

		}

		check(!input_redo(&input) && !textarea_redo(&textarea), "redo stops at latest state");
		check(input_undo(&input) && textarea_undo(&textarea), "undo before branching");
		input_undo_push(&input);
		check(textarea_undo_push(&textarea), "snapshot immediately after undo");
		input_value(9000);
		textarea_value(9000);
		check(!input_redo(&input) && !textarea_redo(&textarea), "editing after undo discards redo");
		check(input_undo(&input) && textarea_undo(&textarea), "branch edit is undoable");
		check_input(MKGUI_UNDO_MAX * 4 - 1);
		check_textarea(MKGUI_UNDO_MAX * 4 - 1);
		check(input_redo(&input) && textarea_redo(&textarea), "branch edit is redoable");
		check_input(9000);
		check_textarea(9000);
		input_undo_push(&input);
		check(textarea_undo_push(&textarea), "new edit immediately after redo records a snapshot");
		input_value(9001);
		textarea_value(9001);
		input_undo_push(&input);
		check(textarea_undo_push(&textarea), "coalesced textarea edit");
		input_value(9002);
		textarea_value(9002);
		check(input_undo(&input) && textarea_undo(&textarea), "coalesced edits undo together");
		check_input(9000);
		check_textarea(9000);
		fail_allocation = 1;
		check(!textarea_undo_push(&textarea), "snapshot allocation failure is reported");
		check(textarea_redo(&textarea), "allocation failure preserves redo history");
		check_textarea(9002);
		check(textarea_undo(&textarea), "undo does not allocate");
		check_textarea(9000);
		fail_allocation = 0;
		textarea_undo_free(&textarea);
		free(textarea.text);

	} else {
		check(0, "initial textarea allocation");

	}

	printf("Undo: %u checks, %u failures\n", checks, failures);
	return failures ? 1 : 0;

}
