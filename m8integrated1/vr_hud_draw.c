#include "vr_hud_draw.h"
#include "vr_hud_math.h"
#include "aeron/asset/opt_model.h"
#include "aeron/scene/draw_list2d.h"
#include "aeron/scene/font_atlas.h"
#include "xwa_remaster/hud.h"
#include <SDL3/SDL.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265f
#endif

/* Collimated-ish reticle distance: beyond bolts/combat, inside far. */
#define VR_HUD_RETICLE_DIST_M 500.f
/* Text angular match: classic HUD text is ~1 deg/char; the raw desktop
 * formula overshoots at VR eye resolutions, so halve it (tunable). */
#define VR_HUD_TEXT_SCALE 0.5f
#define VR_HUD_MAX_BOXES 8

static AeronDrawList2D *hud_lists[2];
static int hud_drawn_logged;
static uint16_t extent_logged_slot, extent_logged_sig;
static uint16_t bm_slot, bm_sig;

/* sRGB-direct PMA tint: the present RT is display-ready sRGB (post
 * tonemap), unlike desktop pre-tonemap linear HUD draws. */
static void hud_argb_to_pma(uint32_t argb, float out[4]) {
    float a = (float)((argb >> 24) & 255u) / 255.f;
    out[0] = (float)((argb >> 16) & 255u) / 255.f * a;
    out[1] = (float)((argb >> 8) & 255u) / 255.f * a;
    out[2] = (float)(argb & 255u) / 255.f * a;
    out[3] = a;
}

static void hud_draw_text(AeronDrawList2D *list, const AeronFontAtlas *font,
                          const char *str, float x, float y, float cell, uint32_t argb) {
    float pen = x;
    float rgba[4];
    if (!list || !font || !font->texture || !str || !(cell > 0.f))
        return;
    if (font->atlas_w <= 0 || font->atlas_h <= 0)
        return;
    hud_argb_to_pma(argb, rgba);
    while (*str) {
        const uint8_t ch = (uint8_t)*str++;
        const AeronFontGlyph *m;
        AeronDrawList2DSprite sprite;
        if (ch < font->first_char || ch >= font->first_char + font->num_chars) {
            pen += cell;
            continue;
        }
        m = &font->glyphs[ch - font->first_char];
        if (!m->atlas_w || !m->atlas_h) {
            pen += cell;
            continue;
        }
        memset(&sprite, 0, sizeof sprite);
        sprite.texture = font->texture;
        sprite.src_u0 = (float)m->atlas_x / (float)font->atlas_w;
        sprite.src_v0 = (float)m->atlas_y / (float)font->atlas_h;
        sprite.src_u1 = (float)(m->atlas_x + m->atlas_w) / (float)font->atlas_w;
        sprite.src_v1 = (float)(m->atlas_y + m->atlas_h) / (float)font->atlas_h;
        sprite.dst_x = pen;
        sprite.dst_y = y;
        sprite.dst_w = cell;
        sprite.dst_h = cell;
        sprite.tint[0] = rgba[0];
        sprite.tint[1] = rgba[1];
        sprite.tint[2] = rgba[2];
        sprite.tint[3] = rgba[3];
        sprite.blend = AERON_BLIT2D_BLEND_PMA;
        sprite.filter = AERON_BLIT2D_FILTER_LINEAR;
        AeronDrawList_AddSprite(list, &sprite);
        pen += cell;
    }
}

