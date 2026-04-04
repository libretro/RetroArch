#if defined(_MSC_VER) && !defined(_XBOX) && (_MSC_VER >= 1500 && _MSC_VER < 1900)
#if (_MSC_VER >= 1700)
/* https://support.microsoft.com/en-us/kb/980263 */
#pragma execution_character_set("utf-8")
#endif
#pragma warning(disable:4566)
#endif

/*
##### NOTE FOR TRANSLATORS ####

PLEASE do NOT modify any `msg_hash_*.h` files, besides `msg_hash_us.h`!

Translations are handled using the localization platform Crowdin:
https://crowdin.com/project/retroarch

Translations from Crowdin are applied automatically and will overwrite
any changes made to the other localization files.
As a result, any submissions directly altering `msg_hash_*.h` files
other than `msg_hash_us.h` will be rejected.
*/

/* Top-Level Menu */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MAIN_MENU,
   "เมนูหลัก"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_TAB,
   "ตั้งค่า"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FAVORITES_TAB,
   "รายการโปรด"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HISTORY_TAB,
   "ประวัติ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_IMAGES_TAB,
   "รูปภาพ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MUSIC_TAB,
   "เพลง"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_TAB,
   "วีดีโอ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_TAB,
   "เล่นออนไลน์"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_TAB,
   "เรียกดู"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENTLESS_CORES_TAB,
   "Core ที่ไม่ต้องใช้ไฟล์เกม"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_TAB,
   "นำเข้าเกม"
   )

/* Main Menu */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SETTINGS,
   "เมนูด่วน"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SETTINGS,
   "เข้าถึงการตั้งค่าสำคัญในเกมได้อย่างรวดเร็ว"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_LIST,
   "โหลด Core"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_LIST,
   "เลือก Core ที่ต้องการใช้งาน"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_LIST_UNLOAD,
   "ปิด Core"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_LIST_UNLOAD,
   "ปิด Core ที่โหลดไว้"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_CORE_LIST,
   "ค้นหาไฟล์ Libretro Core โดยตำแหน่งเริ่มต้นจะขึ้นอยู่กับ \"Core Directory\" ที่ตั้งไว้ หากไม่ได้ระบุจะเริ่มที่ Root \nแต่ถ้าตั้งเป็นโฟลเดอร์หรือที่อยู่ไฟล์ไว้ ระบบจะเริ่มค้นหาจากตำแหน่งนั้นทันที"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOAD_CONTENT_LIST,
   "เปิดเกม"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOAD_CONTENT_LIST,
   "เลือก Core ที่ต้องการใช้งาน"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_LOAD_CONTENT_LIST,
   "ค้นหาเกม ในการเปิดเกมคุณจำเป็นต้องมี 'Core' และไฟล์เกมที่ต้องการใช้งาน\nหากต้องการกำหนดจุดเริ่มต้นในการค้นหา ให้ตั้งค่าที่ 'โฟลเดอร์เบราว์เซอร์ไฟล์' หากไม่ได้ตั้งไว้จะเริ่มที่ root\n[...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOAD_DISC,
   "โหลดแผ่น"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOAD_DISC,
   "โหลดแผ่นจากสื่อบันทึกข้อมูล เลือก Core ที่ต้องการใช้งานกับแผ่น (โหลด Core) ก่อนเป็นอันดับแรก"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DUMP_DISC,
   "คัดลอกข้อมูลจากแผ่น"
   )
MSG_HASH( /* FIXME Is a specific image format used? Is it determined automatically? User choice? */
   MENU_ENUM_SUBLABEL_DUMP_DISC,
   "คัดลอกข้อมูลจากแผ่นลงในหน่วยความจำเครื่อง โดยจะถูกบันทึกเป็นไฟล์ image"
   )
