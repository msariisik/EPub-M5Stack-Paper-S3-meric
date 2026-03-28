// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#define __BOOK_PARAM_CONTROLLER__ 1
#include "controllers/book_param_controller.hpp"

#include "controllers/app_controller.hpp"
#include "controllers/common_actions.hpp"
#include "controllers/books_dir_controller.hpp"
#include "controllers/book_controller.hpp"
#include "models/books_dir.hpp"
#include "models/epub.hpp"
#include "models/config.hpp"
#include "models/page_locs.hpp"
#include "models/toc.hpp"
#include "viewers/menu_viewer.hpp"
#include "viewers/form_viewer.hpp"
#include "viewers/msg_viewer.hpp"

#if EPUB_INKPLATE_BUILD && !BOARD_TYPE_PAPER_S3
  #include "esp_system.h"
  #include "eink.hpp"
  #include "esp.hpp"
  #include "soc/rtc.h"
#endif

#include <sys/stat.h>

static int8_t show_images;
static int8_t font_size;
static int8_t use_fonts_in_book;
static int8_t font;
static int8_t done_res;

static int8_t old_font_size;
static int8_t old_show_images;
static int8_t old_use_fonts_in_book;
static int8_t old_font;

#if INKPLATE_6PLUS || TOUCH_TRIAL
  static constexpr int8_t BOOK_PARAMS_FORM_SIZE = 5;
  static constexpr int8_t GO_TO_PAGE_FORM_SIZE = 2;
#else
  static constexpr int8_t BOOK_PARAMS_FORM_SIZE = 4;
  static constexpr int8_t GO_TO_PAGE_FORM_SIZE = 1;
#endif

static uint16_t specific_page_number = 1;
static FormEntry go_to_page_form_entries[GO_TO_PAGE_FORM_SIZE] = {
  { .caption = "Page Number:",
    .u = { .val = { .value = &specific_page_number,
                    .min = 1,
                    .max = 9999 } },
    .entry_type = FormEntryType::UINT16 }
  #if INKPLATE_6PLUS || TOUCH_TRIAL
    , { .caption = " GO ",
        .u = { .ch = { .value = &done_res,
                       .choice_count = 0,
                       .choices = nullptr } },
        .entry_type = FormEntryType::DONE }
  #endif
};

static FormEntry book_params_form_entries[BOOK_PARAMS_FORM_SIZE] = {
  { .caption = "Font Size:",
    .u = { .ch = { .value = &font_size,
                   .choice_count = 4,
                   .choices = FormChoiceField::font_size_choices } },
    .entry_type = FormEntryType::HORIZONTAL },
  { .caption = "Use fonts in book:",
    .u = { .ch = { .value = &use_fonts_in_book,
                   .choice_count = 2,
                   .choices = FormChoiceField::yes_no_choices } },
    .entry_type = FormEntryType::HORIZONTAL },
  { .caption = "Font:",
    .u = { .ch = { .value = &font,
                   .choice_count = 8,
                   .choices = FormChoiceField::font_choices } },
    .entry_type = FormEntryType::VERTICAL },
  { .caption = "Show Images in book:",
    .u = { .ch = { .value = &show_images,
                   .choice_count = 2,
                   .choices = FormChoiceField::yes_no_choices } },
    .entry_type = FormEntryType::HORIZONTAL },
  #if INKPLATE_6PLUS || TOUCH_TRIAL
    { .caption = " DONE ",
      .u = { .ch = { .value = &done_res,
                     .choice_count = 0,
                     .choices = nullptr } },
      .entry_type = FormEntryType::DONE }
  #endif
};

static void
book_parameters()
{
  BookParams * book_params = epub.get_book_params();

  book_params->get(BookParams::Ident::SHOW_IMAGES,        &show_images      );
  book_params->get(BookParams::Ident::FONT_SIZE,          &font_size        );
  book_params->get(BookParams::Ident::USE_FONTS_IN_BOOK,  &use_fonts_in_book);
  book_params->get(BookParams::Ident::FONT,               &font             );
  
  if (show_images       == -1) config.get(Config::Ident::SHOW_IMAGES,        &show_images      );
  if (font_size         == -1) config.get(Config::Ident::FONT_SIZE,          &font_size        );
  if (use_fonts_in_book == -1) config.get(Config::Ident::USE_FONTS_IN_BOOKS, &use_fonts_in_book);
  if (font              == -1) config.get(Config::Ident::DEFAULT_FONT,       &font             );
  
  old_show_images        = show_images;
  old_use_fonts_in_book  = use_fonts_in_book;
  old_font               = font;
  old_font_size          = font_size;
  done_res               = 1;

  form_viewer.show(
    book_params_form_entries, 
    BOOK_PARAMS_FORM_SIZE, 
    "(Any item change will trigger book refresh)");

  book_param_controller.set_book_params_form_is_shown();
}

static void
revert_to_defaults()
{
  page_locs.stop_document();
  
  EPub::BookFormatParams * book_format_params = epub.get_book_format_params();

  BookParams * book_params = epub.get_book_params();

  old_use_fonts_in_book = book_format_params->use_fonts_in_book;
  old_font              = book_format_params->font;

  constexpr int8_t default_value = -1;

  book_params->put(BookParams::Ident::SHOW_IMAGES,       default_value);
  book_params->put(BookParams::Ident::FONT_SIZE,         default_value);
  book_params->put(BookParams::Ident::FONT,              default_value);
  book_params->put(BookParams::Ident::USE_FONTS_IN_BOOK, default_value);
  
  epub.update_book_format_params();

  book_params->save();

  msg_viewer.show(MsgViewer::MsgType::INFO, 
                  false, false, 
                  "E-book parameters reverted", 
                  "E-book parameters reverted to default values.");

  if (old_use_fonts_in_book != book_format_params->use_fonts_in_book) {
    if (book_format_params->use_fonts_in_book) {
      epub.load_fonts();
    }
    else {
      fonts.clear();
      fonts.clear_glyph_caches();
    }
  }

  if (old_font != book_format_params->font) {
    fonts.adjust_default_font(book_format_params->font);
  }
}

