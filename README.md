# M5Stack Paper S3 - Advanced E-Book Reader & Dashboard

An optimized E-Book reader and interactive dashboard specifically designed for the **M5Stack Paper S3** (ESP32-S3 e-ink device). 

This project is a powerful extension of the EPub-InkPlate engine, enhanced with a modern UI, real-time weather integration, and personal tools.

## Key Features

### 📖 Enhanced E-Book Reader
- **EPub 2/3 Support**: Read your favorite books with full font (TTF/OTF) and image support.
- **Custom Fonts**: Ships with premium fonts like Crimson, Caladea, and Red Hat.
- **Optimized for S3**: Tailored for the Paper S3's 540x960 e-ink display with smooth refresh cycles.

### 📱 Interactive Main Menu
- **Icon-Driven Navigation**: Easy-to-touch icons for all major features.
- **Fast Switching**: Jump between reading, weather, and tools with a single tap.

### 📊 Real-Time Weather Dashboard
- **Live Forecast**: Fetches real-time weather data and hourly precipitation forecasts via the Open-Meteo API.
- **Visual Analytics**: 4-hour bar graph to track rain and plan your tasks.
- **WiFi Aware**: Automatically connects and syncs data to keep your dashboard live.

### 📇 VCard QR Dashboard
- **Instant Sharing**: Display a personal VCard QR code for quick contact sharing.
- **Profile Photo**: Displays your profile picture (JPEG) directly from the SD card.
- **Auto-Flip Mode**: The screen rotates **180°** automatically in this view, so you can easily show it to someone standing across from you.

---

## 🚀 Quick Start

### 1. Build & Install
```bash
# Clone the repository
git clone --recurse-submodules https://github.com/meric-sioux/EPub-M5Stack-Paper-S3.git
cd EPub-M5Stack-Paper-S3

# Flash to your Paper S3
pio run -e paper_s3 -t upload
```

### 2. SD Card Setup
Ensure your micro-SD card is formatted as FAT32 and contains the following structure:
- `/books/`: Place your `.epub` files here.
- `/fonts/`: Place your `.ttf`/`.otf` fonts here.
- `config.txt`: Your personalized settings (see below).
- `photo.jpg`: Your profile picture for the VCard.

### 3. Personalization (`config.txt`)
Edit the configuration to match your location and details:
```text
latitude=51.4408
longitude=5.4778
vcard_name=Your Name
vcard_tel=+00 123 456 789
vcard_email=you@example.com
vcard_photo=photo.jpg
```

---

## 🧩 Credits & Fork Info
This project is a fork of [EPub-InkPlate](https://github.com/turgu1/EPub-InkPlate), extensively modified for the ESP32-S3 hardware.

- **Upstream README**: For detailed lower-level driver info and original project history, see [README_FORKED.md](README_FORKED.md).
- **Driver**: Powered by [epdiy](https://github.com/vroland/epdiy) for high-performance e-ink rendering.
- **QR Engine**: Using [qrcodegen](https://github.com/nayuki/QR-Code-generator).

---

Developed for the **M5Stack Paper S3**. Premium aesthetics, powerful performance.
