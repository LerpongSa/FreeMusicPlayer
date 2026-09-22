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
  - **ทุกจุดที่ล้าง playlist ทิ้ง (ปุ่ม Clear, Load Playlist, หรือ Add ISO ที่
    เคลียร์ playlist เดิมก่อนเริ่มแปลง — ดูหัวข้อ Add ISO ด้านล่าง) หยุดเพลงที่
    กำลังเล่นอยู่ด้วยเสมอ** เรียก `m_engine->stop()` ก่อน `m_playlist->clear()`
    ทุกครั้ง — ไม่งั้นแถวของเพลงที่กำลังเล่นหายไปจาก playlist แล้ว แต่ตัวเสียง
    ยังเล่นค้างต่อไปเฉย ๆ (ปุ่ม Clear เดิมมี `stop()` อยู่แล้ว แต่ Load Playlist
    กับ Add ISO ไม่เคยมี จนผู้ใช้แจ้งเป็นบั๊ก 2026-09-19)
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
    **playlist เดิม (ถ้ามี) จะถูกเสนอให้ Save ก่อนเปิด dialog เลือกไฟล์ `.iso`
    ด้วยซ้ำ** — `promptSaveCurrentPlaylist()` เปิด dialog "Save Playlist" แบบ
    เดียวกับปุ่มบน toolbar ให้ก่อนเสมอถ้า playlist เดิมไม่ว่าง แต่เป็นแค่ทางเลือก
    กันลืมเซฟ ไม่ใช่ gate: ปิด dialog นั้นยังไงก็ตาม (Save จริงหรือ Cancel) ก็ไป
    เปิด dialog เลือกไฟล์ `.iso` ต่อทันที **ส่วน Clear playlist เดิมทิ้งจริง ๆ
    ยังผูกกับตอนเลือกไฟล์ `.iso` สำเร็จแล้วเท่านั้น** — ถ้า cancel ตอนเลือกไฟล์
    `.iso` (ไม่ได้เลือกเลย) playlist เดิมจะไม่ถูก Clear เลย ต่อให้เพิ่งกด Cancel
    ตอน Save Playlist ไปก่อนหน้านี้ก็ตาม (ยืนยันกับผู้ใช้แล้ว 2026-09-19) เมื่อ
    เลือกไฟล์ `.iso` สำเร็จแล้ว ก็ Clear + เริ่มแปลงต่อทันที ไม่ว่าจะเคย Save
    Playlist จริงหรือ Cancel ไปตอนต้นก็ตาม เพราะแทร็กที่แปลงจาก ISO จะกลายเป็น
    playlist ใหม่ล้วน ๆ ไม่ใช่ต่อท้ายของเดิม **ตอน Clear playlist เดิมนี้ ปกเพลง
    กับ Visualizer จะถูกรีเซ็ตกลับเป็นค่าเริ่มต้นด้วย** ไม่งั้นปกเพลง/รูป
    Visualizer ของเพลงที่เพิ่งถูก Clear ทิ้งจะค้างอยู่บนจอตลอดที่ ISO กำลังแปลง —
    `updateCoverArt(QString())` วาด placeholder "ไม่มีปก" แบบเดียวกับเพลงที่ไม่มี
    artwork ฝังมา ส่วน `Visualizer::reset()` (เมธอดใหม่) เคลียร์ state ที่เป็น
    animation ทั้งหมด (band magnitude, waveform, VU ballistics, particles,
    spectrogram history ฯลฯ) แล้ว repaint ทันที — คนละเรื่องกับ `setActive(false)`
    ที่แค่หยุด timer แต่เฟรมสุดท้ายที่วาดค้างไว้ยังไม่หาย
    **แปลงเสร็จหมดแล้ว กด OK ที่ dialog
    สรุปผล ("Converted N track(s) to FLAC in...") ปุ๊บ เพลงแรกเริ่มเล่นทันที**
    — `onIsoImportFinished()` เรียก `playIndex(0, true)` ต่อจาก
    `QMessageBox::information()` (ซึ่งเป็น dialog แบบ modal ค้างจนกว่าจะกด OK
    อยู่แล้ว) โดยใช้ index 0 ตรง ๆ เพราะ playlist เดิมถูก Clear ไปก่อนเริ่มแปลง
    เสมอ (ดูบูลเลตด้านบน) แทร็กที่เพิ่ง `addFilesToPlaylist()` เข้าไปจึงเริ่มที่
    index 0 เสมอเช่นกัน — ข้ามขั้นตอนถ้าไม่มีแทร็กแปลงสำเร็จเลย (ไม่มี dialog OK
    ให้กดตั้งแต่แรก เพราะ error path คนละเส้นทาง)
