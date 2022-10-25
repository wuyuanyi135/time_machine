//
// Created by wuyua on 2022/10/26.
//
#include <cassert>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <thread>

#include "time_machine.h"

using namespace std;
using namespace std::chrono;

struct Color {
  uint8_t r, g, b;
  friend ostream& operator<<(ostream& os, const Color& color) {
    os << "(" << (int)color.r << ", " << (int)color.g << ", " << (int)color.b << ")";
    return os;
  }
};

template <>
Color interpolate(const Color& init, const Color& final, uint8_t percent) {
  percent = std::min((uint8_t)100, percent);
  return Color{
      static_cast<uint8_t>((final.r - init.r) * percent / 100 + init.r),
      static_cast<uint8_t>((final.g - init.g) * percent / 100 + init.g),
      static_cast<uint8_t>((final.b - init.b) * percent / 100 + init.b),
  };
}

struct ColorfulLED : public TimeMachine<Color> {
  Color current_state{0, 0, 0};
  void on_ramp_update(const Color& state) override {
    current_state = state;
    cout << "Led RGB set to " << current_state << "\n";
  }
  void on_all_loops_done() override {
    cout << "On all loops done\n";
  }
  void on_stopped() override {
    cout << "On stopped\n";
  }
  void on_ramp_start(bool restart) override {
    cout << "On ramp start (Restart: " << (restart ? "Yes" : "No") << ")\n";
  }
  void on_start_hold(bool begin_or_end) override {
    cout << "Start Hold Phase: " << (begin_or_end ? "Begins" : "Ends") << "\n";
  }
  void on_end_hold(bool begin_or_end) override {
    cout << "End Hold Phase: " << (begin_or_end ? "Begins" : "Ends") << "\n";
  }
  size_t get_tick() override {
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
  }
  const Color& get_current_state() override {
    return current_state;
  }
};

int main() {
  ColorfulLED led;
  cout << "Initial state: " << led.get_current_state() << "\n";
  cout << "> Step \n";
  led.step(Color{255, 0, 255});
  cout << "> Steps \n";
  led.steps({Color{255, 0, 0}, Color{0, 0, 0}}, 200);
  for (int i = 0; i < 10; ++i) {
    led.tick();
    this_thread::sleep_for(100ms);
  }
  cout << "> Ramp \n";
  led.ramp(Color{255, 234, 1}, 1000);
  for (int i = 0; i < 11; ++i) {
    led.tick();
    this_thread::sleep_for(100ms);
  }
  return 0;
}