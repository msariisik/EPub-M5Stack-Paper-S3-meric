// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#define __MENU_VIEWER__ 1
#include "viewers/menu_viewer.hpp"

#include "controllers/app_controller.hpp"
#include "models/fonts.hpp"
#include "screen.hpp"
#include "viewers/book_viewer.hpp"
#include "viewers/page.hpp"
#include "viewers/screen_bottom.hpp"
#if EPUB_INKPLATE_BUILD
#include "esp.hpp"
#endif

static const std::string TOUCH_AND_HOLD_STR = "Touch and hold icon for info.";

void MenuViewer::show(MenuEntry *the_menu, uint8_t entry_index,
                      bool clear_screen) {
  Font *font = fonts.get(1);

  if (font == nullptr) {
    LOG_E("Internal error (Main Font not available!");
    return;
  }

  line_height = font->get_line_height(CAPTION_SIZE);
  text_height = line_height - font->get_descender_height(CAPTION_SIZE);

  font = fonts.get(0);

  if (font == nullptr) {
    LOG_E("Internal error (Drawings Font not available!");
    return;
  }

  Font::Glyph *icon = font->get_glyph('A', ICON_SIZE);

  if (icon == nullptr) {
    icon_height = 50;
    icon_ypos = 10 + icon_height;
    text_ypos = icon_ypos + line_height + 10;
  } else {
    icon_height = icon->dim.height;
    icon_ypos = 10 + icon_height;
    text_ypos = icon_ypos + line_height + 10;
  }

  region_height =
      text_ypos + 20; // Default, will be updated in show() if multi-row

  Page::Format fmt = {.line_height_factor = 1.0,
                      .font_index = 0,
                      .font_size = ICON_SIZE,
                      .indent = 0,
                      .margin_left = 0,
                      .margin_right = 0,
                      .margin_top = 0,
                      .margin_bottom = 0,
                      .screen_left = 10,
                      .screen_right = 10,
                      .screen_top = 10,
                      .screen_bottom = 100,
                      .width = 0,
                      .height = 0,
                      .vertical_align = 0,
                      .trim = true,
                      .pre = false,
                      .font_style = Fonts::FaceStyle::NORMAL,
                      .align = CSS::Align::LEFT,
                      .text_transform = CSS::TextTransform::NONE,
                      .display = CSS::Display::INLINE};

  page.start(fmt);

  page.clear_region(Dim{Screen::get_width(), region_height}, Pos{0, 0});

  menu = the_menu;

  struct VisibleItem {
    int m_idx;
    char ch;
    Font::Glyph *glyph;
  } items[MAX_MENU_ENTRY];

  uint8_t count = 0;
  uint8_t total = 0;

  // PASS 1: Identify all visible icons
  while ((total < MAX_MENU_ENTRY) && (menu[total].icon != Icon::END_MENU)) {
    if (menu[total].visible) {
      items[count].m_idx = total;
      items[count].ch = icon_char[(int)menu[total].icon];
      items[count].glyph = font->get_glyph(items[count].ch, ICON_SIZE);
      count++;
    }
    total++;
  }

  if (count == 0)
    return;

  // PASS 2: Layout calculation
  int row_val = 0;
  int row_count[MAX_MENU_ENTRY];
  for (int j = 0; j < MAX_MENU_ENTRY; j++)
    row_count[j] = 0;
  int item_row[MAX_MENU_ENTRY];
  Pos p(ICONS_LEFT_OFFSET, icon_ypos);

  for (int i = 0; i < count; i++) {
    if (row_count[row_val] > 0 &&
        p.x + SPACE_BETWEEN_ICONS >
            Screen::get_width() - ICONS_RIGHT_OFFSET) {
      row_val++;
      p.x = ICONS_LEFT_OFFSET;
      p.y += ICON_SIZE + 42;
    }
    item_row[i] = row_val;
    row_count[row_val]++;

    int mIdx = items[i].m_idx;
    entry_locs[mIdx].pos = p;
    if (items[i].glyph) {
      entry_locs[mIdx].pos.y += items[i].glyph->yoff;
      entry_locs[mIdx].dim = items[i].glyph->dim;
    } else {
      entry_locs[mIdx].dim = Dim(0, 0);
    }
    p.x += SPACE_BETWEEN_ICONS;
  }

  for (int r = 0; r <= row_val; r++) {
    int row_w = row_count[r] * SPACE_BETWEEN_ICONS;
    int start_x = (Screen::get_width() >> 1) - (row_w >> 1);
    for (int i = 0; i < count; i++) {
      if (item_row[i] == r) {
        entry_locs[items[i].m_idx].pos.x = start_x;
        start_x += SPACE_BETWEEN_ICONS;
      }
    }
  }

  text_ypos = p.y + line_height + 15;
  region_height = text_ypos + 65;

  page.clear_region(Dim{Screen::get_width(), region_height}, Pos{0, 0});

  // PASS 4: Drawing icons
  for (int i = 0; i < count; i++) {
    int mIdx = items[i].m_idx;
    page.put_char_at(items[i].ch,
                     Pos(entry_locs[mIdx].pos.x,
                         entry_locs[mIdx].pos.y -
                             (items[i].glyph ? items[i].glyph->yoff : 0)),
                     fmt);
  }

  // PASS 5: Selecting highlighted entry and Drawing caption
  int safety = 0;
  while (!menu[entry_index].visible && safety < MAX_MENU_ENTRY) {
    entry_index++;
    if (entry_index >= total) {
      entry_index = items[0].m_idx;
      break;
    }
    safety++;
  }
  current_entry_index = entry_index;

  fmt.font_index = 1;
  fmt.font_size = CAPTION_SIZE;

  std::string txt = menu[entry_index].caption;
#if (INKPLATE_6PLUS || TOUCH_TRIAL)
  txt = TOUCH_AND_HOLD_STR;
  hint_shown = false;
#endif

  Dim dim;
  fonts.get(1)->get_size(txt.c_str(), &dim, CAPTION_SIZE);
  int tw = dim.width;
  int tx = (Screen::get_width() >> 1) - (tw >> 1);
  if (tx < 10)
    tx = 10;
  page.put_str_at(txt, Pos{(uint16_t)tx, text_ypos}, fmt);

#if !(INKPLATE_6PLUS || TOUCH_TRIAL)
  page.put_highlight(Dim(entry_locs[entry_index].dim.width + 8,
                         entry_locs[entry_index].dim.height + 8),
                     Pos(entry_locs[entry_index].pos.x - 4,
                         entry_locs[entry_index].pos.y - 4));
#endif

  page.put_highlight(Dim(Screen::get_width() - 20, 3),
                     Pos(10, region_height - 12));
  max_index = total - 1;
  ScreenBottom::show();
  page.paint(clear_screen);
}

