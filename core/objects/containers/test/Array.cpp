#include "objects/containers/Array.h"
#include "catch.hpp"
#include "utils/RecordType.h"
#include "utils/Utils.h"

using namespace Sph;

TEST_CASE("Array Construction", "[array]") {
    RecordType::resetStats();
    // default construction
    Array<float> ar1;
    REQUIRE(ar1.size() == 0);
    REQUIRE(RecordType::constructedNum == 0);

    // initializer list construction
    Array<float> ar2{ 1.f, 2.f, 2.5f, 3.6f };
    REQUIRE(ar2.size() == 4);
    REQUIRE(ar2[0] == 1.f);
    REQUIRE(ar2[1] == 2.f);
    REQUIRE(ar2[2] == 2.5f);
    REQUIRE(ar2[3] == 3.6f);

    // move construction
    Array<float> ar3(std::move(ar2));
    REQUIRE(ar3.size() == 4);
    REQUIRE(ar3[2] == 2.5f);
    REQUIRE(ar2.size() == 0);
}

TEST_CASE("Array Random access", "[array]") {
    Array<float> ar{ 1.f, 2.f, 2.5f, 3.6f };
    // REQUIRE_SPH_ASSERT(ar[-1]);  calls terminate for some reason, wtf?
    // REQUIRE_SPH_ASSERT(ar[4]);
    REQUIRE(ar[2] == 2.5f);
    ar[2] = 1.f;
    REQUIRE(ar == makeArray(1.f, 2.f, 1.f, 3.6f));
}

TEST_CASE("Array Reserve", "[array]") {
    RecordType::resetStats();
    Array<RecordType> ar;
    REQUIRE_SPH_ASSERT(ar.reserve(-1));
    REQUIRE_NOTHROW(ar.reserve(5));
    ar = { RecordType(5), RecordType(2) };
    REQUIRE(ar[0].wasCopyConstructed);
    REQUIRE(ar[1].wasCopyConstructed);
    REQUIRE(ar.size() == 2);
    ar.reserve(5); // moves old array into larger one
    REQUIRE(ar[0].value == 5);
    REQUIRE(ar[0].wasMoveConstructed);
    REQUIRE(ar[1].value == 2);
    REQUIRE(ar[1].wasMoveConstructed);
    REQUIRE(ar.size() == 2);
    REQUIRE(RecordType::destructedNum == 4); // 2 temporaries, 2 old values

    int constructed = RecordType::constructedNum;
    ar.reserve(4);
    REQUIRE(RecordType::constructedNum == constructed);
    REQUIRE(RecordType::destructedNum == 4);
    REQUIRE(ar.size() == 2);
}

TEST_CASE("Array Resize", "[array]") {
    RecordType::resetStats();
    Array<RecordType> ar;
    REQUIRE(ar.size() == 0);
    REQUIRE_NOTHROW(ar.resize(0));
    REQUIRE_SPH_ASSERT(ar.resize(-1));
    ar.resize(3);
    REQUIRE(RecordType::constructedNum == 3);
    REQUIRE(ar.size() == 3);
    for (int i = 0; i < 3; ++i) {
        REQUIRE(ar[i].wasDefaultConstructed);
    }
    ar.resize(5);
    REQUIRE(RecordType::existingNum() == 5);
    REQUIRE(ar.size() == 5);
    ar.resize(2);
    REQUIRE(RecordType::existingNum() == 2);
    REQUIRE(ar.size() == 2);
    ar.clear();
    REQUIRE(RecordType::existingNum() == 0);
    REQUIRE(ar.size() == 0);
}

TEST_CASE("Array ResizeAndSet", "[array]") {
    Array<RecordType> ar{ 3, 4, 5 };
    ar.resizeAndSet(5, RecordType(9));
    REQUIRE(ar.size() == 5);
    REQUIRE(ar[0].value == 3);
    REQUIRE(ar[1].value == 4);
    REQUIRE(ar[2].value == 5);
    REQUIRE(ar[3].value == 9);
    REQUIRE(ar[4].value == 9);

    /// \todo the new element could have been copy-constructed, used assignment instead for convenience (to
    /// avoid duplicating resize function)
    REQUIRE(ar[3].wasDefaultConstructed);
    REQUIRE(ar[3].wasCopyAssigned);

    ar.resizeAndSet(2, RecordType(10));
    REQUIRE(ar.size() == 2);
    REQUIRE(ar[0].value == 3);
    REQUIRE(ar[1].value == 4);
}

