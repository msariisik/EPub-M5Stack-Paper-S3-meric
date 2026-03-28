#pragma once
#include "global.hpp"
#include "controllers/event_mgr.hpp"

class DashboardController
{
  public:
    enum class State { DEMO, FETCHING, REAL, ERROR };

  private:
    static constexpr char const * TAG = "DashboardController";
    float rain_data[12];
    State state;
    char  status_msg[64];

    // Button hit regions (set in draw_graph, used in input_event)
    int btn_refresh_x, btn_refresh_y, btn_refresh_w, btn_refresh_h;
    int btn_back_x,    btn_back_y,    btn_back_w,    btn_back_h;

    void draw_graph();
    bool fetch_data_sync(); // runs on calling task, returns true on success
    void load_demo_data();

  public:
    DashboardController() : state(State::DEMO) {
      for(int i=0; i<12; i++) rain_data[i] = 0.0f;
      status_msg[0] = '\0';
      btn_refresh_x = btn_refresh_y = btn_refresh_w = btn_refresh_h = 0;
      btn_back_x    = btn_back_y    = btn_back_w    = btn_back_h    = 0;
    };
    void enter();
    void leave(bool going_to_deep_sleep = false);
    void input_event(const EventMgr::Event & event);
};

#if __DASHBOARD_CONTROLLER__
  DashboardController dashboard_controller;
#else
  extern DashboardController dashboard_controller;
#endif