#ifdef HAVE_LAKKA
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EJECT_DISC,
   "เอาแผ่นออก"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_EJECT_DISC,
   "เอาแผ่นออกจากไดรฟ์ CD/DVD"
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLISTS_TAB,
   "รายการเล่น"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLISTS_TAB,
   "เนื้อหาที่สแกนแล้วและตรงกับฐานข้อมูลจะปรากฏที่นี่"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_CONTENT_LIST,
   "นำเข้าเกม"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ADD_CONTENT_LIST,
   "สร้างและอัปเดตรายการเล่นโดยการสแกนเกม"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHOW_WIMP,
   "แสดงเมนูหน้าหลัก"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SHOW_WIMP,
   "เปิดเมนูหน้าหลักรูปแบบดั้งเดิม"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_DISABLE_KIOSK_MODE,
   "ปิดโหมดล็อกเมนู"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_DISABLE_KIOSK_MODE,
   "แสดงการตั้งค่าระบบทั้งหมด"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ONLINE_UPDATER,
   "อัปเดตออนไลน์"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ONLINE_UPDATER,
   "ดาวน์โหลดส่วนเสริม องค์ประกอบ และเนื้อหาสำหรับ RetroArch"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY,
   "เล่นออนไลน์"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY,
   "เข้าร่วมหรือสร้างห้องเล่นออนไลน์"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS,
   "ตั้งค่า"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS,
   "ตั้งค่าโปรแกรม"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INFORMATION_LIST,
   "ข้อมูล"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INFORMATION_LIST_LIST,
   "แสดงข้อมูลระบบ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONFIGURATIONS_LIST,
   "ไฟล์การตั้งค่าระบบ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONFIGURATIONS_LIST,
   "จัดการและสร้างไฟล์การตั้งค่าระบบ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HELP_LIST,
   "ช่วยเหลือ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_HELP_LIST,
   "เรียนรู้เพิ่มเติมเกี่ยวกับวิธีการทำงานของโปรแกรม"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RESTART_RETROARCH,
   "เริ่มใหม่"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RESTART_RETROARCH,
   "เริ่มการทำงานแอป RetroArch ใหม่"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUIT_RETROARCH,
   "ออก"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUIT_RETROARCH,
   "ออกจากแอป RetroArch เปิดใช้งาน บันทึกการตั้งค่าระบบเมื่อออก"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUIT_RETROARCH_NOSAVE,
   "ออกจากแอป RetroArch ปิดใช้งาน บันทึกการตั้งค่าระบบเมื่อออก"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_QUIT_RETROARCH,
   "ออกจาก RetroArch หากบังคับปิดโปรแกรมด้วยวิธีรุนแรง (SIGKILL ฯลฯ) จะทำให้ RetroArch ปิดตัวลงโดยไม่มีการบันทึกการตั้งค่าระบบในทุกกรณี สำหรับระบบที่คล้าย Unix การใช้ SIGINT/SIGTERM จะช่วยให้ปิดการทำงานได้อย่[...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_SYNC_NOW,
   "ซิงค์ตอนนี้"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_SYNC_NOW,
   "เรียกใช้งานการซิงค์คลาวด์ด้วยตนเอง"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_RESOLVE_KEEP_LOCAL,
   "แก้ไขความขัดแย้งของข้อมูล: เก็บไฟล์ในเครื่องไว้"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_RESOLVE_KEEP_LOCAL,
   "แก้ไขความขัดแย้งของข้อมูลทั้งหมดโดยการอัปโหลดไฟล์ในเครื่องไปยังเซิร์ฟเวอร์"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_RESOLVE_KEEP_SERVER,
   "แก้ไขความขัดแย้งของข้อมูล: เก็บไฟล์บนเซิร์ฟเวอร์ไว้"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_RESOLVE_KEEP_SERVER,
   "แก้ไขความขัดแย้งของข้อมูลทั้งหมดโดยการดาวน์โหลดไฟล์จากเซิร์ฟเวอร์เพื่อเขียนทับไฟล์ในเครื่อง"
   )

/* Main Menu > Load Core */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DOWNLOAD_CORE,
   "ดาวน์โหลดคอร์ Core"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DOWNLOAD_CORE,
   "ดาวน์โหลดและติดตั้ง Core จากอัปเดตออนไลน์"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SIDELOAD_CORE_LIST,
   "ติดตั้งหรือคืนค่า Core"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SIDELOAD_CORE_LIST,
   "ติดตั้งหรือคืนค่า Core จากไดเรกทอรี Downloads"
   )
MSG_HASH( /* FIXME Maybe add a description? */
   MENU_ENUM_LABEL_VALUE_START_VIDEO_PROCESSOR,
   "เริ่มการทำงาน Video Processor"
   )
MSG_HASH( /* FIXME Maybe add a description? */
   MENU_ENUM_LABEL_VALUE_START_NET_RETROPAD,
   "เริ่มการใช้งาน Remote RetroPad"
   )

/* Main Menu > Load Content */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FAVORITES,
   "เปิดไดเรกทอรีเริ่มต้น"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DOWNLOADED_FILE_DETECT_CORE_LIST,
   "ดาวน์โหลด"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OPEN_ARCHIVE,
   "เรียกดูไฟล์ บีบอัด"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOAD_ARCHIVE,
   "โหลดไฟล์ บีบอัด"
   )

/* Main Menu > Load Content > Playlists */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GOTO_FAVORITES,
   "รายการโปรด"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GOTO_FAVORITES,
   "คอนเทนต์ที่เพิ่มใน 'รายการโปรด' จะแสดงที่นี่"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GOTO_MUSIC,
   "เพลง"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GOTO_MUSIC,
   "เพลงที่เคยเล่นจะแสดงที่นี่"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GOTO_IMAGES,
   "รูปภาพ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GOTO_IMAGES,
   "รูปภาพที่เคยดูจะแสดงที่นี่"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GOTO_VIDEO,
   "วีดีโอ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GOTO_VIDEO,
   "วิดีโอที่เคยเล่นจะแสดงที่นี่"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GOTO_EXPLORE,
   "เรียกดู"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GOTO_EXPLORE,
   "เรียกดูคอนเทนต์ทั้งหมดที่ตรงกับฐานข้อมูลผ่านเมนูค้นหาแบบแบ่งหมวดหมู่"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GOTO_CONTENTLESS_CORES,
   "Core ที่ไม่ต้องใช้ไฟล์เกม"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GOTO_CONTENTLESS_CORES,
   "Core ที่ติดตั้งไว้และสามารถทำงานได้โดยไม่ต้องโหลดคอนเทนต์จะแสดงที่นี่"
   )

