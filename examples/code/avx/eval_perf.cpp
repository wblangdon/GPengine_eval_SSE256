//extern const char* rev = "$Revision: 1.19 $";
// Magpie fixed stub for evolving AVX eval for GPengine.cpp
//
//Changes
//WBL 17 Oct 2025 bugfix error2, allow 16 bit answers
//WBL 14 Oct 2025 check all registers (not just R0) add using mprotect
//WBL 12 Oct 2025 Based on div_perf.cpp r1.17 and GPengine.cpp r1.119
//WBL 11 Oct 2025 organise combined div.cpp as 3 functions
//WBL 10 Oct 2025 combine div_funcs.cpp with div.cpp
//WBL  8 Oct 2025 Switch to AVX based on GPengine.cpp r1.65 Interpret16
//WBL  7 Oct 2025 Switch to instruction count, clean output for fit.bat
//WBL  2 Oct 2025 based on interpret from GPengine.cpp r1.119, main.cpp r1.22


/* compile:
  g++ -fpermissive -fmax-errors=1 -Dregtype=int eval_perf.cpp 
 */

#include <stdlib.h>
#include <iostream>
#include <assert.h>
#include <string.h>
#include <fstream>
#include <stdio.h>

#include <immintrin.h>

#include <climits>

#include <malloc.h>
#include <signal.h>
#include <stdio.h>
#include <sys/mman.h>
#include <unistd.h>

//from GPengine.h r1.56
typedef int OP;
typedef OP instr[4];
#define validvar(x)    (x>= 0 && x < NumVar)
#define validreg(x)    (x > IntRangeEnd && x <= (IntRangeEnd + NumVar))
#define validopcode(x) (x>= 0 && x < NumOp)
#define validvalue(x)  (x >= IntRangeStart && x <= IntRangeEnd)
#define validInstr(x) (\
  validvar(   x[0]) && \
  validreg(   x[1]) && \
  validopcode(x[2]) && \
  (validreg(x[3]) || validvalue(x[3])))

const int IntRangeStart = 0;
const int IntRangeEnd = 127;

const int NumVar = 8;
enum { plus_op, minus_op, mul_op, div_op, NumOp };

using namespace std;

//share between main and div_eval
typedef unsigned char retval;
//#define regtype retval chosen by Magpie
#define npar 16
#define InstrArg InstrArg_magpie
#define InstrReg InstrReg_magpie
#include "eval.cpp"
#undef InstrReg
#undef InstrArg

unsigned int rrand(int max) 
{
	return (rand()%max);	
}

//Based on https://github.com/wblangdon/linux_perf_api/blob/main/demo_perf.c
//and test_prog.c r1.4
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/perf_event.h>
#include <asm/unistd.h>
static long
perf_event_open(struct perf_event_attr *hw_event, pid_t pid,
		int cpu, int group_fd, unsigned long flags){
  int ret;
  ret = syscall(__NR_perf_event_open, hw_event, pid, cpu,group_fd, flags);
  return ret;
}
int perf_start(){
  struct perf_event_attr pe;
  int fd;

  memset(&pe, 0, sizeof(pe));
  //set up perf's event data structure.
  //There are many things perf can report, here ask for instruction count
  //Note your CPU may not support all perf's options
  pe.type = PERF_TYPE_HARDWARE;	//PERF_TYPE_HW_CACHE;
  //pe.type = PERF_TYPE_SOFTWARE;
  pe.size = sizeof(pe);
  //PERF_COUNT_HW_CACHE_MAX
  //PERF_COUNT_HW_CACHE_OP_MAX
  //PERF_COUNT_HW_CACHE_RESULT_MAX
  //? not yet ? PERF_COUNT_SW_MAX
  pe.config = PERF_COUNT_HW_INSTRUCTIONS; //PERF_COUNT_HW_CACHE_MISSES;
  //pe.config = PERF_COUNT_SW_CPU_CLOCK;
  pe.disabled = 1;
  pe.exclude_kernel = 1;
  pe.exclude_hv = 1;

  fd = perf_event_open(&pe, 0, -1, -1, 0);
  if (fd == -1) {
    fprintf(stderr, "Error on perf_event_open %llx\n", pe.config);
    return EXIT_FAILURE;
  }

  //ioctl(fd, PERF_EVENT_IOC_RESET, 0);
  //ioctl(fd, PERF_EVENT_IOC_ENABLE, 0);
  return fd;
}
long long perf_end(const int fd) {
  long long count = -1;
  ioctl(fd, PERF_EVENT_IOC_DISABLE, 0);
  if(errno != 0) {//check all is well 
    //Might get for example errno == EBADF 9 /* Bad file number */ bad fd ?
    fprintf(stderr,"perf_end(%d) Opps errno is %d\n",fd,errno);
    exit(errno);
  }
  read(fd, &count, sizeof(count)); //instructions is first 8 bytes read from fd
  return count;
}

