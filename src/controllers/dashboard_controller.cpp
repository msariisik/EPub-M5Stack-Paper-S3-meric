#define __DASHBOARD_CONTROLLER__ 1
#include "controllers/dashboard_controller.hpp"
#include "controllers/app_controller.hpp"
#include "controllers/event_mgr.hpp"
#include "screen.hpp"
#include "viewers/page.hpp"

#if !EPUB_LINUX_BUILD
#include "cJSON.h"
#include "controllers/wifi.hpp"
#include "esp_http_client.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_heap_caps.h"

// Includes needed to tear down and rebuild the screen driver
#include "inkplate_platform.hpp"
#include "models/config.hpp"
#include "models/fonts.hpp"
#include <unistd.h>
#endif

#include <cmath>
#include <cstdio>
#include <cstring>
#include <time.h>

// ──────────────────────────────────────────────────────────────
// Demo (simulated) data
// ──────────────────────────────────────────────────────────────
void DashboardController::load_demo_data() {
  const float mock[12] = {0.1f, 0.5f, 1.2f, 2.8f, 4.5f, 7.2f,
                          5.0f, 2.5f, 1.0f, 0.4f, 0.1f, 0.0f};
  for (int i = 0; i < 12; i++)
    rain_data[i] = mock[i];
}

#if !EPUB_LINUX_BUILD

#define FETCH_STACK_WORDS 4096 // 16 KB is the max safe size for internal RAM stack

struct FetchCtx {
  const char *lat;
  const char *lon;
  float *data;
  char msg[64];
  bool success;
  SemaphoreHandle_t done;
};

void do_fetch_task(void *pv) {
  FetchCtx *ctx = (FetchCtx *)pv;
  ctx->success = false;

  {
    std::string lat = "51.4408";
    std::string lon = "5.4778";
    config.get(Config::Ident::LATITUDE, lat);
    config.get(Config::Ident::LONGITUDE, lon);

    if (!wifi.start()) {
      snprintf(ctx->msg, sizeof(ctx->msg), "WiFi start failed");
      goto done;
    }

    char url[512];
    snprintf(url, sizeof(url),
             "http://api.open-meteo.com/v1/forecast"
             "?latitude=%s&longitude=%s"
             "&minutely_15=precipitation&forecast_days=1",
             lat.c_str(), lon.c_str());

    esp_http_client_config_t http_cfg = {};
    http_cfg.url = url;
    http_cfg.timeout_ms = 20000;

    esp_http_client_handle_t client = esp_http_client_init(&http_cfg);

    if (!client) {
      snprintf(ctx->msg, sizeof(ctx->msg), "HTTP init failed");
      wifi.stop();
      goto done;
    }

    if (esp_http_client_open(client, 0) != ESP_OK) {
      snprintf(ctx->msg, sizeof(ctx->msg), "HTTP open failed");
      esp_http_client_cleanup(client);
      wifi.stop();
      goto done;
    }
    esp_http_client_fetch_headers(client);

    const int buf_size = 8192;
    char *buffer = (char *)malloc(buf_size + 1);
    if (!buffer) {
      snprintf(ctx->msg, sizeof(ctx->msg), "Out of memory");
      esp_http_client_cleanup(client);
      wifi.stop();
      goto done;
    }

    int total = 0;
    while (total < buf_size) {
      int n = esp_http_client_read(client, buffer + total, buf_size - total);
      if (n <= 0) break;
      total += n;
    }
    buffer[total] = '\0';

    esp_http_client_cleanup(client);
    wifi.stop();

    if (total == 0) {
      snprintf(ctx->msg, sizeof(ctx->msg), "Empty result");
      free(buffer);
      goto done;
    }

    cJSON *root = cJSON_Parse(buffer);
    free(buffer);

    if (!root) {
      snprintf(ctx->msg, sizeof(ctx->msg), "JSON parse failed");
      goto done;
    }

    cJSON *min15 = cJSON_GetObjectItem(root, "minutely_15");
    cJSON *precip = min15 ? cJSON_GetObjectItem(min15, "precipitation") : nullptr;

    if (precip && cJSON_IsArray(precip)) {
      int arr_size = cJSON_GetArraySize(precip);
      time_t now;
      struct tm timeinfo;
      time(&now);
      localtime_r(&now, &timeinfo);
      int start = timeinfo.tm_hour * 4 + (timeinfo.tm_min / 15);

      if (start + 12 > arr_size) start = arr_size - 12;
      if (start < 0) start = 0;

      for (int i = 0; i < 12 && (start + i) < arr_size; i++) {
        cJSON *item = cJSON_GetArrayItem(precip, start + i);
        ctx->data[i] = (item && cJSON_IsNumber(item)) ? (float)item->valuedouble : 0.0f;
      }
      ctx->success = true;
      snprintf(ctx->msg, sizeof(ctx->msg), "Live data");
    }
    cJSON_Delete(root);
  }
done:
  vTaskDelay(pdMS_TO_TICKS(100));

  // Task done, parent will delete it
  xSemaphoreGive(ctx->done);   
  vTaskSuspend(NULL);          
}
#endif

