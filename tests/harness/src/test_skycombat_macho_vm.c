/* Copyright 2026 Rhett Creighton; SPDX-License-Identifier: Apache-2.0
 * purpose: Observe the actual loader's permission transitions with inert memory and injected refusals. */
#include "test/test_core.h"
#include "../../../apps/skycombat/part/macho_load.h"
#include "../../../apps/skycombat/part/macho_fixture_a.h"
#include "zsha256/zsha256.h"
/* Inert VM vocabulary: no POSIX headers or native mappings in this TU. */
enum { PROT_NONE=0,PROT_READ=1,PROT_WRITE=2,PROT_EXEC=4,
       MAP_PRIVATE=2,MAP_ANON=0x1000,MAP_JIT=0x800 };
#define MAP_FAILED ((void *)-1)
static unsigned vm_failures,vm_wx,vm_maps,vm_changes,vm_flushes,vm_releases;
static unsigned vm_deny_change;
static bool vm_deny_map,vm_deny_release;
static void *vm_arena,*vm_allocation;
static int vm_rights[2];
#define VM_CHECK(x) do {if(!(x)){fprintf(stderr,"FAIL VM %u: %s\n",(unsigned)__LINE__,#x);vm_failures++;}} while(0)
static void *vm_map(void *addr,size_t n,int prot,int flags,int fd,int64_t off)
{
 (void)addr;(void)flags;(void)fd;(void)off;vm_maps++;
 VM_CHECK(prot==PROT_NONE);VM_CHECK(!(flags&MAP_JIT));
 if((prot&(PROT_WRITE|PROT_EXEC))==(PROT_WRITE|PROT_EXEC)){vm_wx++;return MAP_FAILED;}
 if(vm_deny_map)return MAP_FAILED;
 const size_t padding=16383;
 if(n>SIZE_MAX-padding)return MAP_FAILED;
 vm_allocation=malloc(n+padding);
 if(!vm_allocation)return MAP_FAILED;
 uintptr_t at=(uintptr_t)vm_allocation;
 if(at>UINTPTR_MAX-padding){free(vm_allocation);vm_allocation=NULL;return MAP_FAILED;}
 vm_arena=(void *)((at+padding)&~(uintptr_t)padding);
 vm_rights[0]=prot;vm_rights[1]=prot;return vm_arena;
}
static int vm_protect(void *addr,size_t n,int prot)
{
 vm_changes++;VM_CHECK(n==1024u*1024u);
 if((prot&(PROT_WRITE|PROT_EXEC))==(PROT_WRITE|PROT_EXEC)){vm_wx++;return -1;}
 size_t slot=((uint8_t *)addr-(uint8_t *)vm_arena)/(1024u*1024u);
 if(slot>1){VM_CHECK(false);return -1;}
 VM_CHECK(prot==(PROT_READ|PROT_WRITE) || prot==(PROT_READ|PROT_EXEC));
 if(prot==(PROT_READ|PROT_EXEC))VM_CHECK(vm_rights[slot]==(PROT_READ|PROT_WRITE));
 if(vm_changes==vm_deny_change)return -1;
 vm_rights[slot]=prot;return 0;
}
static int vm_release(void *addr,size_t n)
{
 VM_CHECK(addr==vm_arena);VM_CHECK(n==2u*1024u*1024u);
 vm_releases++;if(vm_deny_release)return -1;
 free(vm_allocation);vm_allocation=NULL;vm_arena=NULL;return 0;
}
static void vm_flush(void *addr,size_t n)
{
 (void)n;vm_flushes++;
 size_t slot=((uint8_t *)addr-(uint8_t *)vm_arena)/(1024u*1024u);
 VM_CHECK(slot<2);if(slot<2)VM_CHECK(vm_rights[slot]==(PROT_READ|PROT_EXEC));
}
static void *vm_clear(void *addr,int value,size_t n)
{
 if(n==1024u*1024u){
  uintptr_t first=(uintptr_t)vm_arena,at=(uintptr_t)addr;
  bool inside=vm_arena && at>=first && at-first<2u*1024u*1024u;
  VM_CHECK(inside);
  if(inside){
   size_t slot=(size_t)(at-first)/(1024u*1024u);
   VM_CHECK((at-first)%(1024u*1024u)==0);
   VM_CHECK(vm_rights[slot]==(PROT_READ|PROT_WRITE));
  }
 }
 return memset(addr,value,n);
}
#define SKY_MACHO_VM_TEST 1
#define memset vm_clear
#define mmap vm_map
#define mprotect vm_protect
#define munmap vm_release
#define sys_icache_invalidate vm_flush
#define sky_part_load vm_load
#define sky_part_call_begin vm_call_begin
#define sky_part_call_end vm_call_end
#define sky_part_host_dispose vm_dispose
#include "../../../apps/skycombat/part/macho_load.c"
#undef sky_part_load
#undef sky_part_call_begin
#undef sky_part_call_end
#undef sky_part_host_dispose
#undef memset
#undef mmap
#undef mprotect
#undef munmap
static void vm_preserved(const struct sky_part_host *h,const struct sky_part_host *old,
 const uint8_t hash[32])
{
 VM_CHECK(!memcmp(h,old,sizeof(*h)));
 uint8_t now[32];zsha256((uint8_t *)h->arena+(size_t)h->slot*SLOT,SLOT,now);
 VM_CHECK(!memcmp(now,hash,32));VM_CHECK(vm_rights[h->slot]==(PROT_READ|PROT_EXEC));
}
static void vm_replacement_failures(struct sky_part_host *h,const uint8_t *bytes,
 size_t n,const uint8_t digest[32])
{
 struct sky_part_host old=*h;uint8_t hash[32];
 zsha256((uint8_t *)h->arena+(size_t)h->slot*SLOT,SLOT,hash);
 for(unsigned step=1;step<=2;step++){
  vm_deny_change=vm_changes+step;
  VM_CHECK(vm_load(h,bytes,n,digest)==SKY_LOAD_PLATFORM);
  vm_preserved(h,&old,hash);
 }
 vm_deny_change=0;
 VM_CHECK(vm_load(h,bytes,n,digest)==SKY_LOAD_OK);
 VM_CHECK(h->current.id==2 && h->slot==1);
 VM_CHECK(vm_call_begin(h)!=NULL);
 unsigned before=vm_changes;
 VM_CHECK(vm_load(h,bytes,n,digest)==SKY_LOAD_BUSY);
 VM_CHECK(vm_changes==before);VM_CHECK(!vm_dispose(h));VM_CHECK(vm_call_end(h));
 VM_CHECK(vm_load(h,bytes,n,digest)==SKY_LOAD_OK);
 VM_CHECK(h->current.id==3 && h->slot==0);
 VM_CHECK(vm_rights[0]==(PROT_READ|PROT_EXEC));VM_CHECK(vm_rights[1]==(PROT_READ|PROT_EXEC));
}
static void vm_initial_failures(const uint8_t *bytes,size_t n,const uint8_t digest[32])
{
 struct sky_part_host h={0};
 vm_deny_map=true;VM_CHECK(vm_load(&h,bytes,n,digest)==SKY_LOAD_MEMORY);
 VM_CHECK(h.arena==NULL && h.current.id==0);vm_deny_map=false;
 for(unsigned step=1;step<=2;step++){
  vm_deny_change=vm_changes+step;
  VM_CHECK(vm_load(&h,bytes,n,digest)==SKY_LOAD_PLATFORM);
  VM_CHECK(h.arena==NULL && h.current.id==0 && h.last_id==0 && vm_arena==NULL);
 }
 vm_deny_change=vm_changes+2;vm_deny_release=true;
 VM_CHECK(vm_load(&h,bytes,n,digest)==SKY_LOAD_PLATFORM);
 VM_CHECK(h.failed && h.arena!=NULL && h.current.id==0 && h.last_id==0);
 unsigned before=vm_maps;
 VM_CHECK(vm_load(&h,bytes,n,digest)==SKY_LOAD_PLATFORM);VM_CHECK(vm_maps==before);
 VM_CHECK(vm_call_begin(&h)==NULL);VM_CHECK(!vm_dispose(&h));VM_CHECK(h.failed);
 vm_deny_release=false;VM_CHECK(vm_dispose(&h));VM_CHECK(h.arena==NULL && !h.failed);
 vm_deny_change=0;
}
int macho_vm_cases(void);
int macho_vm_cases(void)
{
 vm_failures=vm_wx=vm_maps=vm_changes=vm_flushes=vm_releases=vm_deny_change=0;
 struct sky_part_host h={0};uint8_t bytes[sizeof(macho_fixture_a)],hash[32];
 memcpy(bytes,macho_fixture_a,sizeof(bytes));
 /* Raw compiler 7/7 declarations never determine actual mapping requests. */
 zsha256(bytes,sizeof(bytes),hash);
 VM_CHECK(vm_load(&h,bytes,sizeof(bytes),hash)==SKY_LOAD_OK);
 VM_CHECK(vm_wx==0);VM_CHECK(vm_maps==1);VM_CHECK(vm_changes==2);
 VM_CHECK(vm_flushes==1);VM_CHECK(h.current.id==1);
 if(h.current.id)vm_replacement_failures(&h,bytes,sizeof(bytes),hash);
 vm_deny_release=true;VM_CHECK(!vm_dispose(&h));
 VM_CHECK(h.failed && h.current.id==3 && h.arena!=NULL);
 VM_CHECK(vm_call_begin(&h)==NULL);
 VM_CHECK(vm_load(&h,bytes,sizeof(bytes),hash)==SKY_LOAD_PLATFORM);
 vm_deny_release=false;
 VM_CHECK(vm_dispose(&h));VM_CHECK(vm_arena==NULL);
 vm_initial_failures(bytes,sizeof(bytes),hash);
 VM_CHECK(vm_wx==0);
 /* The observer never maps executable memory or invokes candidate functions. */
 printf("G1 VM observer: %u failures, %u RWX requests\n",vm_failures,vm_wx);
 return (int)vm_failures;
}