void VrHud_BuildEye(int eye, const VrFlightSnapshot *s, const float vp[16], int w, int h) {
    const XwaHudState *hud;
    const AeronFontAtlas *font = NULL;
    uint32_t primary;
    float uscale, cell;
    int font_scale, tier;
    int want_reticle = 0, want_box = 0, want_text = 0;
    char line[96];
    if (eye < 0 || eye > 1 || !s || !vp || w <= 0 || h <= 0)
        return;
    if (!hud_lists[eye])
        hud_lists[eye] = AeronDrawList_Create(512);
    if (!hud_lists[eye])
        return;
    AeronDrawList_Begin(hud_lists[eye], NULL, w, h, AERON_DRAWLIST2D_LOAD, NULL);
    if (!s->hud.valid || !s->hud.hud_enabled)
        return;
    hud = &s->hud;
    primary = hud->hud_colors[0] ? hud->hud_colors[0] : 0xffffffffu;
    uscale = (float)h / 2000.f;
    if (!(uscale > 0.25f))
        uscale = 0.25f;
    /* Desktop tier formula from classic_hud_scale. */
    font_scale = (int)(hud->classic_hud_scale * 10.f);
    if (font_scale < 1)
        font_scale = 1;
    tier = font_scale < 12 ? 2 : (font_scale < 15 ? 1 : 0);
    font = XwaRemasterHud_FlightFont(tier, NULL);
    cell = (float)font_scale * ((float)h / 480.f) * VR_HUD_TEXT_SCALE;

    /* 1. Reticle at the projected ship-forward anchor. */
    if (hud->reticle.visible && s->player_index < s->object_count) {
        float anchor[3], rx, ry, rw;
        int behind = 0;
        VrHud_ShipForward(s->objects[s->player_index].model_to_local_m,
                          VR_HUD_RETICLE_DIST_M, anchor);
        if (VrHud_Project(vp, anchor, w, h, &rx, &ry, &rw, &behind)) {
            float rgba[4], arm = 14.f * uscale, ring = 22.f * uscale;
            int k;
            hud_argb_to_pma(primary, rgba);
            AeronDrawList_AddLine(hud_lists[eye], rx - arm, ry, rx + arm, ry,
                                  2.f * uscale, rgba, AERON_BLIT2D_BLEND_PMA, NULL);
            AeronDrawList_AddLine(hud_lists[eye], rx, ry - arm, rx, ry + arm,
                                  2.f * uscale, rgba, AERON_BLIT2D_BLEND_PMA, NULL);
            for (k = 0; k < 16; k++) {
                float a0 = (float)k * M_PI / 8.f, a1 = (float)(k + 1) * M_PI / 8.f;
                AeronDrawList_AddLine(hud_lists[eye], rx + ring * cosf(a0), ry + ring * sinf(a0),
                                      rx + ring * cosf(a1), ry + ring * sinf(a1),
                                      2.f * uscale, rgba, AERON_BLIT2D_BLEND_PMA, NULL);
            }
            AeronDrawList_AddFill(hud_lists[eye], rx - uscale, ry - uscale,
                                  2.f * uscale, 2.f * uscale, rgba, AERON_BLIT2D_BLEND_PMA, NULL);
            want_reticle = 1;
        }
    }

    /* 2. Target boxes for carried craft with matching box records. */
    if (hud->target_box_count && hud->target.valid) {
        float focal = vp[0] * (float)w * 0.5f;
        unsigned drawn = 0;
        uint32_t oi;
        for (oi = 0; oi < s->object_count && drawn < VR_HUD_MAX_BOXES; oi++) {
            const VrFlightObject *o = &s->objects[oi];
            const XwaHudTargetBox *box = NULL;
            float center[3], bx, by, bw;
            float radius_m = 10.f;
            float half;
            float rgba[4];
            uint16_t bi;
            int behind = 0;
            if (oi == s->player_index || !VrHud_ShipGenus(o->genus))
                continue;
            for (bi = 0; bi < hud->target_box_count; bi++) {
                if (hud->target_boxes[bi].slot == o->id.slot &&
                    hud->target_boxes[bi].signature == o->id.signature) {
                    box = &hud->target_boxes[bi];
                    break;
                }
            }
            if (!box)
                continue;
            if (box->extent > 0)
                radius_m = (float)box->extent * AERON_OPT_METERS_PER_UNIT;
            if (o->id.slot != extent_logged_slot || o->id.signature != extent_logged_sig) {
                SDL_Log("M8_VR_TARGET_EXTENT slot=%u extent=%d radius_m=%.1f",
                        o->id.slot, box->extent, radius_m);
                extent_logged_slot = o->id.slot;
                extent_logged_sig = o->id.signature;
            }
            center[0] = o->model_to_local_m[3];
            center[1] = o->model_to_local_m[7];
            center[2] = o->model_to_local_m[11];
            if (!VrHud_Project(vp, center, w, h, &bx, &by, &bw, &behind))
                continue;
            if (!(focal > 0.f))
                continue;
            half = VrHud_AngularHalfPx(radius_m, bw, focal);
            if (half < 6.f * uscale)
                half = 6.f * uscale;
            if (half > (float)h * 0.45f)
                half = (float)h * 0.45f;
            hud_argb_to_pma(XwaSnapshotExport_FlightPaletteColor(box->color_index), rgba);
            AeronDrawList_AddFrame(hud_lists[eye], bx - half, by - half, half * 2.f, half * 2.f,
                                   2.f * uscale, rgba, AERON_BLIT2D_BLEND_PMA, NULL);
            drawn++;
            want_box = 1;
        }
    }

    /* 3. Off-screen arrow toward the selected target (+ cyan bridge aid). */
    { int mmode = VrHud_TargetMarker((s->hud.valid && s->hud.hud_enabled)?1:0,
            hud->target.valid?1:0, s->target_index, s->object_count, s->player_index);
      uint32_t mcolor;
      if (mmode) {
        mcolor = mmode==1 ? primary : 0xff00ffffu;
        const VrFlightObject *t = &s->objects[s->target_index];
        float center[3], tx, ty, tw;
        float ax, ay;
        int behind = 0;
        int vis;
        center[0] = t->model_to_local_m[3];
        center[1] = t->model_to_local_m[7];
        center[2] = t->model_to_local_m[11];
        int need_arrow;
        vis = VrHud_Project(vp, center, w, h, &tx, &ty, &tw, &behind);
        if (behind) {
            tx = (float)w - tx;
            ty = (float)h - ty;
        }
        need_arrow = behind || !vis;
        if (!need_arrow)
            need_arrow = !VrHud_ClampToRect(tx, ty, w, h, 60.f * uscale, &ax, &ay);
        else
            VrHud_ClampToRect(tx, ty, w, h, 60.f * uscale, &ax, &ay);
        if (need_arrow) {
            /* Chevron at the clamped edge pointing at the true direction. */
            float dx = tx - (float)w * 0.5f, dy = ty - (float)h * 0.5f;
            float len = sqrtf(dx * dx + dy * dy);
            float rgba[4], tip = 14.f * uscale;
            float ux, uy, px, py;
            if (len > 1e-3f) {
                ux = dx / len;
                uy = dy / len;
            } else {
                ux = 0.f;
                uy = 1.f; /* dead behind: point down ("turn around") */
            }
            px = -uy;
            py = ux;
            hud_argb_to_pma(mcolor, rgba);
            AeronDrawList_AddLine(hud_lists[eye], ax, ay,
                                  ax - ux * tip + px * tip * 0.6f, ay - uy * tip + py * tip * 0.6f,
                                  3.f * uscale, rgba, AERON_BLIT2D_BLEND_PMA, NULL);
            AeronDrawList_AddLine(hud_lists[eye], ax, ay,
                                  ax - ux * tip - px * tip * 0.6f, ay - uy * tip - py * tip * 0.6f,
                                  3.f * uscale, rgba, AERON_BLIT2D_BLEND_PMA, NULL);
        } else if (mmode == 2) {
            /* Bridge aid, onscreen: diamond on the tiny/far ship. */
            float dd = 10.f * uscale, drgba[4];
            hud_argb_to_pma(mcolor, drgba);
            AeronDrawList_AddLine(hud_lists[eye], tx, ty - dd, tx + dd, ty,
                                  2.f * uscale, drgba, AERON_BLIT2D_BLEND_PMA, NULL);
            AeronDrawList_AddLine(hud_lists[eye], tx + dd, ty, tx, ty + dd,
                                  2.f * uscale, drgba, AERON_BLIT2D_BLEND_PMA, NULL);
            AeronDrawList_AddLine(hud_lists[eye], tx, ty + dd, tx - dd, ty,
                                  2.f * uscale, drgba, AERON_BLIT2D_BLEND_PMA, NULL);
            AeronDrawList_AddLine(hud_lists[eye], tx - dd, ty, tx, ty - dd,
                                  2.f * uscale, drgba, AERON_BLIT2D_BLEND_PMA, NULL);
        }
        if (mmode == 2 && (t->id.slot != bm_slot || t->id.signature != bm_sig)) {
            SDL_Log("M8_VR_BRIDGE_MARKER id=%u:%u opt=%s", t->id.slot, t->id.signature,
                    t->asset.opt_basename);
            bm_slot = t->id.slot; bm_sig = t->id.signature;
        }
      } }

    /* 4. Target + status text. */
    if (font && font->texture) {
        float ty = 12.f * uscale;
        if (hud->target.valid) {
            SDL_snprintf(line, sizeof line, "%.29s %u.%02u", hud->target.name,
                         hud->target.distance_whole, hud->target.distance_frac);
            hud_draw_text(hud_lists[eye], font, line, (float)w * 0.5f - cell * 10.f, ty, cell, primary);
            want_text = 1;
        }
        ty = 12.f * uscale;
        SDL_snprintf(line, sizeof line, "SHD %d/%d", hud->instruments.shield_front + hud->instruments.shield_rear,
                     hud->instruments.shield_max);
        hud_draw_text(hud_lists[eye], font, line, 12.f * uscale, ty, cell, primary);
        ty += cell * 1.2f;
        SDL_snprintf(line, sizeof line, "HUL %d/%d", hud->instruments.hull_max - hud->instruments.hull_damage,
                     hud->instruments.hull_max);
        hud_draw_text(hud_lists[eye], font, line, 12.f * uscale, ty, cell, primary);
        ty += cell * 1.2f;
        SDL_snprintf(line, sizeof line, "SPD %u THR %u", hud->instruments.speed, hud->instruments.throttle_speed);
        hud_draw_text(hud_lists[eye], font, line, 12.f * uscale, ty, cell, primary);
        ty += cell * 1.2f;
        { unsigned msl = hud->reticle.selected_warhead < 16 ? hud->instruments.warhead_count[hud->reticle.selected_warhead] : 0;
          SDL_snprintf(line, sizeof line, "MSL %u", msl);
          hud_draw_text(hud_lists[eye], font, line, 12.f * uscale, ty, cell, primary);
          ty += cell * 1.2f; }
        SDL_snprintf(line, sizeof line, "%s %s", hud->reticle.in_range ? "RNG" : "---",
                     hud->reticle.missile_lock_state ? "LOCK" : "----");
        hud_draw_text(hud_lists[eye], font, line, 12.f * uscale, ty, cell,
                      hud->reticle.missile_lock_state ? 0xffff0000u : primary);
        /* Shield/hull bars mirror desktop flash semantics (white while flashing). */
        {
            float rgba[4], bw = 120.f * uscale, bh = 6.f * uscale;
            float bx = 12.f * uscale, by = ty + cell * 1.3f;
            float sfrac = hud->instruments.shield_max > 0
                ? (float)(hud->instruments.shield_front + hud->instruments.shield_rear) /
                  (float)hud->instruments.shield_max : 0.f;
            float hfrac = hud->instruments.hull_max > 0
                ? (float)(hud->instruments.hull_max - hud->instruments.hull_damage) /
                  (float)hud->instruments.hull_max : 0.f;
            if (sfrac < 0.f) sfrac = 0.f;
            if (sfrac > 1.f) sfrac = 1.f;
            if (hfrac < 0.f) hfrac = 0.f;
            if (hfrac > 1.f) hfrac = 1.f;
            hud_argb_to_pma(hud->instruments.shield_damage_flash ? 0xffffffffu : 0xff00f40du, rgba);
            AeronDrawList_AddFill(hud_lists[eye], bx, by, bw * sfrac, bh, rgba,
                                  AERON_BLIT2D_BLEND_PMA, NULL);
            hud_argb_to_pma(hud->instruments.hull_damage_flash ? 0xffffffffu : 0xff00ff00u, rgba);
            AeronDrawList_AddFill(hud_lists[eye], bx, by + bh * 1.5f, bw * hfrac, bh, rgba,
                                  AERON_BLIT2D_BLEND_PMA, NULL);
        }
        /* Threats (slow VR-safe blink for missiles). */
        if (hud->threats.laser || hud->threats.turret || hud->threats.beam || hud->threats.missile) {
            int blink = (s->game_time_ms / 500) & 1;
            if (hud->threats.missile && blink)
                hud_draw_text(hud_lists[eye], font, "MISSILE", (float)w * 0.5f - cell * 3.5f,
                              (float)h - cell * 2.f, cell, 0xffff0000u);
            else if (hud->threats.beam)
                hud_draw_text(hud_lists[eye], font, "BEAM", (float)w * 0.5f - cell * 2.f,
                              (float)h - cell * 2.f, cell, 0xffff0000u);
            else if (hud->threats.laser || hud->threats.turret)
                hud_draw_text(hud_lists[eye], font, "HOSTILE", (float)w * 0.5f - cell * 3.5f,
                              (float)h - cell * 2.f, cell, 0xffffff00u);
        }
    }

    /* 5. VR-safe damage overlay (no strobe; latched low alpha while flashing). */
    if (hud->instruments.hull_damage_flash) {
        float rgba[4] = { 0.10f, 0.f, 0.f, 0.10f };
        AeronDrawList_AddFill(hud_lists[eye], 0.f, 0.f, (float)w, (float)h, rgba,
                              AERON_BLIT2D_BLEND_PMA, NULL);
    } else if (hud->instruments.shield_damage_flash) {
        float rgba[4] = { 0.08f, 0.08f, 0.08f, 0.08f };
        AeronDrawList_AddFill(hud_lists[eye], 0.f, 0.f, (float)w, (float)h, rgba,
                              AERON_BLIT2D_BLEND_PMA, NULL);
    }

    if (!(hud_drawn_logged & (1 << eye))) {
        SDL_Log("M8_VR_HUD_DRAWN eye=%d reticle=%d box=%d text=%d", eye, want_reticle, want_box, want_text);
        hud_drawn_logged |= 1 << eye;
    }
}

void VrHud_PrepareEye(int eye, AeronCommandBuffer *cmd) {
    if (eye < 0 || eye > 1 || !hud_lists[eye] || !cmd)
        return;
    (void)AeronDrawList_Prepare(hud_lists[eye], cmd);
}

void VrHud_CompositeEye(int eye, AeronCommandBuffer *cmd, AeronRenderPass *pass,
                        AeronRenderTarget *target) {
    if (eye < 0 || eye > 1 || !hud_lists[eye] || !cmd || !pass || !target)
        return;
    AeronDrawList_RenderIntoPass(hud_lists[eye], cmd, pass, target);
}

void VrHud_Reset(void) {
    int i;
    for (i = 0; i < 2; i++) {
        if (hud_lists[i])
            AeronDrawList_Destroy(hud_lists[i]);
        hud_lists[i] = NULL;
    }
    hud_drawn_logged = 0;
    extent_logged_slot = extent_logged_sig = 0;
    bm_slot = bm_sig = 0;
}
