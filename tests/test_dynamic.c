// Copyright (c) 2026, Peter Fors
// SPDX-License-Identifier: MIT
//
// Stress test for dynamic widget add/insert/remove.
// Exercises vm_arena commit boundaries (256, 512, ...) and verifies
// widget array integrity after each operation.

#include "../mkgui.c"

static uint32_t fail_count;

#define CHECK(cond, fmt, ...) do { \
	if(!(cond)) { \
		fprintf(stderr, "FAIL line %d: " fmt "\n", __LINE__, ##__VA_ARGS__); \
		++fail_count; \
	} \
} while(0)

// [=]===^=[ verify_integrity ]====================================[=]
static void verify_integrity(struct mkgui_window *win, const char *tag) {
	for(uint32_t i = 0; i < win->widget_count; ++i) {
		uint32_t id = win->widgets[i].id;
		CHECK(id != 0, "%s: widget[%u] has id 0", tag, i);
		int32_t idx = find_widget_idx(win, id);
		CHECK(idx == (int32_t)i, "%s: find_widget_idx(%u) = %d, expected %u", tag, id, idx, i);
	}
	for(uint32_t i = 0; i < win->widget_count; ++i) {
		for(uint32_t j = i + 1; j < win->widget_count; ++j) {
			CHECK(win->widgets[i].id != win->widgets[j].id, "%s: duplicate id %u at [%u] and [%u]", tag, win->widgets[i].id, i, j);
		}
	}
}

// [=]===^=[ verify_order ]========================================[=]
static void verify_order(struct mkgui_window *win, uint32_t *expected_ids, uint32_t count, const char *tag) {
	CHECK(win->widget_count == count, "%s: count %u, expected %u", tag, win->widget_count, count);
	uint32_t n = win->widget_count < count ? win->widget_count : count;
	for(uint32_t i = 0; i < n; ++i) {
		CHECK(win->widgets[i].id == expected_ids[i], "%s: widgets[%u].id = %u, expected %u", tag, i, win->widgets[i].id, expected_ids[i]);
	}
}

// [=]===^=[ test_basic_add ]======================================[=]
static void test_basic_add(struct mkgui_window *win) {
	fprintf(stderr, "  test_basic_add...\n");
	uint32_t base = win->widget_count;

	for(uint32_t i = 0; i < 10; ++i) {
		struct mkgui_widget w = {0};
		w.type = MKGUI_LABEL;
		w.id = 5000 + i;
		w.parent_id = 1;
		uint32_t ok = mkgui_widget_add(win, w, 0);
		CHECK(ok == 1, "add_widget returned 0 for id %u", w.id);
	}
	CHECK(win->widget_count == base + 10, "count %u, expected %u", win->widget_count, base + 10);

	for(uint32_t i = 0; i < 10; ++i) {
		struct mkgui_widget *w = find_widget(win, 5000 + i);
		CHECK(w != NULL, "find_widget(%u) returned NULL", 5000 + i);
	}
	verify_integrity(win, "basic_add");
}

// [=]===^=[ test_insert_after ]====================================[=]
static void test_insert_after(struct mkgui_window *win) {
	fprintf(stderr, "  test_insert_after...\n");

	struct mkgui_widget a = { .type = MKGUI_LABEL, .id = 6001, .parent_id = 1 };
	struct mkgui_widget b = { .type = MKGUI_LABEL, .id = 6002, .parent_id = 1 };
	struct mkgui_widget c = { .type = MKGUI_LABEL, .id = 6003, .parent_id = 1 };
	mkgui_widget_add(win, a, 0);
	mkgui_widget_add(win, c, 0);
	mkgui_widget_add(win, b, 6001);

	int32_t ia = find_widget_idx(win, 6001);
	int32_t ib = find_widget_idx(win, 6002);
	int32_t ic = find_widget_idx(win, 6003);
	CHECK(ia >= 0 && ib >= 0 && ic >= 0, "insert_after: widgets not found");
	if(ia >= 0 && ib >= 0 && ic >= 0) {
		CHECK(ib == ia + 1, "insert_after: b(%d) should be right after a(%d)", ib, ia);
		CHECK(ic > ib, "insert_after: c(%d) should be after b(%d)", ic, ib);
	}
	verify_integrity(win, "insert_after");
}

