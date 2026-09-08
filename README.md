# FreeMusicPlayer

Music Player บน Windows เขียนด้วย C++17 + Qt 6.11.1 (Qt Widgets + Qt Multimedia)
รองรับ MP3 / WAV / FLAC / M4A(AAC) / OGG / Opus / WMA / AIFF / WavPack (`.wv`) /
DSD (`.dsf`) และไฟล์เสียงอื่น ๆ ที่ backend FFmpeg ของ Qt Multimedia ถอดรหัสได้

## ฟีเจอร์

- **ปกเพลง (Cover Art)** — ดึงรูปปกจากแท็กในไฟล์โดยตรง รองรับ ID3v2.2/.3/.4
  (MP3), RIFF `id3 ` chunk (WAV), `METADATA_BLOCK_PICTURE` (FLAC), atom
  `covr` (MP4/M4A) และ fallback เป็นไฟล์รูปในโฟลเดอร์เดียวกัน
  (cover/folder/front/album/artwork.jpg/png/webp/bmp) ถ้าไฟล์เพลงไม่มีรูปฝังมา
- **อ่าน/แก้ไขแท็ก (Tag Editor)** — ชื่อเพลง/ศิลปิน/อัลบั้มที่แสดง อ่านจากแท็ก
  จริงในไฟล์ (`TIT2`/`TPE1`/`TALB` ของ ID3v2, `TITLE`/`ARTIST`/`ALBUM` ของ
  Vorbis comment, atom `©nam`/`©ART`/`©alb` ของ MP4) แล้ว fallback ไปใช้
  ชื่อไฟล์ + ชื่อโฟลเดอร์เมื่อไม่มีแท็ก ปุ่ม **"Edit Tag..."** เปิดหน้าต่างแก้
  Title/Artist/Album และเปลี่ยน/ลบรูปปกที่ฝังในไฟล์ได้
  - **เขียนกลับได้**: MP3, WAV, FLAC — เขียนผ่านไฟล์ชั่วคราว (`QSaveFile`)
    แล้วสลับทับของเดิมเมื่อสำเร็จเท่านั้น (ดิสก์เต็ม/สิทธิ์ไม่พอ ไม่ทำให้ไฟล์
    เสียครึ่ง ๆ) และคง frame/field อื่น ๆ ทั้งหมดไว้ (TRACKNUMBER, GENRE,
    DATE, RIFF chunk อื่น ฯลฯ). MP3 ที่เป็น ID3v2.2 จะอ่านได้แต่เขียนกลับเป็น
    ID3v2.3
  - **อ่านอย่างเดียว**: MP4/M4A — การเขียนต้องปรับขนาด atom ซ้อนชั้นและตาราง
    sample-offset ซึ่งเสี่ยงทำไฟล์สื่อเสียหาย จึงยังไม่เปิดให้ Save
- **Playlist** — Add Files / Add Folder, Load/Save เป็น `.m3u8`, Clear,
  ลบหลายรายการพร้อมกัน (multi-select), คลิกขวาเพื่อ Play/Remove/Show in Folder,
  ลากไฟล์จาก Explorer มาวางบนหน้าต่างเพื่อเพิ่มเข้า playlist ได้เลย
- **Seek bar** — คลิกตรงไหนก็ได้บนแถบความคืบหน้าเพื่อกระโดดไปเล่นตรงจุดนั้น
  ทันที (ไม่ต้องลากหัว slider) ลากต่อจากจุดที่คลิกได้ด้วย
- **Sound Visualizer** — วิเคราะห์สเปกตรัมด้วย FFT แบบ log-spaced band เปิด/ปิด
  ได้ มีรูปแบบการวาดให้เลือก **15 แบบ**: Bars, Mirrored Bars, Wave,
  Line Spectrum, Circular, Dots, VU Meter, Particles, Brick Box, Spectrogram,
  Spiral, Ribbon, Orbit, Tunnel, Sunburst — จับคู่กับชุดสีได้ **16 ชุด**:
  Purple, Ocean, Sunset, Neon Green, Hot Pink, Cyan, Fire, Gold, Emerald,
  Lavender, Coral, Ice, Crimson, Amber, Midnight, Lime (เลือกรูปแบบ × ชุดสี
  อิสระต่อกัน)