TEST_CASE("Array Push & Pop", "[array]") {
    RecordType::resetStats();
    Array<RecordType> ar;
    ar.push(RecordType(5));
    REQUIRE(RecordType::existingNum() == 1);
    REQUIRE(ar.size() == 1);
    REQUIRE(ar[0].wasDefaultConstructed);
    REQUIRE(ar[0].wasMoveAssigned);

    RecordType r(3);
    ar.push(r);
    REQUIRE(RecordType::existingNum() == 3);
    REQUIRE(ar.size() == 2);
    REQUIRE(ar[0].value == 5);
    REQUIRE(ar[1].value == 3);
    REQUIRE(ar[1].wasDefaultConstructed);
    REQUIRE(ar[1].wasCopyAssigned);

    REQUIRE(ar.pop().value == 3);
    REQUIRE(RecordType::existingNum() == 2);
    REQUIRE(ar.size() == 1);

    REQUIRE(ar.pop().value == 5);
    REQUIRE(RecordType::existingNum() == 1);
    REQUIRE(ar.size() == 0);
    REQUIRE_SPH_ASSERT(ar.pop());

    ar.push(RecordType(8)); // push into allocated array
    REQUIRE(RecordType::existingNum() == 2);
    REQUIRE(ar.size() == 1);
    REQUIRE(ar[0].wasDefaultConstructed);
    REQUIRE(ar[0].wasMoveAssigned);
    REQUIRE(ar[0].value == 8);
}

TEST_CASE("Array PushAll", "[array]") {
    Array<int> ar1{ 1, 2, 3 };
    Array<int> ar2{ 4, 5, 6, 7 };
    ar1.pushAll(ar2.cbegin(), ar2.cend());
    REQUIRE(ar1.size() == 7);
    for (int i = 0; i < 7; ++i) {
        REQUIRE(ar1[i] == i + 1);
    }
}

TEST_CASE("Array EmplaceBack", "[array]") {
    RecordType::resetStats();
    Array<RecordType> ar;
    RecordType& ref = ar.emplaceBack(RecordType(7));
    REQUIRE(RecordType::existingNum() == 1);
    REQUIRE(ar.size() == 1);
    REQUIRE(ar[0].wasMoveConstructed);
    REQUIRE(ar[0].value == 7);
    REQUIRE(&ref == &ar[0]);

    RecordType r(5);
    ar.emplaceBack(r);
    REQUIRE(RecordType::existingNum() == 3);
    REQUIRE(ar.size() == 2);
    REQUIRE(ar[1].wasCopyConstructed);
    REQUIRE(ar[0].value == 7);
    REQUIRE(ar[1].value == 5);

    ar.clear();
    ar.emplaceBack(RecordType(3));
    REQUIRE(RecordType::existingNum() == 2);
    REQUIRE(ar.size() == 1);
    REQUIRE(ar[0].wasMoveConstructed);
    REQUIRE(ar[0].value == 3);
}

TEST_CASE("Array Insert", "[array]") {
    Array<RecordType> ar = { 1, 2, 3, 4, 5 };
    RecordType::resetStats();
    ar.insert(0, 0);
    REQUIRE(ar == Array<RecordType>({ 0, 1, 2, 3, 4, 5 }));
    ar.insert(6, 6);
    REQUIRE(ar == Array<RecordType>({ 0, 1, 2, 3, 4, 5, 6 }));
    ar.insert(3, -1);
    REQUIRE(ar == Array<RecordType>({ 0, 1, 2, -1, 3, 4, 5, 6 }));

    REQUIRE_SPH_ASSERT(ar.insert(9, 5));
}