// Removed books_list as it was moved to home page.

static void go_to_specific_page_form()
{
  int16_t pg_count = page_locs.get_page_count();
  if (pg_count == -1) {
    msg_viewer.show(MsgViewer::ALERT, false, false, "Not Ready", "The book is still loading its pages.");
  } else {
    specific_page_number = page_locs.get_page_nbr(book_controller.get_current_page_id()) + 1;
    if (specific_page_number < 1) specific_page_number = 1;
    
    go_to_page_form_entries[0].u.val.max = pg_count;
    
    form_viewer.show(
      go_to_page_form_entries, 
      GO_TO_PAGE_FORM_SIZE, 
      "(Enter page number)");

    book_param_controller.set_go_to_page_form_is_shown();
  }
}

static void go_to_first_page()
{
  if (book_controller.go_to_first_page()) {
    app_controller.set_controller(AppController::Ctrl::LAST);
  }
}

static void go_to_last_page()
{
  if (book_controller.go_to_last_page()) {
    app_controller.set_controller(AppController::Ctrl::LAST);
  }
}

// Removed delete_book as it was moved to the option controller.

static void 
toc_ctrl()
{
  app_controller.set_controller(AppController::Ctrl::TOC);
}

extern bool start_web_server();
extern bool  stop_web_server();

// Removed wifi_mode as it was moved to home page.

static void
power_off()
{
  books_dir_controller.save_last_book(book_controller.get_current_page_id(), true); 
  
  CommonActions::power_it_off();
}

static void
return_to_main_menu()
{
  app_controller.set_controller(AppController::Ctrl::MAIN);
}

// IMPORTANT!!
// The first (menu[0]) and the last menu entry (the one before END_MENU) MUST ALWAYS BE VISIBLE!!!

static MenuViewer::MenuEntry menu[] = {
  { MenuViewer::Icon::RETURN,      "Return to the e-books reader",         CommonActions::return_to_last, true , true },
  { MenuViewer::Icon::TOC,         "Table of Content",                     toc_ctrl                     , false, true },
  { MenuViewer::Icon::PREV_MENU,   "Go to First Page",                     go_to_first_page             , true , true },
  { MenuViewer::Icon::BOOK,        "Go to Specific Page",                  go_to_specific_page_form     , true , true },
  { MenuViewer::Icon::FONT_PARAMS, "Current e-book parameters",            book_parameters              , true , true },
  { MenuViewer::Icon::REVERT,      "Revert e-book parameters to "
                                   "default values",                       revert_to_defaults           , true , true },  
  { MenuViewer::Icon::NEXT_MENU,   "Go to Last Page",                      go_to_last_page              , true , true },
  { MenuViewer::Icon::POWEROFF,    "Power OFF (Deep Sleep)",               power_off                    , true , true },
  { MenuViewer::Icon::HOME,        "Go back to the Main Menu",             return_to_main_menu          , true , true },
  { MenuViewer::Icon::END_MENU,    nullptr,                                nullptr                      , false, true }
}; 

void
BookParamController::set_font_count(uint8_t count)
{
  book_params_form_entries[2].u.ch.choice_count = count;
}

void 
BookParamController::enter()
{
  menu[1].visible = toc.is_ready() && !toc.is_empty();
  menu_viewer.show(menu);
  book_params_form_is_shown = false;
}

void 
BookParamController::leave(bool going_to_deep_sleep)
{

}

void 
BookParamController::input_event(const EventMgr::Event & event)
{
  if (book_params_form_is_shown) {
    if (form_viewer.event(event)) {
      book_params_form_is_shown = false;
      // if (ok) {
        BookParams * book_params = epub.get_book_params();

        if (show_images       !=       old_show_images) book_params->put(BookParams::Ident::SHOW_IMAGES,        show_images      );
        if (font_size         !=         old_font_size) book_params->put(BookParams::Ident::FONT_SIZE,          font_size        );
        if (font              !=              old_font) book_params->put(BookParams::Ident::FONT,               font             );
        if (use_fonts_in_book != old_use_fonts_in_book) book_params->put(BookParams::Ident::USE_FONTS_IN_BOOK,  use_fonts_in_book);
        
        if (book_params->is_modified()) epub.update_book_format_params();

        book_params->save();

        if (old_use_fonts_in_book != use_fonts_in_book) {
          if (use_fonts_in_book) {
            epub.load_fonts();
          }
          else {
            fonts.clear();
            fonts.clear_glyph_caches();
          }
        }
 
        if (old_font != font) {
          fonts.adjust_default_font(font);
        }
     // }
      menu_viewer.clear_highlight();
    }
  }
  else if (go_to_page_form_is_shown) {
    if (form_viewer.event(event)) {
      go_to_page_form_is_shown = false;
      if (book_controller.go_to_specific_page(specific_page_number)) {
        app_controller.set_controller(AppController::Ctrl::LAST);
      }
      menu_viewer.clear_highlight();
    }
  }
  else {
    if (menu_viewer.event(event)) {
      app_controller.set_controller(AppController::Ctrl::LAST);
    }
  }
}