- **Equalizer** — 10-band graphic EQ (31Hz–16kHz) เปิด/ปิดได้ พร้อมพรีเซ็ต
  สำเร็จรูป **8 แบบ**: Flat, Pop, Rock, Jazz, Classical, Bass Boost,
  Treble Boost, Vocal Boost และปรับเองได้ (จะกลายเป็น "Custom" อัตโนมัติ)
- **Theme** — แท็บ Theme ให้เลือกสีพื้นหลังกับสี accent เอง แล้วเฉดสีอื่น ๆ
  (พาเนล เส้นขอบ ไฮไลต์ตอน hover) จะถูกสร้างต่อจากสองสีนั้นให้อัตโนมัติ กด
  reset กลับเป็นธีมมืดมาตรฐานได้
- **Sleep timer (แท็บ Shutdown)** — ตั้งเวลาถอยหลัง (ชม./นาที/วินาที) เมื่อ
  ครบให้ **ปิดโปรแกรม** อย่างเดียว หรือ **ปิดโปรแกรมแล้วสั่ง Shutdown เครื่อง**
- **สลับตำแหน่งแท็บได้** — ลากแท็บด้วยปุ่มเมาส์ซ้าย (ของ Qt เอง) หรือปุ่มเมาส์
  ขวา (สลับที่กับแท็บที่ลากไปทับ) ลำดับที่จัดไว้จะถูกจำไว้
- **จำค่าที่ตั้งไว้** — volume, mute, shuffle, repeat mode, EQ (เปิด/ปิด +
  พรีเซ็ต/ค่า custom), visualizer (เปิด/ปิด + รูปแบบ + ชุดสี), สีธีม, ลำดับแท็บ,
  ตำแหน่ง/ขนาดหน้าต่าง และ playlist ทั้งหมด จะถูกบันทึกไว้และโหลดกลับมา
  อัตโนมัติในครั้งถัดไป (ไม่ auto-play ตอนเปิดโปรแกรม)

## โครงสร้างโปรเจกต์

```
FreeMusicPlayer/
├── CMakeLists.txt
├── build.bat              ปุ่มเดียวสำหรับ configure+build
├── clean.bat               ลบ build/ เพื่อ reconfigure ใหม่
├── package_portable.bat    รวม build/ เป็นโฟลเดอร์พกพา FreeMusicPlayer-Portable/
├── src/
│   ├── main.cpp
│   ├── MainWindow.*         UI หลักทั้งหมด, ผูก signal/slot ทุกส่วนเข้าด้วยกัน
│   ├── AudioEngine.*        เพลย์แบ็กเอนจิน (ดูหัวข้อสถาปัตยกรรมด้านล่าง)
│   ├── Equalizer.*          10-band EQ + พรีเซ็ต
│   ├── BiquadFilter.h        RBJ peaking biquad filter (ต่อแบนด์ต่อแชนแนล)
│   ├── Visualizer.*          widget วาดสเปกตรัม/คลื่นเสียง (15 รูปแบบ × 16 ชุดสี)
│   ├── FFT.h                 radix-2 FFT แบบ in-place
│   ├── Playlist.*            โมเดล playlist + shuffle/repeat state machine
│   ├── CoverArtExtractor.*   parser ปกเพลงของแต่ละ container (อ่านอย่างเดียว)
│   ├── TagEditor.*           อ่าน/เขียนแท็ก Title/Artist/Album + รูปปก ต่อ container
│   ├── TagEditDialog.*       หน้าต่างโมดัลแก้แท็ก + เปลี่ยน/ลบรูปปก
│   ├── TextDecoder.*         ตัวช่วยอ่านข้อความแท็กเก่า (sniff TIS-620/UTF-8/Latin-1)
│   ├── Settings.*            wrapper รอบ QSettings (รองรับ portable mode)
│   ├── IconFactory.*         วาดไอคอนปุ่มเองด้วย QPainter (ธีมมืด)
│   └── Theme.h                สี + QSS ของทั้งแอป (รองรับพาเลตต์ custom)
└── resources/
    ├── resources.qrc          ไอคอนหน้าต่าง/taskbar ตอนรัน (app.png)
    ├── app.rc                 ไอคอนของตัว .exe (app.ico หลายขนาด 16–256px)
    └── icons/app.ico, app.png
```

