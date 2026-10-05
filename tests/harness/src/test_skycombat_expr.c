/* Copyright 2026 Rhett Creighton; SPDX-License-Identifier: Apache-2.0
 * purpose: Reject malformed HUD byte arrays and alternate literal encodings. */
#include "test/test_core.h"
#include "../../../apps/skycombat/part/expr_validate.c"
static const uint8_t expr_empty[32]={'H','U','D','X',0,0,32,0,1};
static const uint8_t expr_rect[88]={
 [0]='H',[1]='U',[2]='D',[3]='X',[6]=32,[8]=1,[12]=2,[14]=1,
 [32]=1,[34]=255,[35]=255,[36]=255,[37]=255,[38]=255,[39]=255,
 [48]=2,[50]=255,[51]=255,[52]=255,[53]=255,[54]=255,[55]=255,[56]=13,
 [64]=1,[66]=1,[82]=255,[83]=255};
static const uint8_t expr_literal[53]={
 [0]='H',[1]='U',[2]='D',[3]='X',[6]=32,[8]=1,[16]=1,[18]=1,[20]=1,
 [34]=1,[40]=1,[42]=255,[43]=255,[48]=1,[52]='A'};
static int expr_expect(const uint8_t *b, size_t n, enum expr_status want)
{
 struct expr_info info, before; memset(&info,0x5a,sizeof info); before=info;
 struct expr_error error={0}; enum expr_status got=expr_validate(b,n,&info,&error);
 bool ok=got==want && error.code==got && error.reason &&
         (got==EX_OK || !memcmp(&before,&info,sizeof info));
 if(!ok) printf("expr bytes n=%zu expected=%u got=%u reason=%s atomic=%d\n",
               n,(unsigned)want,(unsigned)got,error.reason?error.reason:"missing",
               !memcmp(&before,&info,sizeof info));
 return !ok;
}
static int expr_headers(void)
{
 int failures=expr_expect(expr_empty,32,EX_OK); uint8_t b[33];
 for(size_t n=0;n<32;n++) failures+=expr_expect(expr_empty,n,EX_BOUNDS);
 const unsigned offsets[]={0,4,5,6,7,8,9,10,11,24,25,26,27,28,29,30,31};
 for(size_t j=0;j<sizeof offsets/sizeof *offsets;j++) {
  memcpy(b,expr_empty,32); b[offsets[j]]^=1; failures+=expr_expect(b,32,EX_FORMAT);
 }
 const unsigned at[]={12,14,16,18,20}; const unsigned over[]={257,65,65,257,1025};
 for(unsigned j=0;j<5;j++) {
  memcpy(b,expr_empty,32); zcl_write_u32_le(b+at[j],over[j]);
  failures+=expr_expect(b,32,EX_LIMIT);
 }
 memcpy(b,expr_empty,32); b[32]=0; failures+=expr_expect(b,33,EX_BOUNDS);
 uint8_t too_large[EXPR_MAX_BYTES+1]={0};
 failures+=expr_expect(too_large,sizeof too_large,EX_LIMIT);
 memcpy(b,expr_empty,32); b[20]=1; b[32]='A'; failures+=expr_expect(b,33,EX_LITERAL);
 failures+=expr_expect(NULL,32,EX_ARGUMENT);
 struct expr_error e; failures+=expr_validate(expr_empty,32,NULL,&e)!=EX_ARGUMENT;
 return failures;
}
static int expr_records(void)
{
 int failures=expr_expect(expr_rect,88,EX_OK); uint8_t b[88];
 for(size_t n=32;n<88;n++) failures+=expr_expect(expr_rect,n,EX_BOUNDS);
 const unsigned pos[]={32,33,34,36,38,44,56,56,64,65,66,68,82,84};
 const uint8_t val[]={0,1,0,0,0,1,16,255,5,1,0,1,0,1};
 const enum expr_status want[]={EX_OPCODE,EX_FORMAT,EX_REFERENCE,EX_REFERENCE,
  EX_REFERENCE,EX_FORMAT,EX_FIELD_ID,EX_FIELD_ID,EX_SCHEMA,EX_FORMAT,EX_TYPE,
  EX_TYPE,EX_SCHEMA,EX_FORMAT};
 for(unsigned j=0;j<sizeof pos/sizeof *pos;j++) {
  memcpy(b,expr_rect,88); b[pos[j]]=val[j]; failures+=expr_expect(b,88,want[j]);
 }
 for(unsigned field=0;field<16;field++) {
  memcpy(b,expr_rect,64); b[14]=0; b[56]=(uint8_t)field;
  failures+=expr_expect(b,64,EX_OK);
  memcpy(b,expr_rect,88); b[56]=(uint8_t)field;
  failures+=expr_expect(b,88,field>=13?EX_OK:EX_TYPE);
 }
 /* Every arity/type rule: two I32 constants, BOOL field, then this opcode. */
 uint8_t ops[96]={0}; memcpy(ops,expr_rect,64); ops[12]=4; ops[14]=0;
 memcpy(ops+64,expr_rect+32,16); ops[64]=EX_FIELD; ops[72]=13;
 for(unsigned op=3;op<=15;op++) {
  ops[80]=(uint8_t)op; zcl_write_u32_le(ops+88,0);
  static const unsigned arities[16]={0,0,0,2,2,2,2,3,3,2,2,2,2,2,1,3};
  unsigned arity=arities[op], a=op>=12?2:0, br=op==EX_SELECT?0:a;
  zcl_write_u16_le(ops+82,(uint16_t)a);
  zcl_write_u16_le(ops+84,arity>1?(uint16_t)br:EXPR_NONE);
  zcl_write_u16_le(ops+86,arity>2?0:EXPR_NONE);
  /* Expression 1 is changed from FIELD to an independent I32 constant. */
  memcpy(ops+48,expr_rect+32,16);
  failures+=expr_expect(ops,96,EX_OK);
  ops[88]=1; failures+=expr_expect(ops,96,EX_FORMAT); ops[88]=0;
  zcl_write_u16_le(ops+82,3); failures+=expr_expect(ops,96,EX_REFERENCE);
  zcl_write_u16_le(ops+82,(uint16_t)(a==2?0:2));
  failures+=expr_expect(ops,96,EX_TYPE);
 }
 memcpy(b,expr_rect,88); b[40]=1; failures+=expr_expect(b,88,EX_SCHEMA);
 for(unsigned kind=2;kind<=4;kind++) {
  memcpy(b,expr_rect,88); b[64]=(uint8_t)kind;
  failures+=expr_expect(b,88,kind==2?EX_OK:EX_SCHEMA);
 }
 return failures;
}
static int expr_text_cases(void)
{
 int failures=expr_expect(expr_literal,53,EX_OK); uint8_t b[66];
 struct expr_info info={0}; failures+=expr_validate(expr_literal,53,&info,NULL)!=EX_OK;
 failures+=info.texts!=1 || info.pieces!=1 || info.literal_bytes!=1 || info.worst_text!=1 || info.worst_draw_text!=0;
 const unsigned pos[]={32,34,36,40,41,42,44,48,52};
 const uint8_t val[]={1,0,1,3,1,0,2,2,31};
 const enum expr_status want[]={EX_REFERENCE,EX_REFERENCE,EX_FORMAT,EX_OPCODE,
  EX_FORMAT,EX_LITERAL,EX_LITERAL,EX_LITERAL,EX_LITERAL};
 for(unsigned j=0;j<sizeof pos/sizeof *pos;j++) {
  memcpy(b,expr_literal,53); b[pos[j]]=val[j]; failures+=expr_expect(b,53,want[j]);
 }
 memcpy(b,expr_literal,53); b[20]=2; b[44]=1; b[52]='B'; b[53]='A';
 failures+=expr_expect(b,54,EX_LITERAL); /* B1: same template A, unused prefix. */
 b[44]=0; failures+=expr_expect(b,54,EX_LITERAL); /* unused suffix */
 memcpy(b,expr_literal,53); b[18]=2; b[34]=2; b[20]=2;
 memcpy(b+52,b+40,12); b[64]='A'; b[65]='B';
 failures+=expr_expect(b,66,EX_LITERAL); /* shared/overlapping ranges */
 b[44]=1; b[56]=0; failures+=expr_expect(b,66,EX_LITERAL); /* reversed */
 b[44]=0; b[56]=1; failures+=expr_expect(b,66,EX_OK);
 b[48]=0; b[56]=0; b[20]=1; b[64]='B';
 failures+=expr_expect(b,65,EX_OK); /* zero-length at cursor */
 b[44]=1; failures+=expr_expect(b,65,EX_LITERAL); /* alternate zero offset */
 memcpy(b,expr_literal,53); b[20]=0; b[48]=0; failures+=expr_expect(b,52,EX_OK);
 b[40]=2; failures+=expr_expect(b,52,EX_TYPE); /* missing DECIMAL ref */
 return failures;
}
static int expr_limits(void)
{
 uint8_t b[EXPR_MAX_BYTES]={0}; memcpy(b,expr_empty,32);
 zcl_write_u16_le(b+12,256); b[14]=64; b[16]=64; zcl_write_u16_le(b+18,256);
 zcl_write_u32_le(b+20,1024); size_t at=32;
 for(unsigned i=0;i<256;i++,at+=16) memcpy(b+at,expr_rect+32,16);
 b[48]=2; b[56]=13;
 for(unsigned i=0;i<64;i++,at+=8) {zcl_write_u16_le(b+at,(uint16_t)(4*i)); b[at+2]=4;}
 for(unsigned i=0;i<256;i++,at+=12) {
  b[at]=1; b[at+2]=255; b[at+3]=255;
  zcl_write_u32_le(b+at+4,16*((i+3)/4)); if(i%4==0) b[at+8]=16;
 }
 size_t draws=at;
 for(unsigned i=0;i<64;i++,at+=24) memcpy(b+at,expr_rect+64,24);
 memset(b+at,'A',1024); int failures=expr_expect(b,sizeof b,EX_OK);
 for(unsigned i=0;i<64;i++) { b[draws+24*i]=3; zcl_write_u16_le(b+draws+24*i+18,0); }
 failures+=expr_expect(b,sizeof b,EX_OK); /* repeated 16-byte template: 1024 */
 struct expr_info info={0}; failures+=expr_validate(b,sizeof b,&info,NULL)!=EX_OK;
 failures+=info.expressions!=256 || info.draws!=64 || info.texts!=64 ||
           info.pieces!=256 || info.literal_bytes!=1024 || info.worst_text!=1024 || info.worst_draw_text!=1024;
 const size_t pieces=32+4096+512;
 b[pieces+12]=2; zcl_write_u16_le(b+pieces+14,0); zcl_write_u32_le(b+pieces+16,0);
 failures+=expr_expect(b,sizeof b,EX_TEXT_BUDGET); /* DECIMAL adds 11 */
 b[pieces+12]=1; zcl_write_u16_le(b+pieces+14,EXPR_NONE); zcl_write_u32_le(b+pieces+16,16);
 b[pieces+8]=17; b[pieces+252*12+8]=15;
 for(unsigned i=1;i<=252;i++) zcl_write_u32_le(b+pieces+12*i+4,16*((i+3)/4)+1);
 failures+=expr_expect(b,sizeof b,EX_TEXT_BUDGET); /* 64 repeated 17-byte draws */
 b[pieces+8]=16; b[pieces+252*12+8]=16;
 for(unsigned i=1;i<=252;i++) zcl_write_u32_le(b+pieces+12*i+4,16*((i+3)/4));
 b[draws+14]=1; failures+=expr_expect(b,sizeof b,EX_TYPE); /* font BOOL */
 return failures;
}
int test_skycombat_expr(void);
int test_skycombat_expr(void)
{
 int failures=expr_headers()+expr_records()+expr_text_cases()+expr_limits();
 printf("test_skycombat_expr: %s (%d failures)\n",failures?"FAILED":"PASS",failures);
 return failures;
}
