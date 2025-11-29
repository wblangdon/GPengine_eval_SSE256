//extern const char* rev = "$Revision: 1.17 $";
// GPengine.cpp: implementation of the CGPengine class.
//
//////////////////////////////////////////////////////////////////////
//W.B.Langdon @ cs.ucl.ac.uk 23 August 2000 Elvis Hand-Eye cordination experiment
//Changes
//WBL 11 Oct 2025 organise combined div.cpp as 3 functions
//WBL 10 Oct 2025 combine div_funcs.cpp with div.cpp
//WBL  8 Oct 2025 Switch to AVX based on GPengine.cpp r1.65 Interpret16
//WBL  7 Oct 2025 Switch to instruction count, clean output for fit.bat
//WBL  2 Oct 2025 based on interpret from GPengine.cpp r1.119, main.cpp r1.22


/* compile:
  g++ -fpermissive -fmax-errors=2 div_perf.cpp 
 */

//#include "GPengine.h"
#include <stdlib.h>
#include <iostream>
#include <assert.h>
#include <string.h>
#include <fstream>
#include <stdio.h>

#include <immintrin.h>

using namespace std;

//share between main and div_eval
typedef unsigned char retval;
#define regtype retval
#define npar 16
#include "div.cpp"

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

uint32_t div32[256*256];

void init_div(){
  for(int op1=0;op1<256;op1++){
  for(int op2=0;op2<256;op2++){
    if (op2 == 0)
      div32[op1*256+op2] = 0;
    else
      div32[op1*256+op2] = (retval)(op1/op2);
  }}
}

