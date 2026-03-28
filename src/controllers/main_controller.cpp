#define __MAIN_CONTROLLER__ 1
#include "controllers/main_controller.hpp"
#include "controllers/app_controller.hpp"
#include "viewers/menu_viewer.hpp"
#include "controllers/common_actions.hpp"

static void enter_books_dir()
{
  app_controller.set_controller(AppController::Ctrl::DIR);
}

static void enter_dashboard()
{
  app_controller.set_controller(AppController::Ctrl::DASHBOARD);
}

static MenuViewer::MenuEntry menu[] = {
  { MenuViewer::Icon::BOOK_LIST,   "E-Books Reader",  enter_books_dir,      true, true },
  { MenuViewer::Icon::INFO,        "Dashboard",       enter_dashboard,      true, true },
  { MenuViewer::Icon::END_MENU,    nullptr,           nullptr,              false, false }
};

void MainController::enter()
{
  menu_viewer.show(menu);
}

void MainController::leave(bool going_to_deep_sleep)
{
}

void MainController::input_event(const EventMgr::Event & event)
{
  menu_viewer.event(event);
}
