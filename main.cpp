#include <iostream>
#include "time_machine.h"
#include <chrono>
#include <thread>

using namespace std;
using namespace std::chrono;

struct IntTimeMachine : public TimeMachine<int> {
  int current_state = 0;
  size_t get_tick() override {
    return chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now().time_since_epoch()).count();
  }
  const int& get_current_state() override {
    return current_state;
  }
  void on_ramp_update(const int& state) override {
    current_state = state;
    cout << "Set state to " << state << "\n";
  }
};
int main() {
  IntTimeMachine machine;
  machine.step(5);

  machine.step(10);
  this_thread::sleep_for(500ms);
  machine.ramp(100, 2000);
  for (int i = 0; i < 30; ++i) {
    machine.tick();
    this_thread::sleep_for(100ms);
  }

  machine.execute_specs({
      IntTimeMachine::TimeMachineSpec{200, 100, 500, 100},
      IntTimeMachine::TimeMachineSpec{0  , 100, 500, 100},
      IntTimeMachine::TimeMachineSpec{100, 100, 500, 100}
  }, 2);
  for (int i = 0; i < 30; ++i) {
    machine.tick();
    this_thread::sleep_for(100ms);
  }

  return 0;
}