const int FitnessCaseNum = 64;
int regO[FitnessCaseNum/16];
int reg1[FitnessCaseNum/16];
int reg2[FitnessCaseNum/16];
unsigned char Output[FitnessCaseNum];
unsigned char Input1[FitnessCaseNum];
unsigned char Input2[FitnessCaseNum];

unsigned char prot_div(const unsigned char op1, const unsigned char op2){
  if (op2 == 0) return 0;
  return op1/op2;
}
void settest(const int i, const int op1, const int op2, const int debug){
  assert(  i>=0 &&   i<FitnessCaseNum);
  Output[i] = prot_div(op1,op2);
  Input1[i] = op1;
  Input2[i] = op2;
  //cout<<"settest("<<i<<","<<op1<<","<<op2<<","<<debug<<") "
  //    <<(int)Output[i]<<endl;
  assert(op1>=0 && op1<256);
  assert(op2>=0 && op2<256);
}
void divtests(){
  /*
  64 fitness cases.
  no read or writes outside inputs or outputs
    50% div 0 output 0
    50% else output   0 eg  10/11 :  4 12%
	     output   1 eg  12/11 :  4 12%
	     output 255 ie 255/1  :  4 12%
	     output >=1 eg 200/1  :  4 12%
	     output >=2 eg 174/72 : 16 25%
  */
  for(int i=0;i<FitnessCaseNum;i++){
    if(rrand(2)) settest(i,rrand(256),0,-1);
    else {
      const int r   = rrand(8);
      const int op2 = 2+rrand(254);
      switch (r){
      case 0: settest(i,rrand(op2),     op2,r); break;
      case 1: {
       	const int op1 = op2+((op2<128)? rrand(op2) : rrand(256-op2));
	settest(i,op1,                  op2,r); break;}
      case 2:  settest(i,255,             1,r); break;
      case 3:  settest(i,rrand(256),      1,r); break;
      default: {// >=2 
	const int ratio = 2+rrand(125);
	const int op2   = 1+rrand(255/ratio);
	const int op1   = ratio*op2+rrand(256-ratio*op2);
	settest(i,op1,                 op2,r);	break;}
      }//switch r
    }//else
  }//endfor i
}

int InstrArg(const OP code)
{
	const int x = code-IntRangeEnd-1;
	assert(code<=IntRangeEnd || (x    >= 0 &&    x < NumVar));
	if(code>IntRangeEnd) return x;       //code for Var
	else                 return INT_MAX; //code is constant, do not use
}

retval InstrArg(const OP code, const retval reg[], const int answers[NumVar])
{
	if(code>IntRangeEnd) {
	  const int x = InstrArg(code);
	  assert(answers[x]>=0 && answers[x]<256);
	}
	if(code>IntRangeEnd) return reg[InstrArg(code)]; //code for Var
	else                 return code;                //code is constant
}

int InstrReg(const OP code) //return code not value
{
	const int x = code-IntRangeEnd-1;
	assert(   x >= 0 &&    x < NumVar);
	return x;
}

