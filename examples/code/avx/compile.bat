#WBL 7 October 2025 compile code for AVX Magpie experiments
# to speed up GPengine interpreter

#Modifications:
#WBL 13 Oct 2025 Switch to eval
#WBL  9 Oct 2025 check have not changed feedstock XML, but changed something
#WBL  8 Oct 2025 Added __m256i AVX
#WBL  7 Oct 2025 based on tsd-extra/compile.bat, div_perf.make r1.2
#   malloc_extras/fit.bat r1.73

setenv start `pwd`
setenv test_srt `gawk -v dir="$start" '\
END{s=index(dir,"/_magpie_work/");\
if(s==0){print "../test.srt"; exit}\
split(substr(dir,s+1),t,"/");\
printf("%s_magpie_logs/%s_test.srt\n",substr(dir,1,s),t[2])}' /dev/null`

if(!(-e $test_srt)) cp /dev/null $test_srt

echo $0 ' $Revision: 1.11 $ ' "start $HOST" `pwd` `date -Ins` >> $test_srt

rm -f eval_perf.exe > /dev/null
rm -f div_perf.exe > /dev/null

if(-e ../__original__) then
diff ../__original__ .  >> $test_srt
setenv save $status
if($save > 1) then
  echo "error on diff dir status $save"      >> $test_srt
  exit 10
endif
if($save == 0) then
  setenv warm `gawk '(FNR>12){exit warm=1}END{print warm==1}' $test_srt`
  if($warm) then
    echo "diff dir 0 (warm $warm) but need change" >> $test_srt
    exit 10
  endif # report error
endif  # no changes

#abort if Magpie is changing wrong stuff
diff ../__original__/diffs.cpp           . > /dev/null
setenv save $status
if($save) then
  echo "diff diffs.cpp status $save"           >> $test_srt
  exit $save
endif
diff ../__original__/IntrinsicsGuide.cpp . > /dev/null
setenv save $status
if($save) then
  echo "diff IntrinsicsGuide.cpp status $save" >> $test_srt
  exit $save
endif
endif #../__original__ exists ok

#not -DNDEBUG or -g

g++ -O3 -Dregtype="int" -fmax-errors=1 -fpermissive -march=skylake \
  eval_perf.cpp -o eval_perf.exe >>& $test_srt
#g++ -O3 -fmax-errors=1 -fpermissive -march=skylake div_perf.cpp \
#  -o div_perf.exe >>& $test_srt
setenv save $status

echo "$0 done, status $save" `date -Ins` >> $test_srt
exit $save

