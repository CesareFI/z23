/* Copyright 2026 Rhett Creighton; SPDX-License-Identifier: Apache-2.0
 * purpose: Verify exact Mach-O fixture admission and private-snapshot open gating in the canonical runner. */
#include "test/test_core.h"
#include "../../../apps/skycombat/part/macho_admission.c"
#include "../../../apps/skycombat/part/macho_fixture_a.h"
#include "../../../apps/skycombat/part/macho_fixture_b.h"
#include "../../../apps/skycombat/part/macho_fixture_b_tampered.h"
#include "../../../apps/skycombat/part/macho_fixture_strict_fixture.h"
#include "../../../apps/skycombat/part/macho_load.h"
static enum sky_load_verdict macho_test_loader(struct sky_part_host *,const void *,size_t,const uint8_t[32]);
/* Only the dispatch observation is substituted. Admission and the public
 * production gate below execute the exact application implementations. */
#define sky_part_load macho_test_loader
#define sky_part_open macho_test_open
#include "../../../apps/skycombat/part/part_open.c"
#undef sky_part_load
#undef sky_part_open
static unsigned tests,failed,calls;
static const uint8_t *wanted;
static size_t wanted_n;
static uint8_t wanted_hash[32];
static enum sky_load_verdict inject=SKY_LOAD_OK;
#define MACHO_CHECK(x) do { if(!(x)){fprintf(stderr,"FAIL %u: %s\n",(unsigned)__LINE__,#x);failed++;} } while(0)
static enum sky_load_verdict macho_test_loader(struct sky_part_host *h,const void *b,size_t n,const uint8_t d[32])
{
 calls++;MACHO_CHECK(h!=NULL);MACHO_CHECK(n==wanted_n);MACHO_CHECK(b!=wanted);
 if(n==wanted_n){MACHO_CHECK(!memcmp(b,wanted,n));}
 MACHO_CHECK(!memcmp(d,wanted_hash,32));return inject;
}
static uint32_t r32(const unsigned char *p)
{ return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24; }
static void w32(unsigned char *p,uint32_t v)
{ for(unsigned i=0;i<4;++i)p[i]=(unsigned char)(v>>(i*8)); }
static void w64(unsigned char *p,uint64_t v)
{ for(unsigned i=0;i<8;++i)p[i]=(unsigned char)(v>>(i*8)); }
static void check(const char *label,const unsigned char *b,size_t n,enum sky_macho_verdict want)
{
 unsigned char hash[32];zsha256(b,n,hash);
 enum sky_macho_verdict got=sky_part_admit_macho(b,n,hash);
 ++tests;if(got!=want)++failed;
 printf("%s %s got=%u expected=%u\n",got==want?"PASS":"FAIL",label,got,want);
}
struct strict_layout {size_t segment,section,symcmd,at,ds,other,data,str;unsigned index;};
static bool locate_strict_fixture(const unsigned char *original,size_t n,struct strict_layout *out)
{
 size_t segment=0,symcmd=0,at=32;
 for(unsigned i=0;i<r32(original+16);++i) {
  if(at+8>n)return false;
  if(r32(original+at)==0x19)segment=at;
  if(r32(original+at)==2)symcmd=at;
  at+=r32(original+at+4);
 }
 if(!segment || !symcmd || at>n)return false;
 size_t section=segment+72,sym=r32(original+symcmd+8),str=r32(original+symcmd+16);
 unsigned ns=r32(original+symcmd+12),descriptor=ns;
 for(unsigned i=0;i<ns;++i)
  if(!strcmp((const char *)original+str+r32(original+sym+i*16),"_sky_hud_part_v1"))descriptor=i;
 if(descriptor==ns || ns<2)return false;
 size_t ds=sym+descriptor*16,other=sym+(descriptor?0u:1u)*16;
 unsigned index=original[ds+5];
 size_t data=r32(original+section+(index-1)*80+48);
 *out=(struct strict_layout){segment,section,symcmd,at,ds,other,data,str,index};return true;
}
static int strict_cases(void)
{
 unsigned char original[4096],b[4096];size_t n=sizeof(macho_fixture_strict_fixture);
 if(n>sizeof(original) || n<32)return 2;
 memcpy(original,macho_fixture_strict_fixture,n);
 w32(original+88,5);w32(original+92,5);
 struct strict_layout layout;
 if(!locate_strict_fixture(original,n,&layout))return 2;
 size_t segment=layout.segment,section=layout.section,symcmd=layout.symcmd,at=layout.at;
 size_t ds=layout.ds,other=layout.other,data=layout.data,str=layout.str;
 unsigned index=layout.index;
 check("real arm64 part admitted",original,n,SKY_MACHO_ADMIT);
#define MUT(label,pos,value,want) do { memcpy(b,original,n);w32(b+(pos),(value));check((label),b,n,(want)); } while(0)
 MUT("fat magic",0,0xcafebabe,SKY_MACHO_FORMAT);
 MUT("non-arm64",4,0x1000007,SKY_MACHO_TARGET);
 MUT("arm64e",8,2,SKY_MACHO_TARGET);
 MUT("not MH_OBJECT",12,2,SKY_MACHO_FORMAT);
 memcpy(b,original,n);b[other+4]=1;check("undefined symbol",b,n,SKY_MACHO_UNDEFINED);
 memcpy(b,original,n);b[other+4]=11;check("indirect symbol",b,n,SKY_MACHO_INDIRECT);
 memcpy(b,original,n);b[other+7]|=1;check("symbol resolver",b,n,SKY_MACHO_RESOLVER);
 memcpy(b,original,n);b[other+6]|=0x40;check("weak import",b,n,SKY_MACHO_UNDEFINED);
 MUT("linker option",symcmd,0x2d,SKY_MACHO_LINKER_OPTION);
 unsigned startup[]={9,10,22};
 for(unsigned i=0;i<3;++i) {
  char label[64];snprintf(label,sizeof label,"startup type %u",startup[i]);
  MUT(label,section+64,startup[i],SKY_MACHO_STARTUP);
 }
 for(unsigned type=17;type<=21;++type) {
  char label[64];snprintf(label,sizeof label,"thread-local type %u",type);
  MUT(label,section+64,type,SKY_MACHO_TLS);
 }
 memcpy(b,original,n);b[str+r32(b+ds)]='X';check("missing descriptor",b,n,SKY_MACHO_MISSING);
 MUT("duplicate descriptor",other,r32(original+ds),SKY_MACHO_DUPLICATE);
 MUT("ABI word not one",data,2,SKY_MACHO_ABI);
 check("truncated header",original,31,SKY_MACHO_BOUNDS);
 check("truncated commands",original,at-1,SKY_MACHO_BOUNDS);
 MUT("section overlaps headers",section+48,0,SKY_MACHO_OVERLAP);
 MUT("symbol table overlaps headers",symcmd+8,0,SKY_MACHO_OVERLAP);
 MUT("command header too small",segment+4,4,SKY_MACHO_BOUNDS);
 MUT("section table overlaps next command",segment+64,3,SKY_MACHO_BOUNDS);
 MUT("oversized commands",20,0xffffffffu,SKY_MACHO_BOUNDS);
 for(unsigned type=3;type<=10;++type) {
  if(type==4 || type>=5 || type==3) {
   char label[64];snprintf(label,sizeof label,"unsupported pair/addend relocation %u",type);
   size_t rel=r32(original+section+(index-1)*80+56);
   MUT(label,rel+4,(r32(original+rel+4)&0x0fffffffu)|(type<<28),SKY_MACHO_RELOCATION);
  }
 }
 unsigned char hash[32]={0};++tests;
 if(sky_part_admit_macho(original,n,hash)!=SKY_MACHO_DIGEST)++failed;
 printf("%u cases, %u failures\n",tests,failed);return failed?1:0;
}
static void refused(struct sky_part_host *h,const uint8_t *b,size_t n,
 const uint8_t d[32],enum sky_open_verdict reason)
{
 unsigned before=calls;struct sky_part_host old=*h;
 struct sky_open_result r=macho_test_open(h,b,n,d);
 MACHO_CHECK(r.verdict==reason);MACHO_CHECK(calls==before);
 MACHO_CHECK(!memcmp(h,&old,sizeof(*h)));
 if(reason==SKY_OPEN_DIGEST){MACHO_CHECK(r.admission==SKY_MACHO_DIGEST);}
 if(reason==SKY_OPEN_ADMISSION){MACHO_CHECK(r.admission!=SKY_MACHO_ADMIT);}
}
/* Each transient mutation starts from the exact compiler-produced A bytes.
 * The strict profile's RX declarations are the only seed normalization. */