bool DashboardController::fetch_data_sync() {
#if EPUB_LINUX_BUILD
  load_demo_data();
  return true;
#else
  fonts.clear_glyph_caches();
  vTaskDelay(pdMS_TO_TICKS(100));
  Screen::get_singleton().deinit(); // Free Internal RAM for WiFi
  vTaskDelay(pdMS_TO_TICKS(100));
  
  FetchCtx ctx;
  ctx.data = rain_data;
  ctx.success = false;
  ctx.done = xSemaphoreCreateBinary();

  TaskHandle_t h = nullptr;
  if (xTaskCreate(do_fetch_task, "fetchTask", FETCH_STACK_WORDS, &ctx, 10, &h) != pdPASS) {
    snprintf(status_msg, sizeof(status_msg), "Task spawn failed");
    vSemaphoreDelete(ctx.done);
    return false;
  }
  
  if (xSemaphoreTake(ctx.done, pdMS_TO_TICKS(30000)) != pdTRUE) {
    vTaskDelete(h);
    snprintf(status_msg, sizeof(status_msg), "Fetch task timeout");
  } else {
    vTaskDelete(h);
    snprintf(status_msg, sizeof(status_msg), "%s", ctx.msg);
  }

  vSemaphoreDelete(ctx.done);

#if !EPUB_LINUX_BUILD
  vTaskDelay(pdMS_TO_TICKS(200)); 
  // Restore screen state on ESP32 after WiFi shutdown
  Screen::get_singleton().setup(Screen::PixelResolution::THREE_BITS, Screen::Orientation::RIGHT);
#endif
  return ctx.success;
#endif
}

void DashboardController::enter() {
  if (state != State::REAL && state != State::FETCHING) {
    load_demo_data();
    state = State::DEMO;
    snprintf(status_msg, sizeof(status_msg), "(example - tap Refresh for live data)");
  }
  // No orientation change here; remains in the app's default (Portrait)
  draw_graph();
}

void DashboardController::leave(bool going_to_deep_sleep) {
  if (!going_to_deep_sleep) {
    // Screen::get_singleton().setup(Screen::PixelResolution::THREE_BITS, Screen::Orientation::BOTTOM);
  }
}