inline int abs(const int x){
  assert(x >= -255 && x <= 255);
  return (x<0)? -x : x;
}
int main(const int argc, const char *argv[]) { //later as interface to Python?
  //cout<<  "#div_perf $Revision: 1.17 $"<<flush;
  int seed = 2035742;
  for(int i = 1; i<argc;i++) {
    if(*argv[i]=='s' || *argv[i]=='S') seed=atoi(&argv[i][1]);
  }
  //cout<< "seed=" <<seed<<endl;
  srand(seed);
  divtests();
  init_div();
  const int regsize = 8*npar;
  regtype output[regsize];memset(output,0xf0,sizeof(output));
  regtype input1[regsize];memset(input1,0x5a,sizeof(input1));
  regtype input2[regsize];memset(input2,0xa5,sizeof(input2));
  //use same registers for 16 test cases, leave unusual pattern in others
  for (int i=0;i<FitnessCaseNum/16;i++) {
    regO[i] = rrand(8);
    reg1[i] = rrand(8);
    reg2[i] = rrand(8);
    //cout<<" regO["<<i<<"]="<<regO[i]
    //<<" reg1["<<i<<"]="<<reg1[i]
    //<<" reg2["<<i<<"]="<<reg2[i]
    //<<endl;
  }
  regtype answer[FitnessCaseNum];
  long long times[FitnessCaseNum/npar];
  
  const int fd = perf_start();
// run program from div.cpp r1.6
for (int i=0;i<FitnessCaseNum;i+=npar) {
  const int r1 = reg1[i/16]*npar; assert(r1>=0 && r1<regsize-npar+1);
  const int r2 = reg2[i/16]*npar; assert(r2>=0 && r2<regsize-npar+1);
  const int rO = regO[i/16]*npar; assert(rO>=0 && rO<regsize-npar+1);
  memcpy(&input1[r1],&Input1[i],npar*sizeof(regtype));
  memcpy(&input2[r2],&Input2[i],npar*sizeof(regtype));
  //regtype val;
  ioctl(fd, PERF_EVENT_IOC_RESET, 0);
  ioctl(fd, PERF_EVENT_IOC_ENABLE, 0);
  //#include "div.cpp"
  div_eval(output,input1,input2,r1,r2,rO,div32);
  times[i/npar] = perf_end(fd);

  assert(regO[i/16] == 6||regO[i/16]==4||regO[i/16]==1||regO[i/16]==3);
  assert(reg1[i/16] == 4||reg1[i/16]==5||reg1[i/16]==4||reg1[i/16]==7);
  assert(reg2[i/16] == 6||reg2[i/16]==7||reg2[i/16]==2||reg2[i/16]==3);
  //assert(output[regO[i/16]] == val);

  //answer[i] = output[rO];
  memcpy(&answer[i],&output[rO],npar*sizeof(regtype));
 }//endfor FitnessCaseNum
  close(fd); //perf_end();

  for (int i=0;i<regsize;i++) {
    for (int j=0;j<FitnessCaseNum/16;j++) {if(i/16==reg1[j]) goto skip1;}
    regtype pattern1;memset(&pattern1,0x5a,sizeof(pattern1));
    if(input1[i] != pattern1){
      cerr<<"input1 corrupted "<<i<<" "<<flush;
      unsigned char out[sizeof(regtype)];
      memcpy(out,&input1[i],sizeof(regtype));
      for(int j=0;j<sizeof(regtype);j++){
	char buff[80]; sprintf(buff,"%02x",out[j]);cerr<<buff<<flush;
      }
      cerr<<endl;
      cout<<"MAGPIE_FITNESS: 100000000"<<endl;
      return 2;
    }
  skip1:;
  }
  for (int i=0;i<regsize;i++) {
    for (int j=0;j<FitnessCaseNum/16;j++) {if(i/16==reg2[j]) goto skip2;}
    regtype pattern2;memset(&pattern2,0xa5,sizeof(pattern2));
    if(input2[i] != pattern2){
      cerr<<"input2 corrupted "<<i<<" "<<flush;
      unsigned char out[sizeof(regtype)];
      memcpy(out,&input2[i],sizeof(regtype));
      for(int j=0;j<sizeof(regtype);j++){
	char buff[80]; sprintf(buff,"%02x",out[j]);cerr<<buff<<flush;
      }
      cerr<<endl;
      cout<<"MAGPIE_FITNESS: 100000000"<<endl;
      return 2;
    }
  skip2:;
  }
  for (int i=0;i<regsize;i++) {
    for (int j=0;j<FitnessCaseNum/16;j++) {if(i/16==regO[j]) goto skipO;}
    regtype patternO;memset(&patternO,0xf0,sizeof(patternO));
    if(output[i] != patternO){
      cerr<<"output corrupted "<<i<<" "<<flush;
      unsigned char out[sizeof(regtype)];
      memcpy(out,&output[i],sizeof(regtype));
      for(int j=0;j<sizeof(regtype);j++){
	char buff[80]; sprintf(buff,"%02x",out[j]);cerr<<buff<<flush;
      }
      cerr<<endl;
      cout<<"MAGPIE_FITNESS: 100000000"<<endl;
      return 2;
    }
  skipO:;
  }
  long long sum = 0;
  int errors = 0;
  for (int i=0;i<FitnessCaseNum;i++) {
    const int ans   = convertregtype(answer[i]);
    const int error = Output[i] - ans;
    /*cout<<"i"<<i
	<<" "<<(int)Input1[i]
	<<"/"<<(int)Input2[i]
	<<"=> "<<(int)Output[i]
	<<" regO="<<regO[i/16]
	<<" "<<(int)answer[i]<<" "<<ans
	<<" error="<<error
	<<endl;
    */
    errors += abs(error);
  }
  for (int i=0;i<FitnessCaseNum/npar;i++) {
    /*cout<<"i"<<i
	<<" "<<times[i]
	<<endl;
    */
    sum    += times[i];
  }
  //cout<<"errors "<<errors<<" ";
  //cout<<"sum "<<sum<<endl;
  if(errors>0) cout<<"MAGPIE_FITNESS: "<<10000000+errors<<endl;
  else         cout<<"MAGPIE_FITNESS: "<<sum<<endl;
  return (errors>0);
}