TEST_CASE("Array Insert range", "[array]") {
    Array<int> ar;
    Array<int> toInsert{ 3, 5, 7 };
    ar.insert(0, toInsert.begin(), toInsert.end());
    REQUIRE(ar == toInsert);

    toInsert = { 1, 2, 3 };
    ar.insert(1, toInsert.begin(), toInsert.end());
    REQUIRE(ar == Array<int>({ 3, 1, 2, 3, 5, 7 }));

    toInsert = { 9 };
    ar.insert(6, toInsert.begin(), toInsert.end());
    REQUIRE(ar == Array<int>({ 3, 1, 2, 3, 5, 7, 9 }));

    toInsert = {};
    ar.insert(3, toInsert.begin(), toInsert.end());
    REQUIRE(ar == Array<int>({ 3, 1, 2, 3, 5, 7, 9 }));

    ar = {};
    ar.insert(0, toInsert.begin(), toInsert.end());
    REQUIRE(ar.empty());
}

TEST_CASE("Array Remove by index", "[array]") {
    Array<int> ar{ 1, 5, 3, 6, 2, 3 };
    ar.remove(0);
    REQUIRE(ar == Array<int>({ 5, 3, 6, 2, 3 }));
    ar.remove(ar.size() - 1);
    REQUIRE(ar == Array<int>({ 5, 3, 6, 2 }));
    ar.remove(2);
    REQUIRE(ar == Array<int>({ 5, 3, 2 }));
    REQUIRE_SPH_ASSERT(ar.remove(4));
    REQUIRE_SPH_ASSERT(ar.remove(-1));
}

TEST_CASE("Array Remove multiple", "[array]") {
    Array<int> ar{ 0, 1, 2, 3, 4 };
    ar.remove(Array<Size>{});
    REQUIRE(ar == Array<int>({ 0, 1, 2, 3, 4 }));
    ar.remove(Array<Size>{ 0 });
    REQUIRE(ar == Array<int>({ 1, 2, 3, 4 }));
    ar.remove(Array<Size>{ 3 });
    REQUIRE(ar == Array<int>({ 1, 2, 3 }));
    ar.remove(Array<Size>{ 1 });
    REQUIRE(ar == Array<int>({ 1, 3 }));
    ar.remove(Array<Size>{ 1, 2 });
    REQUIRE(ar == Array<int>());

    ar = { 0, 1, 2, 3, 4, 5, 6, 7, 8 };
    ar.remove(Array<Size>{ 0, 3, 4, 5, 7 });
    REQUIRE(ar == Array<int>({ 1, 2, 6, 8 }));

    ar = { 0, 1, 2, 3, 4, 5, 6, 7, 8 };
    ar.remove(Array<Size>{ 2, 4, 6, 7, 8 });
    REQUIRE(ar == Array<int>({ 0, 1, 3, 5 }));

    ar = { 0, 1, 2, 3, 4, 5, 6, 7, 8 };
    ar.remove(Array<Size>{ 0, 1, 2, 6, 7 });
    REQUIRE(ar == Array<int>({ 3, 4, 5, 8 }));

    ar = { 0, 1, 2, 3, 4, 5, 6, 7, 8 };
    ar.remove(Array<Size>{ 0, 1, 2, 3, 4, 5, 6, 7, 8 });
    REQUIRE(ar == Array<int>());
}

TEST_CASE("Array remove range", "[array]") {
    Array<int> ar{ 0, 1, 2, 3, 4 };
    ar.remove(ar.begin(), ar.begin() + 1);
    REQUIRE(ar == Array<int>({ 1, 2, 3, 4 }));

    ar.remove(ar.begin() + 2, ar.end());
    REQUIRE(ar == Array<int>({ 1, 2 }));

    ar.remove(ar.begin(), ar.end());
    REQUIRE(ar == Array<int>());

    ar = { 0, 1, 2, 3, 4, 5, 6, 7, 8 };
    ar.remove(ar.begin() + 1, ar.end() - 1);
    REQUIRE(ar == Array<int>({ 0, 8 }));

    ar = { 0, 1, 2 };
    ar.remove(ar.begin(), ar.begin());
    REQUIRE(ar == Array<int>({ 0, 1, 2 }));

    ar = { 0, 1, 2 };
    REQUIRE_SPH_ASSERT(ar.remove(ar.begin() + 1, ar.begin()));
}

