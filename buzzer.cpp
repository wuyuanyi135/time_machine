//
// Created by wuyua on 2022/10/25.
//

#include <cassert>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <thread>

#include "time_machine.h"

using namespace std;
using namespace std::chrono;

/// Buzzer does not have interpolated states
struct BuzzerMachine : public TimeMachine<int> {
  StateType current_freq{};

  void on_ramp_update(const int& state) override {
    current_freq = state;
    std::cout << "Current Freq = " << current_freq << "\n";
  }
  void on_all_loops_done() override {
    std::cout << "Buzzer off \n";
  }
  size_t get_tick() override {
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
  }
  const int& get_current_state() override {
    return current_freq;
  }
  int interpolate(const int& init, const int& final, uint8_t percent) {
    // Shouldn't be called
    assert(false);
    return 0;
  }
};

int main() {
  BuzzerMachine machine;
  machine.step(300);
  machine.step(0);

  machine.steps({400, 300, 500, 300}, 1000);
  for (int i = 0; i < 50; ++i) {
    machine.tick();
    this_thread::sleep_for(100ms);
  }
}