#include "system/Timer.h"
#include "catch.hpp"
#include <thread>

using namespace Sph;

TEST_CASE("Timer", "[timer]") {
    Timer timer(0);
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    REQUIRE(timer.elapsed<TimerUnit::MILLISECOND>() == 300);
    std::this_thread::sleep_for(std::chrono::milliseconds(350));
    REQUIRE(timer.elapsed<TimerUnit::MILLISECOND>() == 650);

    timer.restart();
    REQUIRE(timer.elapsed<TimerUnit::MILLISECOND>() == 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    REQUIRE(timer.elapsed<TimerUnit::MILLISECOND>() == 150);
}

TEST_CASE("Start expired", "[timer]") {
    Timer timer1(1000);
    REQUIRE(!timer1.isExpired());
    REQUIRE(timer1.elapsed<TimerUnit::MILLISECOND>() == 0);

    Timer timer2(1000, TimerFlags::START_EXPIRED);
    REQUIRE(timer2.isExpired());
    REQUIRE(timer2.elapsed<TimerUnit::MILLISECOND>() == 1000);
}

TEST_CASE("Execute Callback", "[timer]") {
    int value = 0;
    Timer timer(400, [&value](){
        value = 11;
    });
    Timer measuringTimer(0);
    while (true) {
        if (!timer.isExpired()) {
            REQUIRE(value == 0);
        } else {
            REQUIRE(value == 11);
            REQUIRE(measuringTimer.elapsed<TimerUnit::MILLISECOND>() >= 400);
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
    }   
}

TEST_CASE("Stoppable timer", "[timer]") {
    StoppableTimer timer;
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    REQUIRE(timer.elapsed<TimerUnit::MILLISECOND>() == 50);
    timer.stop();
    REQUIRE(timer.elapsed<TimerUnit::MILLISECOND>() == 50);
    std::this_thread::sleep_for(std::chrono::milliseconds(80));
    REQUIRE(timer.elapsed<TimerUnit::MILLISECOND>() == 50);
    timer.resume();
    REQUIRE(timer.elapsed<TimerUnit::MILLISECOND>() == 50);
    std::this_thread::sleep_for(std::chrono::milliseconds(70));
    REQUIRE(timer.elapsed<TimerUnit::MILLISECOND>() == 120);
}
