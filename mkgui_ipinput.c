// Copyright (c) 2026, Peter Fors
// SPDX-License-Identifier: MIT

#define IPINPUT_PAD_PX    4
#define IPINPUT_DOT_W_PX  6

// [=]===^=[ ipinput_field_x ]=====================================[=]
static int32_t ipinput_field_x(struct mkgui_ctx *ctx, int32_t rx, int32_t rw, uint32_t field) {
	int32_t pad = sc(ctx, IPINPUT_PAD_PX);
	int32_t dot_w = sc(ctx, IPINPUT_DOT_W_PX);
	int32_t field_w = (rw - pad * 2 - dot_w * 3) / 4;
	return rx + pad + (int32_t)field * (field_w + dot_w);
}

// [=]===^=[ ipinput_field_w ]=====================================[=]
static int32_t ipinput_field_w(struct mkgui_ctx *ctx, int32_t rw) {
	int32_t pad = sc(ctx, IPINPUT_PAD_PX);
	int32_t dot_w = sc(ctx, IPINPUT_DOT_W_PX);
	return (rw - pad * 2 - dot_w * 3) / 4;
}

// [=]===^=[ ipinput_hit_field ]===================================[=]
static int32_t ipinput_hit_field(struct mkgui_ctx *ctx, int32_t rx, int32_t rw, int32_t mx) {
	int32_t fw = ipinput_field_w(ctx, rw);
	for(uint32_t i = 0; i < 4; ++i) {
		int32_t fx = ipinput_field_x(ctx, rx, rw, i);
		if(mx >= fx && mx < fx + fw) {
			return (int32_t)i;
		}
	}
	if(mx < rx + rw / 2) {
		return 0;
	}
	return 3;
}

// [=]===^=[ ipinput_octet_str ]===================================[=]
static void ipinput_octet_str(uint8_t val, char *buf) {
	snprintf(buf, 4, "%u", val);
}

// [=]===^=[ ipinput_format ]======================================[=]
static void ipinput_format(struct mkgui_ipinput_data *ip, char *buf, uint32_t buf_size) {
	snprintf(buf, buf_size, "%u.%u.%u.%u", ip->octets[0], ip->octets[1], ip->octets[2], ip->octets[3]);
}

// [=]===^=[ render_ipinput ]======================================[=]
static void render_ipinput(struct mkgui_ctx *ctx, uint32_t idx) {
	struct mkgui_widget *w = &ctx->widgets[idx];
	int32_t rx = ctx->rects[idx].x;
	int32_t ry = ctx->rects[idx].y;
	int32_t rw = ctx->rects[idx].w;
	int32_t rh = ctx->rects[idx].h;

	uint32_t focused = (ctx->focus_id == w->id);
	draw_patch(ctx, MKGUI_STYLE_SUNKEN, rx, ry, rw, rh, ctx->theme.input_bg, focused ? ctx->theme.splitter : ctx->theme.widget_border);

	struct mkgui_ipinput_data *ip = find_ipinput_data(ctx, w->id);
	if(!ip) {
		return;
	}

	int32_t ty = ry + (rh - ctx->font_height) / 2;
	int32_t fw = ipinput_field_w(ctx, rw);
	uint32_t tc = (w->flags & MKGUI_DISABLED) ? ctx->theme.text_disabled : ctx->theme.text;
	int32_t inset = sc(ctx, 2);

	for(uint32_t i = 0; i < 4; ++i) {
		int32_t fx = ipinput_field_x(ctx, rx, rw, i);
		char buf[4];
		ipinput_octet_str(ip->octets[i], buf);

		if(focused && ip->active_field == i && ip->sel_all) {
			int32_t tw = text_width(ctx, buf);
			int32_t sx = fx + (fw - tw) / 2;
			int32_t sel_x = sx < fx ? fx : sx;
			int32_t sel_w = tw < fw ? tw : fw;
			draw_rect_fill(ctx->pixels, ctx->win_w, ctx->win_h, sel_x, ry + inset, sel_w, rh - inset * 2, ctx->theme.selection);
			push_text_clip(sx, ty, buf, ctx->theme.sel_text, fx, ry + 1, fx + fw, ry + rh - 1);
		} else if(focused && ip->active_field == i) {
			char edit[4];
			snprintf(edit, sizeof(edit), "%s", ip->edit_buf);
			int32_t tw = text_width(ctx, edit);
			int32_t sx = fx + (fw - tw) / 2;
			push_text_clip(sx, ty, edit, tc, fx, ry + 1, fx + fw, ry + rh - 1);
			int32_t cx = sx + tw;
			draw_vline(ctx->pixels, ctx->win_w, ctx->win_h, cx, ry + inset, rh - inset * 2, ctx->theme.text);
		} else {
			int32_t tw = text_width(ctx, buf);
			int32_t sx = fx + (fw - tw) / 2;
			push_text_clip(sx, ty, buf, tc, fx, ry + 1, fx + fw, ry + rh - 1);
		}

		if(i < 3) {
			int32_t dx = fx + fw + 1;
			push_text(dx, ty, ".", tc);
		}
	}
}