void DashboardController::draw_graph() {
  int sw = Screen::get_width();  
  int sh = Screen::get_height(); 

  Page::Format fmt = {
      .line_height_factor = 1.0,
      .font_index = 1,
      .font_size = 14,
      .indent = 0,
      .margin_left = 0,
      .margin_right = 0,
      .margin_top = 0,
      .margin_bottom = 0,
      .screen_left = 0,
      .screen_right = 0,
      .screen_top = 0,
      .screen_bottom = 0,
      .width = (int16_t)sw,
      .height = (int16_t)sh,
      .vertical_align = 0,
      .trim = true,
      .pre = false,
      .font_style = Fonts::FaceStyle::NORMAL,
      .align = CSS::Align::CENTER,
      .text_transform = CSS::TextTransform::NONE,
      .display = CSS::Display::INLINE};

  page.set_compute_mode(Page::ComputeMode::DISPLAY);
  page.start(fmt);

  fmt.font_size = 18;
  page.put_str_at("Rain Forecast", Pos(Page::HORIZONTAL_CENTER, 55), fmt);

  fmt.font_size = 11;
  page.put_str_at(status_msg, Pos(Page::HORIZONTAL_CENTER, 115), fmt);

  const int margin_l = 60;
  const int margin_r = 150; 
  const int graph_x = margin_l;
  const int graph_w = sw - margin_l - margin_r; 
  const int graph_h = 560;                      
  const int graph_y = 780; 

  const float px_per_mm = 70.0f; // Adjusted for landscape height
  struct { float mm; const char *label; } thresholds[] = {
      {0.5f, "LICHT"}, {2.5f, "MATIG"}, {7.5f, "ZWAAR"},
  };
  for (auto &t : thresholds) {
    int hy = (int)(t.mm * px_per_mm);
    if (hy > graph_h) hy = graph_h;
    page.put_highlight(Dim(graph_w, 1), Pos(graph_x, graph_y - hy));
    fmt.font_size = 12;
    fmt.align = CSS::Align::LEFT;
    page.put_str_at(t.label, Pos(graph_x + graph_w + 4, graph_y - hy + 5), fmt);
  }

  fmt.font_size = 11;
  fmt.align = CSS::Align::CENTER;
  time_t now;
  struct tm timeinfo;
  time(&now);
  localtime_r(&now, &timeinfo);
  int current_h = timeinfo.tm_hour;
  int current_m = timeinfo.tm_min;

  for (int t = 0; t <= 3; t++) {
    int target_h = (current_h + t) % 24;
    int seg = (t == 3) ? 11 : t * 4;
    int tx = graph_x + (graph_w * seg) / 11;
    page.put_highlight(Dim(1, graph_h), Pos(tx, graph_y - graph_h));
    char tbuf[8]; snprintf(tbuf, sizeof(tbuf), "%02d:%02d", target_h, current_m);
    page.put_str_at(tbuf, Pos(tx, graph_y - graph_h - 14), fmt);
  }

  for (int px = 0; px <= graph_w; px++) {
    float f_idx = (float)px * 11.0f / (float)graph_w;
    int i = (int)f_idx;
    if (i >= 11) i = 10;
    float rem = f_idx - i;
    float val = rain_data[i] + (rain_data[i + 1] - rain_data[i]) * rem;
    int bar_h = (int)(val * px_per_mm);
    if (bar_h > 1) page.put_highlight(Dim(1, bar_h), Pos(graph_x + px, graph_y - bar_h));
  }

  page.put_highlight(Dim(graph_w + 1, 2), Pos(graph_x, graph_y + 1));

  const int btn_h = 50;
  const int btn_w = 200;
  const int btn_y = sh - 75;
  const int total_btns_w = btn_w * 2 + 40;
  const int start_x = (sw - total_btns_w) / 2;

  btn_refresh_x = start_x; btn_refresh_y = btn_y; btn_refresh_w = btn_w; btn_refresh_h = btn_h;
  page.put_rounded(Dim(btn_w, btn_h), Pos(start_x, btn_y));
  fmt.font_size = 14; page.put_str_at("Refresh", Pos(start_x + btn_w / 2, btn_y + 36), fmt);

  btn_back_x = start_x + btn_w + 40; btn_back_y = btn_y; btn_back_w = btn_w; btn_back_h = btn_h;
  page.put_rounded(Dim(btn_w, btn_h), Pos(btn_back_x, btn_y));
  page.put_str_at("Back", Pos(btn_back_x + btn_w / 2, btn_y + 36), fmt);

  page.paint(true);
}

void DashboardController::input_event(const EventMgr::Event &event) {
  if (event.kind == EventMgr::EventKind::TAP) {
    int tx = (int)event.x;
    int ty = (int)event.y;
    printf("DASH: Tap RawX=%d RawY=%d\n", tx, ty);

    if (tx >= btn_back_x && tx <= btn_back_x + btn_back_w && ty >= btn_back_y && ty <= btn_back_y + btn_back_h) {
      app_controller.set_controller(AppController::Ctrl::MAIN);
      return;
    }
    if (tx >= btn_refresh_x && tx <= btn_refresh_x + btn_refresh_w && ty >= btn_refresh_y && ty <= btn_refresh_y + btn_refresh_h) {
      if (state == State::FETCHING) return; // Prevent double-fetch
      state = State::FETCHING;
      snprintf(status_msg, sizeof(status_msg), "Fetching live data...");
      draw_graph(); 
      if (fetch_data_sync()) { state = State::REAL; } else { state = State::ERROR; }
      draw_graph();
    }
  }
}
