TESTS = test_ir1 test_ir2 test_ir3 test_ir4 test_ir6_ismminmax		\
	test_snd1 test_snd2 test_snd4 test_snd3 test_snd5 test_pos1	\
	test_pos2 test_pos_trackinterp test_snd_door1 test_snd_door2	\
	test_pos_door test_snd_layers 

RECEIVERS = omni nsp amb3h0v amb3h3v amb1h0v amb1h1v cardioid	\
  neukom_basic neukom_inphase hann vbap vbap3d hoa2d ortf	\
  intensityvector vmic chmap hoa2d_fuma cardioidmod debugpos	\
  hoa2d_fuma_hos

TEST_REC = $(patsubst %,test_rec_%,$(RECEIVERS))


FAILING_TESTS = test_ir5

all: $(TESTS) $(TEST_REC)

test_ir%: test_ir%.tsc expected_ir%.wav
	LD_LIBRARY_PATH=../build/ ../build/tascar_renderir -o test_ir$*.wav $< -t 1 -l 4000 -f 44100 
	LD_LIBRARY_PATH=../build/ ../build/compare_sndfile expected_ir$*.wav test_ir$*.wav 1e-9
	rm -f test_ir$*.wav

test_snd%: test_snd%.tsc expected_snd%.wav
	LD_LIBRARY_PATH=../build/ ../build/tascar_renderfile -i zeros.wav -d -f 64 -o test_snd$*.wav $<
	LD_LIBRARY_PATH=../build/ ../build/compare_sndfile expected_snd$*.wav test_snd$*.wav 1e-9
	rm -f test_snd$*.wav

db_test_rec_%: test_rec.tsc
	echo run -i zeros.wav -d -f 64 -o test_rec_$*.wav $< > gdbcmd
	LD_LIBRARY_PATH=../build/ RECEIVER=$* gdb ../build/tascar_renderfile -x gdbcmd
	rm -f test_rec_$*.wav

test_rec_%: test_rec.tsc
	LD_LIBRARY_PATH=../build/ RECEIVER=$* ../build/tascar_renderfile -i zeros.wav -d -f 64 -o test_rec_$*.wav $<
	rm -f test_rec_$*.wav
#LD_LIBRARY_PATH=../build/ ../build/compare_sndfile expected_rec_$*.wav test_rec_$*.wav 1e-9

test_pos%: test_pos%.tsc expected_pos%.wav
	LD_LIBRARY_PATH=../build/ ../build/tascar_renderfile -i zeros.wav -d -f 64 -o test_pos$*.wav $<
	LD_LIBRARY_PATH=../build/ ../build/compare_sndfile expected_pos$*.wav test_pos$*.wav 1e-9
	rm -f test_pos$*.wav

db_test_ir%: test_ir%.tsc expected_ir%.wav
	echo run -o test_ir$*.wav $< -t 1 -l 4000 -f 44100 > gdbcmd
	LD_LIBRARY_PATH=../build/ gdb ../build/tascar_renderir -x gdbcmd
	LD_LIBRARY_PATH=../build/ ../build/compare_sndfile expected_ir$*.wav test_ir$*.wav 1e-9
	rm -f test_ir$*.wav

db_test_snd%: test_snd%.tsc expected_snd%.wav
	echo run -i zeros.wav -o test_snd$*.wav -d -f 64 $< > gdbcmd
	LD_LIBRARY_PATH=../build/ gdb ../build/tascar_renderfile -x gdbcmd
	LD_LIBRARY_PATH=../build/ ../build/compare_sndfile expected_snd$*.wav test_snd$*.wav 1e-9
	rm -f test_snd$*.wav

db_test_pos%: test_pos%.tsc expected_pos%.wav
	echo run -i zeros.wav -o test_pos$*.wav -d -f 64 $< > gdbcmd
	LD_LIBRARY_PATH=../build/ gdb ../build/tascar_renderfile -x gdbcmd
	LD_LIBRARY_PATH=../build/ ../build/compare_posfile expected_pos$*.wav test_pos$*.wav 1e-9
	rm -f test_pos$*.wav

leak_test_snd%: test_snd%.tsc expected_snd%.wav
	LD_LIBRARY_PATH=../build/ valgrind --leak-check=full ../build/tascar_renderfile -i zeros.wav -o test_snd$*.wav -d -f 64 $<
	LD_LIBRARY_PATH=../build/ ../build/compare_sndfile expected_snd$*.wav test_snd$*.wav 1e-9
	rm -f test_snd$*.wav

leak_test_pos%: test_pos%.tsc expected_pos%.wav
	LD_LIBRARY_PATH=../build/ valgrind --leak-check=full ../build/tascar_renderfile -i zeros.wav -o test_pos$*.wav -d -f 64 $<
	LD_LIBRARY_PATH=../build/ ../build/compare_posfile expected_pos$*.wav test_pos$*.wav 1e-9
	rm -f test_pos$*.wav

clean:
	rm -f *~ test*.wav gdbcmd
