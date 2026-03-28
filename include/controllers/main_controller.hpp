#pragma once
#include "global.hpp"
#include "controllers/event_mgr.hpp"

class MainController
{
  private:
    static constexpr char const * TAG = "MainController";

  public:
    MainController() { };
    void enter();
    void leave(bool going_to_deep_sleep = false);
    void input_event(const EventMgr::Event & event);
};

#if __MAIN_CONTROLLER__
  MainController main_controller;
#else
  extern MainController main_controller;
#endif