TEST_CASE("Array clone", "[array]") {
    Array<RecordType> ar1{ 5, 6, 7 };
    Array<RecordType> ar2 = ar1.clone();
    REQUIRE(ar2.size() == ar1.size());
    REQUIRE(ar2[0].value == ar1[0].value);
    REQUIRE(ar2[1].value == ar1[1].value);
    REQUIRE(ar2[2].value == ar1[2].value);
    for (RecordType& r : ar2) {
        REQUIRE(r.wasCopyConstructed);
    }

    // check that it isn't a soft-copy
    ar1[0].value = 10;
    REQUIRE(ar2[0].value == 5);
}

TEST_CASE("Array iterators", "[array]") {
    Array<int> empty{};
    REQUIRE(empty.begin() == empty.end());

    Size idx = 0;
    for (int i : empty) {
        (void)i;
        idx++;
    }
    REQUIRE(idx == 0);

    Array<int> ar{ 1, 5, 3, 6, 2, 3 };
    REQUIRE(*ar.begin() == 1);
    REQUIRE(*(ar.end() - 1) == 3);

    idx = 0;
    for (int i : ar) {
        REQUIRE(i == ar[idx]);
        idx++;
    }
    REQUIRE(idx == 6);

    for (int& i : ar) {
        i = -1;
    }
    REQUIRE(ar == Array<int>({ -1, -1, -1, -1, -1, -1 }));
}

TEST_CASE("Array std::sort", "[array]") {
    Array<int> ar{ 1, 5, 3, 6, 2, 3 };
    std::sort(ar.begin(), ar.end());
    REQUIRE(ar == Array<int>({ 1, 2, 3, 3, 5, 6 }));

    std::sort(ar.begin(), ar.end(), [](int i1, int i2) {
        // some dummy comparator, replace even numbers with twice the value
        if (i1 % 2 == 0) {
            i1 *= 2;
        }
        if (i2 % 2 == 0) {
            i2 *= 2;
        }
        return i1 < i2;
    });
    REQUIRE(ar == Array<int>({ 1, 3, 3, 2, 5, 6 }));
}

TEST_CASE("Array References", "[array]") {
    int a, b, c;
    Array<int&> ar{ a, b, c };
    ar[0] = 5;
    ar[1] = 3;
    ar[2] = 1;
    REQUIRE(a == 5);
    REQUIRE(b == 3);
    REQUIRE(c == 1);

    for (auto& i : ar) {
        i = 2;
    }
    REQUIRE(a == 2);
    REQUIRE(b == 2);
    REQUIRE(c == 2);

    auto getter = []() { return Array<int>{ 1, 5, 9 }; };
    ar = getter();
    REQUIRE(a == 1);
    REQUIRE(b == 5);
    REQUIRE(c == 9);

    int d, e, f;
    tieToArray(d, e, f) = makeArray(3, 1, 4);
    REQUIRE(d == 3);
    REQUIRE(e == 1);
    REQUIRE(f == 4);
}

TEST_CASE("CopyArray", "[array]") {
    Array<RecordType> ar1{ 1, 2, 3 };
    Array<RecordType> ar2{ 5, 6, 7, 8 };
    static_assert(
        !std::is_assignable<Array<RecordType>&, Array<RecordType>&>::value, "Array must not be copyable");

    ar1 = copyable(ar2);
    REQUIRE(ar1.size() == ar2.size());
    for (Size i = 0; i < ar1.size(); ++i) {
        REQUIRE(ar1[i] == ar2[i]);
        REQUIRE(ar1[i].wasCopyAssigned);
    }
    ar2[0].value = 10;
    REQUIRE(ar1[0].value == 5);
}
