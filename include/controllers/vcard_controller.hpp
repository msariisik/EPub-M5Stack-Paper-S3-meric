#pragma once
#include "global.hpp"
#include "controllers/event_mgr.hpp"
#include "models/image.hpp"

class VCardController
{
  public:
    VCardController();
    void enter();
    void leave(bool going_to_deep_sleep);
    void input_event(const EventMgr::Event & event);

  private:
    static constexpr char const * TAG = "VCardController";
    Image::ImageData photo;
    bool is_valid;
    void draw();
};

#if __VCARD_CONTROLLER__
  VCardController vcard_controller;
#else
  extern VCardController vcard_controller;
#endif
