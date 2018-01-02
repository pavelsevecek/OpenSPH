#pragma once

#include "common/Globals.h"
#include "objects/wrappers/Outcome.h"

/// \file SequenceTest.h
/// \brief Helper class for perfoming larger number of related tests.
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2018

NAMESPACE_SPH_BEGIN

/// \brief Helper class for perfoming larger number of related tests.
///
/// Useful to get better error report in case test fails. Template parameter Test must implement
/// operator()(Size idx) that runs idx-th test. Operator shall return Outcome object, containing success if
/// test passed or error message if test failed.
template <typename Test>
class SequenceTest {
private:
    Test& test;
    // has state to remember the failed index, error message based on failed state is later printed by Catch
    Size failedIdx = 0;
    Outcome result{ SUCCESS };

public:
    explicit SequenceTest(Test& test)
        : test(test) {}

    bool performTest(const Size idx) {
        result = test(idx);
        if (!result) {
            failedIdx = idx;
            return false;
        }
        return true;
    }

    friend std::ostream& operator<<(std::ostream& stream, const SequenceTest& sequence) {
        if (sequence.result) {
            stream << "All tests in sequence passed" << std::endl;
            return stream;
        }
        stream << "Test sequence failed with index " << sequence.failedIdx << std::endl;
        stream << sequence.result.error() << std::endl;
        return stream;
    }

    const Outcome& outcome() const {
        return result;
    }
};


/// We have to return and compare custom types; if we returned true or false, Catch wouldn't print our report.
struct SequenceSuccessful {
    friend std::ostream& operator<<(std::ostream& stream, const SequenceSuccessful&) {
        stream << "Sequence Test:";
        return stream;
    }
};
const SequenceSuccessful SEQUENCE_SUCCESS;

template <typename Test>
INLINE bool operator==(const SequenceSuccessful, const SequenceTest<Test>& sequence) {
    return (bool)sequence.outcome();
}

/// Syntactic suggar to avoid using SEQUENCE_SUCCESS constant.
#define REQUIRE_SEQUENCE(test, from, to) REQUIRE(SEQUENCE_SUCCESS == testSequence(test, from, to))


/// Performs a sequence of test by passing indices in given range (INCLUDING lower bound and EXCLUDING upper
/// bounds, behaves as begin() and end() iterators) into functor. The testing is interupted once a test fais.
///
/// Usage:
/// auto test = [](const Size idx) {
///     if (idx > 0) {
///         return SUCCESS;
///     } else {
///         makeFailed("Index is not positive");
///     }
/// };
/// REQUIRE_SEQUENCE(test, 1, 5));

template <typename Test>
SequenceTest<Test> testSequence(Test& test, const Size from, const Size to) {
    ASSERT(from < to);
    SequenceTest<Test> sequenceTest(test);
    for (Size idx = from; idx < to; ++idx) {
        if (!sequenceTest.performTest(idx)) {
            return sequenceTest; // return sequence in current state (failed with idx)
        }
    }
    return sequenceTest;
}

NAMESPACE_SPH_END
