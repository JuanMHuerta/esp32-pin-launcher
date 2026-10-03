CC ?= cc
CFLAGS ?= -O3 -g -Wall -Wextra -Werror
CPPFLAGS += -I../main
LDLIBS += -lm
BIN ?= ../build/host

all: $(BIN)/solver-validation $(BIN)/test-motion $(BIN)/test-walk $(BIN)/test-axis-translation $(BIN)/test-tilt $(BIN)/test-shake $(BIN)/test-boundaries $(BIN)/test-render

$(BIN):
	mkdir -p $@

$(BIN)/solver-validation: ../main/fluid.c ../main/render.c preview.c ../main/fluid.h ../main/render.h ../main/fluid_config.h | $(BIN)
	$(CC) $(CPPFLAGS) $(CFLAGS) ../main/fluid.c ../main/render.c preview.c $(LDLIBS) -o $@

$(BIN)/test-motion: ../main/motion.c test_motion.c ../main/motion.h ../main/fluid_config.h | $(BIN)
	$(CC) $(CPPFLAGS) $(CFLAGS) ../main/motion.c test_motion.c $(LDLIBS) -o $@

$(BIN)/test-walk: ../main/motion.c ../main/fluid.c ../main/render.c test_walk.c ../main/motion.h ../main/fluid.h ../main/render.h ../main/fluid_config.h | $(BIN)
	$(CC) $(CPPFLAGS) $(CFLAGS) ../main/motion.c ../main/fluid.c ../main/render.c test_walk.c $(LDLIBS) -o $@

$(BIN)/test-axis-translation: ../main/motion.c ../main/fluid.c ../main/render.c test_axis_translation.c ../main/motion.h ../main/fluid.h ../main/render.h ../main/fluid_config.h | $(BIN)
	$(CC) $(CPPFLAGS) $(CFLAGS) ../main/motion.c ../main/fluid.c ../main/render.c test_axis_translation.c $(LDLIBS) -o $@

$(BIN)/test-tilt: ../main/motion.c ../main/fluid.c test_tilt.c ../main/motion.h ../main/fluid.h ../main/fluid_config.h | $(BIN)
	$(CC) $(CPPFLAGS) $(CFLAGS) ../main/motion.c ../main/fluid.c test_tilt.c $(LDLIBS) -o $@

$(BIN)/test-shake: ../main/motion.c ../main/fluid.c test_shake.c ../main/motion.h ../main/fluid.h ../main/fluid_config.h | $(BIN)
	$(CC) $(CPPFLAGS) $(CFLAGS) ../main/motion.c ../main/fluid.c test_shake.c $(LDLIBS) -o $@

.PHONY: all

$(BIN)/test-boundaries: ../main/fluid.c ../main/render.c test_boundaries.c ../main/fluid.h ../main/render.h ../main/fluid_config.h | $(BIN)
	$(CC) $(CPPFLAGS) $(CFLAGS) ../main/fluid.c ../main/render.c test_boundaries.c $(LDLIBS) -o $@

$(BIN)/test-render: ../main/fluid.c ../main/render.c test_render.c ../main/fluid.h ../main/render.h ../main/fluid_config.h | $(BIN)
	$(CC) $(CPPFLAGS) $(CFLAGS) ../main/fluid.c ../main/render.c test_render.c $(LDLIBS) -o $@
