ACLOCAL_AMFLAGS = -I m4

if LIBEVENTM_OPT
    MAYBE_LIBEVENTM = libeventm
endif
SUBDIRS = $(MAYBE_LIBEVENTM) libserver servers client

TESTS = \
	test/sample.test

.PHONY: help
help:
	@echo "Build targets:"
	@echo "    all          Build mutated"
	@echo "    check        Run mutated test-suite"
	@echo "    recheck      Run only previously failed tests"
	@echo ""
	@echo "Clean targets:"
	@echo "    clean              Remove built mutated files"
	@echo "    distclean          Remove built mutated files and build-system files"
	@echo "    maintainer-clean   Remove all derived files"
	@echo ""
	@echo "Documentation targets:"
	@echo "    dvi          Produce documentation in dvi format"
	@echo "    html         Produce documentation in html format"
	@echo "    pdf          Produce documentation in pdf format"
	@echo "    ps           Produce documentation in ps format"
	@echo ""
	@echo "Development targets:"
	@echo "    format       Format all the source code to project style"
	@echo "    clang-scan   Run clang static analyzer over mutated"
	@echo "    ccpcheck     Run ccpcheck static analyzer over mutated"
	@echo "    cscope       Produce a csope file for mutated"
	@echo "    ctags        Produce a (vi) ctags file for mutated"
	@echo "    tags         Produce an emacs tag file for mutated"
	@echo ""
	@echo "Packaging targets:"
	@echo "    dist         Package mutated into tarball for distribution"
	@echo "    install      Build mutated and install on local system"
	@echo "    uninstall    Remove previous install from local system"
	@echo ""

.PHONY: format
format:
	@list='$(SUBDIRS)'; for subdir in $$list; do \
		find $$subdir -iregex '.*\.\(cc\|hh\)$$' -exec clang-format -i {} +; \
	done

.PHONY: clang-scan
clang-scan:
	@make clean
	@scan-build ./configure
	@scan-build make

.PHONY: cppcheck
cppcheck:
	@cppcheck --std=c++11 \
		--enable=warning \
		--enable=performance \
		--enable=portability \
		--enable=style \
		client libserver servers 2> cppcheck.err 1>/dev/null
	@echo "Finished! (check cppcheck.err)..."
	@echo ""
	@head cppcheck.err
