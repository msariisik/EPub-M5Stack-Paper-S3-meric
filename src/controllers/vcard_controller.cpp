#define __VCARD_CONTROLLER__ 1
#include "controllers/vcard_controller.hpp"
#include "controllers/app_controller.hpp"
#include "models/config.hpp"
#include "models/sd_image_loader.hpp"
#include "viewers/page.hpp"
#include "viewers/screen_bottom.hpp"
#include "screen.hpp"
#include "helpers/qrcodegen.h"
#include "alloc.hpp"

#include <cstdio>
#include <cstring>

VCardController::VCardController() : is_valid(false) {}

void VCardController::enter() {
    is_valid = true;
    screen.set_orientation(Screen::Orientation::TOP);
    draw();
}

void VCardController::leave(bool going_to_deep_sleep) {
    screen.set_orientation(Screen::Orientation::BOTTOM);
    if (photo.bitmap) {
        free(photo.bitmap);
        photo.bitmap = nullptr;
    }
}

void VCardController::input_event(const EventMgr::Event & event) {
    if (event.kind == EventMgr::EventKind::TAP) {
        app_controller.set_controller(AppController::Ctrl::LAST);
    }
}

void VCardController::draw() {
    std::string name, title, org, tel, email, url, photo_fn;
    config.get(Config::Ident::VCARD_NAME, name);
    config.get(Config::Ident::VCARD_TITLE, title);
    config.get(Config::Ident::VCARD_ORG, org);
    config.get(Config::Ident::VCARD_TEL, tel);
    config.get(Config::Ident::VCARD_EMAIL, email);
    config.get(Config::Ident::VCARD_URL, url);
    config.get(Config::Ident::VCARD_PHOTO, photo_fn);

    // Prepare VCard string (Simplified and using CRLF for better recognition)
    char vcard[512];
    snprintf(vcard, sizeof(vcard),
             "BEGIN:VCARD\r\nVERSION:3.0\r\nFN:%s\r\nTEL;TYPE=CELL:%s\r\nEMAIL:%s\r\nURL:%s\r\nEND:VCARD",
             name.c_str(), tel.c_str(), email.c_str(), url.c_str());

    // Generate QR Code
    uint8_t qrcode[qrcodegen_BUFFER_LEN_MAX];
    uint8_t tempBuffer[qrcodegen_BUFFER_LEN_MAX];
    bool ok = qrcodegen_encodeText(vcard, tempBuffer, qrcode, qrcodegen_Ecc_MEDIUM, qrcodegen_VERSION_MIN, qrcodegen_VERSION_MAX, qrcodegen_Mask_AUTO, true);

    // Setup Page
    Page::Format fmt = {
        .line_height_factor = 1.2,
        .font_index = 1, // System font
        .font_size = 12,
        .indent = 0,
        .margin_left = 20,
        .margin_right = 20,
        .margin_top = 20,
        .margin_bottom = 20,
        .screen_left = 0,
        .screen_right = 0,
        .screen_top = 0,
        .screen_bottom = 0,
        .width = (int16_t)Screen::get_width(),
        .height = (int16_t)Screen::get_height(),
        .vertical_align = 0,
        .trim = true,
        .pre = false,
        .font_style = Fonts::FaceStyle::NORMAL,
        .align = CSS::Align::CENTER,
        .text_transform = CSS::TextTransform::NONE,
        .display = CSS::Display::BLOCK
    };

    page.start(fmt);

    // 1. Photo
    char photo_path[128];
    snprintf(photo_path, sizeof(photo_path), "%s/%s", MAIN_FOLDER, photo_fn.c_str());
    if (SDImageLoader::load_jpeg(photo_path, photo, Dim(300, 300))) {
        int16_t px = (Screen::get_width() - photo.dim.width) / 2;
        page.put_image(photo, Pos(px, 40));
    }

    // 2. Text Details
    fmt.font_size = 20;
    fmt.font_style = Fonts::FaceStyle::BOLD;
    page.put_str_at(name, Pos(Page::HORIZONTAL_CENTER, 360), fmt);
    
    // 3. QR Code
    if (ok) {
        int size = qrcodegen_getSize(qrcode);
        int scale = 7; // Reduced to ~75% of previous size (10)
        int qr_pixel_size = size * scale;
        int qx = (Screen::get_width() - qr_pixel_size) / 2;
        int qy = 385; // Positioned closer to the name

        for (int y = 0; y < size; y++) {
            for (int x = 0; x < size; x++) {
                if (qrcodegen_getModule(qrcode, x, y)) {
                    page.set_region(Dim(scale, scale), Pos(qx + x * scale, qy + y * scale));
                }
            }
        }
    }

    ScreenBottom::show();
    page.paint(true);
}