## Build

ต้องมี Qt 6.11.1 กับ MinGW kit ที่ `C:\Qt\6.11.1\mingw_64` และคอมไพเลอร์ MinGW
ที่มากับ Qt ที่ `C:\Qt\Tools\mingw1310_64` (ติดตั้งผ่าน `C:\Qt\MaintenanceTool.exe`
เลือก "Desktop (MinGW 64-bit)" component และ **Qt Multimedia** module ด้วย
เพราะเป็น module แยกที่ไม่ได้ติดตั้งมาโดยอัตโนมัติ)

```bat
build.bat
```

รันแล้วได้ `build\FreeMusicPlayer.exe` — สคริปต์รัน `windeployqt` ให้อัตโนมัติ
หลัง build เสร็จ เพื่อคัดลอก Qt DLL/plugin ที่จำเป็นมาไว้ข้าง ๆ .exe

**สำคัญ**: ไลบรารี Qt mingw_64 ที่ build มาสำเร็จรูปนั้นผูกกับคอมไพเลอร์ MinGW
ตัวที่มากับ Qt เท่านั้น (GCC 13.1.0) ถ้าใช้ MinGW ตัวอื่น (WinLibs/MSYS2/TDM-GCC)
จะ link ไม่ผ่านและเจอ error แบบ `undefined reference to '__imp___argc'`
— `build.bat` ระบุ compiler ให้ตรงตัวไว้แล้วเพื่อกันปัญหานี้

