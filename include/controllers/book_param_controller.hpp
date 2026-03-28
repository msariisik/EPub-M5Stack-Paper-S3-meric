// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#pragma once
#include "global.hpp"

#include "controllers/event_mgr.hpp"

class BookParamController
{
  private:
    static constexpr char const * TAG = "BookParamController";

    bool book_params_form_is_shown;
    bool go_to_page_form_is_shown;

  public:
    BookParamController() : 
      book_params_form_is_shown(false), 
      go_to_page_form_is_shown(false) { };

    void    input_event(const EventMgr::Event & event);
    void          enter();
    void          leave(bool going_to_deep_sleep = false);
    void set_font_count(uint8_t count);

    inline void set_book_params_form_is_shown() { book_params_form_is_shown = true; }
    inline void set_go_to_page_form_is_shown()  { go_to_page_form_is_shown  = true; }
};

#if __BOOK_PARAM_CONTROLLER__
  BookParamController book_param_controller;
#else
  extern BookParamController book_param_controller;
#endif