// [=]===^=[ test_insert_first_child ]==============================[=]
static void test_insert_first_child(struct mkgui_window *win) {
	fprintf(stderr, "  test_insert_first_child...\n");

	struct mkgui_widget vbox = { .type = MKGUI_VBOX, .id = 7000, .parent_id = 1 };
	struct mkgui_widget child1 = { .type = MKGUI_LABEL, .id = 7001, .parent_id = 7000 };
	struct mkgui_widget child2 = { .type = MKGUI_LABEL, .id = 7002, .parent_id = 7000 };
	struct mkgui_widget first = { .type = MKGUI_LABEL, .id = 7003, .parent_id = 7000 };

	mkgui_widget_add(win, vbox, 0);
	mkgui_widget_add(win, child1, 7000);
	mkgui_widget_add(win, child2, 7001);
	mkgui_widget_add(win, first, 7000);

	int32_t iv = find_widget_idx(win, 7000);
	int32_t i_first = find_widget_idx(win, 7003);
	int32_t i1 = find_widget_idx(win, 7001);
	int32_t i2 = find_widget_idx(win, 7002);
	CHECK(iv >= 0 && i_first >= 0 && i1 >= 0 && i2 >= 0, "first_child: widgets not found");
	if(iv >= 0 && i_first >= 0) {
		CHECK(i_first == iv + 1, "first_child: 7003(%d) should be right after vbox(%d)", i_first, iv);
		CHECK(i1 > i_first, "first_child: 7001(%d) should be after 7003(%d)", i1, i_first);
		CHECK(i2 > i1, "first_child: 7002(%d) should be after 7001(%d)", i2, i1);
	}
	verify_integrity(win, "first_child");
}

// [=]===^=[ test_remove ]==========================================[=]
static void test_remove(struct mkgui_window *win) {
	fprintf(stderr, "  test_remove...\n");
	uint32_t before = win->widget_count;

	struct mkgui_widget w = { .type = MKGUI_LABEL, .id = 8001, .parent_id = 1 };
	mkgui_widget_add(win, w, 0);
	CHECK(win->widget_count == before + 1, "remove: add failed");

	uint32_t ok = mkgui_widget_remove(win, 8001);
	CHECK(ok == 1, "remove: returned 0");
	CHECK(win->widget_count == before, "remove: count %u, expected %u", win->widget_count, before);
	CHECK(find_widget(win, 8001) == NULL, "remove: widget still found");
	verify_integrity(win, "remove");
}

// [=]===^=[ test_remove_cascade ]==================================[=]
static void test_remove_cascade(struct mkgui_window *win) {
	fprintf(stderr, "  test_remove_cascade...\n");
	uint32_t before = win->widget_count;

	struct mkgui_widget parent = { .type = MKGUI_VBOX, .id = 9000, .parent_id = 1 };
	struct mkgui_widget child1 = { .type = MKGUI_LABEL, .id = 9001, .parent_id = 9000 };
	struct mkgui_widget child2 = { .type = MKGUI_LABEL, .id = 9002, .parent_id = 9000 };
	struct mkgui_widget grandchild = { .type = MKGUI_LABEL, .id = 9003, .parent_id = 9001 };

	mkgui_widget_add(win, parent, 0);
	mkgui_widget_add(win, child1, 9000);
	mkgui_widget_add(win, grandchild, 9001);
	mkgui_widget_add(win, child2, 9001);

	uint32_t ok = mkgui_widget_remove(win, 9000);
	CHECK(ok == 1, "cascade: returned 0");
	CHECK(win->widget_count == before, "cascade: count %u, expected %u", win->widget_count, before);
	CHECK(find_widget(win, 9000) == NULL, "cascade: 9000 still found");
	CHECK(find_widget(win, 9001) == NULL, "cascade: 9001 still found");
	CHECK(find_widget(win, 9002) == NULL, "cascade: 9002 still found");
	CHECK(find_widget(win, 9003) == NULL, "cascade: 9003 still found");
	verify_integrity(win, "cascade");
}

