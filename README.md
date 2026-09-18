# FreeMusicPlayer

Music Player บน Windows เขียนด้วย C++17 + Qt 6.11.1 (Qt Widgets + Qt Multimedia)
รองรับ MP3 / WAV / FLAC / M4A (AAC และ ALAC/Apple Lossless) / `.m4b` / `.alac` /
`.caf` / OGG / Opus / WMA / AIFF / WavPack (`.wv`) / DSD (`.dsf`) และไฟล์เสียง
อื่น ๆ ที่ backend FFmpeg ของ Qt Multimedia ถอดรหัสได้ ไฟล์ ALAC ในกล่อง MP4/CAF
ตรวจจับ codec จริงในคอนเทนเนอร์ จึงแสดงบรรทัดฟอร์แมตเป็น `ALAC` ไม่ใช่ `M4A` ลอย ๆ

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
  ลากไฟล์จาก Explorer มาวางบนหน้าต่างเพื่อเพิ่มเข้า playlist ได้เลย — เพลงที่
  โหลดอยู่จริง (ไม่ใช่แค่แถวที่คลิกเลือกไว้) มีไอคอนรูปลำโพงกำกับหน้าชื่อไฟล์
  เสมอ ไม่ว่าจะ pause/seek อยู่ หรือคลิกเลือกแถวอื่นดูอยู่ก็ตาม
  - **Add ISO...** — แปลงเพลงจากไฟล์ `.iso` เป็น `.flac` แล้วเพิ่มเข้า playlist
    อัตโนมัติ เลือกไฟล์ `.iso` แล้วเริ่มแปลงทันที **ไม่ถามว่าจะเก็บไฟล์ที่ไหน**
    — สร้างโฟลเดอร์ชื่อเดียวกับไฟล์ `.iso` ไว้ข้าง ๆ ไฟล์ต้นฉบับให้เองเสมอ
    (เช่น `Some Album.iso` → โฟลเดอร์ `Some Album\` ข้าง ๆ กัน ตามธรรมเนียม
    โปรแกรม rip แผ่นทั่วไป) รองรับ 2 แบบ: **SACD ISO** (อ่าน 2-channel area ตรงจาก
    ScarletBook TOC ของแผ่น, demux DSD ดิบทีละ sector/packet เอง, ห่อเป็น
    `.dsf` ชั่วคราวแล้วถอดรหัสผ่าน `QAudioDecoder`/FFmpeg ตัวเดียวกับที่เล่น
    `.dsf` ปกติ, สุดท้ายเข้ารหัส FLAC เอง — **เพลงที่เป็น DST-compressed
    ถูกข้ามพร้อมแจ้งเตือน ไม่รองรับ** เพราะ DST เป็นสเปกที่ซับซ้อนเกินกว่าจะ
    เขียนเองแล้วมั่นใจว่าถูกต้องโดยไม่มีไฟล์ทดสอบจริงมา verify, ส่วน
    multichannel area (MULCHTOC) ไม่อ่านเลย ใช้เฉพาะ 2-channel) และ **CD-DA
    image ดิบ** (แยกแทร็กจากไฟล์ `.cue` ข้าง ๆ ถ้ามี ไม่มีก็ถือทั้งไฟล์เป็น
    แทร็กเดียว) ทั้ง FLAC encoder (fixed predictor + Rice coding, ไม่พึ่ง
    libFLAC) และ DSF writer เขียนขึ้นเองทั้งหมด ไม่มีการเรียกโปรแกรมภายนอก
    เลย — verify แล้วด้วยการเข้ารหัสแล้วถอดกลับผ่าน `QAudioDecoder` จริงของ
    แอปเอง เทียบ sample ต้นทาง/ปลายทางตรงกัน (ดูหัวข้อสถาปัตยกรรมด้านล่าง)
- **Seek bar** — คลิกตรงไหนก็ได้บนแถบความคืบหน้าเพื่อกระโดดไปเล่นตรงจุดนั้น
  ทันที (ไม่ต้องลากหัว slider) ลากต่อจากจุดที่คลิกได้ด้วย
- **Sound Visualizer** — วิเคราะห์สเปกตรัมด้วย FFT แบบ log-spaced band เปิด/ปิด
  ได้ มีรูปแบบการวาดให้เลือก **15 แบบ**: Bars, Mirrored Bars, Wave,
  Line Spectrum, Circular, Dots, VU Meter, Particles, Brick Box, Spectrogram,
  Spiral, Ribbon, Orbit, Tunnel, Sunburst — จับคู่กับชุดสีได้ **17 ชุด**:
  Purple, Ocean, Sunset, Neon Green, Hot Pink, Cyan, Fire, Gold, Emerald,
  Lavender, Coral, Ice, Crimson, Amber, Midnight, Lime และ **Rainbow**
  (ไล่เฉดสีรุ้งทีละ band แดง→ม่วง แทนคู่สีหลัก/รอง) — เลือกรูปแบบ × ชุดสี
  อิสระต่อกัน
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
│   ├── Visualizer.*          widget วาดสเปกตรัม/คลื่นเสียง (15 รูปแบบ × 17 ชุดสี)
│   ├── FFT.h                 radix-2 FFT แบบ in-place
│   ├── Playlist.*            โมเดล playlist + shuffle/repeat state machine
│   ├── CoverArtExtractor.*   parser ปกเพลงของแต่ละ container (อ่านอย่างเดียว)
│   ├── TagEditor.*           อ่าน/เขียนแท็ก Title/Artist/Album + รูปปก ต่อ container
│   ├── TagEditDialog.*       หน้าต่างโมดัลแก้แท็ก + เปลี่ยน/ลบรูปปก
│   ├── TextDecoder.*         ตัวช่วยอ่านข้อความแท็กเก่า (sniff TIS-620/UTF-8/Latin-1)
│   ├── Settings.*            wrapper รอบ QSettings (รองรับ portable mode)
│   ├── IconFactory.*         วาดไอคอนปุ่มเองด้วย QPainter (ธีมมืด)
│   ├── Theme.h                สี + QSS ของทั้งแอป (รองรับพาเลตต์ custom)
│   ├── IsoAudioExtractor.*   อ่าน SACD ISO / CD-DA image, demux เป็น PCM/DSD ดิบ
│   ├── DsfWriter.*           เขียน .dsf ชั่วคราวจาก DSD ที่ demux มา (ดูหัวข้อ Add ISO)
│   ├── FlacEncoder.*         FLAC encoder เขียนเอง (fixed predictor + Rice coding)
│   └── IsoImportWorker.*     QThread worker คุม pipeline ทั้งก้อนของปุ่ม Add ISO
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

**Build type**: `CMakeLists.txt` default เป็น `Release` (`-O3`) เองถ้าไม่ได้
สั่งอย่างอื่นมา (`if(NOT CMAKE_BUILD_TYPE) ... FORCE`) — ตั้งใจไว้แบบนี้เพราะ
`build.bat`'s `cmake --build ... --config Release` เป็น no-op สำหรับ
generator "MinGW Makefiles" (ใช้ได้เฉพาะ multi-config generator อย่าง Visual
Studio/Ninja Multi-Config) ค่า optimization จริง ๆ มาจาก `CMAKE_BUILD_TYPE`
ตอน configure เท่านั้น ก่อนแก้จุดนี้ (2026-09-18) `CMAKE_BUILD_TYPE` ไม่เคยถูก
set มาก่อนเลย ทำให้ทั้งแอป — ไม่ใช่แค่ฟีเจอร์ Add ISO — build แบบไม่ optimize
(`-O0`) มาโดยตลอด ตัว "ISO to FLAC แปลงช้ามาก" ที่ผู้ใช้เจอคือจุดที่ทำให้สังเกต
เห็นปัญหานี้ (Rice-coding เป็น tight arithmetic loop ที่ -O0 กับ -O3 ต่างกัน
มาก) แต่จริง ๆ กระทบทุก path ที่กิน CPU หนักในแอป (FFT, biquad EQ,
visualizer) เท่า ๆ กัน ถ้าต้องการ build แบบ Debug (มี symbol ให้ debug) ให้
สั่ง `-DCMAKE_BUILD_TYPE=Debug` ตอน configure เอง (จะ override ค่า default นี้)

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
  - **DSF / FLAC / WAV หน่วงเพิ่มอีก 5 วินาที** หลังถอดรหัสเสร็จ ก่อนเริ่มเล่น
    เสียงจริง (เฉพาะ 3 นามสกุลนี้ — format อื่นเริ่มเล่นทันทีตามปกติ) เผื่อเวลา
    ให้ output device/DAC relock sample rate หรือ bit depth ใหม่ ไม่ให้เสียง
    วูบหรือแตกในช่วงแรกของเพลง สลับเพลงระหว่างรอ (Next/Prev) จะยกเลิก delay
    ที่ค้างอยู่ ไม่เล่นเพลงเก่าซ้อน
  - **เคอร์เซอร์เป็นรูป Loading (busy cursor) จน "มีเสียงออกจริง" ไม่ใช่แค่จน
    ถอดรหัสเสร็จ** — ครอบคลุมทั้งช่วง `Loading` (ถอดรหัส) และช่วง delay ข้างบน
    (state ยังเป็น `Loading` ตลอดสองช่วงนี้) แต่ต่อให้ state เปลี่ยนเป็น
    `Playing` แล้ว ปุ่ม `play()` ก็แค่เรียก `QAudioSink::start()` — ยังไม่ได้
    ยืนยันว่า sink เริ่ม pull ข้อมูลจริงหรือยัง จึงเก็บ cursor ไว้ต่ออีกนิดจนกว่า
    `QAudioSink::stateChanged` จะรายงาน `QAudio::ActiveState` จริง (signal
    `AudioEngine::audioActive()`) ค่อยคืนเคอร์เซอร์ปกติ — กด pause/resume มือ
    เองจะไม่โดน cursor busy ซ้ำ (ไม่เคย set busy จาก state `Paused` อยู่แล้ว) มี
    safety net ที่ `onEngineError()` กันเคส output error ทำให้ cursor ค้างค้าง
    ไปตลอดด้วย
  - **ปุ่ม Play/Pause เปลี่ยนเป็นไอคอน Pause ทันทีที่ double-click/Next/Prev**
    (ไม่ต้องรอถอดรหัส/settle delay เสร็จก่อน) — `updatePlayPauseIcon()` เดิม
    ผูกกับ `AudioEngine::State::Playing` ล้วน ๆ ซึ่งกว่าจะถึง state นั้นต้องรอ
    ถอดรหัส + delay (สำหรับ DSF/FLAC/WAV) เสร็จก่อน ทำให้ปุ่มโชว์ไอคอน Play
    ค้างอยู่หลายวินาทีทั้งที่ผู้ใช้กด "เล่น" ไปแล้ว `MainWindow::playIndex()`
    (จุดรวมของ double-click/Next/Prev/context-menu Play/auto-advance ทุกทาง)
    จึง set ไอคอน Pause ไว้ล่วงหน้าทันทีเมื่อ `autoPlay=true` — ถ้าถอดรหัส
    ล้มเหลว (state กลับไป `Stopped`) หรือเป็นการโหลดแบบไม่ auto-play ไอคอนก็ยัง
    ถูกคืนกลับเป็น Play ให้ถูกต้องผ่าน `onEngineStateChanged()` ตามปกติ
  - **แก้บั๊ก: ไอคอนกระพริบ Play → Pause → Play → Pause ก่อนเพลงจะเล่นจริง**
    (พบตอน double-click ไฟล์ DSF/FLAC/WAV เพราะ 3 นามสกุลนี้มี settle delay
    ยาว 5 วินาทีให้เห็นบั๊กชัด) ต้นเหตุคือ `QAudioDecoder::finished()` ยิง
    signal ซ้อนกัน 2 ครั้งต่อการถอดรหัส 1 รอบ (ยืนยันด้วย debug log) — สาเหตุคือ
    `AudioEngine::onDecoderFinished()` เดิมเรียก `m_decoder->setSource(QUrl())`
    (คืน handle ไฟล์ให้ TagEditor แก้ tag ได้) ตั้งแต่ต้นฟังก์ชัน ซึ่งการ set
    source เป็นค่าว่างขณะ decoder อยู่ใน state "finished" ทำให้ Qt ยิง
    `finished()` ซ้ำแบบ synchronous กลับเข้ามาเรียก `onDecoderFinished()` ซ้อน
    ตัวเองอีกรอบก่อนที่ call แรกจะ return — รอบที่ซ้อนเข้ามานี้ดันไปกิน
    `m_pendingAutoPlay` ทิ้งก่อน (set เป็น false, ตั้ง timer settle delay)
    พอ call แรกกลับมาทำงานต่อแล้วมาเช็ค `m_pendingAutoPlay` ก็เจอค่า false
    ที่ถูกกินไปแล้ว เลยตกไปที่ branch `else setState(Paused)` ทำให้ไอคอนเด้ง
    กลับเป็น Play ค้างอยู่จนกว่า timer settle delay จะยิง `play()` จริง ๆ ตอน
    5 วินาทีผ่านไป — แก้โดยย้าย `setSource(QUrl())` ไปไว้ท้ายสุดของฟังก์ชัน
    (หลังจาก `m_ready = true` แล้ว) พร้อม guard `if (m_ready) return;` ที่ต้น
    ฟังก์ชัน ทำให้ call ที่ซ้อนเข้ามาจาก `setSource()` เจอ guard แล้ว no-op
    ทันทีแทนที่จะรันทับซ้อนอีกรอบ
- **ใช้หน่วยความจำ ~10MB ต่อเพลง 1 นาที** (float 32-bit, stereo) แลกกับการ
  **seek ได้ทันที** ไม่มีดีเลย์ เพราะ `QAudioDecoder` เองไม่รองรับการ seek
- **Visualizer แสดงสัญญาณเสียงต้นฉบับ (ก่อนปรับ EQ)** ไม่ใช่เสียงหลัง EQ ที่
  ส่งออกลำโพงจริง ๆ เพื่อเลี่ยงการรัน filter chain ซ้ำสองชุดพร้อมกัน

## สถาปัตยกรรมการ Import ISO (ปุ่ม "Add ISO...")

```
.iso ──> IsoAudioExtractor::open()          sniff SACDMTOC (LSN 510) / CD001 / .cue ข้างไฟล์
           │                                 → รายชื่อแทร็ก + ตำแหน่ง (LSN หรือ byte range)
           ▼
     extractTrackToFlac() ต่อแทร็ก
           │
   ┌───────┴────────┐
   │ SACD (DSD)      │ CD-DA (PCM)
   ▼                 ▼
 demux sector/packet  อ่าน byte range ตรง ๆ
 ตาม audio_sector_t   (16-bit/44.1kHz stereo)
   │
   ▼
 DsfWriter → .dsf ชั่วคราว (bit-reversal + de-interleave)
   │
   ▼
 QAudioDecoder/FFmpeg ตัวเดียวกับที่เล่น .dsf ปกติ → PCM float
           │
           ▼
     FlacEncoder::encode() → .flac จริง → เพิ่มเข้า playlist
```

รันทั้งหมดใน `IsoImportWorker` บน `QThread` แยก (ผ่าน `moveToThread`) ไม่บล็อก
UI — ต้องมี event loop ของ thread เดินอยู่เพราะ `QAudioDecoder` ข้างในถูกใช้
แบบ synchronous (ผูกกับ `QEventLoop` ท้องถิ่นรอ signal `finished`)

**Progress ระหว่างแปลง**: หน้าต่าง progress แสดงทั้งภาพรวม (แท็กที่เท่าไหร่/
ทั้งหมดกี่แท็ก, bar หนึ่งขั้นต่อหนึ่งแท็กที่แปลงเสร็จ) และความคืบหน้าภายใน
แท็กที่กำลังแปลงอยู่แบบ real-time (`ReadingSacdAudio`/`WritingDsf`/
`DecodingDsd`/`ReadingCdda`/`EncodingFlac` — ดู `IsoAudioExtractor::
ProgressPhase`) พร้อม % ของ phase นั้น เช่น "Track 3 of 10: ชื่อเพลง -
Encoding FLAC (57%)" — เปอร์เซ็นต์คำนวณจริงจากจำนวน sector/sample ที่
ประมวลผลแล้ว ไม่ใช่ค่าประมาณ, ตัวเลขทั้งหมดบังคับเป็นเลขอารบิก (`QLocale::c()`
บน progress dialog) ไม่ให้กลายเป็นเลขไทยตาม system locale

**ทำไมไม่พึ่งโปรแกรม/ไลบรารีภายนอก**: ทั้ง DSF container writer และ FLAC
encoder เขียนขึ้นเองทั้งหมดในโปรเจกต์ (`DsfWriter.*`, `FlacEncoder.*`) ตาม
philosophy เดียวกับ `FFT.h`/`BiquadFilter.h` ที่มีอยู่แล้ว — ไม่ต้องพึ่ง
`ffmpeg.exe`/`flac.exe` แยกที่ผู้ใช้ต้องติดตั้งเอง ก่อนใช้งานจริงมีการ
เข้ารหัสแล้วถอดกลับผ่าน `QAudioDecoder` ของแอปเองเทียบ sample ต้นทาง/ปลายทาง
(รวมถึงสร้าง SACD ISO สังเคราะห์ทดสอบ pipeline เต็มทาง TOC parsing → demux →
DSF → decode → FLAC) ยืนยันว่า bitstream ถูกต้องจริงก่อนใช้งาน — ไม่ใช่แค่
compile ผ่านแล้วเดาว่าถูก

**สิ่งที่ตั้งใจไม่รองรับ** (ดูรายละเอียดเหตุผลในหัวข้อ "ข้อจำกัดที่รู้อยู่แล้ว"
ด้านล่าง): SACD track ที่เป็น DST-compressed, SACD multichannel area
(MULCHTOC), FLAC output เป็น verbatim/fixed-predictor + Rice coding เท่านั้น
(ไม่มี LPC/stereo decorrelation แบบ `flac -8`) — ไฟล์ที่ได้เล็กกว่า WAV จริง
แต่ไม่ได้บีบอัดสุดเหมือน libFLAC

**ความเร็ว**: การเลือก fixed-predictor order + Rice parameter ใช้วิธี
ประมาณค่าที่ถูกก่อน (sum-of-absolute-residual สำหรับ order, มาตรฐานเดียวกับที่
libFLAC's `estimate_best_order()` ใช้ / estimate-แล้ว-refine ±1 สำหรับ Rice
parameter) แทนการลองครบทุกค่าแล้วเทียบ (5 order × 15 ค่า Rice parameter ต่อ
subframe แบบเดิม) — ลด full-array pass ต่อ subframe จาก ~75 ครั้งเหลือ ~14
ครั้ง โดยผลลัพธ์ยัง valid/lossless เท่าเดิมทุกกรณี (ต่างแค่บีบอัดได้ไม่สุดในบาง
เคสที่หายากมาก) วัดจริง: เข้ารหัส 5 นาที 44.1kHz/16-bit ได้ใน ~2 วินาที (build
Release) — ดูหัวข้อ Build ด้านบนสำหรับอีกสาเหตุหลักที่ทำให้การแปลงช้า
(`CMAKE_BUILD_TYPE` ไม่เคย set มาก่อน)

**Cancel หยุดทันที**: ปุ่ม Cancel ใน progress dialog เดิม (2026-09-18 ก่อนแก้)
เชื่อม signal `canceled` เข้ากับ `IsoImportWorker::cancel()` ด้วย
`Qt::AutoConnection` (default) ซึ่งข้าม thread จริง Qt จะ resolve เป็น
**queued** เสมอ — แปลว่า `cancel()` จะไม่ถูกเรียกจนกว่า event loop ของ worker
thread จะได้ spin อีกครั้ง ซึ่งระหว่าง `demuxTrackAudio()`/
`FlacEncoder::encode()` (เป็น loop C++ ธรรมดา ไม่มี event loop ของตัวเอง) ไม่มี
จังหวะให้ spin เลยจนกว่าทั้ง track (หรือทั้ง import) จะเสร็จไปเอง — กด Cancel
แล้วเหมือนไม่มีอะไรเกิดขึ้นจนกว่างานจะเสร็จตามปกติ แก้โดย:
1. ใส่ `Qt::DirectConnection` ตรง ๆ ให้ `connect()` เส้นนี้ — `cancel()` แค่
   flip `std::atomic_bool` เดียว ปลอดภัยที่จะเรียกตรง ๆ ข้าม thread แบบ
   synchronous (ไม่เหมือน slot ทั่วไปที่อาจแตะ Qt object ที่ไม่ thread-safe)
2. เพิ่ม `isCancelled` callback เข้าไปทุก loop ที่กินเวลานาน —
   `demuxTrackAudio()` เช็คทุก sector, `FlacEncoder::encode()` เช็คทุก block
   (4096 samples) และระหว่างคำนวณ MD5 (แยก chunk ทีละ ~1M samples แทนแฮชรวด
   เดียวทั้งไฟล์), `decodeToInt32Pcm()` เช็คผ่าน `QTimer` poll ทุก 100ms
   ระหว่างรอ `QAudioDecoder` (เพราะจุดนั้นไม่มี loop ของเราให้เช็คตรง ๆ)
3. ไฟล์ผลลัพธ์ที่เขียนไปแล้วบางส่วน (`.flac`, `.dsf` ชั่วคราว) ถูกลบทิ้งเมื่อ
   cancel กลางทาง ไม่เหลือไฟล์เสีย/ไม่สมบูรณ์ค้างไว้

ทดสอบด้วยการ cancel กลางไฟล์ 10 นาทีจริง — ยืนยันว่าหยุดใน < 1 วินาที (ไม่ใช่
รอจนกว่า track จะ encode ครบตามปกติ)

## Thread safety

- ตำแหน่งเล่นปัจจุบัน (`m_frameCursor`), mute, และ end-of-track flag เป็น
  `std::atomic` เพราะถูกอ่าน/เขียนข้าม UI thread กับ audio thread ของ
  `QAudioSink`
- ค่าสัมประสิทธิ์ EQ และ delay-line state ป้องกันด้วย `QMutex` ใน `Equalizer`
  — ล็อกครั้งเดียวต่อ audio block (ไม่ใช่ต่อ sample) ฝั่ง `AudioEngine::pullAudio()`
  และทุกจุดที่ UI thread แก้ค่า gain/preset ก็ล็อกก่อนแก้เสมอ
- `IsoImportWorker` รันทั้ง pipeline บน `QThread` ของตัวเอง (ดูหัวข้อ
  Import ISO ด้านบน) — `cancel()` เป็น `std::atomic_bool` เรียกข้าม thread ได้
  ตรง ๆ **แต่ต้องระบุ `Qt::DirectConnection` ตรง ๆ ตอน `connect()`** (ปล่อยเป็น
  default `Qt::AutoConnection` จะกลาย เป็น queued ข้าม thread เสมอ ทำให้
  `cancel()` ไม่ถูกเรียกจนกว่า worker thread's event loop จะได้ spin — ดูหัวข้อ
  "Cancel หยุดทันที" ด้านบน) ส่วน signal ความคืบหน้าทุกตัวยังส่งกลับ UI thread
  ผ่าน queued connection ตามปกติของ Qt (ไม่ต้องรีบ เพราะแค่ update ตัวเลข
  ไม่ใช่ต้องหยุดงานทันทีแบบ cancel)

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
- **FlacEncoder / DsfWriter / IsoAudioExtractor** (ปุ่ม Add ISO): เข้ารหัส
  สัญญาณสังเคราะห์ (ไซน์เวฟหลายความถี่ + ช่วงเงียบ, ทั้ง 44.1/88.2kHz,
  16/24-bit, mono/stereo, ความยาวคี่ที่ตัด block สุดท้ายไม่พอดี) แล้วถอดกลับ
  ผ่าน `QAudioDecoder` จริงของแอป เทียบ sample ต้นทาง/ปลายทางคลาดเคลื่อนไม่
  เกิน ~1 LSB (rounding เท่านั้น ไม่ใช่ bug); DsfWriter ตรวจ byte ของ header/
  payload ตรงกับค่าที่คำนวณมือทุก byte, ยืนยันด้วยว่า FFmpeg ยอมรับ
  `block_size_per_channel` ที่ไม่ใช่ 4096 (ค่าที่ IsoAudioExtractor ใช้จริง
  คือ 4704 ตามขนาดเฟรมธรรมชาติของ SACD); IsoAudioExtractor ทดสอบ end-to-end
  ด้วย SACD ISO สังเคราะห์ (Master TOC + Area TOC + SACDTRL1 + audio sector
  จริงตามสเปก) และ CD-DA image + `.cue` สังเคราะห์ ผ่านทั้ง `open()` และ
  `extractTrackToFlac()` เหมือนที่ UI เรียกจริง; progress callback (ทุก
  `ProgressPhase`) ตรวจว่าค่า % เริ่มที่ 0 จบที่ 100 และไม่มีทางลดลงระหว่างทาง
  ด้วย; หลังเปลี่ยนมาใช้ order/Rice-parameter แบบประมาณค่า (ดูหัวข้อ "ความเร็ว"
  ด้านบน) เข้ารหัสซ้ำแล้วถอดกลับเทียบ sample อีกรอบ — ผลตรงกันทุก sample
  (`max abs error: 0.0`, ไม่ใช่แค่ในทน tolerance เหมือนตอนแรก) ยืนยันว่าการลด
  จำนวนค่าที่ลองไม่ได้ทำให้ bitstream เพี้ยน

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
- **Add ISO ไม่รองรับ SACD track ที่เป็น DST-compressed** (ตรวจพบแล้วข้าม
  พร้อมแจ้งเตือนต่อแทร็ก ไม่ใช่แปลงออกมาเพี้ยน) — DST เป็น lossless codec
  เฉพาะของ SACD ที่ซับซ้อนมาก (predictive + arithmetic coding) การเขียน
  decoder เองโดยไม่มีไฟล์ทดสอบจริงมา verify มีความเสี่ยงสูงเกินไปที่จะเชื่อถือ
  ได้ ส่วน uncompressed DSD track เล่น/แปลงได้ปกติ
- **Add ISO อ่านเฉพาะ SACD 2-channel area** — multichannel area (MULCHTOC)
  ไม่ถูกอ่านเลย เพื่อเลี่ยงต้องตัดสินใจเรื่อง downmix ลง stereo เอง
- **FLAC ที่ Add ISO สร้างไม่ได้บีบอัดสูงสุด** — ใช้ fixed predictor (order
  0-4) + Rice coding แบบ partition เดียวต่อ subframe เท่านั้น ไม่มี LPC
  (linear predictor) หรือ stereo decorrelation แบบที่ `flac -8` ใช้ — ไฟล์
  เล็กกว่า WAV จริงแต่ใหญ่กว่า FLAC ที่เข้ารหัสด้วย libFLAC ระดับสูงสุด
