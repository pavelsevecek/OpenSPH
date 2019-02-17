#include "common/Traits.h"
#include <fstream>

using namespace Sph;

namespace {

struct DummyCallable {
    void operator()(float, int) {}
    int operator()(double, float) {
        return 0;
    }
};

enum class TestEnum { DUMMY };

static_assert(!IsCallable<float>::value, "static test failed");
static_assert(!IsCallable<float, int>::value, "static test failed");
static_assert(IsCallable<DummyCallable, float, int>::value, "static test failed");
static_assert(!IsCallable<DummyCallable, float, float>::value, "static test failed");
static_assert(IsCallable<DummyCallable, double, float>::value, "static test failed");
static_assert(!IsCallable<DummyCallable, double, float, int>::value, "static test failed");

static_assert(HasStreamOperator<int, std::ofstream>::value, "static test failed");
static_assert(HasStreamOperator<std::string, std::ofstream>::value, "static test failed");
static_assert(!HasStreamOperator<DummyCallable, std::ofstream>::value, "static test failed");
static_assert(!HasStreamOperator<TestEnum, std::ofstream>::value, "static test failed");

static_assert(std::is_same<int, Undecay<int, float>>::value, "invalid Undecay");
static_assert(std::is_same<int&, Undecay<int, float&>>::value, "invalid Undecay");
static_assert(std::is_same<const int, Undecay<int, const float>>::value, "invalid Undecay");
static_assert(std::is_same<const int&, Undecay<int, const float&>>::value, "invalid Undecay");


static_assert(std::is_same<int, ConvertToSize<int>>::value, "invalid EnumToInt");
static_assert(std::is_same<float, ConvertToSize<float>>::value, "invalid EnumToInt");
static_assert(std::is_same<bool, ConvertToSize<bool>>::value, "invalid EnumToInt");
static_assert(std::is_same<int, ConvertToSize<TestEnum>>::value, "invalid EnumToInt");
static_assert(std::is_same<int&, ConvertToSize<TestEnum&>>::value, "invalid EnumToInt");


static_assert(AllTrue<true, true, true, true>::value == true, "invalid AllTrue");
static_assert(AllTrue<true, true, false, true>::value == false, "invalid AllTrue");
static_assert(AllTrue<true, true, true, false>::value == false, "invalid AllTrue");
static_assert(AllTrue<false>::value == false, "invalid AllTrue");
static_assert(AllTrue<true>::value == true, "invalid AllTrue");

static_assert(AnyTrue<true, true, true, true>::value == true, "invalid AnyTrue");
static_assert(AnyTrue<false, false, false, true>::value == true, "invalid AnyTrue");
static_assert(AnyTrue<true, true, true, false>::value == true, "invalid AnyTrue");
static_assert(AnyTrue<false, false, false, false>::value == false, "invalid AnyTrue");
static_assert(AnyTrue<true>::value == true, "invalid AnyTrue");
static_assert(AnyTrue<false>::value == false, "invalid AnyTrue");

} // namespace
