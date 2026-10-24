/* G5 canonical SHA-256 independent published vectors, model-free. */
#include "../studio_g5_sha256.h"
#include <stdio.h>
#include <string.h>
static int check(const char *input,const char *expected){
 unsigned char bytes[32];char actual[65];
 g5_sha256(input,strlen(input),bytes);
 for(unsigned i=0;i<32;i++)
  (void)sprintf(actual+2*i,"%02x",bytes[i]);
 return strcmp(actual,expected)==0;
}
int main(void){
 if(!check("","e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855")||
    !check("abc","ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad")||
    !check("The quick brown fox jumps over the lazy dog",
           "d7a8fbb307d7809469ca9abcb0082e4f8d5651e46d3cdb762d02d0bf37c9e592")){
  fputs("G5_SHA256_VECTOR_RED\n",stderr);return 1;
 }
 puts("G5_SHA256_VECTORS_PASS 3/3");
 return 0;
}
