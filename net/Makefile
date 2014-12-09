.PHONY: test indent clean check coverage

test:
	(cd tcp; make test) && (cd http; make test) && echo "ALL TESTS PASS"

indent:
	(find . -name "*.cc" ; find . -name "*.h") | xargs clang-format-3.5 -i

clean:
	(cd tcp; make clean) && (cd http; make clean)

check:
	(cd tcp; make check) && (cd http; make check)

coverage:
	(cd tcp; make coverage) && (cd http; make coverage)