static void assurance_seed(uint8_t *b)
{
 memcpy(b,macho_fixture_a,sizeof(macho_fixture_a));
 w32(b+32+56,5);w32(b+32+60,5);
}
static void assurance_mutation(const char *label,size_t at,uint64_t value,
 unsigned width,enum sky_macho_verdict reason)
{
 uint8_t b[sizeof(macho_fixture_a)];assurance_seed(b);
 if(width==8)w64(b+at,value);else w32(b+at,(uint32_t)value);
 check(label,b,sizeof(b),reason);
 uint8_t hash[32];zsha256(b,sizeof(b),hash);
 struct sky_part_host h={0};refused(&h,b,sizeof(b),hash,SKY_OPEN_ADMISSION);
}
/* MH_OBJECT segment declarations describe the compiler's combined sections,
 * not receiver mapping rights. Keep exact unmodified compiler bytes. */
static void compiler_object_cases(void)
{
 const uint8_t *fixtures[]={macho_fixture_a,macho_fixture_b,macho_fixture_strict_fixture};
 const size_t sizes[]={sizeof(macho_fixture_a),sizeof(macho_fixture_b),sizeof(macho_fixture_strict_fixture)};
 const char *labels[]={"raw compiler A 7/7 admitted","raw compiler B 7/7 admitted","raw strict compiler 7/7 admitted"};
 for(unsigned i=0;i<3;i++){
  MACHO_CHECK(r32(fixtures[i]+88)==7 && r32(fixtures[i]+92)==7);
  check(labels[i],fixtures[i],sizes[i],SKY_MACHO_ADMIT);
  uint8_t digest[32];zsha256(fixtures[i],sizes[i],digest);
  wanted=fixtures[i];wanted_n=sizes[i];memcpy(wanted_hash,digest,32);
  struct sky_part_host h={0};unsigned before=calls;
  struct sky_open_result r=macho_test_open(&h,fixtures[i],sizes[i],digest);
  MACHO_CHECK(r.verdict==SKY_OPEN_OK && r.admission==SKY_MACHO_ADMIT);
  MACHO_CHECK(r.loader==SKY_LOAD_OK && calls==before+1);
 }
 uint8_t b[sizeof(macho_fixture_strict_fixture)];
 memcpy(b,macho_fixture_strict_fixture,sizeof(b));w32(b+92,5);
 check("segment 7/5 declarations admitted",b,sizeof(b),SKY_MACHO_ADMIT);
 w32(b+92,3);check("segment 7/3 declarations admitted",b,sizeof(b),SKY_MACHO_ADMIT);
 /* An RX segment seed isolates section refusal from segment metadata. */
 w32(b+88,5);w32(b+92,5);
 struct strict_layout layout;
 if(!locate_strict_fixture(b,sizeof(b),&layout)){MACHO_CHECK(false);return;}
 size_t section=layout.section+(layout.index-1u)*80;
 MACHO_CHECK(!memcmp(b+section+16,"__DATA",6));
 w32(b+section+64,0x80000000u);
 check("data section marked pure executable refuses",b,sizeof(b),SKY_MACHO_FORMAT);
 w32(b+section+64,0x400u);
 check("data section marked partly executable refuses",b,sizeof(b),SKY_MACHO_FORMAT);
 w32(b+section+64,0x80000400u);
 memset(b+section,0,16);memcpy(b+section,"__data",6);
 check("writable data plus executable section refuses",b,sizeof(b),SKY_MACHO_FORMAT);
}
static void assurance_cases(void)
{
 compiler_object_cases();
 assurance_mutation("G1 initprot beyond maxprot",88,1,4,SKY_MACHO_FORMAT);
 assurance_mutation("G1 unknown protection bits",88,13,4,SKY_MACHO_FORMAT);
 assurance_mutation("G2 unknown header flags",24,0x80002000u,4,SKY_MACHO_FORMAT);
 assurance_mutation("G2 dynamic-loader header claim",24,0x2004u,4,SKY_MACHO_FORMAT);
 assurance_mutation("G2 segment VM overflow",56,UINT64_MAX,8,SKY_MACHO_FORMAT);
 assurance_mutation("G2 section below segment",56,16,8,SKY_MACHO_FORMAT);
 assurance_mutation("G2 section beyond VM extent",64,1,8,SKY_MACHO_FORMAT);
 assurance_mutation("G2 section beyond file extent",80,1,8,SKY_MACHO_FORMAT);
 assurance_mutation("G2 nonzero segment flags",100,1,4,SKY_MACHO_FORMAT);
}
/* Shift the fixed compiler fixture's descriptor one byte inside its section.
 * Adjust all affected extents/tables, preserving valid ABI bytes and fixup. */