/* Main Menu > Online Updater */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_UPDATER_LIST,
   "ดาวน์โหลด Core"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_INSTALLED_CORES,
   "อัปเดต Core ที่ติดตั้งแล้ว"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_UPDATE_INSTALLED_CORES,
   "อัปเดต Core ทั้งหมดที่ติดตั้งแล้ว เป็นเวอร์ชันล่าสุด"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SWITCH_INSTALLED_CORES_PFD,
   "เปลี่ยน Core เป็นเวอร์ชัน Play Store"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SWITCH_INSTALLED_CORES_PFD,
   "แทนที่ Core เก่าและที่ติดตั้งเองทั้งหมดด้วยเวอร์ชันล่าสุดจาก Play Store หากมี"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PL_THUMBNAILS_UPDATER_LIST,
   "อัปเดตภาพตัวอย่างเพลย์ลิสต์"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PL_THUMBNAILS_UPDATER_LIST,
   "ดาวน์โหลดภาพตัวอย่างสำหรับรายการในเพลย์ลิสต์ที่เลือก"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DOWNLOAD_CORE_CONTENT,
   "ดาวน์โหลดคอนเทนต์"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DOWNLOAD_CORE_CONTENT,
   "ดาวน์โหลดคอนเทนต์ฟรีสำหรับ Core ที่เลือก"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DOWNLOAD_CORE_SYSTEM_FILES,
   "ดาวน์โหลดไฟล์ระบบของ Core"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DOWNLOAD_CORE_SYSTEM_FILES,
   "ดาวน์โหลดไฟล์ระบบเสริมที่จำเป็นเพื่อให้ Core ทำงานได้อย่างถูกต้องและมีประสิทธิภาพสูงสุด"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_CORE_INFO_FILES,
   "อัปเดตไฟล์ข้อมูล Core"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_ASSETS,
   "อัปเดตทรัพยากร"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_AUTOCONFIG_PROFILES,
   "อัปเดต Controller Profiles"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_CHEATS,
   "อัปเดต สูตรโกง"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_DATABASES,
   "อัพเดต ฐานข้อมูล"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_OVERLAYS,
   "อัปเดต Overlays"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_GLSL_SHADERS,
   "อัปเดต GLSL Shaders"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_CG_SHADERS,
   "อัปเดต Cg Shaders"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_SLANG_SHADERS,
   "อัปเดต Slang Shaders"
   )

/* Main Menu > Information */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFORMATION,
   "ข้อมูล Core"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_INFORMATION,
   "ดูข้อมูลเกี่ยวกับแอปพลิเคชัน/Core"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISC_INFORMATION,
   "ข้อมูลแผ่น"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DISC_INFORMATION,
   "ดูข้อมูลเกี่ยวกับแผ่น"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_INFORMATION,
   "ข้อมูลเครือข่าย"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETWORK_INFORMATION,
   "ดูอินเทอร์เฟซเครือข่ายและที่อยู่ IP ที่เกี่ยวข้อง"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFORMATION,
   "ข้อมูลระบบ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SYSTEM_INFORMATION,
   "ดูข้อมูลเฉพาะของอุปกรณ์"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_MANAGER,
   "จัดการฐานข้อมูล"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DATABASE_MANAGER,
   "ดูฐานข้อมูล"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CURSOR_MANAGER,
   "จัดการตัวชี้"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CURSOR_MANAGER,
   "ดูประวัติการค้นหา"
   )

/* Main Menu > Information > Core Information */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_CORE_NAME,
   "ชื่อ Core"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_CORE_LABEL,
   "ป้าย Core"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_CORE_VERSION,
   "เวอร์ชั่น Core"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_SYSTEM_NAME,
   "ชื่อระบบ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_SYSTEM_MANUFACTURER,
   "ผู้ผลิตระบบ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_CATEGORIES,
   "หมวดหมู่"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_AUTHORS,
   "ผู้สร้าง"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_PERMISSIONS,
   "อนุญาต"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_LICENSES,
   "ใบอนุญาต"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_SUPPORTED_EXTENSIONS,
   "นามสกุลไฟล์ที่รองรับ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_REQUIRED_HW_API,
   "Graphics API ที่ต้องการ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_SAVESTATE_SUPPORT_LEVEL,
   "รองรับ Save State"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_SAVESTATE_DISABLED,
   "ไม่มี"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_SAVESTATE_BASIC,
   "พื้นฐาน (บันทึก/โหลด)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_SAVESTATE_SERIALIZED,
   "ต่อเนื่อง (บันทึก/โหลด, ย้อนเวลา)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_SAVESTATE_DETERMINISTIC,
   "คงตัว (บันทึก/โหลด, ย้อนเวลา, ลดดีเลย์, เล่นออนไลน์)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_FIRMWARE_IN_CONTENT_DIRECTORY,
   "หมายเหตุ: เปิดใช้งาน 'ไฟล์ระบบอยู่ในโฟลเดอร์เนื้อหา' แล้ว"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_FIRMWARE_PATH,
   "กำลังค้นหา: %s"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MISSING_REQUIRED,
   "ไม่พบ, จำเป็นต้องมี:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MISSING_OPTIONAL,
   "ไม่พบ, ตัวเลือกเพิ่มเติม:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PRESENT_REQUIRED,
   "พบแล้ว, จำเป็นต้องมี:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PRESENT_OPTIONAL,
   "พบแล้ว, ตัวเลือกเพิ่มเติม:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_LOCK,
   "ล็อก Core ที่ติดตั้งแล้ว"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_LOCK,
   "ป้องกันการแก้ไข Core ที่ติดตั้งอยู่ในปัจจุบัน อาจใช้เพื่อหลีกเลี่ยงการอัปเดตที่ไม่ต้องการ เมื่อเนื้อหาจำเป็นต้องใช้ Core เวอร์ชันเฉพาะ (เช่น ชุด Arcade ROM) หรือเมื่อรูปแบบ Save State ของ Core มีการ[...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_SET_STANDALONE_EXEMPT,
   "ยกเว้นจากเมนู 'Core ที่ไม่ใช้เนื้อหา'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_SET_STANDALONE_EXEMPT,
   "ป้องกันไม่ให้ Core นี้แสดงในแท็บ/เมนู 'Core ที่ไม่ใช้เนื้อหา' จะมีผลเฉพาะเมื่อตั้งค่าโหมดการแสดงผลเป็น 'กำหนดเอง' เท่านั้น"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_DELETE,
   "ลบ Core"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_DELETE,
   "ลบ Core นี้ออกจากเครื่อง"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_CREATE_BACKUP,
   "สำรอง Core"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_CREATE_BACKUP,
   "สร้างไฟล์สำรองสำหรับ Core ที่ติดตั้งอยู่ในปัจจุบัน"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_RESTORE_BACKUP_LIST,
   "คืนค่าไฟล์สำรอง"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_RESTORE_BACKUP_LIST,
   "ติดตั้ง Core เวอร์ชันก่อนหน้าจากรายการไฟล์สำรอง"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_DELETE_BACKUP_LIST,
   "ลบไฟล์สำรอง"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_DELETE_BACKUP_LIST,
   "ลบไฟล์ออกจากรายการข้อมูลสำรองที่ถูกจัดเก็บไว้"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_BACKUP_MODE_AUTO,
   "[อัตโนมัติ]"
   )