- **Mini Player** — ปุ่มที่ 7 ในแถบปุ่มเล่นเพลง ต่อจาก Repeat (ไม่ได้แยกไปอยู่
  ต่างหาก เพราะเป็นส่วนหนึ่งของกลุ่มปุ่มเดียวที่ต้องเหลืออยู่ตอนย่อโหมด) กดสลับ
  เข้า/ออกโหมดย่อ: ซ่อน cover art/แท็บ Playlist-Equalizer-Theme-Shutdown/
  Visualizer/seek bar ทั้งหมด (รวมอยู่ใน widget เดียวคือ
  `m_normalModeContainer`) เหลือแค่แถบปุ่มเล่นเพลงทั้ง 7 ปุ่ม แล้วย่อหน้าต่างลง
  มาพอดีเนื้อหาด้วย `adjustSize()` — ต้อง `layout()->invalidate()` +
  `activate()` ก่อนเรียก `adjustSize()` เสมอ ไม่งั้นหน้าต่างจะไม่ย่อขนาดจริง
  (`setVisible(false)` invalidate layout แบบ lazy, `adjustSize()` ที่เรียกต่อ
  ทันทีจะยังอ่าน sizeHint เก่าอยู่ - ยืนยันด้วยการทดสอบจริง 2026-09-20) กลับไป
  โหมดปกติจะคืนขนาดหน้าต่างเดิมก่อนย่อ (`m_preMiniPlayerSize`) ไม่ใช่ขนาด
  default และ **ไม่บันทึกขนาดหน้าต่างตอนอยู่ใน Mini Player ลง settings** —
  ถ้าปิดแอปตอนอยู่โหมดนี้ ครั้งหน้าเปิดมาจะเป็นโหมดปกติที่ขนาดหน้าต่างเดิม
  (ก่อนเข้า Mini Player ครั้งล่าสุด) เสมอ ไม่ใช่โหมดเต็มบีบอยู่ในหน้าต่างจิ๋ว
  - **ตำแหน่งหน้าต่าง: เข้า Mini Player ไปมุมขวาบนของจอ, ออกแล้วกลับมากลางจอ**
    (ผู้ใช้ขอ 2026-09-20) ใช้ `screen()->availableGeometry()` (พื้นที่ที่ไม่รวม
    taskbar) เว้นขอบ 12px เข้าไปมุมขวาบนทุกครั้งที่เข้า แล้วตอนออกก็ย้ายมา
    กึ่งกลางพื้นที่นั้น ไม่ว่าก่อนหน้าจะลากไปวางไว้ตรงไหนก็ตาม (เลือกจอที่หน้าต่าง
    อยู่ตอนนั้น ถ้ามีหลายจอ) — ต้อง **เลื่อนการ `move()` ไปรันหลังจากนั้น 1
    รอบ event loop** (`QTimer::singleShot(0, ...)`) ห้ามเรียกทันทีหลังสลับ
    frameless flag เพราะ Qt ยังจำ frame margin ของหน้าต่างแบบมีกรอบเก่าอยู่
    ทำให้ตำแหน่งเพี้ยนลงมาเท่าความสูง title bar พอดี (31px) ทั้งตอนเข้า Mini
    Player และตอนคำนวณกึ่งกลางตอนออก (คลาดไป ~8px) ยืนยันด้วยการวัดจริงว่า
    ห่างขอบขวา/บน 12px เป๊ะทุกรอบ และ restore ขนาดยังตรง 996×831
  - **ไม่มี title bar, มุมหน้าต่างโปร่งใส, เห็นแค่แถบปุ่มมนลอยอยู่ (rounded
    pill)** — เข้า Mini Player แล้วหน้าต่างเปลี่ยนเป็น frameless
    (`Qt::FramelessWindowHint`) ทันที ต้อง `hide()`/`show()` คู่กับ
    `setWindowFlag()` เสมอ เพราะเปลี่ยน window flag บน top-level widget ที่
    แสดงอยู่แล้วไม่มีผลจนกว่าจะถูกซ่อน/โชว์ใหม่ (Windows ต้องสร้าง native
    window ใหม่ตาม style bit ที่เปลี่ยน) ส่วน translucent
    (`Qt::WA_TranslucentBackground`) **ตั้งครั้งเดียวใน `setupUi()` ก่อน
    `show()` ครั้งแรก แล้วเปิดค้างไว้ตลอด ไม่สลับตามโหมด**:
    - **แก้บั๊ก: เข้า Mini Player ครั้งแรกไม่โปร่งใส (มีกล่องสี่เหลี่ยมสีดำรอบ
      pill) ครั้งที่ 2 เป็นต้นไปปกติ** (ผู้ใช้แจ้ง 2026-09-20, reproduce ได้จริง
      ด้วย screenshot เทียบครั้งที่ 1 กับครั้งที่ 2) เดิมสลับ
      `WA_TranslucentBackground` ตาม `on`/`off` พร้อมกับ frameless flag ทำให้
      ตอนเข้าครั้งแรก native window ที่สร้างไปแล้วไม่มี alpha channel
      (Qt ต้องการให้ตั้ง attribute นี้ตั้งแต่ก่อน window ถูกสร้างครั้งแรกถึงจะ
      ได้ surface แบบมี alpha แน่นอน) ย้ายไปตั้งใน `setupUi()` แล้วเลิกสลับ
      ทำให้ครั้งแรกโปร่งใสเหมือนครั้งต่อ ๆ ไป — เปิดค้างไว้ในโหมดปกติได้เพราะ
      `Theme::appStyleSheet()` วาดพื้นทึบให้ทุก widget อยู่แล้ว ตรวจแล้วโหมด
      ปกติหน้าตาเหมือนเดิมทุกอย่าง (title bar, พื้นหลังทึบ, ขนาดคืนค่า 996×831)
    แถบปุ่มเล่นเพลงถูกห่อด้วย widget ใหม่ชื่อ `m_transportContainer`
    (object name `"MiniPlayerPill"`) ซึ่งเป็นจุดเดียวที่ยังมีพื้นหลัง/มุมมน
    (`border-radius: 22px`) ส่วน `QMainWindow`/central widget โปร่งใสหมด —
    **พื้นหลัง pill รวมถึงตัวปุ่มเองก็โปร่งแสง 50% ทั้งหมด** (ผู้ใช้ขอ
    2026-09-20 หลังเห็นว่า pill ทึบแสง 100%, แล้วขอเพิ่มอีกรอบให้ตัวปุ่มเอง
    โปร่งแสงด้วยหลังเจอ icon บน desktop ซ้อนอยู่ข้างใต้ปุ่ม Power แล้วมองไม่เห็น)
    ใช้ `rgba(r, g, b, 128)` กับสีของแต่ละส่วน (`bg0` สำหรับ pill, `bg2`
    สำหรับ `#TransportButton` ปกติ, `accent`/`accentHi` สำหรับ `#PlayButton`
    และปุ่มที่ `:checked` เช่น Mini Player ตอนกำลัง active) — Qt Style
    Sheets' `rgba()` รับค่า alpha เป็น int 0-255 ไม่ใช่ 0-1 แบบ CSS ทั่วไป 128
    จึงเป็นจุดกึ่งกลาง ~50% ไอคอนบนปุ่ม (วาดจาก `IconFactory::make()` เป็น
    pixel ทึบบน PNG โปร่งใส) ยังคมชัดปกติเพราะเป็นคนละ layer จากพื้นหลังปุ่ม —
    ทำผ่าน `applyWindowStyleSheet()` ที่เติม CSS override ต่อท้าย
    `Theme::appStyleSheet()` เดิม (ไม่ได้เขียน stylesheet แยกใหม่ทั้งชุด) มี
    ผลเฉพาะตอน `m_miniPlayerMode` เท่านั้น เรียกทั้งตอน setupUi() และทุกครั้งที่
    เปลี่ยนธีม (`applyThemePalette()`) กันธีมเปลี่ยนระหว่างอยู่ใน Mini Player
    แล้วโปร่งใสหาย **ไม่มีขอบให้ลาก resize และไม่มีปุ่ม minimize เดิมให้กด**
    (ยืนยันกับผู้ใช้แล้ว 2026-09-20) — ย้ายหน้าต่างด้วยการลากค้างที่ไหนก็ได้บน
    ตัวหน้าต่างที่ไม่ใช่ปุ่ม (`mousePressEvent`/`mouseMoveEvent`/
    `mouseReleaseEvent` ที่ override ไว้ ทำงานเฉพาะตอน `m_miniPlayerMode`) —
    Alt+F4 ยังปิดได้ตรงๆ เหมือนเดิมเพราะ Windows ส่ง WM_SYSCOMMAND/SC_CLOSE
    โดยไม่สนว่าหน้าต่างมี title bar หรือไม่
    - **แก้บั๊ก: เห็น icon บน desktop (เช่น VLC) โผล่มาข้าง ๆ ปุ่มได้** (ผู้ใช้ส่ง
      รูปมาชี้ 2026-09-20) สาเหตุคือ margin ซ้อนกัน 2 ชั้น: `rootLayout` ของ
      `centralWidget()` เอง (`14,14,14,10`, ตั้งไว้ครั้งเดียวตอน `setupUi()`
      ไม่เคยถูกปรับตามโหมด) บวกกับ margin ของ `m_transportContainer` เอง
      (`14,10,14,10` เฉพาะตอน mini mode) รวมกันกลายเป็นแถบหน้าต่างโปร่งใส
      "เปล่า ๆ" กว้างมากรอบ pill (ไม่ใช่ส่วนโค้งมนของ pill เลย) กว้างพอที่ icon
      บน desktop จะโผล่ผ่านมาให้เห็นชัด ๆ ติดกับปุ่มได้ แก้ด้วยการลด
      `centralWidget()->layout()`'s margins ลงเหลือแค่ `2,2,2,2` เฉพาะตอน
      mini mode ด้วย (คืนกลับ `14,14,14,10` เดิมตอนออกจากโหมด) ให้หน้าต่าง
      หุบชิดตัว pill จริง ๆ — ยืนยันแล้วว่ามุมโค้งของ pill ไม่โดนตัดขาดไปด้วย และ
      restore ขนาดตอนออกจากโหมดยังตรงเป๊ะเหมือนเดิม
  - **ปุ่ม Exit (ไอคอนปุ่มเพาเวอร์) ซ้ายสุดของแถบปุ่ม** — แยกห่างจาก
    Play/Next/ปุ่มอื่น ๆ ในกลุ่มไปอยู่อีกฝั่งเพื่อกันคลิกโดนพลาด (ผู้ใช้ขอย้าย
    จากขวาสุดมาซ้ายสุด 2026-09-20) เพิ่มเข้ามาเพราะโหมด Mini Player ไม่มี
    title bar/ปุ่ม close ของ Windows ให้กดปิดแอปอีกต่อไป กดแล้ว `close()`
    ทันที เหมือนกดปุ่ม close จริง ๆ ของหน้าต่าง — ยังเจอ dialog ยืนยัน "Are you
    sure you want to exit?" จาก `closeEvent()` เหมือนเดิมทุกอย่าง ไม่ได้
    bypass ความปลอดภัยจุดนี้ ไม่ใช่ปุ่มลัดของแท็บ Shutdown (sleep timer) —
    เป็นคนละฟีเจอร์กัน แค่ปิดโปรแกรมทันทีเฉย ๆ
    - **แก้บั๊ก: มีแถบสีเข้มทึบแสงค้างอยู่ใต้ pill** (ผู้ใช้ส่งรูปวงสีแดงชี้มา
      2026-09-20) สาเหตุคือ **status bar ของ `QMainWindow`** (สร้างจากการเรียก
      `statusBar()` ครั้งแรกใน `setupUi()`, ใช้โชว์ข้อความชั่วคราวเช่น "Playlist
      saved.") เป็นคนละส่วนกับ `centralWidget()`'s layout ไปเลย ซ่อน
      `m_normalModeContainer` ไม่มีผลกับมันเลย แล้วก็ไม่ได้อยู่ในรายการที่
      `applyWindowStyleSheet()` ทำให้โปร่งใสด้วย เลยยังคงค้างเป็นแท่งทึบเต็ม
      ความกว้างหน้าต่างอยู่ใต้ pill ตลอด แก้ด้วยการเพิ่ม
      `statusBar()->setVisible(!on)` เข้าไปใน `onMiniPlayerToggled()` ซ่อนไปพร้อม
      กับเข้า Mini Player ทุกครั้ง — ระหว่างแก้จุดนี้ก็เจอว่า resize กลับ
      ขนาดเดิมตอนออกจาก Mini Player เพี้ยนไปด้วย (สั้นกว่าเดิม ~39px เพราะ
      status bar เพิ่งถูกโชว์กลับมาแล้ว Qt/Windows ยังไม่ relayout เสร็จตอนที่
      `resize(m_preMiniPlayerSize)` รัน) แก้ด้วยการเลื่อน `resize()` นั้นไปรันผ่าน
      `QTimer::singleShot(0, ...)` แทน (ให้ relayout จบก่อนค่อย resize) ยืนยัน
      แล้วว่าขนาดหน้าต่างตรงเป๊ะกับก่อนเข้า Mini Player
- **ปุ่ม Previous / Next / Stop เปลี่ยนเป็นสี accent หลังกด แล้วกลับเองเมื่อ
  เมาส์ Loading (busy cursor) หาย** — กดแล้วปุ่มนั้นเปลี่ยนเป็นสี accent ของธีม
  ทันที ใช้ได้ทั้งโหมดปกติและ Mini Player
  - **ประวัติการปรับระยะเวลา (ผู้ใช้สั่ง 2026-09-20):** แรกสุดผู้ใช้พิมพ์ "2
    นาที" (ถามยืนยันแล้วว่าหมายถึง 2 วินาที) → ลดเหลือ 1 วินาที → สุดท้ายขอเป็น
    "จนกว่า Mouse Loading หาย" คือให้ค้างสีไว้จนกว่า busy cursor ที่ขึ้นตอนโหลด
    เพลง (`onEngineStateChanged()`/`m_loadingCursorActive`) จะหายไป
  - **ขั้นต่ำ 1 วินาทียังอยู่:** Stop ไม่ได้โหลดอะไรเลย ถ้าใช้กฎ "จนกว่า
    loading หาย" ล้วน ๆ ปุ่มจะกลับสีทันทีจนไม่เห็นเลยว่ากะพริบ และ Next/Prev
    ที่โหลดเสร็จเร็วมาก (เช่น ไฟล์เล็ก) ก็จะแวบเดียวเช่นกัน จึงให้ทุกปุ่มค้างสี
    อย่างน้อย `kTransportFlashMs` (1 วินาที) ก่อน แล้วถ้าครบเวลาแล้ว busy cursor
    ยังอยู่ ค่อยค้างต่อ (property `flashWaitLoad`) จนกว่า
    `releaseLoadingCursor()` จะถูกเรียกตอนเคอร์เซอร์หาย ถึงจะกลับสีปกติ
  - **`releaseLoadingCursor()`:** รวมจุดที่เคยเรียก
    `QApplication::restoreOverrideCursor()` เอง 3 จุด (audio เริ่มมีเสียง,
    โหลดจบแบบอื่นที่ไม่ใช่ Playing, engine error) มาไว้ที่เดียว เพื่อให้ทุกทาง
    ที่เคอร์เซอร์หายปล่อยปุ่มที่ค้างสีรออยู่ด้วยเสมอ ไม่มีทางหลุดจนสีค้างถาวร
  - **กลไก:** `flashTransportButton()` ตั้ง dynamic property `flash=true` บน
    ปุ่ม แล้ว QSS เลือกด้วย `QPushButton#TransportButton[flash="true"]` (อยู่ใน
    `Theme.h` ใช้ `%7` = accent จึงตามธีมที่ผู้ใช้เลือกเองอัตโนมัติ) — **ต้อง
    `unpolish()`/`polish()` ปุ่มหลังเปลี่ยน property ทุกครั้ง** ไม่งั้น Qt ไม่
    re-evaluate selector ให้ สีจะไม่เปลี่ยน
  - **กดซ้ำระหว่างกะพริบ = เริ่มนับขั้นต่ำ 1 วินาทีใหม่ (และเลิกรอ loading ของการกดก่อนหน้า):** timer ที่ตั้งไว้ครั้งก่อนจะ
    ยังยิงตามเวลาเดิม จึงเก็บเลขรุ่นไว้ใน property `flashGen` (เพิ่มทีละ 1
    ทุกครั้งที่กด) timer จะปิดสีเฉพาะเมื่อเลขรุ่นยังตรงกับตอนที่ตั้ง — กัน
    timer เก่าปิดสีก่อนเวลาของการกดครั้งล่าสุด
  - **โหมด Mini Player:** ปุ่มโปร่งแสง (`rgba(bg2,128)`) อยู่แล้ว จึงเพิ่ม
    selector `[flash="true"]` เข้าไปในกฎ override สี accent ของ
    `applyWindowStyleSheet()` ให้กะพริบเป็น accent โปร่งแสงระดับเดียวกับปุ่ม
    Play ไม่ใช่สีทึบที่ดูโดดออกมาจาก pill
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

**คอขวดจริงคือ decode DSD→PCM ไม่ใช่การอ่าน/แปลง byte** (วัดจริง 2026-09-19 บน
SACD ISO จริง track 3:45 / 154 MB): ก่อนแก้ demux 2.4 วิ + เขียน .dsf 0.5 วิ +
FLAC encode 3.7 วิ แต่ช่วง `decodeToInt32Pcm()` (QAudioDecoder/FFmpeg) กิน
**527 วินาที** ≈ 99% ของทั้งหมด (รวม ~534 วิ) สาเหตุคือใน callback `bufferReady`
เรียก `outInterleaved.reserve(size() + n)` ทุก buffer — libstdc++ `reserve()`
จองความจุ "เท่าที่ขอเป๊ะ" ไม่เผื่อโต จึง realloc + ก๊อปทั้ง vector (~160 MB ตอน
ท้าย) ใหม่ทุกครั้ง = O(n²) แก้เป็น `resize()` (โตแบบ geometric) → decode เหลือ
~6 วิ รวมทั้ง track ~10.5 วิ (**เร็วขึ้น ~51 เท่า**, ผลลัพธ์เหมือนเดิมทุก
sample) ส่วนที่เคยสงสัยไว้ — อ่านทีละ sector (อ่านเป็นก้อน 4 MiB อยู่แล้ว),
กลับบิตทีละ byte, เขียน .dsf ชั่วคราว, progress callback — รวมกันไม่ถึง ~1 วิ
ต่อ track จึงไม่คุ้มแก้ (ส่วน .dsf ชั่วคราวเป็นทางเดียวที่ส่ง DSD เข้า
QAudioDecoder/FFmpeg ได้ ไม่ต้องเขียน DSD decimation filter เอง)

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

- **`QAudioSink` อยู่บน audio thread ของตัวเอง (`AudioEngine::m_audioThread`)
  ไม่ใช่ UI thread — แก้บั๊ก "ลาก/ย้ายหน้าต่างแล้วเพลงสะดุดหรือเงียบประมาณ 1
  วินาที" (ผู้ใช้แจ้ง 2026-09-22 ทั้งตอนลาก Mini Player และหน้าต่างหลัก)**
  - **สาเหตุ (วัดจริง ไม่ได้เดา):** backend ของ `QAudioSink` บน Windows ดึงข้อมูล
    จาก `QIODevice` (`pullAudio()`) ด้วย event/timer ของ thread ที่ sink อาศัย
    อยู่ — เดิมคือ UI thread ตอนลากหน้าต่างด้วย title bar Windows เข้า modal
    move loop ที่หยุด event dispatch ปกติตราบเท่าที่ยังกดเมาส์ค้าง จึงไม่มีการ
    ดึงเสียงเลย ใส่ตัวจับเวลาชั่วคราวใน `pullAudio()` แล้วจำลองการกดค้าง 1.5
    วินาทีแล้วลากด้วย `mouse_event` จริง: ช่องว่างระหว่างการดึงเสียงยาว **525 ms
    บน main thread** ขณะที่ buffer ของอุปกรณ์เสียงมีแค่ ~130 ms (เพลง 88.2kHz
    float stereo) → buffer หมด = เสียงเงียบ (เครื่องผู้ใช้นานถึง ~1 วินาที) ส่วน
    Mini Player ที่ลากด้วย `mouseMoveEvent` เองก็ทำให้ main thread งานหนักเช่นกัน
    (ทุก `move()` ของหน้าต่าง translucent ต้อง composite ใหม่)
  - **วิธีแก้:** ย้าย `QAudioSink` + `PcmIODevice` ไปสร้าง/ใช้/ทำลายบน
    `QThread` แยก (priority `TimeCriticalPriority`) ไม่ว่า UI จะค้างเพราะอะไรก็ไม่
    มีผลกับการป้อนเสียงอีก การเรียก `start/stop/setVolume` จาก UI thread ผ่าน
    `runOnAudioThread()` = `QMetaObject::invokeMethod(m_audioCtx, fn,
    Qt::BlockingQueuedConnection)` (รอจนเสร็จ ลำดับ stop → ตั้ง cursor → start ใน
    `seek()` จึงเหมือนเดิมเป๊ะ) โดย `m_audioCtx` เป็น `QObject` เปล่า ๆ ที่ย้ายไปอยู่
    บน audio thread เพื่อเป็นเป้าให้ `invokeMethod` — ไม่ต้องมี `Q_OBJECT`/moc
    เพิ่ม ส่วน `QAudioSink::stateChanged` ต่อไว้กับ context เดียวกัน (รันบน audio
    thread ตรง ๆ) แล้วค่อย emit `audioActive()`/`errorOccurred()` ให้ UI thread
    รับผ่าน queued connection อัตโนมัติ
  - **ผลวัดหลังแก้:** ลากด้วยวิธีเดียวกัน (ทั้ง title bar หลัก และลาก Mini Player)
    **ไม่มีช่องว่างเกิน 60 ms เลย** เพลงเดินต่อเนื่อง และ Pause/Resume/Next/
    Mini Player ยังทำงานปกติ — ข้อควรรู้: ระหว่างลากด้วย title bar หน้าจอ (แถบ
    เวลา/visualizer) ยังหยุดนิ่งชั่วคราวเพราะเป็นงานของ UI thread เอง แต่เสียงไม่
    ตกหล่นแล้ว
  - **ข้อควรระวังตอนแก้โค้ดส่วนนี้:** `m_sink`/`m_ioDevice` เป็น raw pointer ที่
    ต้องแตะผ่าน `runOnAudioThread()` เท่านั้น (สร้าง/ลบบน audio thread เพราะ
    `QAudioSink` ต้องถูกใช้จาก thread ที่มันอยู่) และห้ามเรียก `runOnAudioThread()`
    ขณะถือ mutex ของ `Equalizer` (audio thread ต้องล็อกตัวเดียวกันใน `pullAudio()`
    → deadlock)
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