retval InstrReg(const OP code, const retval reg[], const int answers[NumVar])
{
	const int x = InstrReg(code);
	assert(answers[x]>=0 && answers[x]<256);
	return reg[x];
}

void eval(const int prog, const int pc, const int thread,
	  const instr I, retval reg[NumVar], int answers[NumVar]){
  retval val;
  const retval op1 = InstrReg(I[1],reg,answers);
  const retval op2 = InstrArg(I[3],reg,answers);
  switch(I[2]) {
  case plus_op: 
    val = op1+op2;
    break;
  case minus_op: 
    val = op1-op2;
    break;
  case mul_op: 				
    val = op1*op2;
    break;
  case div_op: 
    if (op2 == 0)
      val = 0;
    else
      val = op1/op2;
    break;
  default:
    exit(10);
  }
  reg[I[0]]     = val;
  answers[I[0]] = val;
  /*cout<<"eval "<<prog<<" "<<pc<<" "<<thread<<":\t"
      <<I[1]<<"("<<(int)op1<<") "
      <<I[2]<<" "
      <<I[3]<<"("<<(int)op2<<") "
      <<"\tgives "<<(int)val<<" into "<<I[0]<<endl;
  assert(op1!=252 && op2 != 252); //could be ok but probably an error
  */
}
void evaln(const int prog, const int pc,
	   const instr I, retval regs[npar][NumVar],int answers[npar][NumVar]){
  assert(validInstr(I));  
  for(int i=0;i<npar;i++) eval(prog,pc,i,I,regs[i],answers[i]);
}

OP GenerateReg(const int nreg, const int regs[]){
  return IntRangeEnd + regs[rrand(nreg)] + 1; //code for Var
}
OP GenerateArg(const int nreg, const int regs[]){// Randomly choose a var or a const
  if(rrand(10)<2) return GenerateReg(nreg,regs); //code for Var
  else            return (IntRangeEnd-IntRangeStart)*double(rand())/RAND_MAX+IntRangeStart;
}
int ChooseOp(const int ops[NumOp]){
  int nmissing = 0;
  int missing[NumOp];
  for(int i=0;i<NumOp;i++){
    if(ops[i]==0){
      missing[nmissing] = i;
      nmissing++;
    }
  }
  return (nmissing)? missing[rrand(nmissing)] : rrand(NumOp);
}
void tests(const int regO[4], const int reg1[4], const int reg2[4],
	   instr prog[4][4], int answers[4][npar][NumVar]){
  assert(FitnessCaseNum/16==4);
  memset(answers,255,4*npar*NumVar*sizeof(int)); //-1 means not over written
  int ops[NumOp] = {0,0,0,0};
  
  for(int i=0;i<4;i++){
    assert(i*npar+15<FitnessCaseNum);
    retval regs[npar][NumVar]; memset(regs,252,sizeof(regs));
    for(int j=0;j<npar;j++) answers[i][j][reg1[i]] = regs[j][reg1[i]] = Input1[i*npar+j];
    for(int j=0;j<npar;j++) answers[i][j][reg2[i]] = regs[j][reg2[i]] = Input2[i*npar+j];

    //fabricate 4 instruction GPengine program starting with protected div
    //CGPengine::GenerateInstr(instr I)
    prog[i][0][0] = rrand(NumVar);
    prog[i][0][1] = IntRangeEnd + reg1[i] + 1; //code for Var, GenerateReg()
    prog[i][0][2] = div_op;
    prog[i][0][3] = IntRangeEnd + reg2[i] + 1; //code for Var, GenerateArg() 
    evaln(i,0,prog[i][0],regs,answers[i]);
    int r3[3] = {reg1[i],reg2[i],prog[i][0][0]};
    ops[prog[i][0][2]]++;
    
    /*follow protected div by three (almost) random instructions */
    prog[i][1][0] = rrand(NumVar);
    prog[i][1][1] = GenerateReg(3,r3);
    prog[i][1][2] = (i<2)? rrand(NumOp) : ChooseOp(ops); //ensure test all ops eventually
    prog[i][1][3] = GenerateArg(3,r3);
    evaln(i,1,prog[i][1],regs,answers[i]);
    int r4[4]; memcpy(r4,r3,sizeof(r3)); r4[4-1]=prog[i][0][0];
    ops[prog[i][1][2]]++;

    prog[i][2][0] = rrand(NumVar);
    prog[i][2][1] = GenerateReg(4,r4);
    prog[i][2][2] = (i<2)? rrand(NumOp) : ChooseOp(ops); //ensure test all ops eventually
    prog[i][2][3] = GenerateArg(4,r4);
    evaln(i,2,prog[i][2],regs,answers[i]);
    int r5[5]; memcpy(r5,r4,sizeof(r4)); r5[5-1]=prog[i][0][0];
    ops[prog[i][2][2]]++;

    prog[i][3][0] = regO[i];
    prog[i][3][1] = GenerateReg(5,r5);
    prog[i][3][2] = (i<2)? rrand(NumOp) : ChooseOp(ops); //ensure test all ops eventually
    prog[i][3][3] = GenerateArg(5,r5);
    evaln(i,3,prog[i][3],regs,answers[i]);
    ops[prog[i][3][2]]++;
  }//endfor i<4
  assert(ops[0] && ops[1] && ops[2] && ops[3]);
}