/* Main Menu > Information > System Information */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_BUILD_DATE,
   "วันที่สร้าง"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RETROARCH_VERSION,
   "เวอร์ชั่น RetroArch"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_GIT_VERSION,
   "เวอร์ชั่น Git"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_CPU_MODEL,
   "รุ่น CPU"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_CPU_FEATURES,
   "คุณสมบัติ CPU"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_ARCHITECTURE,
   "สถาปัตยกรรม CPU"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_JIT_AVAILABLE,
   "รองรับ JIT"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE,
   "แหล่งพลังงาน"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DISPLAY_METRIC_MM_WIDTH,
   "ความกว้างจอแสดงผล (มม.)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DISPLAY_METRIC_MM_HEIGHT,
   "ความสูงจอแสดงผล (มม.)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DISPLAY_METRIC_DPI,
   "DPI หน้าจอ"
   )

/* Main Menu > Information > Database Manager > Information */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_NAME,
   "ชื่อ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_DESCRIPTION,
   "รายละเอียด"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_GENRE,
   "ประเภท"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ACHIEVEMENTS,
   "ความสำเร็จ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_CATEGORY,
   "หมวดหมู่"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_LANGUAGE,
   "ภาษา"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_REGION,
   "ภูมิภาค"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_CONSOLE_EXCLUSIVE,
   "เฉพาะคอนโซล"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_PLATFORM_EXCLUSIVE,
   "เฉพาะแพลตฟอร์ม"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_SCORE,
   "คะแนน"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_MEDIA,
   "สื่อ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_CONTROLS,
   "การควบคุม"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_GAMEPLAY,
   "เกมเพลย์"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_NARRATIVE,
   "เนื้อเรื่อง"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_PACING,
   "จังหวะเกม"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_PERSPECTIVE,
   "มุมมอง"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_SETTING,
   "ฉากหลัง"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_VISUAL,
   "งานภาพ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_VEHICULAR,
   "พาหนะ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_PUBLISHER,
   "ผู้จำหน่าย"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_DEVELOPER,
   "ผู้พัฒนา"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ORIGIN,
   "แหล่งที่มา"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_FRANCHISE,
   "​แฟรนไชส์"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_TGDB_RATING,
   "คะแนนจาก TGDB"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_FAMITSU_MAGAZINE_RATING,
   "คะแนนจาก Famitsu Magazine"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_EDGE_MAGAZINE_REVIEW,
   "รีวิวจาก Edge Magazine"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_EDGE_MAGAZINE_RATING,
   "คะแนน Edge Magazine"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_EDGE_MAGAZINE_ISSUE,
   "ฉบับ Edge Magazine"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_RELEASE_MONTH,
   "เดือนที่วางขาย"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_RELEASE_YEAR,
   "ปีที่วางขาย"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_BBFC_RATING,
   "คะแนนจาก BBFC"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ESRB_RATING,
   "คะแนนจาก ESRB"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ELSPA_RATING,
   "คะแนนจาก ELSPA"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_PEGI_RATING,
   "คะแนนจาก PEGI"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ENHANCEMENT_HW,
   "ฮาร์ดแวร์เสริมประสิทธิภาพ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_CERO_RATING,
   "คะแนนจาก CERO"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_SERIAL,
   "ระหัสแผ่น"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ANALOG,
   "รองรับ อนาล็อก"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_RUMBLE,
   "รองรับระบบสั่น"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_COOP,
   "รองรับ Co-op"
   )

