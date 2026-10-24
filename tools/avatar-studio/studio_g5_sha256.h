#pragma once
/* Standalone SHA-256 for canonical G5 request fingerprints. No raw Rig data. */
#include <stddef.h>
#include <stdint.h>
#include <string.h>
typedef struct {
 uint32_t h[8]; uint64_t nbits; unsigned char block[64]; size_t used;
} G5Sha256;
static uint32_t g5_rotr(uint32_t v, unsigned n){return (v>>n)|(v<<(32-n));}
static void g5_sha_block(G5Sha256 *s,const unsigned char b[64]){
 static const uint32_t k[64]={
  0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
  0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
  0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
  0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
  0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
  0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
  0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
  0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
 };
 uint32_t w[64];
 for(unsigned i=0;i<16;i++)w[i]=((uint32_t)b[4*i]<<24)|((uint32_t)b[4*i+1]<<16)|((uint32_t)b[4*i+2]<<8)|b[4*i+3];
 for(unsigned i=16;i<64;i++){
  uint32_t a=w[i-15],z=w[i-2];
  w[i]=(g5_rotr(z,17)^g5_rotr(z,19)^(z>>10))+w[i-7]+(g5_rotr(a,7)^g5_rotr(a,18)^(a>>3))+w[i-16];
 }
 uint32_t a=s->h[0],bb=s->h[1],c=s->h[2],d=s->h[3],e=s->h[4],f=s->h[5],g=s->h[6],h=s->h[7];
 for(unsigned i=0;i<64;i++){
  uint32_t t1=h+(g5_rotr(e,6)^g5_rotr(e,11)^g5_rotr(e,25))+((e&f)^(~e&g))+k[i]+w[i];
  uint32_t t2=(g5_rotr(a,2)^g5_rotr(a,13)^g5_rotr(a,22))+((a&bb)^(a&c)^(bb&c));
  h=g;g=f;f=e;e=d+t1;d=c;c=bb;bb=a;a=t1+t2;
 }
 s->h[0]+=a;s->h[1]+=bb;s->h[2]+=c;s->h[3]+=d;
 s->h[4]+=e;s->h[5]+=f;s->h[6]+=g;s->h[7]+=h;
}
static void g5_sha_init(G5Sha256 *s){
 static const uint32_t init[8]={0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};
 memcpy(s->h,init,sizeof init);s->nbits=0;s->used=0;
}
static void g5_sha_update(G5Sha256 *s,const void *src,size_t n){
 const unsigned char *p=(const unsigned char*)src;
 for(size_t i=0;i<n;i++){
  s->block[s->used++]=p[i];s->nbits+=8;
  if(s->used==64){g5_sha_block(s,s->block);s->used=0;}
 }
}
static void g5_sha_final(G5Sha256 *s,unsigned char out[32]){
 const uint64_t bits=s->nbits;
 s->block[s->used++]=0x80;
 if(s->used>56){while(s->used<64)s->block[s->used++]=0;g5_sha_block(s,s->block);s->used=0;}
 while(s->used<56)s->block[s->used++]=0;
 for(int i=7;i>=0;i--)s->block[s->used++]=(unsigned char)(bits>>(i*8));
 g5_sha_block(s,s->block);
 for(unsigned i=0;i<8;i++)for(unsigned j=0;j<4;j++)out[4*i+j]=(unsigned char)(s->h[i]>>(24-8*j));
 memset(s,0,sizeof *s);
}
static void g5_sha256(const void *input,size_t n,unsigned char out[32]){
 G5Sha256 s;g5_sha_init(&s);g5_sha_update(&s,input,n);g5_sha_final(&s,out);
}