// [=]===^=[ ipinput_commit_edit ]=================================[=]
static void ipinput_commit_edit(struct mkgui_ipinput_data *ip) {
	if(ip->editing) {
		uint32_t val = 0;
		for(uint32_t i = 0; ip->edit_buf[i]; ++i) {
			val = val * 10 + (uint32_t)(ip->edit_buf[i] - '0');
		}
		if(val > 255) {
			val = 255;
		}
		ip->octets[ip->active_field] = (uint8_t)val;
		ip->editing = 0;
		ip->edit_buf[0] = '\0';
		ip->edit_len = 0;
	}
}

// [=]===^=[ ipinput_start_edit ]=================================[=]
static void ipinput_start_edit(struct mkgui_ipinput_data *ip) {
	ip->editing = 1;
	ip->edit_buf[0] = '\0';
	ip->edit_len = 0;
	ip->sel_all = 0;
}

// [=]===^=[ ipinput_select_field ]================================[=]
static void ipinput_select_field(struct mkgui_ipinput_data *ip, uint32_t field) {
	ipinput_commit_edit(ip);
	ip->active_field = field;
	ip->sel_all = 1;
	ip->editing = 0;
}

// [=]===^=[ handle_ipinput_click ]================================[=]
static void handle_ipinput_click(struct mkgui_ctx *ctx, uint32_t widget_id, int32_t mx) {
	struct mkgui_ipinput_data *ip = find_ipinput_data(ctx, widget_id);
	if(!ip) {
		return;
	}
	uint32_t widx = (uint32_t)find_widget_idx(ctx, widget_id);
	if(widx >= ctx->widget_count) {
		return;
	}
	int32_t rx = ctx->rects[widx].x;
	int32_t rw = ctx->rects[widx].w;
	int32_t field = ipinput_hit_field(ctx, rx, rw, mx);
	if(field >= 0 && field < 4) {
		ipinput_select_field(ip, (uint32_t)field);
	}
	dirty_all(ctx);
}