/* Main Menu > Configuration File */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONFIGURATIONS,
   "โหลดการตั้งค่า"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONFIGURATIONS,
   "โหลดการตั้งค่าที่มีอยู่ และแทนที่ ค่าปัจจุบัน"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVE_CURRENT_CONFIG,
   "บันทึกการตั้งค่าปัจจุบัน"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVE_CURRENT_CONFIG,
   "เขียนทับไฟล์การตั้งค่าปัจจุบัน"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVE_NEW_CONFIG,
   "บันทึกการตั้งค่าใหม่"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVE_NEW_CONFIG,
   "บันทึกแยกเป็นไฟล์ใหม่"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVE_AS_CONFIG,
   "บันทึกการตั้งค่าเป็น"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVE_AS_CONFIG,
   "บันทึกการตั้งค่าปัจจุบันเป็นไฟล์ใหม่ที่กำหนดเอง"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVE_MAIN_CONFIG,
   "บันทึกการตั้งค่าหลัก"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVE_MAIN_CONFIG,
   "บันทึกการตั้งค่าปัจจุบันเป็นไฟล์การตั้งค่าหลัก"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RESET_TO_DEFAULT_CONFIG,
   "คืนค่าเริ่มต้น"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RESET_TO_DEFAULT_CONFIG,
   "รีเซ็ตการตั้งค่าปัจจุบันเป็นค่าเริ่มต้น"
   )

/* Main Menu > Help */

/* Main Menu > Help > Basic Menu Controls */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_SCROLL_UP,
   "เลื่อนขึ้น"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_SCROLL_DOWN,
   "เลื่อนลง"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_CONFIRM,
   "ยืนยัน"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_INFO,
   "ข้อมูล"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_START,
   "เริ่ม"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_TOGGLE_MENU,
   "เปิด/ปิดเมนู"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_QUIT,
   "ออก"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_TOGGLE_KEYBOARD,
   "เปิด/ปิดแป้นพิมพ์"
   )

/* Settings */

