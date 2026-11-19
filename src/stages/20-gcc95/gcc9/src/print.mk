# make -s -f Makefile -f /steps/src/print.mk TARGET (rung gcc9, 20-gcc95
# README): prints make's own values, no shell quoting in between.
# gcc9-top: the top level's bootstrap settings (do-compare unexpanded).
gcc9-top: ; $(foreach v,BUILD_CONFIG STAGE1_CHECKING STAGE1_LANGUAGES STAGE1_CFLAGS STAGE1_CXXFLAGS STAGE1_TFLAGS STAGE2_CFLAGS STAGE2_CXXFLAGS STAGE2_TFLAGS STAGE3_CFLAGS STAGE3_CXXFLAGS STAGE3_TFLAGS STAGE1_LDFLAGS POSTSTAGE1_LDFLAGS BOOT_LDFLAGS LDFLAGS LDFLAGS_FOR_BUILD CFLAGS_FOR_TARGET CXXFLAGS_FOR_TARGET LDFLAGS_FOR_TARGET,$(info $(v) = $($(v))))$(info do-compare = $(value do-compare))@:
# gcc9-fp-gcc (in gcc/, with CXXFLAGS= on the command line): the flags
# gcc/Makefile compiles its own sources with, but -O and -g
gcc9-fp-gcc: ; $(info $(ALL_CXXFLAGS) $(ALL_CPPFLAGS))@:
# gcc9-fp-libgcc (in TARGET/libgcc): libgcc2.c's flags but -DL_name
gcc9-fp-libgcc: ; $(info $(INTERNAL_CFLAGS))@:
# gcc9-fp-libstdcxx (in TARGET/libstdc++-v3/src/c++11): LTCXXCOMPILE's
# flags; CXX adds -nostdinc++
gcc9-fp-libstdcxx: ; $(info $(TOPLEVEL_INCLUDES) $(AM_CPPFLAGS) $(CPPFLAGS) $(AM_CXXFLAGS) $(CXXFLAGS))@:
