#define __DASHBOARD_CONTROLLER__ 1
#include "controllers/dashboard_controller.hpp"
#include "controllers/app_controller.hpp"
#include "viewers/page.hpp"
#include "screen.hpp"

void DashboardController::enter()
{
  Page::Format fmt = {
    .line_height_factor = 1.0,
    .font_index         =   1,
    .font_size          =  60, // Large text
    .indent             =   0,
    .margin_left        =  10,
    .margin_right       =  10,
    .margin_top         =  10,
    .margin_bottom      =  10,
    .screen_left        =   0,
    .screen_right       =   0,
    .screen_top         =   0,
    .screen_bottom      =   0,
    .width              =   0,
    .height             =   0,
    .vertical_align     =   0,
    .trim               = true,
    .pre                = false,
    .font_style         = Fonts::FaceStyle::NORMAL,
    .align              = CSS::Align::CENTER,
    .text_transform     = CSS::TextTransform::NONE,
    .display            = CSS::Display::INLINE
  };

  page.start(fmt);

  // Center "meric"
  page.put_str_at("meric", Pos(Page::HORIZONTAL_CENTER, Screen::get_height() / 2), fmt);

  // "Go Back" instruction at bottom
  fmt.font_size = 16;
  page.put_str_at("Tap anywhere to go back", Pos(Page::HORIZONTAL_CENTER, Screen::get_height() - 150), fmt);

  page.paint(true);
}

void DashboardController::leave(bool going_to_deep_sleep)
{
}

void DashboardController::input_event(const EventMgr::Event & event)
{
  // Any tap/interaction goes back to Main Screen
  app_controller.set_controller(AppController::Ctrl::MAIN);
}
