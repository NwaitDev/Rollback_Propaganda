// Entry point of the C++ tests.
//   make test                                 every test
//   ./build/check/unit_tests -tc="*rollback*"   only the test cases whose name matches
//   ./build/check/unit_tests --help           the other options (list, filters, verbosity...)
//
// doctest writes its report on std::cerr: test::quiet silences std::cout (where
// a game prints every step) without hiding the failures.
#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest/doctest.h>

#include <iostream>

int main(int argc, char** argv) {
	doctest::Context context(argc, argv);
	context.setCout(&std::cerr);
	return context.run();
}