MSG_HASH(
   MENU_ENUM_SUBLABEL_DRIVER_SETTINGS,
   "เปลี่ยน drivers ที่ระบบใช้งาน"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SETTINGS,
   "วิดีโอ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SETTINGS,
   "เปลี่ยน drivers ที่ระบบใช้งาน"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_SETTINGS,
   "เสียง"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_SETTINGS,
   "เปลี่ยนการตั้งค่าสัญญาณเสียง เข้า/ออก"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_SETTINGS,
   "การควบคุม"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_SETTINGS,
   "เปลี่ยนการตั้งค่าตัวควบคุม คีย์บอร์ด และเมาส์"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LATENCY_SETTINGS,
   "ความหน่วง"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LATENCY_SETTINGS,
   "เปลี่ยนการตั้งค่าความหน่วงของวิดีโอ เสียง และการควบคุม"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_SETTINGS,
   "เปลี่ยนการตั้งค่า Core"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONFIGURATION_SETTINGS,
   "การกำหนดค่า"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONFIGURATION_SETTINGS,
   "เปลี่ยนการตั้งค่าเริ่มต้นสำหรับ ไฟล์กำหนดค่า"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVING_SETTINGS,
   "การบันทึก"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVING_SETTINGS,
   "เปลี่ยนการตั้งค่าการบันทึกข้อมูล"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_SETTINGS,
   "ซิงค์คลาวด์"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_SETTINGS,
   "เปลี่ยนการตั้งค่าการซิงค์คลาวด์"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_ENABLE,
   "เปิดใช้งานการซิงค์คลาวด์"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_ENABLE,
   "พยายาม Sync Configs, Sram, และ States ไปยังผู้ให้บริการ พื้นที่จัดเก็บข้อมูลบนคลาวด์"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_DESTRUCTIVE,
   "การซิงค์คลาวด์แบบเขียนทับ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_SYNC_SAVES,
   "ซิงค์: Saves/States"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_SYNC_CONFIGS,
   "ซิงค์: ไฟล์กำหนดค่า Config"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_SYNC_THUMBS,
   "ซิงค์: รูปภาพปก"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_SYNC_SYSTEM,
   "ซิงค์: ไฟล์ระบบ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_SYNC_SAVES,
   "เมื่อเปิดใช้งาน Saves/States จะถูกซิงค์ไปยังคลาวด์"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_SYNC_CONFIGS,
   "เมื่อเปิดใช้งาน Saves/States จะถูกซิงค์ไปยังคลาวด์"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_SYNC_THUMBS,
   "เมื่อเปิดใช้งาน รูปภาพตัวอย่าง จะถูกซิงค์ไปยังคลาวด์ โดยปกติไม่แนะนำให้เปิดใช้งาน ยกเว้นกรณีที่มี รูปภาพตัวอย่าง แบบกำหนดเองจำนวนมาก มิฉะนั้นการใช้ ตัวดาวน์โหลดรูปภาพตัวอย่[...]"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_SYNC_SYSTEM,
   "เมื่อเปิดใช้งาน ไฟล์ระบบจะถูกซิงค์ไปยังคลาวด์ ซึ่งอาจทำให้ใช้เวลาในการซิงค์เพิ่มขึ้นอย่างมาก โปรดใช้งานด้วยความระมัดระวัง"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_DESTRUCTIVE,
   "เมื่อปิดใช้งาน ไฟล์จะถูกย้ายไปยังโฟลเดอร์สำรองข้อมูลก่อนที่จะถูกเขียนทับหรือลบทิ้ง"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_SYNC_MODE,
   "โหมด ซิงค์"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_SYNC_MODE,
   "อัตโนมัติ: ซิงค์เมื่อเริ่มใช้งาน RetroArch และเมื่อปิด Core ที่รันอยู่กำหนดเอง: ซิงค์เฉพาะเมื่อกดปุ่ม 'ซิงค์ทันที' ด้วยตนเองเท่านั้น"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_SYNC_MODE_AUTOMATIC,
   "อัตโนมัติ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_SYNC_MODE_MANUAL,
   "กำหนดเอง"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_DRIVER,
   "คลาวด์ซิงค์ ทำงานเบื้องหลัง"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_DRIVER,
   "เลือกโปรโตคอลเครือข่ายสำหรับพื้นที่จัดเก็บข้อมูลบนคลาวด์ที่จะใช้งาน"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_URL,
   "URL พื้นที่จัดเก็บข้อมูลคลาวด์"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_URL,
   "URL สำหรับจุดเชื่อมต่อ API ไปยังบริการพื้นที่จัดเก็บข้อมูลบนคลาวด์"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_USERNAME,
   "ชื่อผู้ใช้"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_USERNAME,
   "ชื่อผู้ใช้งานสำหรับบัญชีพื้นที่จัดเก็บข้อมูลบนคลาวด์ของคุณ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_PASSWORD,
   "รหัสผ่าน"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_PASSWORD,
   "รหัสผ่านสำหรับบัญชีพื้นที่จัดเก็บข้อมูลบนคลาวด์ของคุณ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_ACCESS_KEY_ID,
   "ID คีย์การเข้าถึง"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_ACCESS_KEY_ID,
   "ID คีย์การเข้าถึงสำหรับบัญชีพื้นที่จัดเก็บข้อมูลบนคลาวด์ของคุณ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_SECRET_ACCESS_KEY,
   "คีย์การเข้าถึงลับ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_SECRET_ACCESS_KEY,
   "คีย์การเข้าถึงลับสำหรับบัญชีพื้นที่จัดเก็บข้อมูลบนคลาวด์ของคุณ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_S3_URL,
   "URL สำหรับ S3 Endpoint ของพื้นที่จัดเก็บข้อมูลบนคลาวด์ของคุณ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOGGING_SETTINGS,
   "การบันทึกข้อมูล"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOGGING_SETTINGS,
   "เปลี่ยนการตั้งค่าการบันทึกข้อมูล"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_FILE_BROWSER_SETTINGS,
   "ตัวจัดการไฟล์"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_FILE_BROWSER_SETTINGS,
   "เปลี่ยนการตั้งค่าตัวจัดการไฟล์"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_CONFIG,
   "ไฟล์กำหนดค่า"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_COMPRESSED_ARCHIVE,
   "ไฟล์บีบอัด"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_RECORD_CONFIG,
   "ไฟล์กำหนดค่าการบันทึก"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_CURSOR,
   "ไฟล์ตัวชี้ ฐานข้อมูล"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_CONFIG,
   "ไฟล์กำหนดค่า"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_SHADER_PRESET,
   "ไฟล์ Shader preset"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_SHADER,
   "ไฟล์ Shader"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_REMAP,
   "ไฟล์กำหนดปุ่ม"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_CHEAT,
   "ไฟล์สูตรโกง"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_OVERLAY,
   "ไฟล์ Overlay"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_RDB,
   "ไฟล์ฐานข้อมูล"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_FONT,
   "ไฟล์ฟอนต์ TrueType"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_PLAIN_FILE,
   "ไฟล์ทั่วไป"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_MOVIE_OPEN,
   "วิดีโอ เลือกเพื่อเปิดไฟล์นี้ด้วยเครื่องเล่นวิดีโอ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_MUSIC_OPEN,
   "เพลง เลือกเพื่อเปิดไฟล์นี้ด้วยเครื่องเล่นเพลง"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_IMAGE,
   "ไฟล์รูปภาพ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_IMAGE_OPEN_WITH_VIEWER,
   "รูปภาพ เลือกเพื่อเปิดไฟล์นี้ด้วยโปรแกรมดูรูปภาพ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_CORE_SELECT_FROM_COLLECTION,
   "Libretro core การเลือกนี้จะเป็นการเชื่อมโยง Core นี้เข้ากับตัวเกม"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_CORE,
   "Libretro core เลือกไฟล์นี้เพื่อให้ RetroArch โหลด Core นี้"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_DIRECTORY,
   "ไดเรกทอรี เลือกเพื่อเปิดไดเรกทอรีนี้"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FRAME_THROTTLE_SETTINGS,
   "จำกัดเฟรมเรต"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FRAME_THROTTLE_SETTINGS,
   "เปลี่ยนการตั้งค่าการย้อนกลับ, การเร่งความเร็ว และสโลว์โมชัน"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RECORDING_SETTINGS,
   "บันทึกวิดีโอ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RECORDING_SETTINGS,
   "เปลี่ยนการตั้งค่าการบันทึก"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ONSCREEN_DISPLAY_SETTINGS,
   "การแสดงผลบนหน้าจอ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ONSCREEN_DISPLAY_SETTINGS,
   "ปลี่ยนการตั้งค่าภาพซ้อนทับหน้าจอ (Overlay), แป้นพิมพ์ และการแจ้งเตือนบนหน้าจอ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_USER_INTERFACE_SETTINGS,
   "ส่วนติดต่อผู้ใช้"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_USER_INTERFACE_SETTINGS,
   "เปลี่ยนการตั้งค่าส่วนติดต่อผู้ใช้"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_SETTINGS,
   "บริการ AI"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AI_SERVICE_SETTINGS,
   "เปลี่ยนการตั้งค่าบริการ AI (การแปลภาษา/การอ่านออกเสียง/อื่นๆ)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACCESSIBILITY_SETTINGS,
   "การช่วยเหลือพิเศษ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACCESSIBILITY_SETTINGS,
   "เปลี่ยนการตั้งค่าเสียงบรรยายสำหรับการเข้าถึง"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_POWER_MANAGEMENT_SETTINGS,
   "การจัดการพลังงาน"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_POWER_MANAGEMENT_SETTINGS,
   "เปลี่ยนการตั้งค่าการจัดการพลังงาน"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RETRO_ACHIEVEMENTS_SETTINGS,
   "ความสำเร็จ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RETRO_ACHIEVEMENTS_SETTINGS,
   "เปลี่ยนการตั้งค่าความสำเร็จ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_SETTINGS,
   "เครือข่าย"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETWORK_SETTINGS,
   "เปลี่ยนการตั้งค่าเซิร์ฟเวอร์และเครือข่าย"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SETTINGS,
   "รายการเล่น"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_SETTINGS,
   "เปลี่ยนการตั้งค่าเพลย์ลิสต์"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_USER_SETTINGS,
   "ผู้ใช้"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_USER_SETTINGS,
   "เปลี่ยนการตั้งค่าความเป็นส่วนตัว บัญชี และชื่อผู้ใช้"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DIRECTORY_SETTINGS,
   "ไดเรกทอรี"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DIRECTORY_SETTINGS,
   "เปลี่ยนเส้นทางตำแหน่งที่จัดเก็บไฟล์"
   )