/*suppose double is really fast**
#define regtype double
inline unsigned char convertregtype(const double x) {
  assert(x >= 0 && x < 256);
  return x;  //round down
}*/
#define convertregtype(x) x

//https://stackoverflow.com/questions/49153315/print-out-the-content-of-m256i-variable
void write_m256i(const __m256i & value)
{
  const size_t n = sizeof(__m256i) / sizeof(int);
  int buffer[n];
  _mm256_storeu_si256((__m256i*)buffer, value);
  for (int i = 0; i < n; i++) {
    char buff[80];sprintf(buff,"%3d ",buffer[i]);
    cout << buff << " ";
  }
  cout<<endl;
}

void protect(const void* target, const size_t size, const int mode){
  const size_t pagesize = sysconf(_SC_PAGE_SIZE);
  //cout<<"pagesize="<<pagesize<<endl;
  assert(pagesize == 4096);
  const long long start = (long long) target    & (~4095);
  const long long end   = start+size+pagesize-1 & (~4095);
  const size_t len = end-start;
  //printf("protect(%p,%lld,%d) %p %p %lld\n",target,size,mode, start,end,len);
  const int status = mprotect((void*)start, len, mode);
  assert(status != -1);
}

//https://stackoverflow.com/questions/12778949/cuda-memory-alignment
__attribute__((aligned(4096)))
unsigned char div32_0[4096];
__attribute__((aligned(4096)))
uint32_t div32[256*256];
__attribute__((aligned(4096)))
unsigned char div32_2[4096];

void init_div(){
  for(int op1=0;op1<256;op1++){
  for(int op2=0;op2<256;op2++){
    if (op2 == 0)
      div32[op1*256+op2] = 0;
    else
      div32[op1*256+op2] = (retval)(op1/op2);
  }}
  protect(div32_0,sizeof(div32_0),PROT_NONE);
  protect(div32,  sizeof(div32),  PROT_READ);
  protect(div32_2,sizeof(div32_2),PROT_NONE);
}

inline int abs(const int x){
  //assert(x >= -255 && x <= 255);
  return (x<0)? -x : x;
}

const int regsize = NumVar*npar;
uint16_t registers16(const regtype registers[regsize], const int k){
  assert(k>=0 && k<regsize);
  assert(sizeof(regtype)==4);
  const regtype a = registers[k/2];
  return (k%2==0)? a & 255 :  (a >> 16) & 255;
}

