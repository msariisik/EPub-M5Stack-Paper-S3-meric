#pragma once
#include "global.hpp"
#include "controllers/event_mgr.hpp"

class DashboardController
{
  private:
    static constexpr char const * TAG = "DashboardController";

  public:
    DashboardController() { };
    void enter();
    void leave(bool going_to_deep_sleep = false);
    void input_event(const EventMgr::Event & event);
};

#if __DASHBOARD_CONTROLLER__
  DashboardController dashboard_controller;
#else
  extern DashboardController dashboard_controller;
#endif
