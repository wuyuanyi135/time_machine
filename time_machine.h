//
// Created by wuyua on 2022/10/25.
//
#pragma once

#include <queue>
#include <vector>

#include "boost/sml.hpp"

template <class TState>
TState interpolate(const TState& init, const TState& final, uint8_t percent) {
  percent = std::min((uint8_t)100, percent);
  return (final - init) * percent / 100 + init;
}

template <class TState>
struct TimeMachine {
  using StateType = TState;
  struct TimeMachineSpec {
    TState next_state;
    size_t start_hold_duration;
    size_t transition_duration;
    size_t end_hold_duration;
  };

  virtual void on_ramp_update(const TState&){};
  virtual void on_all_loops_done(){};
  // This will be called either due to interruption or completion.
  virtual void on_stopped(){};
  virtual void on_ramp_start(bool restart){};
  virtual void on_start_hold(bool begin_or_end){};
  virtual void on_end_hold(bool begin_or_end){};

  virtual size_t get_tick() = 0;
  virtual const TState& get_current_state() = 0;

  /// Ramp to the next state within duration
  /// \param next_state
  /// \param duration
  void ramp(const TState& next_state, size_t duration) {
    sm.process_event(Ramp{{TimeMachineSpec{next_state, 0, duration, 0}}, 0});
  };

  /// Transit to the next state immediately
  /// \param next_state
  void step(const TState& next_state) {
    sm.process_event(Ramp{{TimeMachineSpec{next_state, 0, 0, 0}}, 0});
  }

  /// Transit to each step with the same duration settings
  /// \param states
  /// \param duration
  void steps(const std::vector<TState> states, size_t duration) {
    if (duration == 0) {
      throw std::invalid_argument("duration=0");
    }

    if (states.empty()) {
      throw std::invalid_argument("no state specified");
    }
    Ramp ramp;
    for (int i = 0; i < states.size(); ++i) {
      ramp.ramps.push_back(TimeMachineSpec{states[i], 0, 0, duration});
    }
    ramp.loop_count = 0;
    sm.process_event(ramp);
  }

  /// Execute a sequence of specs
  /// \param specs
  /// \param repeats
  void execute_specs(const std::vector<TimeMachineSpec> specs, int repeats) {
    if (specs.empty()) {
      throw std::invalid_argument("no spec");
    }
    sm.process_event(Ramp{specs, repeats});
  }

  /// Call this periodically
  void tick() {
    sm.process_event(Tick{});
  }

  // Events
  struct Tick {};
  struct Ramp {
    std::vector<TimeMachineSpec> ramps{};
    int loop_count{0};
  };
  struct Stop {};

  struct TimeMachineSM {
    using Self = TimeMachineSM;
    struct RampingSubSM {
     private:
      using Self = RampingSubSM;
      std::vector<TimeMachineSpec> ramps;
      size_t phase_init_at;
      size_t ramp_idx;
      size_t loop_count;  // requested
      size_t loop_done;   // executed
      TState initial_state;

     public:
      struct Entry {};
      struct StartHold {};
      struct Ramping {};
      struct EndHold {};
      struct SwitchingNextLoop {};

     public:
      void configure_ramp(const std::vector<TimeMachineSpec>& ramps, size_t loop_count) {
        this->loop_count = loop_count;
        this->ramps = ramps;
      }

     public:
      // Guards
      bool start_hold_done(TimeMachine& m) {
        return m.get_tick() - phase_init_at >= ramps[ramp_idx].start_hold_duration;
      }

      bool ramping_done(TimeMachine& m) {
        return m.get_tick() - phase_init_at >= ramps[ramp_idx].transition_duration;
      }

      bool end_hold_done(TimeMachine& m) {
        return m.get_tick() - phase_init_at >= ramps[ramp_idx].end_hold_duration;
      }

      bool has_more_spec() {
        return ramp_idx < ramps.size() - 1;
      }

      bool has_more_loop() {
        return loop_count < 0 || loop_done < loop_count;
      }

      // Actions
      void load_initial_ramp() {
        loop_done = 0;
        ramp_idx = 0;
      }

      void start_hold_init(TimeMachine& m) {
        phase_init_at = m.get_tick();
        m.on_start_hold(true);
      }