int main(const int argc, const char *argv[]) {
  //cout<<  "eval_perf $Revision: 1.19 $"<<flush;
  int seed = 2035742;
  for(int i = 1; i<argc;i++) {
    if(*argv[i]=='s' || *argv[i]=='S') seed=atoi(&argv[i][1]);
  }
  //cout<< " seed=" <<seed<<flush;
  //cout<< " regtype size " <<sizeof(regtype)<<" bytes"<<flush;
  //cout<<endl;
  
  srand(seed);
  divtests();
  init_div();
  //use same registers for 16 test cases, leave unusual pattern in others
  for (int i=0;i<FitnessCaseNum/16;i++) {
    regO[i] = rrand(8);
    reg1[i] = rrand(8);
    reg2[i] = rrand(8);
    /*cout<<" regO["<<i<<"]="<<regO[i]
    <<" reg1["<<i<<"]="<<reg1[i]
    <<" reg2["<<i<<"]="<<reg2[i]
    <<endl;
    */
  }

  __attribute__((aligned(4096)))
  unsigned char prog_0[4096];
  __attribute__((aligned(4096)))
  instr prog[4][4];
  __attribute__((aligned(4096)))
  unsigned char prog_2[4096];

  int answers[4][npar][NumVar];
  tests(regO,reg1,reg2,prog,answers);
  protect(prog_0,sizeof(prog_0),PROT_NONE);
  protect(prog,  sizeof(prog),  PROT_READ);
  protect(prog_2,sizeof(prog_2),PROT_NONE);
    
  const int regsize2 = 4096/sizeof(regtype); //expand for mprotect
  assert(NumVar*npar*sizeof(int)<=4096);

  //evolved code can use either registers or reg
  __attribute__((aligned(4096)))
  unsigned char registers0[4096];
  protect(registers0,sizeof(registers0),PROT_NONE);
  __attribute__((aligned(4096)))
  regtype registers[regsize2];
  __attribute__((aligned(4096)))
  unsigned char registers2[4096];
  protect(registers2,sizeof(registers2),PROT_NONE);

  const int regsize4 = 4096/sizeof(retval); //expand for mprotect
  __attribute__((aligned(4096)))
  unsigned char reg_0[4096];
  protect(reg_0,sizeof(reg_0),PROT_NONE);
  __attribute__((aligned(4096)))
  retval reg[regsize4];
  __attribute__((aligned(4096)))
  unsigned char reg_2[4096];
  protect(reg_2,sizeof(reg_2),PROT_NONE);

  //regtype answer[FitnessCaseNum];
  long long times[4];
  
  const int fd = perf_start();
  int errors = 0;
// run program from div.cpp r1.6
for (int i=0;i<4;i++) {
  memset(registers,0x5a,sizeof(registers));
  const int r1 = reg1[i]*npar; assert(r1>=0 && r1<regsize-npar+1);
  const int r2 = reg2[i]*npar; assert(r2>=0 && r2<regsize-npar+1);
  const int rO = regO[i]*npar; assert(rO>=0 && rO<regsize-npar+1);
  assert(r1 != r2);
  for(int j=0;j<npar;j++) registers[r1+j] = Input1[i*npar+j];
  for(int j=0;j<npar;j++) registers[r2+j] = Input2[i*npar+j];
// allow use of both types of registers
  memset(reg,0x5a,sizeof(reg));
  for(int i=0;i<NumVar*npar;i++){
    reg[i] = registers[i];
  }
  ioctl(fd, PERF_EVENT_IOC_RESET, 0);
  ioctl(fd, PERF_EVENT_IOC_ENABLE, 0);

  Interpret16(4, prog[i], registers, reg, div32);
  
  times[i] = perf_end(fd);
  //cout<<"Interpret "<<flush;
  //cout<<"times["<<i<<"] "<<times[i]<<endl;

  assert(regO[i] == 6||regO[i]==4||regO[i]==1||regO[i]==3);
  assert(reg1[i] == 4||reg1[i]==5||reg1[i]==4||reg1[i]==7);
  assert(reg2[i] == 6||reg2[i]==7||reg2[i]==2||reg2[i]==3);

  //todo regtype versus retval
  for (int k=regsize;k<regsize4;k++) {
    if(reg[k] != 0x5a){
      cerr<<"i="<<i<<" reg["<<k<<"]="<<reg[k]<<" "
	  <<"corrupted!! "
	  <<endl;
      //print simple number otherwise Magpie says RUN_PARSE_ERROR
      cout<<"MAGPIE_FITNESS: 100000000"<<endl;
      exit(2); //use exit to avoid segfault due to PROT_NONE;
    }
  }
  regtype pattern1;memset(&pattern1,0x5a,sizeof(pattern1));
  for (int k=regsize;k<regsize2;k++) {
    if(registers[k] != pattern1 ){
      cerr<<"i="<<i<<" registers["<<k<<"]="<<registers[k]<<" "
	  <<"corrupted!! "
	  <<endl;
      //print simple number otherwise Magpie says RUN_PARSE_ERROR
      cout<<"MAGPIE_FITNESS: 100000000"<<endl;
      exit(3); //use exit to avoid segfault due to PROT_NONE;
    }
  }
  for (int k=0;k<regsize;k++) {
                          assert(i>=0 && i<4);
    const int j = k%npar; assert(j>=0 && j<npar);
    const int r = k/npar; assert(r>=0 && r<NumVar);
    /*Too tight. Allow evolved code to use reg/registers as it likes but
      use expected answer to force fitness**
    if((reg[k]       == 0x5a     && answers[i][j][r] != -1 && answers[i][j][r] != 0x5a) &&
       (registers[k] == pattern1 && answers[i][j][r] != -1)) {
      cerr<<"i="<<i
	  <<" reg["<<k<<"]="<<reg[k]<<" "
	  <<" registers["<<k<<"]="<<registers[k]<<" "
	  <<"missing output! "
	  <<"answers["<<i<<"]["<<j<<"]["<<r<<"]"
	  <<"="<<answers[i][j][r]
	  <<endl;
      //print simple number otherwise Magpie says RUN_PARSE_ERROR
      cout<<"MAGPIE_FITNESS: 100000000"<<endl;
      exit(4); //use exit to avoid segfault due to PROT_NONE;
    }
    */
    if(answers[i][j][r] == -1) continue;
    const int expected = answers[i][j][r];
    assert(expected>=0 && expected<256);
    const int error1 = expected - reg[k];
    const int error2 = expected - registers[k];
    const int error3 = expected - registers16(registers,k);
    const int e1 = abs(error1);
    const int e2 = abs(error2);
    const int e3 = abs(error3);
    const int e = (e1<e2)? ((e1<e3)? e1:e3) : ((e2<e3)? e2 : e3);
    /*
    cout<<"k"<<k
	<<" i"<<i
	<<" j"<<j
	<<" r"<<r
	<<" "<<(int)Input1[i]
	<<","<<(int)Input2[i]
	<<" "<<expected
	<<" "<<(int)reg[k]
	<<" "<<registers[k]
	<<" error1="<<error1
	<<" error2="<<error2
	<<" error3="<<error3
	<<" e="<<e
	<<endl;
    */
    const int limite = 80000000/(4*regsize2);
    errors += (e>limite)? limite : e;
  }//endfor k

 }//endfor i
  close(fd); //perf_end();
  long long sum = 0;
  for (int i=0;i<4;i++) {
    /*
    cout<<"i"<<i
	<<" "<<times[i]
	<<endl;
    */
    const long long limit = 10000000/4;
    if(times[i]>limit) sum += limit;
    else               sum += times[i];
  }
  //cerr<<"errors "<<errors<<" "
  //    <<"sum "<<sum<<endl;
  //print simple number otherwise Magpie says RUN_PARSE_ERROR
  if(errors>0) cout<<"MAGPIE_FITNESS: "<<10000000+errors<<endl;
  else         cout<<"MAGPIE_FITNESS: "<<sum<<endl;
  exit(errors!=0);//use exit to avoid segfault due to PROT_NONE
}