// [=]===^=[ handle_ipinput_key ]==================================[=]
static uint32_t handle_ipinput_key(struct mkgui_ctx *ctx, struct mkgui_event *ev, uint32_t ks, uint32_t keymod, const char *buf, int32_t len) {
	struct mkgui_ipinput_data *ip = find_ipinput_data(ctx, ctx->focus_id);
	if(!ip) {
		return 0;
	}
	(void)keymod;

	if(ks == MKGUI_KEY_TAB) {
		if(keymod & MKGUI_MOD_SHIFT) {
			if(ip->active_field > 0) {
				ipinput_select_field(ip, ip->active_field - 1);
				dirty_all(ctx);
				return 1;
			}
			return 0;
		} else {
			if(ip->active_field < 3) {
				ipinput_select_field(ip, ip->active_field + 1);
				dirty_all(ctx);
				return 1;
			}
			return 0;
		}
	}

	if(ks == MKGUI_KEY_LEFT) {
		if(ip->editing) {
			ipinput_commit_edit(ip);
			ev->type = MKGUI_EVENT_IPINPUT_CHANGED;
			ev->id = ctx->focus_id;
		}
		if(ip->active_field > 0) {
			ipinput_select_field(ip, ip->active_field - 1);
		}
		dirty_all(ctx);
		return 1;
	}

	if(ks == MKGUI_KEY_RIGHT) {
		if(ip->editing) {
			ipinput_commit_edit(ip);
			ev->type = MKGUI_EVENT_IPINPUT_CHANGED;
			ev->id = ctx->focus_id;
		}
		if(ip->active_field < 3) {
			ipinput_select_field(ip, ip->active_field + 1);
		}
		dirty_all(ctx);
		return 1;
	}

	if(ks == MKGUI_KEY_BACKSPACE) {
		if(ip->sel_all) {
			ip->octets[ip->active_field] = 0;
			ipinput_start_edit(ip);
			ev->type = MKGUI_EVENT_IPINPUT_CHANGED;
			ev->id = ctx->focus_id;
			dirty_all(ctx);
			return 1;
		}
		if(ip->editing && ip->edit_len > 0) {
			--ip->edit_len;
			ip->edit_buf[ip->edit_len] = '\0';
			dirty_all(ctx);
			return 1;
		}
		if(ip->active_field > 0) {
			ipinput_select_field(ip, ip->active_field - 1);
			dirty_all(ctx);
			return 1;
		}
		return 1;
	}

	if(ks == MKGUI_KEY_RETURN) {
		ipinput_commit_edit(ip);
		ev->type = MKGUI_EVENT_IPINPUT_CHANGED;
		ev->id = ctx->focus_id;
		dirty_all(ctx);
		return 1;
	}

	if(len == 1 && buf[0] == '.') {
		ipinput_commit_edit(ip);
		if(ip->active_field < 3) {
			ipinput_select_field(ip, ip->active_field + 1);
		}
		ev->type = MKGUI_EVENT_IPINPUT_CHANGED;
		ev->id = ctx->focus_id;
		dirty_all(ctx);
		return 1;
	}

	if(len == 1 && buf[0] >= '0' && buf[0] <= '9') {
		if(ip->sel_all) {
			ip->octets[ip->active_field] = 0;
			ipinput_start_edit(ip);
		}
		if(!ip->editing) {
			ipinput_start_edit(ip);
		}
		if(ip->edit_len < 3) {
			char tmp[4];
			memcpy(tmp, ip->edit_buf, ip->edit_len);
			tmp[ip->edit_len] = buf[0];
			tmp[ip->edit_len + 1] = '\0';
			uint32_t val = 0;
			for(uint32_t i = 0; tmp[i]; ++i) {
				val = val * 10 + (uint32_t)(tmp[i] - '0');
			}
			if(val > 255) {
				return 1;
			}
			ip->edit_buf[ip->edit_len] = buf[0];
			++ip->edit_len;
			ip->edit_buf[ip->edit_len] = '\0';

			if(ip->edit_len == 3) {
				ipinput_commit_edit(ip);
				if(ip->active_field < 3) {
					ipinput_select_field(ip, ip->active_field + 1);
				}
				ev->type = MKGUI_EVENT_IPINPUT_CHANGED;
				ev->id = ctx->focus_id;
				dirty_all(ctx);
				return 1;
			}
			dirty_all(ctx);
			return 1;
		}
		return 1;
	}

	return 1;
}

// [=]===^=[ mkgui_ipinput_set ]===================================[=]
MKGUI_API void mkgui_ipinput_set(struct mkgui_ctx *ctx, uint32_t id, const char *ip_string) {
	MKGUI_CHECK(ctx);
	if(!ip_string) {
		ip_string = "";
	}
	struct mkgui_ipinput_data *ip = find_ipinput_data(ctx, id);
	if(!ip) {
		return;
	}
	uint32_t a = 0, b = 0, c = 0, d = 0;
	sscanf(ip_string, "%u.%u.%u.%u", &a, &b, &c, &d);
	ip->octets[0] = (uint8_t)(a > 255 ? 255 : a);
	ip->octets[1] = (uint8_t)(b > 255 ? 255 : b);
	ip->octets[2] = (uint8_t)(c > 255 ? 255 : c);
	ip->octets[3] = (uint8_t)(d > 255 ? 255 : d);
	ip->editing = 0;
	ip->sel_all = 0;
	dirty_all(ctx);
}

// [=]===^=[ mkgui_ipinput_get ]===================================[=]
MKGUI_API const char *mkgui_ipinput_get(struct mkgui_ctx *ctx, uint32_t id) {
	MKGUI_CHECK_VAL(ctx, "");
	static char ipinput_buf[16];
	struct mkgui_ipinput_data *ip = find_ipinput_data(ctx, id);
	if(!ip) {
		snprintf(ipinput_buf, sizeof(ipinput_buf), "0.0.0.0");
		return ipinput_buf;
	}
	ipinput_format(ip, ipinput_buf, sizeof(ipinput_buf));
	return ipinput_buf;
}

// [=]===^=[ mkgui_ipinput_get_u32 ]===============================[=]
MKGUI_API uint32_t mkgui_ipinput_get_u32(struct mkgui_ctx *ctx, uint32_t id) {
	MKGUI_CHECK_VAL(ctx, 0);
	struct mkgui_ipinput_data *ip = find_ipinput_data(ctx, id);
	if(!ip) {
		return 0;
	}
	return ((uint32_t)ip->octets[0] << 24) | ((uint32_t)ip->octets[1] << 16) | ((uint32_t)ip->octets[2] << 8) | (uint32_t)ip->octets[3];
}