static void descriptor_alignment_case(void)
{
 const uint8_t *original=macho_fixture_strict_fixture;
 const size_t n=sizeof(macho_fixture_strict_fixture);
 uint8_t b[sizeof(macho_fixture_strict_fixture)+1];
 struct strict_layout layout;
 if(!locate_strict_fixture(original,n,&layout)){MACHO_CHECK(false);return;}
 size_t section=layout.section+(layout.index-1u)*80;
 size_t off=layout.data,rel=r32(original+section+56);
 memcpy(b,original,off);b[off]=0;
 memcpy(b+off+1,original+off,n-off);
 w32(b+layout.segment+56,5);w32(b+layout.segment+60,5);
 w64(b+layout.segment+32,(uint64_t)r32(original+layout.segment+32)+1);
 w64(b+layout.segment+48,(uint64_t)r32(original+layout.segment+48)+1);
 w64(b+section+40,(uint64_t)r32(original+section+40)+1);
 w32(b+section+56,(uint32_t)rel+1);
 w32(b+layout.symcmd+8,r32(original+layout.symcmd+8)+1);
 w32(b+layout.symcmd+16,r32(original+layout.symcmd+16)+1);
 w32(b+rel+1,r32(original+rel)+1);
 w64(b+layout.ds+1+8,(uint64_t)r32(original+layout.ds+8)+1);
 check("unaligned descriptor refuses before native mapping",b,sizeof(b),SKY_MACHO_ABI);
 uint8_t digest[32];zsha256(b,sizeof(b),digest);
 struct sky_part_host h={0};refused(&h,b,sizeof(b),digest,SKY_OPEN_ADMISSION);
}
static size_t discarded_seed(uint8_t b[4096],size_t *data,size_t *rel)
{
 struct strict_layout layout;
 const uint8_t *original=macho_fixture_strict_fixture;
 size_t n=sizeof(macho_fixture_strict_fixture);
 if(!locate_strict_fixture(original,n,&layout))return 0;
 size_t end=layout.segment+r32(original+layout.segment+4);
 memcpy(b,original,end);memcpy(b+end+80,original+end,n-end);
 memset(b+end,0,80);
 w32(b+20,r32(b+20)+80);w32(b+layout.segment+4,r32(b+layout.segment+4)+80);
 unsigned ns=r32(b+layout.segment+64);w32(b+layout.segment+64,ns+1);
 w64(b+layout.segment+40,r32(original+layout.segment+40)+80);
 w64(b+layout.segment+48,n+16-r32(original+layout.segment+40));
 w64(b+layout.segment+32,40);w32(b+layout.segment+56,5);w32(b+layout.segment+60,5);
 for(unsigned i=0;i<ns;i++){
  size_t s=layout.section+80*i;
  w32(b+s+48,r32(b+s+48)+80);
  if(r32(b+s+60))w32(b+s+56,r32(b+s+56)+80);
 }
 size_t symcmd=layout.symcmd+80;
 w32(b+symcmd+8,r32(b+symcmd+8)+80);w32(b+symcmd+16,r32(b+symcmd+16)+80);
 memcpy(b+end,"__eh_frame",11);memcpy(b+end+16,"__TEXT",7);
 w64(b+end+32,24);w64(b+end+40,16);w32(b+end+52,3);
 *data=n+80;*rel=*data+16;
 w32(b+end+48,(uint32_t)*data);w32(b+end+56,(uint32_t)*rel);w32(b+end+60,1);
 memset(b+*data,0,24);
 unsigned target=(unsigned)((layout.other-r32(original+layout.symcmd+8))/16);
 w32(b+*rel+4,0x0e000000u|target);
 return *rel+8;
}
static void discarded_cases(void)
{
 uint8_t original[4096],b[4096];size_t data,rel;
 size_t n=discarded_seed(original,&data,&rel);
 if(!n){MACHO_CHECK(false);return;}
 check("G3 derived compiler object with valid dropped relocation",original,n,SKY_MACHO_ADMIT);
#define DROP(label,pos,value,width) do {memcpy(b,original,n);if((width)==8)w64(b+(pos),(value));else w32(b+(pos),(uint32_t)(value));check((label),b,n,SKY_MACHO_RELOCATION);} while(0)
 DROP("G3 discarded effective destination overflow",data,INT64_MAX,8);
 DROP("G3 discarded effective destination underflow",data,UINT64_MAX,8);
 DROP("G3 discarded effective destination one-past",data,8,8);
 memcpy(b,original,n);w32(b+rel+4,0x2d000000u|(r32(b+rel+4)&0xffffffu));
 w32(b+rel,1);w32(b+data+1,0x14000000u);
 check("G3 discarded branch unaligned",b,n,SKY_MACHO_RELOCATION);
 DROP("G3 discarded branch wrong opcode",rel+4,0x2d000000u|(r32(original+rel+4)&0xffffffu),4);
 memcpy(b,original,n);w32(b+rel+4,0x06000003u);w64(b+data,24);
 check("G3 dropped source resolves dropped local target",b,n,SKY_MACHO_ADMIT);
 w64(b+data,40);check("G3 dropped target one-past refuses",b,n,SKY_MACHO_RELOCATION);
 memcpy(b,original,n);memcpy(b+264,"__compact_unwind",16);
 memset(b+280,0,16);memcpy(b+280,"__LD",5);
 check("G3 full-width compact_unwind name accepted",b,n,SKY_MACHO_ADMIT);
#undef DROP
}
int test_skycombat_macho_parts(void);
int macho_vm_cases(void);
int test_skycombat_macho_parts(void)
{
 int failures=0;tests=0;failed=0;calls=0;inject=SKY_LOAD_OK;
 TEST("Mach-O original 37 typed strict cases") {
  ASSERT_EQ(strict_cases(),0);ASSERT_EQ(tests,37u);
  assurance_cases();
  descriptor_alignment_case();
  discarded_cases();
  MACHO_CHECK(macho_vm_cases()==0);
  if(failed){printf("FAIL %u assurance assertions\n",failed);failures++;}
  else printf("OK\n");
 }
 _test_next:;
 TEST("Mach-O private snapshot dispatch and typed refusal") {
  struct sky_part_host h={0};uint8_t bd[32];
  const uint8_t *fixtures[]={macho_fixture_a,macho_fixture_b,macho_fixture_b_tampered};
  const size_t sizes[]={sizeof(macho_fixture_a),sizeof(macho_fixture_b),sizeof(macho_fixture_b_tampered)};
  for(unsigned i=0;i<3;i++){
   uint8_t b[sizeof(macho_fixture_b)];size_t n=sizes[i];
   if(n>sizeof(b)){failed++;continue;}
   memcpy(b,fixtures[i],n);w32(b+88,5);w32(b+92,5);uint8_t d[32];zsha256(b,n,d);
   if(i==1)memcpy(bd,d,32);
   wanted=b;wanted_n=n;memcpy(wanted_hash,d,32);
   if(i<2){unsigned before=calls;struct sky_open_result r=macho_test_open(&h,b,n,d);
    MACHO_CHECK(r.verdict==SKY_OPEN_OK);MACHO_CHECK(r.admission==SKY_MACHO_ADMIT);
    MACHO_CHECK(r.loader==SKY_LOAD_OK);MACHO_CHECK(calls==before+1);
   }else refused(&h,b,n,bd,SKY_OPEN_DIGEST);
   uint8_t wrong[32]={0};refused(&h,b,n,wrong,SKY_OPEN_DIGEST);
   b[8]=2;zsha256(b,n,d);refused(&h,b,n,d,SKY_OPEN_ADMISSION);
  }
  MACHO_CHECK(macho_test_open(NULL,"",0,bd).verdict==SKY_OPEN_ARGUMENT);
  uint8_t admitted_a[sizeof(macho_fixture_a)];assurance_seed(admitted_a);
  wanted=admitted_a;wanted_n=sizeof(admitted_a);
  zsha256(wanted,wanted_n,wanted_hash);inject=SKY_LOAD_BUSY;
  unsigned before=calls;struct sky_open_result r=macho_test_open(&h,wanted,wanted_n,wanted_hash);
  MACHO_CHECK(r.verdict==SKY_OPEN_LOADER);MACHO_CHECK(r.loader==SKY_LOAD_BUSY);
  MACHO_CHECK(r.admission==SKY_MACHO_ADMIT);MACHO_CHECK(calls==before+1);
  inject=SKY_LOAD_OK;
#if !defined(__APPLE__) || !defined(__aarch64__)
  /* Call the actual non-native implementation, independent of the spy. */
  MACHO_CHECK(sky_part_load(&h,wanted,wanted_n,wanted_hash)==SKY_LOAD_PLATFORM);
  MACHO_CHECK(h.arena==NULL && h.current.id==0 && h.last_id==0);
  MACHO_CHECK(sky_part_host_dispose(&h));
#endif
  if(failed){printf("FAIL %u Mach-O assertions\n",failed);failures++;}
  else printf("OK\n");
 }
 return failures;
}
