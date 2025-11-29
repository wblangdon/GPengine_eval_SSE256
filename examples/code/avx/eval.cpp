




int InstrArg(const OP code) 
{
	const int x = code-IntRangeEnd-1;
	return x;
}

__m256i InstrArg8(const OP code, const retval reg[], const int j)
{
	if(code>IntRangeEnd){
		const int x = InstrArg(code);
		const __m256i index = _mm256_setr_epi32(0,1,2,3,4,5,6,7);
		const __m256i a     = _mm256_i32gather_epi32((int*)&reg[x*16+j],index,1);
		const __m256i mask  = _mm256_set1_epi32(255);
		return _mm256_and_si256(mask,a);   
	}
	else {
		return _mm256_set1_epi32(code);
	}
}

__m256i InstrArg16(const OP code, const retval reg[])
{
	if(code>IntRangeEnd){
		const int x = InstrArg(code);
	const __m256i index1   = _mm256_setr_epi32(0, 1, 2, 3, 4, 5, 6, 7);
        const __m256i a        = _mm256_i32gather_epi32((int*)&reg[x*16],index1,1);
	const __m256i mask     = _mm256_set1_epi32(255);
	const __m256i aa       = _mm256_and_si256(mask,a); 
	const __m256i shuffle  = _mm256_setr_epi8( 0,  1, 4, 5, 8, 9,12,13,-1,-1,-1,-1,-1,-1,-1,-1,
						   16,17,20,21,24,25,28,29,-1,-1,-1,-1,-1,-1,-1,-1);
	const __m256i aaa      = _mm256_shuffle_epi8(aa,shuffle);
	const __m256i perm1    = _mm256_setr_epi32( 0, 1, 4, 5, 7, 7, 7, 7 );
	const __m256i aaaa     = _mm256_permutevar8x32_epi32(aaa,perm1);

	const __m256i index2   = _mm256_setr_epi32( 8, 9,10,11,12,13,14,15);
        const __m256i b        = _mm256_i32gather_epi32((int*)&reg[x*16],index2,1);
	const __m256i bb       = _mm256_and_si256(mask,b); 
	const __m256i bbb      = _mm256_shuffle_epi8(bb,shuffle);
	const __m256i perm2    = _mm256_setr_epi32(7, 7, 7, 7, 0, 1, 4, 5);
	const __m256i bbbb     = _mm256_permutevar8x32_epi32(bbb,perm2);

	const __m256i out16 = _mm256_or_si256(aaaa,bbbb);
		return out16;
	}
	else {
		return _mm256_set1_epi16(code);
	}
}
int InstrReg(const OP code) 
{
	const int x = code-IntRangeEnd-1;
	return x;
}

__m256i InstrReg16(const OP code, const retval reg[])
{
	const int x = InstrReg(code);
	const __m256i index1   = _mm256_setr_epi32(0, 1, 2, 3, 4, 5, 6, 7);
        const __m256i a        = _mm256_i32gather_epi32((int*)&reg[x*16],index1,1);
	const __m256i mask     = _mm256_set1_epi32(255);
	const __m256i aa       = _mm256_and_si256(mask,a); 
	const __m256i shuffle  = _mm256_setr_epi8( 0,  1, 4, 5, 8, 9,12,13,-1,-1,-1,-1,-1,-1,-1,-1,
						   16,17,20,21,24,25,28,29,-1,-1,-1,-1,-1,-1,-1,-1);
	const __m256i aaa      = _mm256_shuffle_epi8(aa,shuffle);
	const __m256i perm1    = _mm256_setr_epi32( 0, 1, 4, 5, 7, 7, 7, 7 );
	const __m256i aaaa     = _mm256_permutevar8x32_epi32(aaa,perm1);

	const __m256i index2   = _mm256_setr_epi32( 8, 9,10,11,12,13,14,15);
        const __m256i b        = _mm256_i32gather_epi32((int*)&reg[x*16],index2,1);
	const __m256i bb       = _mm256_and_si256(mask,b); 
	const __m256i bbb      = _mm256_shuffle_epi8(bb,shuffle);
	const __m256i perm2    = _mm256_setr_epi32(7, 7, 7, 7, 0, 1, 4, 5);
	const __m256i bbbb     = _mm256_permutevar8x32_epi32(bbb,perm2);

	const __m256i out16 = _mm256_or_si256(aaaa,bbbb);
	return out16;
}
__m256i InstrReg8(const OP code, const retval reg[], const int j)
{
	const int x = InstrReg(code);
	const __m256i index = _mm256_setr_epi32(0,1,2,3,4,5,6,7);
        const __m256i a     = _mm256_i32gather_epi32((int*)&reg[x*16+j],index,1);
	const __m256i mask  = _mm256_set1_epi32(255);
	const __m256i out8  = _mm256_and_si256(mask,a); 
	return out8;
}


void Interpret16(
const int InstrLen, const instr *Instr, regtype registers[NumVar*npar], retval reg[NumVar*npar], const uint32_t div32[256*256]) {

for (int i=0;i<InstrLen;i++) {		
  
  if(Instr[i][2]==div_op) {
  
  for(int j=0;j<2;j++){
    double* output64 = (double*) &reg[int(Instr[i][0])*16+j*8]; 
    const __m256i a    = InstrReg8(Instr[i][1],reg,j*8);
    const __m256i a256 = _mm256_slli_si256(a, 1); 
    const __m256i by   = InstrArg8(Instr[i][3],reg,j*8);
    const __m256i index_vec = _mm256_add_epi32(a256,by);
    const __m256i result_vec = _mm256_i32gather_epi32((int*)div32, index_vec, 4);
    const __m256i shuffle = _mm256_setr_epi8( 0, 4, 8,12,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
					      16,20,24,28,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1);
    const __m256i b = _mm256_shuffle_epi8(result_vec,shuffle);
    const __m256i perm = _mm256_setr_epi32( 0, 4, 7, 7, 7, 7, 7, 7 );
    const __m256i c = _mm256_permutevar8x32_epi32(b,perm);
    
    output64[0] = _mm256_cvtsd_f64((__m256d)c);
  }
  } else {
    

    
    

    const __m256i a = InstrReg16(Instr[i][1],reg);
    const __m256i b = InstrArg16(Instr[i][3],reg);
    __m256i c;

    switch(int(Instr[i][2]))
      {
      case plus_op:{
	
	c = _mm256_add_epi16(a,b);
	break;
      }
      case minus_op:{
	
	c = _mm256_sub_epi16(a,b);
	break;
      }
      case mul_op:{
	
	c = _mm256_mullo_epi16(a,b);
	break;
      }
      }
    
    {
      const __m256i shuffle = _mm256_setr_epi8( 0, 2, 4, 6, 8,10,12,14,-1,-1,-1,-1,-1,-1,-1,-1,
						16,18,20,22,24,26,28,30,-1,-1,-1,-1,-1,-1,-1,-1);
      const __m256i cc      = _mm256_shuffle_epi8(c,shuffle);
      const __m256i perm    = _mm256_setr_epi32( 0, 1, 4, 5, 7, 7, 7, 7 );
      const __m256i ccc     = _mm256_permutevar8x32_epi32(cc,perm);
      const __m128i cccc    = _mm256_castsi256_si128(ccc);
      _mm_store_si128((__m128i*)&reg[int(Instr[i][0])*16],cccc);  
      
    }
  }
 }
}
