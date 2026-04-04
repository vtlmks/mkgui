// Copyright (c) 2026, Peter Fors
// SPDX-License-Identifier: MIT

#define MKGUI_UNDO_COALESCE_MS 500

// [=]===^=[ input_undo_push ]====================================[=]
static void input_undo_push(struct mkgui_input_data *inp) {
	uint32_t now = mkgui_time_ms();
	if(inp->undo_count > 0 && (now - inp->undo_last_ms) < MKGUI_UNDO_COALESCE_MS) {
		return;
	}
	inp->undo_last_ms = now;

	uint32_t slot = inp->undo_pos % MKGUI_UNDO_MAX;
	struct mkgui_input_snap *s = &inp->undo[slot];
	memcpy(s->text, inp->text, MKGUI_MAX_TEXT);
	s->cursor = inp->cursor;
	s->sel_start = inp->sel_start;
	s->sel_end = inp->sel_end;
	++inp->undo_pos;
	inp->undo_count = inp->undo_pos;
	inp->undo_saved = inp->undo_pos;
}

// [=]===^=[ input_undo_push_force ]==============================[=]
static void input_undo_push_force(struct mkgui_input_data *inp) {
	inp->undo_last_ms = 0;
	input_undo_push(inp);
}

// [=]===^=[ input_undo ]=========================================[=]
static uint32_t input_undo(struct mkgui_input_data *inp) {
	if(inp->undo_pos == 0) {
		return 0;
	}
	if(inp->undo_pos == inp->undo_saved) {
		input_undo_push_force(inp);
		--inp->undo_pos;
	}
	if(inp->undo_pos == 0) {
		return 0;
	}
	--inp->undo_pos;
	uint32_t slot = inp->undo_pos % MKGUI_UNDO_MAX;
	struct mkgui_input_snap *s = &inp->undo[slot];
	memcpy(inp->text, s->text, MKGUI_MAX_TEXT);
	inp->cursor = s->cursor;
	inp->sel_start = s->sel_start;
	inp->sel_end = s->sel_end;
	return 1;
}

// [=]===^=[ input_redo ]=========================================[=]
static uint32_t input_redo(struct mkgui_input_data *inp) {
	if(inp->undo_pos + 1 >= inp->undo_saved) {
		return 0;
	}
	++inp->undo_pos;
	uint32_t slot = inp->undo_pos % MKGUI_UNDO_MAX;
	struct mkgui_input_snap *s = &inp->undo[slot];
	memcpy(inp->text, s->text, MKGUI_MAX_TEXT);
	inp->cursor = s->cursor;
	inp->sel_start = s->sel_start;
	inp->sel_end = s->sel_end;
	return 1;
}

// [=]===^=[ textarea_undo_push ]=================================[=]
static void textarea_undo_push(struct mkgui_textarea_data *ta) {
	uint32_t now = mkgui_time_ms();
	if(ta->undo_count > 0 && (now - ta->undo_last_ms) < MKGUI_UNDO_COALESCE_MS) {
		return;
	}
	ta->undo_last_ms = now;

	uint32_t slot = ta->undo_pos % MKGUI_UNDO_MAX;
	struct mkgui_textarea_snap *s = &ta->undo[slot];
	free(s->text);
	s->text = (char *)malloc(ta->text_len + 1);
	if(!s->text) {
		return;
	}
	memcpy(s->text, ta->text, ta->text_len + 1);
	s->text_len = ta->text_len;
	s->cursor = ta->cursor;
	s->sel_start = ta->sel_start;
	s->sel_end = ta->sel_end;
	++ta->undo_pos;
	ta->undo_count = ta->undo_pos;
	ta->undo_saved = ta->undo_pos;
}

// [=]===^=[ textarea_undo_push_force ]============================[=]
static void textarea_undo_push_force(struct mkgui_textarea_data *ta) {
	ta->undo_last_ms = 0;
	textarea_undo_push(ta);
}

// [=]===^=[ textarea_undo ]======================================[=]
static uint32_t textarea_undo(struct mkgui_textarea_data *ta) {
	if(ta->undo_pos == 0) {
		return 0;
	}
	if(ta->undo_pos == ta->undo_saved) {
		textarea_undo_push_force(ta);
		--ta->undo_pos;
	}
	if(ta->undo_pos == 0) {
		return 0;
	}
	--ta->undo_pos;
	uint32_t slot = ta->undo_pos % MKGUI_UNDO_MAX;
	struct mkgui_textarea_snap *s = &ta->undo[slot];
	if(s->text_len + 1 > ta->text_cap) {
		ta->text_cap = s->text_len + 1;
		ta->text = (char *)realloc(ta->text, ta->text_cap);
	}
	memcpy(ta->text, s->text, s->text_len + 1);
	ta->text_len = s->text_len;
	ta->cursor = s->cursor;
	ta->sel_start = s->sel_start;
	ta->sel_end = s->sel_end;
	return 1;
}

// [=]===^=[ textarea_redo ]======================================[=]
static uint32_t textarea_redo(struct mkgui_textarea_data *ta) {
	if(ta->undo_pos + 1 >= ta->undo_saved) {
		return 0;
	}
	++ta->undo_pos;
	uint32_t slot = ta->undo_pos % MKGUI_UNDO_MAX;
	struct mkgui_textarea_snap *s = &ta->undo[slot];
	if(s->text_len + 1 > ta->text_cap) {
		ta->text_cap = s->text_len + 1;
		ta->text = (char *)realloc(ta->text, ta->text_cap);
	}
	memcpy(ta->text, s->text, s->text_len + 1);
	ta->text_len = s->text_len;
	ta->cursor = s->cursor;
	ta->sel_start = s->sel_start;
	ta->sel_end = s->sel_end;
	return 1;
}

// [=]===^=[ textarea_undo_free ]=================================[=]
static void textarea_undo_free(struct mkgui_textarea_data *ta) {
	for(uint32_t i = 0; i < MKGUI_UNDO_MAX; ++i) {
		free(ta->undo[i].text);
		ta->undo[i].text = NULL;
	}
}