// [=]===^=[ test_growth_boundary ]=================================[=]
static void test_growth_boundary(struct mkgui_window *win) {
	fprintf(stderr, "  test_growth_boundary...\n");

	uint32_t target = 600;
	uint32_t start_id = 10000;
	uint32_t before = win->widget_count;

	for(uint32_t i = 0; i < target; ++i) {
		struct mkgui_widget w = {0};
		w.type = MKGUI_LABEL;
		w.id = start_id + i;
		w.parent_id = 1;
		uint32_t ok = mkgui_widget_add(win, w, 0);
		CHECK(ok == 1, "growth: add failed at i=%u (count=%u, cap=%u)", i, win->widget_count, win->widget_cap);
		if(!ok) {
			break;
		}
	}
	CHECK(win->widget_count == before + target, "growth: count %u, expected %u", win->widget_count, before + target);
	CHECK(win->widget_cap >= win->widget_count, "growth: cap %u < count %u", win->widget_cap, win->widget_count);

	for(uint32_t i = 0; i < target; ++i) {
		struct mkgui_widget *w = find_widget(win, start_id + i);
		CHECK(w != NULL, "growth: find_widget(%u) NULL", start_id + i);
	}
	verify_integrity(win, "growth");

	for(uint32_t i = 0; i < target; ++i) {
		mkgui_widget_remove(win, start_id + i);
	}
	CHECK(win->widget_count == before, "growth cleanup: count %u, expected %u", win->widget_count, before);
	verify_integrity(win, "growth_cleanup");
}

// [=]===^=[ test_insert_stress ]====================================[=]
static void test_insert_stress(struct mkgui_window *win) {
	fprintf(stderr, "  test_insert_stress...\n");

	uint32_t anchor_id = 20000;
	struct mkgui_widget anchor = { .type = MKGUI_LABEL, .id = anchor_id, .parent_id = 1 };
	mkgui_widget_add(win, anchor, 0);

	uint32_t count = 300;
	for(uint32_t i = 0; i < count; ++i) {
		struct mkgui_widget w = {0};
		w.type = MKGUI_LABEL;
		w.id = anchor_id + 1 + i;
		w.parent_id = 1;
		uint32_t ok = mkgui_widget_add(win, w, anchor_id);
		CHECK(ok == 1, "insert_stress: add failed at i=%u", i);
		if(!ok) {
			break;
		}
	}

	int32_t ai = find_widget_idx(win, anchor_id);
	CHECK(ai >= 0, "insert_stress: anchor not found");
	if(ai >= 0) {
		for(uint32_t i = 0; i < count; ++i) {
			uint32_t expected_id = anchor_id + count - i;
			CHECK(win->widgets[ai + 1 + (int32_t)i].id == expected_id, "insert_stress: widgets[%d].id = %u, expected %u", ai + 1 + (int32_t)i, win->widgets[ai + 1 + (int32_t)i].id, expected_id);
		}
	}
	verify_integrity(win, "insert_stress");

	for(uint32_t i = 0; i < count; ++i) {
		mkgui_widget_remove(win, anchor_id + 1 + i);
	}
	mkgui_widget_remove(win, anchor_id);
}