/* Core option category placeholders for icons */
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MAPPING_SETTINGS,
   "กำหนดปุ่ม"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MEDIA_SETTINGS,
   "สื่อ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PERFORMANCE_SETTINGS,
   "ประสิทธิภาพ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SOUND_SETTINGS,
   "เสียง"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SPECS_SETTINGS,
   "รายละเอียด"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STORAGE_SETTINGS,
   "พื้นที่จัดเก็บข้อมูล"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_SETTINGS,
   "ระบบ"
   )

#ifdef HAVE_MIST
MSG_HASH(
   MENU_ENUM_SUBLABEL_STEAM_SETTINGS,
   "เปลี่ยนการตั้งค่าที่เกี่ยวข้องกับ Steam"
   )
#endif

/* Settings > Drivers */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DRIVER,
   "การควบคุม"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_DRIVER,
   "ไดรเวอร์อินพุตที่ใช้ ไดรเวอร์วิดีโอบางตัวอาจบังคับให้ต้องใช้ไดรเวอร์อินพุตที่ต่างออกไป (ต้องเริ่มระบบใหม่)"
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DRIVER,
   "วิดีโอ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_DRIVER,
   "เสียง"
   )
#ifdef HAVE_MICROPHONE
#endif

/* Settings > Video */

MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_OUTPUT_SETTINGS,
   "เปลี่ยน drivers ที่ระบบใช้งาน"
   )
#if defined(DINGUX)
#if defined(RS90) || defined(MIYOO)
#endif
#endif

/* Settings > Video > CRT SwitchRes */


/* Settings > Video > Output */

#if defined (WIIU)
#endif
#if defined(DINGUX) && defined(DINGUX_BETA)
#endif

/* Settings > Video > Fullscreen Mode */


/* Settings > Video > Windowed Mode */


/* Settings > Video > Scaling */

#if defined(DINGUX)
#endif
#if defined(RARCH_MOBILE)
#endif

/* Settings > Video > HDR */



/* Settings > Video > Synchronization */


/* Settings > Audio */

#ifdef HAVE_MICROPHONE
#endif

/* Settings > Audio > Output */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_ENABLE,
   "เสียง"
   )

#ifdef HAVE_MICROPHONE
/* Settings > Audio > Input */
#endif

/* Settings > Audio > Resampler */


/* Settings > Audio > Synchronization */


/* Settings > Audio > MIDI */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIDI_INPUT,
   "การควบคุม"
   )

/* Settings > Audio > Mixer Settings > Mixer Stream */


/* Settings > Audio > Menu Sounds */


/* Settings > Input */

#if defined(HAVE_DINPUT) || defined(HAVE_WINRAWINPUT)
#endif
#ifdef ANDROID
#endif



/* Settings > Input > Haptic Feedback/Vibration */


/* Settings > Input > Menu Controls */


/* Settings > Input > Hotkeys */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_QUIT_KEY,
   "ออก"
   )








MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_AI_SERVICE,
   "บริการ AI"
   )


/* Settings > Input > Port # Controls */


/* Settings > Latency */

#if !(defined(HAVE_DYNAMIC) || defined(HAVE_DYLIB))
#endif

/* Settings > Core */

#ifndef HAVE_DYNAMIC
#endif
#ifdef HAVE_MIST







#endif
/* Settings > Configuration */


/* Settings > Saving */


/* Settings > Logging */


/* Settings > File Browser */


/* Settings > Frame Throttle */


/* Settings > Frame Throttle > Rewind */


/* Settings > Frame Throttle > Frame Time Counter */


/* Settings > Recording */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_CONFIG_TYPE_RECORDING_MED_QUALITY,
   "ปานกลาง"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_CONFIG_TYPE_STREAMING_MED_QUALITY,
   "ปานกลาง"
   )

/* Settings > On-Screen Display */


/* Settings > On-Screen Display > On-Screen Overlay */


#if defined(ANDROID)
#endif

/* Settings > On-Screen Display > On-Screen Overlay > Keyboard Overlay */


/* Settings > On-Screen Display > On-Screen Overlay > Overlay Lightgun */


/* Settings > On-Screen Display > On-Screen Overlay > Overlay Mouse */


/* Settings > On-Screen Display > On-Screen Notifications */


