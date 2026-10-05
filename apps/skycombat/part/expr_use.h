/* Copyright 2026 Rhett Creighton; SPDX-License-Identifier: Apache-2.0
 * purpose: Select one pinned startup HUD and draw its admitted inert recipe. */
#ifndef SKY_EXPR_USE_H
#define SKY_EXPR_USE_H
#include "expr_format.h"
#include "base/hex.h"
#include <raylib.h>
#include <stdbool.h>
#include <string.h>
struct sky_expr_option { const char *path; uint8_t pin[32]; };
struct sky_expr_hud { bool active; struct expr_part part; };
/* argv lives through startup; failure leaves out unchanged. No option is default. */
static inline bool sky_expr_option_parse(int argc,char *const argv[],struct sky_expr_option *out)
{
 if(!out || !argv || argc<1)return false;
 struct sky_expr_option candidate={0};
 if(argc==1){*out=candidate;return true;}
 const char prefix[]="--hud-part=";
 if(argc!=2 || !argv[1] || strncmp(argv[1],prefix,sizeof prefix-1) ||
    strlen(argv[1])<sizeof prefix-1+66)return false;
 const char *value=argv[1]+sizeof prefix-1;
 if(value[64]!=':')return false;
 char hex[65];memcpy(hex,value,64);hex[64]=0;
 if(!zcl_hex_decode(hex,candidate.pin,32))return false;
 candidate.path=value+65;*out=candidate;return true;
}
/* Once at startup. Admission owns diagnostics; a refusal selects the baseline. */
static inline void sky_expr_start(const struct sky_expr_option *option,const double fields[XF_COUNT],struct sky_expr_hud *hud)
{
 hud->active=false;
 if(option->path)hud->active=expr_admit_file(option->path,option->pin,fields,256,
   SKY_HUD_MAX_OPS,SKY_HUD_TEXT_BYTES,&hud->part,NULL)==EX_ADMIT_OK;
}
static inline void sky_expr_draw_op(const struct sky_hud_recipe_v1 *recipe,const struct sky_hud_op_v1 *op)
{
 Color color={(unsigned char)(op->rgba>>24),(unsigned char)(op->rgba>>16),
  (unsigned char)(op->rgba>>8),(unsigned char)op->rgba};
 Rectangle rect={(float)op->x,(float)op->y,(float)op->w,(float)op->h};
 if(op->kind==SKY_HUD_OP_RECT){DrawRectangleRec(rect,color);return;}
 if(op->kind==SKY_HUD_OP_RECT_LINES){DrawRectangleLinesEx(rect,1.0f,color);return;}
 char text[SKY_HUD_TEXT_BYTES+1];memcpy(text,recipe->text+op->text_off,op->text_len);text[op->text_len]=0;
 Font font=GetFontDefault();float px=(float)op->font_px;
 int width=MeasureText(text,(int)op->font_px);
 Vector2 at={(float)op->x,(float)op->y};
 if(op->kind==SKY_HUD_OP_TEXT_BOX) {
  rect.x=(float)((int64_t)op->x-width/2-op->w);rect.width=(float)((int64_t)width+2*(int64_t)op->w);
  DrawRectangleRec(rect,color);return;
 } else if(op->align==SKY_HUD_ALIGN_CENTER)at.x=(float)((int64_t)op->x-width/2);
 else if(op->align==SKY_HUD_ALIGN_RIGHT)at.x=(float)((int64_t)op->x-width);
 DrawTextEx(font,text,at,px,1.0f,color);
}
/* Private admitted state stays immutable. Each frame draws the captured recipe. */
static inline void sky_expr_render(const struct sky_expr_hud *hud,void (*builtin)(void *),void *context)
{
 if(!hud->active){builtin(context);return;}
 for(unsigned i=0;i<hud->part.recipe.op_count;i++)
  sky_expr_draw_op(&hud->part.recipe,&hud->part.recipe.ops[i]);
}
#endif