#if (INKPLATE_6PLUS || TOUCH_TRIAL)
uint8_t MenuViewer::find_index(uint16_t x, uint16_t y) {
  LOG_D("Find Index: [%u %u]", x, y);

  // page.put_highlight(Dim(5, 5), Pos(x-2, y-2));
  // page.put_highlight(Dim(7, 7), Pos(x-3, y-3));
  // page.paint(false, true, true);

  for (int8_t idx = 0; idx <= max_index; idx++) {
    if ((x >= entry_locs[idx].pos.x - 15) &&
        (x <= (entry_locs[idx].pos.x + entry_locs[idx].dim.width + 15)) &&
        (y >= (entry_locs[idx].pos.y - 15)) &&
        (y <= (entry_locs[idx].pos.y + entry_locs[idx].dim.height + 15))) {
      return idx;
    }
  }

  return max_index + 1;
}
#endif

void MenuViewer::clear_highlight() {
#if (INKPLATE_6PLUS || TOUCH_TRIAL)
  Page::Format fmt = {.line_height_factor = 1.0,
                      .font_index = 1,
                      .font_size = CAPTION_SIZE,
                      .indent = 0,
                      .margin_left = 0,
                      .margin_right = 0,
                      .margin_top = 0,
                      .margin_bottom = 0,
                      .screen_left = 10,
                      .screen_right = 10,
                      .screen_top = 10,
                      .screen_bottom = 0,
                      .width = 0,
                      .height = 0,
                      .vertical_align = 0,
                      .trim = true,
                      .pre = false,
                      .font_style = Fonts::FaceStyle::NORMAL,
                      .align = CSS::Align::LEFT,
                      .text_transform = CSS::TextTransform::NONE,
                      .display = CSS::Display::INLINE};

  page.start(fmt);

  if (hint_shown) {
    hint_shown = false;

    page.clear_highlight(Dim(entry_locs[current_entry_index].dim.width + 8,
                             entry_locs[current_entry_index].dim.height + 8),
                         Pos(entry_locs[current_entry_index].pos.x - 4,
                             entry_locs[current_entry_index].pos.y - 4));

    page.clear_region(Dim(Screen::get_width(), text_height),
                      Pos(0, text_ypos - line_height));
    page.put_str_at(TOUCH_AND_HOLD_STR, Pos{10, text_ypos}, fmt);
  }

  page.paint(false);
#endif
}