/* Settings > User Interface */

#ifdef _3DS
#endif

/* Settings > User Interface > Menu Item Visibility */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_VIEWS_SETTINGS,
   "ทางลัด"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_VIEWS_SETTINGS,
   "ตั้งค่า"
   )
#ifdef HAVE_LAKKA
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_ADD_CONTENT_ENTRY_DISPLAY_MAIN_TAB,
   "เมนูหลัก"
   )

/* Settings > User Interface > Menu Item Visibility > Quick Menu */


/* Settings > User Interface > Views > Settings */



/* Settings > User Interface > Appearance */


/* Settings > AI Service */


/* Settings > Accessibility */


/* Settings > Power Management */

/* Settings > Achievements */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_ENABLE,
   "ความสำเร็จ"
   )

/* Settings > Achievements > Appearance */


/* Settings > Achievements > Visibility */


/* Settings > Network */


/* Settings > Network > Updater */


/* Settings > Playlists */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HISTORY_LIST_ENABLE,
   "ประวัติ"
   )

/* Settings > Playlists > Playlist Management */


/* Settings > User */


/* Settings > User > Privacy */


/* Settings > User > Accounts */


/* Settings > User > Accounts > RetroAchievements */


/* Settings > User > Accounts > YouTube */


/* Settings > User > Accounts > Twitch */


/* Settings > User > Accounts > Facebook Gaming */


/* Settings > Directory */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_ASSETS_DIRECTORY,
   "ดาวน์โหลด"
   )
MSG_HASH( /* FIXME Not RGUI specific */
   MENU_ENUM_LABEL_VALUE_RGUI_BROWSER_DIRECTORY,
   "เปิดไดเรกทอรีเริ่มต้น"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_DIRECTORY,
   "รายการเล่น"
   )

#ifdef HAVE_MIST
/* Settings > Steam */



#endif

/* Music */

/* Music > Quick Menu */


/* Netplay */


/* Netplay > Host */


/* Import Content */


/* Import Content > Scan File */


/* Import Content > Manual Scan */


/* Explore tab */

/* Playlist > Playlist Item */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INFORMATION,
   "ข้อมูล"
   )

/* Playlist Item > Set Core Association */


/* Playlist Item > Information */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_INFO_LABEL,
   "ชื่อ"
   )

/* Quick Menu */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_LIST,
   "ความสำเร็จ"
   )

/* Quick Menu > Options */


/* Quick Menu > Options > Manage Core Options */


/* Quick Menu > Controls */


/* Quick Menu > Controls > Manage Remap Files */


/* Quick Menu > Controls > Manage Remap Files > Load Remap File */


/* Quick Menu > Cheats */


/* Quick Menu > Cheats > Start or Continue Cheat Search */


/* Quick Menu > Cheats > Load Cheat File (Replace) */


/* Quick Menu > Cheats > Load Cheat File (Append) */


/* Quick Menu > Cheats > Cheat Details */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_DESC,
   "รายละเอียด"
   )

/* Quick Menu > Disc Control */


/* Quick Menu > Shaders */



/* Quick Menu > Shaders > Shader Parameters */


/* Quick Menu > Overrides */


/* Quick Menu > Achievements */


/* Quick Menu > Information */


/* Miscellaneous UI Items */


/* Settings Options */


/* RGUI: Settings > User Interface > Appearance */


/* RGUI: Settings Options */


/* XMB: Settings > User Interface > Appearance */


/* XMB: Settings Options */


/* Ozone: Settings > User Interface > Appearance */




/* MaterialUI: Settings > User Interface > Appearance */


/* MaterialUI: Settings Options */


/* Qt (Desktop Menu) */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_INFO,
   "ข้อมูล"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_TITLE,
   "ตั้งค่า"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_LOAD_CORE,
   "โหลด Core"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_NAME,
   "ชื่อ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_TAB_PLAYLISTS,
   "รายการเล่น"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_TAB_FILE_BROWSER,
   "ตัวจัดการไฟล์"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_INFORMATION,
   "ข้อมูล"
   )

/* Unsorted */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOAD_CONTENT_HISTORY,
   "ประวัติ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_USER,
   "ผู้ใช้"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BROWSE_START,
   "เริ่ม"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HELP,
   "ช่วยเหลือ"
   )

/* Discord Status */


/* Notifications */



/* Lakka */


/* Environment Specific Settings */

#ifdef HAVE_LIBNX
#endif
#ifdef HAVE_LAKKA
#ifdef HAVE_RETROFLAG
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAFESHUTDOWN_ENABLE,
#ifdef HAVE_RETROFLAG_RPI5
   "Retroflag Safe Shutdown"
#else
   "Retroflag Safe Shutdown (Reboot required)"
#endif
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAFESHUTDOWN_ENABLE,
#ifdef HAVE_RETROFLAG_RPI5
   "For use with compatible Retroflag case."
#else
   "For use with compatible Retroflag case. Reboot is required when changing."
#endif
   )
#endif
#ifdef HAVE_LAKKA_SWITCH
#endif
#endif
#ifdef HAVE_LAKKA_SWITCH
#endif
#ifdef GEKKO
#endif
#ifdef UDEV_TOUCH_SUPPORT
#endif
#ifdef HAVE_ODROIDGO2
#else
#endif
#ifdef _3DS
#endif
#ifdef HAVE_QT
#endif
#ifdef HAVE_GAME_AI





#endif
#ifdef HAVE_SMBCLIENT
#endif
