ACLOCAL_AMFLAGS = -I m4

SUBDIRS = libserver servers client

.PHONY: format clang-scan cppcheck

format:
	@list='$(SUBDIRS)'; for subdir in $$list; do \
     find $$subdir -iregex '.*\.\(cc\|hh\)$$' -exec clang-format -i {} +; \
	done

clang-scan:
	@make clean
	@scan-build ./configure
	@scan-build make

cppcheck:
	@cppcheck --std=c++11 \
		--enable=warning \
		--enable=performance \
		--enable=portability \
		--enable=style \
		. 2> cppcheck.err 1>/dev/null
	@echo "Finished! (check cppcheck.err)..."
	@echo ""
	@head cppcheck.err

TESTS = \
	test/sample.test