bool MenuViewer::event(const EventMgr::Event &event) {
  Page::Format fmt = {.line_height_factor = 1.0,
                      .font_index = 1,
                      .font_size = CAPTION_SIZE,
                      .indent = 0,
                      .margin_left = 0,
                      .margin_right = 0,
                      .margin_top = 0,
                      .margin_bottom = 0,
                      .screen_left = 10,
                      .screen_right = 10,
                      .screen_top = 10,
                      .screen_bottom = 0,
                      .width = 0,
                      .height = 0,
                      .vertical_align = 0,
                      .trim = true,
                      .pre = false,
                      .font_style = Fonts::FaceStyle::NORMAL,
                      .align = CSS::Align::LEFT,
                      .text_transform = CSS::TextTransform::NONE,
                      .display = CSS::Display::INLINE};

#if (INKPLATE_6PLUS || TOUCH_TRIAL)

  switch (event.kind) {
  case EventMgr::EventKind::HOLD:
    current_entry_index = find_index(event.x, event.y);
    if (current_entry_index <= max_index) {
      page.start(fmt);

      fmt.font_index = 1;
      fmt.font_size = CAPTION_SIZE;

      page.clear_region(Dim(Screen::get_width(), text_height),
                        Pos(0, text_ypos - line_height));

      std::string txt = menu[current_entry_index].caption;
      page.put_str_at(txt, Pos{10, text_ypos}, fmt);
      hint_shown = true;

      page.paint(false);
    }
    break;

  case EventMgr::EventKind::RELEASE:
#if EPUB_INKPLATE_BUILD
    ESP::delay(1000);
#endif
    clear_highlight();
    hint_shown = false;
    break;

  case EventMgr::EventKind::TAP:
    current_entry_index = find_index(event.x, event.y);
    if (current_entry_index <= max_index) {
      if (menu[current_entry_index].func != nullptr) {
        if (menu[current_entry_index].highlight) {
          page.start(fmt);

          fmt.font_index = 1;
          fmt.font_size = CAPTION_SIZE;

          page.clear_region(Dim(Screen::get_width(), text_height),
                            Pos(0, text_ypos - line_height));

          std::string txt = menu[current_entry_index].caption;
          page.put_str_at(txt, Pos{10, text_ypos}, fmt);
          hint_shown = true;

          page.put_highlight(
              Dim(entry_locs[current_entry_index].dim.width + 8,
                  entry_locs[current_entry_index].dim.height + 8),
              Pos(entry_locs[current_entry_index].pos.x - 4,
                  entry_locs[current_entry_index].pos.y - 4));

          page.paint(false);
        } else {
          hint_shown = false;
        }

        (*menu[current_entry_index].func)();
      }
      return false;
    }
    break;

  default:
    break;
  }
#else
  uint8_t old_index = current_entry_index;

  page.start(fmt);

  switch (event.kind) {
  case EventMgr::EventKind::PREV:
    if (current_entry_index > 0) {
      current_entry_index--;
      // It is expected that the first entry in the menu will always be visible
      while (!menu[current_entry_index].visible)
        current_entry_index--;
    } else {
      current_entry_index = max_index;
    }
    break;
  case EventMgr::EventKind::NEXT:
    if (current_entry_index < max_index) {
      current_entry_index++;
      // It is expected that the last entry in the menu will always be visible
      while (!menu[current_entry_index].visible)
        current_entry_index++;
    } else {
      current_entry_index = 0;
    }
    break;
  case EventMgr::EventKind::DBL_PREV:
    return false;
  case EventMgr::EventKind::DBL_NEXT:
    return false;
  case EventMgr::EventKind::SELECT:
    if (menu[current_entry_index].func != nullptr)
      (*menu[current_entry_index].func)();
    return false;
  case EventMgr::EventKind::DBL_SELECT:
    return true;
  case EventMgr::EventKind::NONE:
    return false;
  }

  if (current_entry_index != old_index) {
    page.clear_highlight(
        Dim(entry_locs[old_index].dim.width + 8,
            entry_locs[old_index].dim.height + 8),
        Pos(entry_locs[old_index].pos.x - 4, entry_locs[old_index].pos.y - 4));

    page.put_highlight(Dim(entry_locs[current_entry_index].dim.width + 8,
                           entry_locs[current_entry_index].dim.height + 8),
                       Pos(entry_locs[current_entry_index].pos.x - 4,
                           entry_locs[current_entry_index].pos.y - 4));

    fmt.font_index = 1;
    fmt.font_size = CAPTION_SIZE;

    page.clear_region(Dim(Screen::get_width(), text_height),
                      Pos(0, text_ypos - line_height));

    std::string txt = menu[current_entry_index].caption;
    page.put_str_at(txt, Pos{10, text_ypos}, fmt);
  }

  ScreenBottom::show();

  page.paint(false);
#endif

  return false;
}