ถ้าต้องการเปลี่ยน compiler/generator ใหม่ ให้รัน `clean.bat` ก่อนแล้วค่อยรัน
`build.bat` อีกครั้ง (CMake cache ค่าคอมไพเลอร์ไว้ ต้องลบ `build\` ก่อนเปลี่ยน)

## Portable copy (คัดลอกไปเครื่องอื่นได้เลย ไม่ต้องติดตั้ง Qt)

หลัง `build.bat` สำเร็จแล้ว รัน:

```bat
package_portable.bat
```

จะได้โฟลเดอร์ `FreeMusicPlayer-Portable\` ที่คัดลอกไปเครื่อง Windows เครื่องไหน
ก็ได้ (แฟลชไดรฟ์ ซิป ส่งให้เพื่อน ฯลฯ) แล้วดับเบิลคลิก `FreeMusicPlayer.exe`
ข้างในรันได้ทันที **ไม่ต้องติดตั้ง Qt หรืออะไรเพิ่มเติมบนเครื่องนั้นเลย**

สคริปต์นี้ทำ 2 อย่างที่ `windeployqt` (ซึ่ง `build.bat` รันให้อัตโนมัติอยู่แล้ว)
ทำไม่ครบ:

1. คัดลอกทุกอย่างจาก `build\` (exe, Qt DLL, โฟลเดอร์ plugin เช่น `platforms\`,
   `imageformats\`, `multimedia\`) ไปโฟลเดอร์ใหม่ที่สะอาด โดยไม่เอาไฟล์
   ภายในของ CMake เอง (`CMakeFiles\`, `Makefile`, `*.cmake` ฯลฯ) ติดไปด้วย
2. เพิ่ม MinGW runtime DLL 3 ตัวที่ `windeployqt` **ไม่** คัดลอกให้ —
   `libgcc_s_seh-1.dll`, `libstdc++-6.dll`, `libwinpthread-1.dll` — ถ้าขาด
   3 ตัวนี้ โปรแกรมจะเปิดไม่ขึ้นเลยบนเครื่องที่ไม่มี MinGW ตัวเดียวกันติดตั้งอยู่

สคริปต์ตรวจสอบให้ด้วยว่าไฟล์ที่จำเป็น (exe, `platforms\qwindows.dll`, DLL
รันไทม์ 3 ตัวข้างต้น) อยู่ครบก่อนบอกว่าเสร็จ ถ้าขาดตัวไหนจะแจ้งเตือนและ
**ไม่** ให้ถือว่า package สำเร็จ

ตั้งค่าทั้งหมด (volume, playlist, EQ, ธีม, ลำดับแท็บ, ตำแหน่งหน้าต่าง ฯลฯ)
ถูกบันทึกเป็น `FreeMusicPlayer.ini` อยู่ข้าง ๆ ตัว .exe เอง (ดู `Settings.*`)
ไม่ใช่ใน Windows registry อยู่แล้วตั้งแต่แรก ดังนั้นย้าย/คัดลอกโฟลเดอร์
`FreeMusicPlayer-Portable\` ไปที่ไหนก็ตาม ค่าที่ตั้งไว้จะติดไปด้วยเสมอ และจะ
ไม่ไปยุ่งกับอะไรนอกโฟลเดอร์นี้เลย (ยกเว้นกรณีโฟลเดอร์ปลายทางเป็น read-only เช่น
`C:\Program Files\...` หรือแผ่น optical disc — กรณีนั้นแอปจะ fallback ไปใช้
registry แทนโดยอัตโนมัติ)

## สถาปัตยกรรมเพลย์แบ็ก (ทำไมถึงมี "หน่วง" สั้น ๆ ตอนเปลี่ยนเพลง)

`QMediaPlayer` ของ Qt Multimedia ไม่มีจุดให้แทรก audio effect ใด ๆ เข้าไปใน
pipeline เลย ทำให้ใส่ Equalizer ไม่ได้ถ้าใช้ class นี้ตรง ๆ โปรแกรมนี้จึงใช้
แนวทาง **decode-to-memory** แทน:

```
QAudioDecoder → PCM float ทั้งเพลงในหน่วยความจำ → Equalizer (biquad ต่อแบนด์) → QAudioSink
                                                          ↑
                                              Visualizer อ่านข้อมูล PCM ดิบ (ก่อน EQ)
                                              จากบัฟเฟอร์เดียวกันนี้แบบ read-only
```

ข้อดี/ข้อแลกเปลี่ยนที่ควรรู้:

- **เปลี่ยนเพลงมีหน่วงสั้น ๆ** เพราะต้องถอดรหัสทั้งไฟล์ก่อนเล่นได้ (ปุ่ม Play
  จะกดได้หลังถอดรหัสเสร็จเท่านั้น — เป็นการตัดสินใจออกแบบเพื่อเลี่ยงบั๊ก
  race-condition ที่ซับซ้อนจากการเล่นไฟล์ที่ยังถอดรหัสไม่เสร็จ)
- **ใช้หน่วยความจำ ~10MB ต่อเพลง 1 นาที** (float 32-bit, stereo) แลกกับการ
  **seek ได้ทันที** ไม่มีดีเลย์ เพราะ `QAudioDecoder` เองไม่รองรับการ seek
- **Visualizer แสดงสัญญาณเสียงต้นฉบับ (ก่อนปรับ EQ)** ไม่ใช่เสียงหลัง EQ ที่
  ส่งออกลำโพงจริง ๆ เพื่อเลี่ยงการรัน filter chain ซ้ำสองชุดพร้อมกัน

## Thread safety

- ตำแหน่งเล่นปัจจุบัน (`m_frameCursor`), mute, และ end-of-track flag เป็น
  `std::atomic` เพราะถูกอ่าน/เขียนข้าม UI thread กับ audio thread ของ
  `QAudioSink`
- ค่าสัมประสิทธิ์ EQ และ delay-line state ป้องกันด้วย `QMutex` ใน `Equalizer`
  — ล็อกครั้งเดียวต่อ audio block (ไม่ใช่ต่อ sample) ฝั่ง `AudioEngine::pullAudio()`
  และทุกจุดที่ UI thread แก้ค่า gain/preset ก็ล็อกก่อนแก้เสมอ

## การตรวจสอบความถูกต้องของอัลกอริทึมหลัก

อัลกอริทึม DSP / parser ที่ละเอียดอ่อนถูกพอร์ตไปรันเทียบใน Python ก่อนแล้วค่อย
พอร์ตมาเป็น C++ ตามที่ยืนยันว่าถูกต้อง:

- **Biquad peaking EQ**: gain ที่ความถี่กลางตรงตามที่ตั้งไว้ (คลาดเคลื่อน
  ~1e-12 dB), pole อยู่ในวงกลมหนึ่งหน่วยเสมอ (เสถียร ไม่มีทาง blow up),
  0dB = unity ตรงเป๊ะทุกความถี่
- **FFT**: เทียบกับ `numpy.fft` ตรงกันถึง ~1e-12, หา bin ของ test tone
  1kHz เจอตรงตำแหน่งที่คำนวณไว้
- **ตัวอ่าน/เขียนแท็ก**: สร้างไฟล์ ID3v2.2/.3/.4 (รวม extended header, UTF-16
  description), RIFF/WAV `id3 ` chunk (รวมกรณี odd-size chunk padding),
  FLAC picture/Vorbis-comment block, MP4 `covr` atom จำลองขึ้นมาแล้ว parse
  ผ่านทุกเคส — และตรวจว่าการเขียนกลับคง frame/field อื่นไว้ครบ
- **ตัว sniff ข้อความเก่า (TIS-620)**: ตรวจพบข้อความไทยจริงถูกต้อง และ
  **ไม่** เข้าใจผิดว่าชื่อแบบ Björk / Sigur Rós / Motörhead เป็นภาษาไทย
  (เช็คว่าต้องมี byte สูงติดกันอย่างน้อย 2 ตัว ไม่ใช่แค่ byte สูงตัวเดียว)
- **Shuffle/Repeat state machine**: จำลอง 500+ รอบแบบสุ่ม ยืนยันว่า
  shuffle+repeat-all เล่นครบทุกเพลงต่อรอบไม่ซ้ำติดกัน, shuffle+repeat-off
  หยุดพอดีตอนเล่นครบ ไม่เล่นวนไม่รู้จบ
- **การลบเพลงหลายรายการพร้อมกัน**: จำลอง 1000+ รอบแบบสุ่ม (ลบก่อน/หลัง/ที่
  ตำแหน่งเพลงปัจจุบัน, ลบไม่ต่อเนื่อง, ลบทั้งหมด) ยืนยันว่า index เพลงที่
  กำลังเล่นอยู่ยังชี้ไปที่เพลงเดิมถูกต้องเสมอ

สคริปต์ตรวจสอบเหล่านี้ไม่ได้รวมมาด้วย (เป็นเครื่องมือช่วยตรวจตอนพัฒนา ไม่ใช่
ส่วนหนึ่งของแอป) แต่ตรรกะที่ผ่านการตรวจสอบแล้วถูกพอร์ตมาเป็น C++ ตรงนี้ทั้งหมด

## ข้อจำกัดที่รู้อยู่แล้ว

- **แก้แท็กของ MP4/M4A ไม่ได้** — อ่าน Title/Artist/Album + รูปปกได้ แต่ยังไม่
  เปิดให้ Save (การปรับขนาด atom ซ้อนชั้น + sample-offset table เสี่ยงทำไฟล์
  เสียหาย); MP3 แบบ ID3v2.2 เขียนกลับเป็น ID3v2.3
- CoverArtExtractor สำหรับ MP4/M4A อ่านเฉพาะ `covr` (ปกเพลง) ไม่ได้อ่าน
  metadata อื่นในนั้นผ่าน path เดียวกัน (ตัวข้อความชื่อเพลง/ศิลปินใช้
  `TagEditor` แยกต่างหาก)
- ยังไม่รองรับ gapless playback ระหว่างเพลง (มีดีเลย์สั้น ๆ ตอนเปลี่ยนเพลง
  ตามที่อธิบายไว้ในหัวข้อสถาปัตยกรรมด้านบน)
