#define __MAIN_CONTROLLER__ 1
#include "controllers/main_controller.hpp"
#include "controllers/app_controller.hpp"
#include "viewers/menu_viewer.hpp"
#include "controllers/common_actions.hpp"

#if DATE_TIME_RTC
  #include "controllers/ntp.hpp"
  #include "controllers/clock.hpp"
  #include "models/config.hpp"
  #include "viewers/msg_viewer.hpp"
  #if EPUB_INKPLATE_BUILD
    #include "esp_system.h"
  #endif
#endif
#if DATE_TIME_RTC
static bool wait_for_key_after_ntp = false;

static void ntp_clock_adjust()
{
  std::string ntp_server;
  config.get(Config::Ident::NTP_SERVER, ntp_server);

  msg_viewer.show(MsgViewer::MsgType::NTP_CLOCK, false, true, 
    "Date/Time Retrieval", 
    "Retrieving Date and Time from NTP Server %s. Please wait.",
    ntp_server.c_str());

  if (ntp.get_and_set_time()) {
    time_t time;
    Clock::get_date_time(time);
    msg_viewer.show(MsgViewer::MsgType::NTP_CLOCK, true, true, 
      "Date/Time Retrieval Completed", 
      "Local Time is %s. The device will now restart.", ctime(&time));
  }
  else {
    msg_viewer.show(MsgViewer::MsgType::NTP_CLOCK, true, true, 
      "Date/Time Retrieval Failed", 
      "Unable to get Date/Time from NTP Server! The device will now restart.");
  }

  wait_for_key_after_ntp = true;
}
#endif

static void enter_books_dir()
{
  app_controller.set_controller(AppController::Ctrl::DIR);
}

static void enter_dashboard()
{
  app_controller.set_controller(AppController::Ctrl::DASHBOARD);
}

static void enter_vcard()
{
  app_controller.set_controller(AppController::Ctrl::VCARD);
}

static MenuViewer::MenuEntry menu[] = {
  { MenuViewer::Icon::BOOK_LIST,   "E-Books Reader",  enter_books_dir,      true, true },
  { MenuViewer::Icon::INFO,        "Dashboard",       enter_dashboard,      true, true },
  #if DATE_TIME_RTC
    { MenuViewer::Icon::NTP_CLOCK, "Sync RTC Time (NTP)", ntp_clock_adjust, true, true },
  #endif
  { MenuViewer::Icon::VCARD,     "VCard QR",         enter_vcard,          true, true },
  { MenuViewer::Icon::END_MENU,    nullptr,           nullptr,              false, false }
};

void MainController::enter()
{
  menu_viewer.show(menu, 0, true);
}

void MainController::leave(bool going_to_deep_sleep)
{
}

void MainController::input_event(const EventMgr::Event & event)
{
#if DATE_TIME_RTC
  if (wait_for_key_after_ntp) {
    if (event.kind != EventMgr::EventKind::NONE) {
      msg_viewer.show(MsgViewer::MsgType::INFO, 
                      false, true, 
                      "Restarting", 
                      "The device is now restarting. Please wait.");
      wait_for_key_after_ntp = false;
      #if EPUB_INKPLATE_BUILD
        esp_restart();
      #endif
    }
    return;
  }
#endif

  menu_viewer.event(event);
}