// [=]===^=[ test_aux_data ]========================================[=]
static void test_aux_data(struct mkgui_window *win) {
	fprintf(stderr, "  test_aux_data...\n");

	uint32_t slider_before = win->slider_count;
	uint32_t input_before = win->input_count;

	struct mkgui_widget s = { .type = MKGUI_SLIDER, .id = 30001, .parent_id = 1 };
	struct mkgui_widget inp = { .type = MKGUI_INPUT, .id = 30002, .parent_id = 1 };
	mkgui_widget_add(win, s, 0);
	mkgui_widget_add(win, inp, 0);

	CHECK(win->slider_count == slider_before + 1, "aux: slider_count %u, expected %u", win->slider_count, slider_before + 1);
	CHECK(win->input_count == input_before + 1, "aux: input_count %u, expected %u", win->input_count, input_before + 1);

	struct mkgui_slider_data *sd = find_slider_data(win, 30001);
	CHECK(sd != NULL, "aux: slider data not found");
	struct mkgui_input_data *id_data = find_input_data(win, 30002);
	CHECK(id_data != NULL, "aux: input data not found");

	mkgui_widget_remove(win, 30001);
	CHECK(win->slider_count == slider_before, "aux: slider_count after remove %u, expected %u", win->slider_count, slider_before);
	CHECK(find_slider_data(win, 30001) == NULL, "aux: slider data still found after remove");

	mkgui_widget_remove(win, 30002);
	CHECK(win->input_count == input_before, "aux: input_count after remove %u, expected %u", win->input_count, input_before);

	for(uint32_t i = 0; i < 50; ++i) {
		struct mkgui_widget sl = { .type = MKGUI_SLIDER, .id = 31000 + i, .parent_id = 1 };
		mkgui_widget_add(win, sl, 0);
	}
	CHECK(win->slider_count == slider_before + 50, "aux growth: slider_count %u, expected %u", win->slider_count, slider_before + 50);
	for(uint32_t i = 0; i < 50; ++i) {
		mkgui_widget_remove(win, 31000 + i);
	}
	CHECK(win->slider_count == slider_before, "aux growth cleanup: slider_count %u", win->slider_count);
}

// [=]===^=[ test_remove_window_blocked ]============================[=]
static void test_remove_window_blocked(struct mkgui_window *win) {
	fprintf(stderr, "  test_remove_window_blocked...\n");
	uint32_t before = win->widget_count;
	uint32_t ok = mkgui_widget_remove(win, 1);
	CHECK(ok == 0, "remove_window: should return 0");
	CHECK(win->widget_count == before, "remove_window: count changed");
}

// [=]===^=[ main ]=================================================[=]
int32_t main(void) {
	struct mkgui_widget initial[] = {
		MKGUI_W(MKGUI_WINDOW, 1, "Test", "", 0, 200, 100, 0, 0, 0),
		MKGUI_W(MKGUI_VBOX,   2, "",     "", 1, 0,   0,   0, 0, 0),
	};

	struct mkgui_ctx *ctx = mkgui_ctx_create();
	struct mkgui_window *win = mkgui_window_create(ctx, NULL, initial, 2, NULL, 0, 0);
	if(!win) {
		fprintf(stderr, "FAIL: mkgui_window_create returned NULL\n");
		mkgui_ctx_destroy(ctx);
		return 1;
	}

	fprintf(stderr, "Running dynamic widget tests...\n");
	fprintf(stderr, "  initial: count=%u cap=%u\n", win->widget_count, win->widget_cap);

	test_basic_add(win);
	test_insert_after(win);
	test_insert_first_child(win);
	test_remove(win);
	test_remove_cascade(win);
	test_growth_boundary(win);
	test_insert_stress(win);
	test_aux_data(win);
	test_remove_window_blocked(win);

	verify_integrity(win, "final");

	mkgui_window_destroy(win);
	mkgui_ctx_destroy(ctx);

	if(fail_count == 0) {
		fprintf(stderr, "All tests passed.\n");
	} else {
		fprintf(stderr, "%u test(s) FAILED.\n", fail_count);
	}
	return fail_count ? 1 : 0;
}