      void start_hold_finish(TimeMachine& m) {
        m.on_start_hold(false);
      }

      void ramping_init(TimeMachine& m) {
        phase_init_at = m.get_tick();
        initial_state = m.get_current_state();
      }

      void ramping_update(TimeMachine& m) {
        if (ramps[ramp_idx].transition_duration == 0) {
          return;
        }

        auto elapsed = m.get_tick() - phase_init_at;
        uint8_t percent = elapsed * 100 / ramps[ramp_idx].transition_duration;
        auto new_state = interpolate(initial_state, ramps[ramp_idx].next_state, percent);
        m.on_ramp_update(new_state);
      }

      void ramping_set_final(TimeMachine& m) {
        // only necessary for 0 duration ramps
        if (ramps[ramp_idx].transition_duration == 0) {
          m.on_ramp_update(ramps[ramp_idx].next_state);
        }
      }

      void end_hold_init(TimeMachine& m) {
        phase_init_at = m.get_tick();
        m.on_end_hold(true);
      }

      void end_hold_finish(TimeMachine& m) {
        m.on_end_hold(false);
      }

      void load_next_ramp() {
        ramp_idx++;
      }

      void load_next_loop() {
        loop_done++;
        load_next_ramp();
      }

      void all_loops_done(TimeMachine& m) {
        m.on_all_loops_done();
      };

      auto operator()() {
        using namespace boost::sml;
        auto has_more_spec = wrap(&Self::has_more_spec);
        auto has_more_loop = wrap(&Self::has_more_loop);

        return make_transition_table(
            // clang-format off
          *state<Entry> / &Self::load_initial_ramp = state<StartHold>
          ,state<StartHold> + on_entry<_> / &Self::start_hold_init
          ,state<StartHold> + on_exit<_> / &Self::start_hold_finish
          ,state<StartHold> [&Self::start_hold_done] = state<Ramping>
          ,state<Ramping> + on_entry<_> / &Self::ramping_init
          ,state<Ramping> + event<Tick> / &Self::ramping_update
          ,state<Ramping> [&Self::ramping_done] / &Self::ramping_set_final = state<EndHold>
          ,state<EndHold> + on_entry<_> / &Self::end_hold_init
          ,state<EndHold> + on_exit<_> / &Self::end_hold_finish
          ,state<EndHold> [&Self::end_hold_done] = state<SwitchingNextLoop>
          ,state<SwitchingNextLoop> [has_more_spec] / &Self::load_next_ramp = state<StartHold>
          ,state<SwitchingNextLoop> [!has_more_spec && has_more_loop] / &Self::load_next_loop = state<StartHold>
          ,state<SwitchingNextLoop> [!has_more_spec && !has_more_loop] / (wrap(&Self::all_loops_done), process(Stop{})) = X
            // clang-format on
        );
      }
    };

    struct Idle {};

    void configure_ramp(const Ramp& evt, RampingSubSM& s) {
      s.configure_ramp(evt.ramps, evt.loop_count);
    }

    void on_start_new_ramp(TimeMachine& m) {
      m.on_ramp_start(false);
    }

    void on_restart_ramp(TimeMachine& m) {
      m.on_ramp_start(true);
    }

    void on_stop(TimeMachine& m) {
      m.on_stopped();
    }

    auto operator()() {
      using namespace boost::sml;
      return make_transition_table(
          // clang-format off
        *state<Idle> + event<Ramp> / (wrap(&Self::configure_ramp), wrap(&Self::on_start_new_ramp)) = state<RampingSubSM>,
         state<RampingSubSM> + event<Ramp> / (wrap(&Self::configure_ramp), wrap(&Self::on_restart_ramp)) = state<RampingSubSM>
        ,state<RampingSubSM> + event<Stop> / &Self::on_stop = state<Idle>
          // clang-format on
      );
    }
  };

 private:
  typename TimeMachineSM::RampingSubSM sub_tm_instance;
  TimeMachineSM tm_instance;

 public:
  boost::sml::sm<TimeMachineSM, boost::sml::process_queue<std::queue>> sm{tm_instance, sub_tm_instance, *this};
};