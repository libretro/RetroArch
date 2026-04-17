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
   "เล่นออนไลน์ Netplay"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_TAB,
   "เรียกดู"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENTLESS_CORES_TAB,
   "Core ที่ไม่มีเนื้อหา"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_TAB,
   "นำเข้าเกม"
   )

/* Main Menu */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SETTINGS,
   "ทางลัด"
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
   "เพลย์ลิสต์"
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
   "สร้างและอัปเดตเพลย์ลิสต์โดยการสแกนเกม"
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
   "เล่นออนไลน์ Netplay"
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
   "Core ที่ไม่มีเนื้อหา"
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
   "เปลี่ยนการตั้งค่า User Interface"
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
   "เพลย์ลิสต์"
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
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMING_SETTINGS,
   "จังหวะเวลา"
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
   MENU_ENUM_LABEL_HELP_INPUT_DRIVER_UDEV,
   "ไดรเวอร์ udev ใช้อ่านข้อมูล evdev เพื่อรองรับคีย์บอร์ด รวมถึงรองรับคีย์บอร์ดคอลแบ็ก เมาส์ และทัชแพด\nโดยค่าเริ่มต้นในหลาย Distro ไฟล์ใน /dev/input จะเข้าถึงได้เฉพาะ root (โหมด 600) คุณสามารถตั้งกฎ udev เพ[...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_INPUT_DRIVER_LINUXRAW,
   "ไดรเวอร์อินพุต linuxraw จำเป็นต้องใช้งานผ่าน TTY\nระบบจะอ่านข้อมูลคีย์บอร์ดโดยตรงจาก TTY ซึ่งใช้งานง่ายแต่ไม่ยืดหยุ่นเท่า udev และไม่รองรับเมาส์\nไดรเวอร์นี้ใช้จอยสติ๊ก API รุ่นเก่า (/dev/input/js)*ฉบ[...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_INPUT_DRIVER_NO_DETAILS,
   "ไดรเวอร์อินพุต\nไดรเวอร์วิดีโออาจบังคับให้ต้องใช้ไดรเวอร์อินพุตที่ต่างออกไปฉบับย่อ:ไดรเวอร์อินพุต\nไดรเวอร์วิดีโอบางตัวอาจบังคับเปลี่ยนตามความเหมาะสม"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_JOYPAD_DRIVER,
   "คอนโทรลเลอร์"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_JOYPAD_DRIVER,
   "ไดรเวอร์คอนโทรลเลอร์ที่ใช้ (จำเป็นต้องเริ่มระบบใหม่)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_JOYPAD_DRIVER_DINPUT,
   "ไดรเวอร์คอนโทรลเลอร์ DirectInput"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_JOYPAD_DRIVER_HID,
   "ไดรเวอร์อุปกรณ์ควบคุม (HID) แบบเข้าถึงฮาร์ดแวร์โดยตรง"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_JOYPAD_DRIVER_LINUXRAW,
   "ไดรเวอร์ Linux แบบ Raw ใช้จอยสติ๊ก API รุ่นเก่า ควรใช้ udev แทนหากทำได้"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_JOYPAD_DRIVER_PARPORT,
   "ไดรเวอร์ Linux สำหรับคอนโทรลเลอร์ที่เชื่อมต่อผ่านพอร์ตขนานโดยใช้อะแดปเตอร์พิเศษ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_JOYPAD_DRIVER_SDL,
   "ไดรเวอร์คอนโทรลเลอร์ที่ทำงานบนไลบรารี SDL"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_JOYPAD_DRIVER_UDEV,
   "ไดรเวอร์คอนโทรลเลอร์ที่ใช้ร่วมกับอินเทอร์เฟซ udev ซึ่งโดยทั่วไปแนะนำให้ใช้งาน ไดรเวอร์นี้ใช้ evdev joypad API รุ่นล่าสุดเพื่อรองรับจอยสติ๊ก รวมถึงรองรับการเสียบเข้า-ถอดออกทันที (Hotplugging) และ[...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_JOYPAD_DRIVER_XINPUT,
   "ไดรเวอร์คอนโทรลเลอร์ XInput ส่วนใหญ่ใช้สำหรับคอนโทรลเลอร์ Xbox"
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DRIVER,
   "วิดีโอ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_DRIVER,
   "ไดรเวอร์วิดีโอที่จะใช้งาน"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_GL1,
   "ไดรเวอร์ OpenGL 1.x เวอร์ชันขั้นต่ำที่ต้องการ: OpenGL 1.1 ไม่รองรับแชเดอร์ (Shaders) ควรใช้ไดรเวอร์ OpenGL เวอร์ชันที่ใหม่กว่าแทน หากทำได้"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_GL,
   "ไดรเวอร์ OpenGL 2.x ไดรเวอร์นี้อนุญาตให้ใช้งาน libretro GL cores ร่วมกับซอฟต์แวร์เรนเดอร์คอร์ได้ เวอร์ชันขั้นต่ำที่ต้องการ: OpenGL 2.0 หรือ OpenGLES 2.0 รองรับรูปแบบแชเดอร์ GLSL ควรใช้ไดรเวอร์ glcore แทน หากทำได้"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_GL_CORE,
   "ไดรเวอร์ OpenGL 3.x ไดรเวอร์นี้อนุญาตให้ใช้งาน libretro GL cores ร่วมกับซอฟต์แวร์เรนเดอร์คอร์ได้ เวอร์ชันขั้นต่ำที่ต้องการ: OpenGL 3.2 หรือ OpenGLES 3.0+ รองรับรูปแบบแชเดอร์ Slang"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_VULKAN,
   "ไดรเวอร์ Vulkan ไดรเวอร์นี้อนุญาตให้ใช้งาน libretro Vulkan cores ร่วมกับซอฟต์แวร์เรนเดอร์คอร์ได้ เวอร์ชันขั้นต่ำที่ต้องการ: Vulkan 1.0 รองรับ HDR และแชเดอร์ Slang"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_SDL1,
   "ไดรเวอร์ซอฟต์แวร์เรนเดอร์ SDL 1.2 ประสิทธิภาพถูกมองว่าอยู่ในระดับที่ไม่เหมาะสม ควรพิจารณาใช้ตัวเลือกนี้เป็นทางเลือกสุดท้ายเท่านั้น"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_SDL2,
   "ไดรเวอร์ซอฟต์แวร์เรนเดอร์ SDL 2 ประสิทธิภาพสำหรับการประมวลผล libretro core ด้วยซอฟต์แวร์จะขึ้นอยู่กับการทำงานของ SDL บนแพลตฟอร์มที่คุณใช้งานอยู่"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_METAL,
   "ไดรเวอร์ Metal สำหรับแพลตฟอร์มของ Apple รองรับรูปแบบ Shader แบบ Slang"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_D3D8,
   "ไดรเวอร์ Direct3D 8 (ไม่รองรับ Shader)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_D3D9_CG,
   "ไดรเวอร์ Direct3D 9 รองรับรูปแบบ Shader แบบ Cg รุ่นเก่า"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_D3D9_HLSL,
   "ไดรเวอร์ Direct3D 9 รองรับรูปแบบ Shader แบบ HLSL"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_D3D10,
   "ไดรเวอร์ Direct3D 10 รองรับรูปแบบ Shader แบบ Slang"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_D3D11,
   "ไดรเวอร์ Direct3D 11 รองรับ HDR และรูปแบบ Shader แบบ Slang"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_D3D12,
   "ไดรเวอร์ Direct3D 12 รองรับ HDR และรูปแบบ Shader แบบ Slang"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_DISPMANX,
   "ไดรเวอร์ DispmanX ใช้ API ของ DispmanX สำหรับ GPU Videocore IV ใน Raspberry Pi 0 ถึง 3 ไม่รองรับ Overlay หรือ Shader"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_CACA,
   "ไดรเวอร์ LibCACA จะแสดงผลเป็น ตัวอักษร แทนที่ภาพกราฟิก ไม่แนะนำให้ใช้งานจริง"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_EXYNOS,
   "ไดรเวอร์ Exynos ระดับล่างที่ใช้บล็อก G2D ใน Samsung Exynos SoC สำหรับการประมวลผล Blit ประสิทธิภาพสำหรับ Core ที่ใช้ซอฟต์แวร์เรนเดอร์จะอยู่ในระดับดีเยี่ยม"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_DRM,
   "ไดรเวอร์วิดีโอ DRM แบบพื้นฐาน เป็นไดรเวอร์ระดับล่างที่ใช้ libdrm ในการขยายขนาดด้วยฮาร์ดแวร์โดยใช้ GPU overlay"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_SUNXI,
   "ไดรเวอร์วิดีโอ Sunxi ระดับล่างที่ใช้บล็อก G2D ใน Allwinner SoC"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_WIIU,
   "ไดรเวอร์ Wii U รองรับรูปแบบ Shader แบบ Slang"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_SWITCH,
   "ไดรเวอร์ Switch รองรับรูปแบบ Shader แบบ GLSL"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_VG,
   "ไดรเวอร์ OpenVG ใช้ API กราฟิกเวกเตอร์ 2D แบบเร่งความเร็วด้วยฮาร์ดแวร์ของ OpenVG"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_GDI,
   "ไดรเวอร์ GDI ใช้ส่วนประสานงานของ Windows รุ่นเก่า ไม่แนะนำให้ใช้งาน"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_NO_DETAILS,
   "ไดรเวอร์วิดีโอปัจจุบัน"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_DRIVER,
   "เสียง"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_DRIVER,
   "ไดรเวอร์เสียงที่ใช้งาน"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_RSOUND,
   "ไดรเวอร์ RSound สำหรับระบบเสียงผ่านเครือข่าย"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_OSS,
   "ไดรเวอร์ Open Sound System รุ่นเก่า"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_ALSA,
   "ไดรเวอร์ ALSA มาตรฐาน"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_ALSATHREAD,
   "ไดรเวอร์ ALSA ที่รองรับการทำงานแบบ Threading"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_TINYALSA,
   "ไดรเวอร์ ALSA ที่ทำงานโดยไม่มีส่วนที่ขึ้นต่อกัน"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_ROAR,
   "ไดรเวอร์ระบบเสียง RoarAudio"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_AL,
   "ไดรเวอร์ OpenAL"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_SL,
   "ไดรเวอร์ OpenSL"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_DSOUND,
   "ไดรเวอร์ DirectSound ที่ใช้งานเป็นหลักบน Windows 95 จนถึง Windows XP"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_WASAPI,
   "ไดรเวอร์ Windows Audio Session API ซึ่ง WASAPI ใช้งานเป็นหลักบน Windows 7 ขึ้นไป"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_PULSE,
   "ไดรเวอร์ PulseAudio หากระบบใช้ PulseAudio ควรเลือกใช้ไดรเวอร์นี้แทนไดรเวอร์อื่นอย่างเช่น ALSA"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_PIPEWIRE,
   "ไดรเวอร์ PipeWire หากระบบใช้ PipeWire ควรเลือกใช้ไดรเวอร์นี้แทนไดรเวอร์อื่นอย่างเช่น PulseAudio"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_JACK,
   "ไดรเวอร์ Jack Audio Connection Kit"
   )
#ifdef HAVE_MICROPHONE
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_DRIVER,
   "ไมโครโฟน"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MICROPHONE_DRIVER,
   "ไดรเวอร์ไมโครโฟนที่ใช้งาน"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_RESAMPLER_DRIVER,
   "ความถี่สัญญาณเสียงไมโครโฟน"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MICROPHONE_RESAMPLER_DRIVER,
   "ไดรเวอร์ตัวเลือกการเปลี่ยนความถี่สัญญาณเสียงไมโครโฟนที่ใช้งาน"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_BLOCK_FRAMES,
   "ขนาดบล็อกข้อมูลไมโครโฟน"
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_RESAMPLER_DRIVER,
   "เปลี่ยนความถี่สัญญาณเสียง"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_RESAMPLER_DRIVER,
   "ไดรเวอร์การเปลี่ยนความถี่สัญญาณเสียงที่ใช้งาน"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_RESAMPLER_DRIVER_SINC,
   "การประมวลผลแบบ Windowed Sinc"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_RESAMPLER_DRIVER_CC,
   "การประมวลผลแบบ Convoluted Cosine"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_RESAMPLER_DRIVER_NEAREST,
   "การประมวลผลแบบ Nearest ซึ่งตัวปรับความถี่สัญญาณนี้จะละเว้นการตั้งค่าคุณภาพทั้งหมด"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CAMERA_DRIVER,
   "กล้อง"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CAMERA_DRIVER,
   "ไดรเวอร์กล้องที่ใช้งาน"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BLUETOOTH_DRIVER,
   "บลูทูธ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BLUETOOTH_DRIVER,
   "ไดรเวอร์บลูทูธที่ใช้งาน"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_WIFI_DRIVER,
   "ไดรเวอร์ Wi-Fi ที่ใช้งาน"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOCATION_DRIVER,
   "พิกัดตำแหน่ง"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOCATION_DRIVER,
   "ไดรเวอร์พิกัดตำแหน่งที่ใช้งาน"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_DRIVER,
   "เมนู"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_DRIVER,
   "ไดรเวอร์เมนูที่ใช้งาน (ต้องรีสตาร์ท)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_MENU_DRIVER_XMB,
   "XMB คือหน้าเมนู RetroArch ที่มีรูปแบบเหมือนเมนูเครื่องคอนโซลยุคที่ 7 รองรับคุณสมบัติต่างๆ ได้เช่นเดียวกับ Ozone"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_MENU_DRIVER_OZONE,
   "Ozone คือหน้าเมนูหลักของ RetroArch ในเกือบทุกแพลตฟอร์ม ซึ่งถูกปรับแต่งมาเพื่อให้ควบคุมได้ง่ายด้วยจอยคอนโทรลเลอร์"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_MENU_DRIVER_RGUI,
   "RGUI คือหน้าเมนูพื้นฐานของ RetroArch ที่กินทรัพยากรเครื่องต่ำที่สุด และสามารถใช้งานร่วมกับจอภาพที่มีความละเอียดต่ำได้ดี"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_MENU_DRIVER_MATERIALUI,
   "บนอุปกรณ์มือถือ RetroArch จะใช้ MaterialUI เป็นหน้าเมนูหลัก โดยถูกออกแบบมาเพื่อรองรับหน้าจอสัมผัสและอุปกรณ์ชี้ตำแหน่งอย่างเมาส์หรือแทร็กบอลโดยเฉพาะ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RECORD_DRIVER,
   "บันทึกวิดีโอ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RECORD_DRIVER,
   "ไดรเวอร์การบันทึกที่ใช้งาน"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIDI_DRIVER,
   "ไดรเวอร์ MIDI ที่ใช้งาน"
   )

/* Settings > Video */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CRT_SWITCHRES_SETTINGS,
   "ปรับความละเอียดจอ CRT"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CRT_SWITCHRES_SETTINGS,
   "ส่งสัญญาณความละเอียดต่ำแบบ Native เพื่อใช้กับหน้าจอ CRT"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_OUTPUT_SETTINGS,
   "ขาออก"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_OUTPUT_SETTINGS,
   "เปลี่ยน drivers ที่ระบบใช้งาน"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FULLSCREEN_MODE_SETTINGS,
   "โหมดเต็มจอ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FULLSCREEN_MODE_SETTINGS,
   "เปลี่ยนการตั้งค่าโหมดเต็มจอ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOWED_MODE_SETTINGS,
   "โหมดหน้าต่าง"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOWED_MODE_SETTINGS,
   "เปลี่ยนการตั้งค่าโหมดหน้าต่าง"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SCALING_SETTINGS,
   "สัดส่วนภาพ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SCALING_SETTINGS,
   "เปลี่ยนการตั้งค่าการปรับสัดส่วนภาพ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_HDR_SETTINGS,
   "เปลี่ยนการตั้งค่า HDR ของวิดีโอ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SYNCHRONIZATION_SETTINGS,
   "ซิงโครไนซ์"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SYNCHRONIZATION_SETTINGS,
   "เปลี่ยนการตั้งค่าการซิงโครไนซ์วิดีโอ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SUSPEND_SCREENSAVER_ENABLE,
   "ไม่พักหน้าจอ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SUSPEND_SCREENSAVER_ENABLE,
   "ป้องกันการเปิดใช้งานโปรแกรมพักหน้าจอของระบบ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_SUSPEND_SCREENSAVER_ENABLE,
   "ระงับการทำงานของโปรแกรมพักหน้าจอ ซึ่งเป็นเพียงคำสั่งแนะนำที่ไดรเวอร์วิดีโอไม่จำเป็นต้องปฏิบัติตามเสมอไป"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_THREADED,
   "วิดีโอแบบแยกเธรด"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_THREADED,
   "เพิ่มประสิทธิภาพโดยแลกกับความหน่วงและอาการภาพกระตุกที่มากขึ้น ควรใช้เมื่อไม่สามารถทำความเร็วได้เต็มสปีดด้วยวิธีอื่นเท่านั้น"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_THREADED,
   "ใช้งานไดรเวอร์วิดีโอแบบแยกเธรด การใช้งานส่วนนี้อาจช่วยเพิ่มประสิทธิภาพ แต่ต้องแลกกับความหน่วงและอาการภาพกระตุกที่อาจเพิ่มมากขึ้น"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_BLACK_FRAME_INSERTION,
   "แทรกเฟรมดำ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_BLACK_FRAME_INSERTION,
   "คำเตือน: การกระพริบอย่างรวดเร็วอาจทำให้เกิดอาการภาพค้างหน้าจอในจอภาพบางรุ่น โปรดใช้งานด้วยความระมัดระวัง // แทรกเฟรมดำระหว่างเฟรมภาพ ช่วยลดความเบลอขณะเคลื่อนไหวได้อย่างมากโ[...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_BLACK_FRAME_INSERTION,
   "แทรกเฟรมดำระหว่างเฟรมภาพเพื่อเพิ่มความคมชัดของการเคลื่อนไหว ให้เลือกใช้ตัวเลือกที่กำหนดไว้สำหรับอัตราการรีเฟรชหน้าจอที่ใช้อยู่ในขณะนี้เท่านั้น ห้ามใช้กับอัตราการรีเฟรชท[...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_BFI_DARK_FRAMES,
   "แทรกเฟรมดำ - จำนวนเฟรมมืด"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_BFI_DARK_FRAMES,
   "ปรับจำนวนเฟรมดำในลำดับการสแกน BFI ทั้งหมด ยิ่งมากยิ่งเพิ่มความคมชัดของการเคลื่อนไหว ยิ่งน้อยยิ่งได้ความสว่างที่สูงขึ้น ไม่สามารถใช้ได้ที่ 120Hz เนื่องจากมีเฟรม BFI ให้ใช้งานได้เพีย[...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_BFI_DARK_FRAMES,
   "ปรับจำนวนเฟรมมืดที่แสดงในลำดับ BFI ยิ่งเฟรมมืดมากจะยิ่งเพิ่มความคมชัดของการเคลื่อนไหวแต่จะลดความสว่างลง ไม่สามารถใช้ได้ที่ 120Hz เนื่องจากมีเฟรมส่วนเกินเพียงเฟรมเดียวจากฐาน 60Hz ซ[...]"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_SUBFRAMES,
   "คำเตือน: การกะพริบอย่างรวดเร็วอาจทำให้เกิดอาการภาพค้างในจอภาพบางรุ่น โปรดใช้งานด้วยความเสี่ยงของท่านเอง // จำลองการสแกนภาพแบบ Rolling Scanline พื้นฐานผ่านซับเฟรมหลายชุด โดยการแบ่งหน้าจ[...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_SHADER_SUBFRAMES,
   "แทรกเชดเดอร์เฟรมส่วนเกินระหว่างเฟรมภาพ สำหรับเอฟเฟกต์เชดเดอร์ใดก็ตามที่ออกแบบมาให้ทำงานด้วยความเร็วที่สูงกว่าอัตราเฟรมของเนื้อหา ให้เลือกใช้ตัวเลือกที่กำหนดไว้สำหรับอัต[...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SCAN_SUBFRAMES,
   "จำลองเส้นสแกนแบบเลื่อน"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SCAN_SUBFRAMES,
   "คำเตือน: การกระพริบอย่างรวดเร็วอาจทำให้เกิดอาการภาพค้างหน้าจอในจอภาพบางรุ่น โปรดใช้งานด้วยความระมัดระวัง // จำลองการสแกนภาพแบบ Rolling Scanline พื้นฐานผ่านซับเฟรมหลายชุด โดยการแบ่งหน้า[...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_SCAN_SUBFRAMES,
   "จำลองการสแกนภาพแบบ Rolling Scanline พื้นฐานผ่านซับเฟรมหลายชุด โดยการแบ่งหน้าจอตามแนวตั้งและเรนเดอร์หน้าจอแต่ละส่วนไล่จากด้านบนลงมาตามจำนวนของซับเฟรมที่มีอยู่"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SMOOTH,
   "กรองภาพแบบไบลิเนียร์"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SMOOTH,
   "เพิ่มความเบลอเล็กน้อยให้กับภาพเพื่อลดความคมของขอบพิกเซล ตัวเลือกนี้ส่งผลกระทบต่อประสิทธิภาพการทำงานน้อยมาก และควรปิดการใช้งานหากมีการใช้เชดเดอร์ร่วมด้วย"
   )
#if defined(DINGUX)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_IPU_FILTER_TYPE,
   "ประมาณค่าพิกเซล"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_DINGUX_IPU_FILTER_TYPE,
   "ระบุวิธีการประมาณค่าพิกเซล เมื่อมีการปรับขนาดภาพผ่านหน่วยประมวลผลภาพภายใน (IPU) แนะนำให้ใช้แบบ 'Bicubic' หรือ 'Bilinear' หากมีการใช้ฟิลเตอร์วิดีโอที่ทำงานผ่าน CPU ตัวเลือกนี้ไม่ส่งผลกระทบต่อ[...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_IPU_FILTER_BICUBIC,
   "ไบคิวบิก"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_IPU_FILTER_BILINEAR,
   "ไบลิเนียร์"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_IPU_FILTER_NEAREST,
   "เนียร์เรสต์ เนเบอร์"
   )
#if defined(RS90) || defined(MIYOO)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_RS90_SOFTFILTER_TYPE,
   "ประมาณค่าพิกเซล"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_DINGUX_RS90_SOFTFILTER_TYPE,
   "ระบุวิธีการประมาณค่าพิกเซลที่จะใช้งานเมื่อปิดการใช้งาน 'การขยายขนาดแบบเลขจำนวนเต็ม' โดยการเลือกแบบ 'จุดที่ใกล้ที่สุด'จะส่งผลกระทบต่อประสิทธิภาพการทำงานน้อยที่สุด"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_RS90_SOFTFILTER_POINT,
   "จุดที่ใกล้ที่สุด"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_RS90_SOFTFILTER_BRESENHAM_HORZ,
   "กึ่งเชิงเส้น"
   )
#endif
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_DELAY,
   "การหน่วงเวลาเชดเดอร์อัตโนมัติ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_DELAY,
   "หน่วงเวลาการโหลดเชดเดอร์อัตโนมัติ (หน่วยเป็นมิลลิวินาที) ซึ่งช่วยแก้ปัญหาการแสดงผลผิดปกติเมื่อใช้ซอฟต์แวร์บันทึกหน้าจอได้"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FILTER,
   "ตัวกรองวิดีโอ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FILTER,
   "ใช้ตัวกรองวิดีโอที่ประมวลผลผ่านหน่วยประมวลผลกลาง ซึ่งอาจทำให้ประสิทธิภาพการทำงานลดลงอย่างมาก โดยตัวกรองวิดีโอบางประเภทอาจรองรับเฉพาะแกนประมวลผลที่ใช้ระบบสีแบบ 32 บิต หรือ 16 [...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_FILTER,
   "ใช้ตัวกรองวิดีโอที่ประมวลผลผ่านหน่วยประมวลผลกลาง ซึ่งอาจทำให้ประสิทธิภาพการทำงานลดลงอย่างมาก โดยตัวกรองวิดีโอบางประเภทอาจรองรับเฉพาะแกนประมวลผลที่ใช้ระบบสีแบบ 32 บิต หรือ 16 [...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_FILTER_BUILTIN,
   "ใช้ตัวกรองวิดีโอที่ประมวลผลผ่านหน่วยประมวลผลกลาง ซึ่งอาจทำให้ประสิทธิภาพการทำงานลดลงอย่างมาก โดยตัวกรองวิดีโอบางประเภทอาจรองรับเฉพาะแกนประมวลผลที่ใช้ระบบสีแบบ 32 บิต หรือ 16 [...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FILTER_REMOVE,
   "ยกเลิกตัวกรองวิดีโอ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FILTER_REMOVE,
   "ยกเลิกการโหลดตัวกรองวิดีโอที่กำลังใช้งาน"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_NOTCH_WRITE_OVER,
   "เปิดใช้งานการแสดงผลเต็มหน้าจอทับส่วนรอยบากในอุปกรณ์ Android และ iOS"
)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_USE_METAL_ARG_BUFFERS,
   "ใช้งานเมทัลอาร์กิวเมนต์บัฟเฟอร์ (ต้องเริ่มระบบใหม่)"
)
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_USE_METAL_ARG_BUFFERS,
   "พยายามเพิ่มประสิทธิภาพการทำงานโดยใช้งานเมทัลอาร์กิวเมนต์บัฟเฟอร์ ซึ่งจำเป็นสำหรับแกนประมวลผลบางตัว แต่อาจทำให้เชดเดอร์บางส่วนทำงานผิดปกติ โดยเฉพาะบนฮาร์ดแวร์หรือระบบปฏิ[...]"
)

/* Settings > Video > CRT SwitchRes */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CRT_SWITCH_RESOLUTION,
   "ปรับความละเอียดจอ CRT"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CRT_SWITCH_RESOLUTION,
   "สำหรับจอแสดงผลแบบ CRT เท่านั้น โดยจะพยายามใช้ความละเอียดและอัตราการรีเฟรชที่ตรงตามค่าเริ่มต้นของแกนประมวลผลหรือเกมนั้นๆ ออกมาให้แม่นยำที่สุด"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CRT_SWITCH_RESOLUTION_SUPER,
   "ความละเอียดระดับซูเปอร์สำหรับจอ CRT"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CRT_SWITCH_RESOLUTION_SUPER,
   "สลับระหว่างความละเอียดดั้งเดิมและความละเอียดระดับซูเปอร์แบบกว้างพิเศษ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CRT_SWITCH_X_AXIS_CENTERING,
   "จัดวางกึ่งกลางแนวนอน"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CRT_SWITCH_X_AXIS_CENTERING,
   "เลือกสลับตัวเลือกเหล่านี้ หากภาพแสดงผลไม่ตรงกึ่งกลางหน้าจออย่างเหมาะสม"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CRT_SWITCH_PORCH_ADJUST,
   "ขนาดแนวนอน"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CRT_SWITCH_PORCH_ADJUST,
   "เลือกสลับตัวเลือกเหล่านี้เพื่อปรับค่าแนวนอนในการเปลี่ยนขนาดภาพ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CRT_SWITCH_VERTICAL_ADJUST,
   "จัดวางกึ่งกลางแนวตั้ง"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CRT_SWITCH_VERTICAL_ADJUST,
   "ลองสลับตัวเลือกเหล่านี้ดู หากภาพบนหน้าจอแสดงผลไม่ตรงกึ่งกลาง"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CRT_SWITCH_HIRES_MENU,
   "ใช้เมนูความละเอียดสูง"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CRT_SWITCH_HIRES_MENU,
   "สลับไปใช้ Modeline ความละเอียดสูง เพื่อใช้งานกับเมนูที่มีความละเอียดสูงในขณะที่ยังไม่ได้โหลดเนื้อหาใดๆ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CRT_SWITCH_RESOLUTION_USE_CUSTOM_REFRESH_RATE,
   "กำหนดอัตราการรีเฟรชเอง"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CRT_SWITCH_RESOLUTION_USE_CUSTOM_REFRESH_RATE,
   "ใช้ค่ารีเฟรชหน้าจอตามที่กำหนดไว้ในไฟล์ตั้งค่าหากจำเป็น"
   )

/* Settings > Video > Output */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MONITOR_INDEX,
   "ลำดับจอภาพ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MONITOR_INDEX,
   "เลือกจอภาพที่ต้องการใช้งาน"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_MONITOR_INDEX,
   "เลือกจอภาพที่ต้องการใช้งาน โดย 0 (ค่าเริ่มต้น) หมายถึงไม่ระบุเจาะจง, 1 ขึ้นไป (1 คือจอภาพแรก) จะเป็นการกำหนดให้ RetroArch ใช้จอภาพนั้นๆ"
   )
#if defined (WIIU)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WIIU_PREFER_DRC,
   "ปรับแต่งสำหรับ Wii U GamePad (จำเป็นต้องเริ่มระบบใหม่)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WIIU_PREFER_DRC,
   "ใช้ขนาดหน้าจอ GamePad แบบ 2x เป็นพอร์ตการมองเห็น ปิดใช้งานเพื่อแสดงผลที่ความละเอียดค่าเดิมของทีวี"
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ROTATION,
   "การหมุนวิดีโอ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_ROTATION,
   "บังคับการหมุนวิดีโอ โดยการหมุนนี้จะถูกเพิ่มเข้าไปในการหมุนที่ Core กำหนดไว้"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCREEN_ORIENTATION,
   "การจัดวางหน้าจอ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SCREEN_ORIENTATION,
   "บังคับการจัดวางหน้าจอจากระบบปฏิบัติการโดยเฉพาะ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_GPU_INDEX,
   "ลำดับ GPU"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_GPU_INDEX,
   "เลือกลำดับ GPU ที่ต้องการใช้งาน"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_OFFSET_X,
   "การชดเชยหน้าจอแนวนอน"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_OFFSET_X,
   "บังคับออฟเซ็ตวิดีโอในแนวนอน โดยจะมีผลย้อนหลังทั้งหมด"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_OFFSET_Y,
   "การชดเชยหน้าจอแนวตั้ง"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_OFFSET_Y,
   "บังคับการชดเชยตำแหน่งวิดีโอในแนวตั้ง โดยการชดเชยนี้จะถูกนำไปใช้กับทุกส่วนของระบบ Global"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_REFRESH_RATE,
   "อัตราการรีเฟรชแนวตั้ง"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_REFRESH_RATE,
   "อัตราการรีเฟรชแนวตั้งของหน้าจอ ใช้สำหรับคำนวณอัตราอินพุตเสียงที่เหมาะสม\nค่านี้จะถูกละเว้นหากเปิดใช้งาน 'วิดีโอแบบแยกเธรด' อยู่"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_REFRESH_RATE_AUTO,
   "อัตราการรีเฟรชหน้าจอโดยประมาณ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_REFRESH_RATE_AUTO,
   "ค่ารีเฟรชหน้าจอโดยประมาณที่แม่นยำในหน่วย Hz"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_REFRESH_RATE_AUTO,
   "อัตราการรีเฟรชที่แม่นยำของหน้าจอ (Hz) ค่านี้ใช้สำหรับคำนวณอัตราอินพุตเสียงด้วยสูตร:\naudio_input_rate = game input rate * display refresh rate / game refresh rate\nหากแกนประมวลผลไม่รายงานค่าใดๆ จะมีการใช้ค่าเริ่มต้นแบบ NTSC เพื่อค[...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_REFRESH_RATE_POLLED,
   "ตั้งค่าอัตราการรีเฟรชตามที่หน้าจอตรวจพบ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_REFRESH_RATE_POLLED,
   "อัตราการรีเฟรชตามที่ไดรเวอร์จอแสดงผลตรวจพบ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_AUTOSWITCH_REFRESH_RATE,
   "สลับอัตราการรีเฟรชโดยอัตโนมัติ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_AUTOSWITCH_REFRESH_RATE,
   "สลับอัตราการรีเฟรชหน้าจอโดยอัตโนมัติตามเนื้อหาปัจจุบัน"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_AUTOSWITCH_REFRESH_RATE_EXCLUSIVE_FULLSCREEN,
   "เฉพาะในโหมดเต็มหน้าจอแบบ Exclusive เท่านั้น"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_AUTOSWITCH_REFRESH_RATE_WINDOWED_FULLSCREEN,
   "เฉพาะในโหมดหน้าต่างเต็มจอเท่านั้น"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_AUTOSWITCH_REFRESH_RATE_ALL_FULLSCREEN,
   "ทุกโหมดเต็มหน้าจอ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_AUTOSWITCH_PAL_THRESHOLD,
   "เกณฑ์การสลับอัตราการรีเฟรชอัตโนมัติสำหรับ PAL"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_AUTOSWITCH_PAL_THRESHOLD,
   "อัตราการรีเฟรชสูงสุดที่จะถือว่าเป็น PAL"
   )
#if defined(DINGUX) && defined(DINGUX_BETA)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_REFRESH_RATE,
   "อัตราการรีเฟรชแนวตั้ง"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_DINGUX_REFRESH_RATE,
   "ตั้งค่าอัตราการรีเฟรชแนวตั้งของหน้าจอ การตั้งค่าเป็น '50 Hz' จะช่วยให้วิดีโอไหลลื่นเมื่อรันเนื้อหาที่เป็น PAL"
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FORCE_SRGB_DISABLE,
   "บังคับปิดใช้งาน sRGB FBO"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FORCE_SRGB_DISABLE,
   "บังคับปิดใช้งานการรองรับ sRGB FBO เนื่องจากไดรเวอร์ Intel OpenGL บางตัวบน Windows มีปัญหาในการแสดงผลวิดีโอเมื่อใช้ sRGB FBO การเปิดตัวเลือกนี้จะช่วยเลี่ยงปัญหาดังกล่าวได้"
   )

/* Settings > Video > Fullscreen Mode */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FULLSCREEN,
   "การแสดงผลแบบเต็มหน้าจอ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FULLSCREEN,
   "แสดงผลแบบเต็มหน้าจอ สามารถเปลี่ยนได้ในขณะที่โปรแกรมทำงาน และสามารถเขียนทับได้ด้วยคำสั่งผ่าน command line"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOWED_FULLSCREEN,
   "โหมดหน้าต่างเต็มหน้าจอ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOWED_FULLSCREEN,
   "หากอยู่ในโหมดเต็มหน้าจอ จะเลือกใช้หน้าต่างแบบเต็มหน้าจอเพื่อป้องกันไม่ให้เกิดการสลับโหมดการแสดงผล"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FULLSCREEN_X,
   "ความกว้างเต็มหน้าจอ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FULLSCREEN_X,
   "กำหนดความกว้างแบบกำหนดเองสำหรับโหมดเต็มหน้าจอที่ไม่ใช่แบบหน้าต่าง หากปล่อยว่างไว้จะใช้ความละเอียดตามหน้าจอเดสก์ท็อป"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FULLSCREEN_Y,
   "ความสูงเต็มหน้าจอ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FULLSCREEN_Y,
   "กำหนดความสูงแบบกำหนดเองสำหรับโหมดเต็มหน้าจอที่ไม่ใช่แบบหน้าต่าง หากปล่อยว่างไว้จะใช้ความละเอียดตามหน้าจอเดสก์ท็อป"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FORCE_RESOLUTION,
   "บังคับความละเอียดบน UWP"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FORCE_RESOLUTION,
   "บังคับความละเอียดให้เป็นขนาดเต็มหน้าจอ หากตั้งค่าเป็น 0 จะใช้ค่าคงที่ที่ 3840 x 2160"
   )

/* Settings > Video > Windowed Mode */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SCALE,
   "ขนาดสัดส่วนหน้าต่าง"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_SCALE,
   "ตั้งค่าขนาดหน้าต่างตามจำนวนเท่า ของขนาดช่องมองภาพ Core ที่กำหนด"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_OPACITY,
   "ความโปร่งใสของหน้าต่าง"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_OPACITY,
   "ตั้งค่าความโปร่งใสของหน้าต่าง"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_SHOW_DECORATIONS,
   "แสดงแถบเครื่องมือหน้าต่าง"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_SHOW_DECORATIONS,
   "แสดงแถบชื่อและขอบหน้าต่าง"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UI_MENUBAR_ENABLE,
   "แสดงแถบเมนู"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_UI_MENUBAR_ENABLE,
   "แสดงแถบเมนูหน้าต่าง"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_SAVE_POSITION,
   "จำตำแหน่งและขนาดหน้าต่าง"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_SAVE_POSITION,
   "แสดงเนื้อหาทั้งหมดในหน้าต่างที่มีขนาดคงที่ตามความกว้างและความสูงที่กำหนดไว้ และบันทึกขนาดกับตำแหน่งของหน้าต่างล่าสุดเมื่อปิด RetroArch หากปิดการใช้งาน ขนาดหน้าต่างจะถูกปรับเปล[...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_CUSTOM_SIZE_ENABLE,
   "ใช้ขนาดหน้าต่างแบบกำหนดเอง"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_CUSTOM_SIZE_ENABLE,
   "แสดงเนื้อหาทั้งหมดในหน้าต่างที่มีขนาดคงที่ตามความกว้างและความสูงที่กำหนดไว้ หากปิดการใช้งาน ขนาดหน้าต่างจะถูกปรับตาม \"ขนาดสัดส่วนหน้าต่าง\" โดยอัตโนมัติ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_WIDTH,
   "ความกว้างหน้าต่าง"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_WIDTH,
   "ตั้งค่าความกว้างหน้าต่างแบบกำหนดเองสำหรับหน้าจอแสดงผล"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_HEIGHT,
   "ความสูงหน้าต่าง"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_HEIGHT,
   "ตั้งค่าความสูงหน้าต่างแบบกำหนดเองสำหรับหน้าจอแสดงผล"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_AUTO_WIDTH_MAX,
   "ความกว้างหน้าต่างสูงสุด"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_AUTO_WIDTH_MAX,
   "ตั้งค่าความกว้างสูงสุดของหน้าต่างแสดงผล เมื่อมีการปรับขนาดโดยอัตโนมัติตาม \"ขนาดสัดส่วนหน้าต่าง\""
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_AUTO_HEIGHT_MAX,
   "ความสูงหน้าต่างสูงสุด"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_AUTO_HEIGHT_MAX,
   "ตั้งค่าความสูงสูงสุดของหน้าต่างแสดงผล เมื่อมีการปรับขนาดโดยอัตโนมัติตาม \"ขนาดสัดส่วนหน้าต่าง\""
   )

/* Settings > Video > Scaling */

MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SCALE_INTEGER,
   "ขยายขนาดวิดีโอเป็นขั้นแบบจำนวนเต็มเท่านั้น โดยขนาดเริ่มต้นจะขึ้นอยู่กับค่าเรขาคณิตและอัตราส่วนภาพที่ Core รายงานมา"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SCALE_INTEGER_AXIS,
   "ขยายสเกลทั้งความสูงหรือความกว้าง หรือทั้งความสูงและความกว้าง โดยการปรับแบบครึ่งขั้น (Half steps) จะใช้ได้กับแหล่งข้อมูลความละเอียดสูงเท่านั้น"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SCALE_INTEGER_SCALING,
   "ปัดลงหรือปัดขึ้นเป็นจำนวนเต็มถัดไป 'โหมดอัจฉริยะ' จะลดขนาดลงมา เมื่อภาพถูกตัดขอบมากเกินไป และสุดท้ายจะย้อนกลับไปใช้การขยายแบบไม่ใช่จำนวนเต็ม หากขอบที่เกิดจากการลดขนาดนั้นก[...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SCALE_INTEGER_SCALING_UNDERSCALE,
   "การลดขนาด"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SCALE_INTEGER_SCALING_OVERSCALE,
   "การขยายเกินขนาด"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SCALE_INTEGER_SCALING_SMART,
   "อัจฉริยะ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO_INDEX,
   "อัตราส่วนภาพ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_ASPECT_RATIO_INDEX,
   "ตั้งค่าอัตราส่วนภาพการแสดงผล"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO,
   "อัตราส่วนภาพตามค่าปรับแต่ง"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_ASPECT_RATIO,
   "ค่าตัวเลขทศนิยมสำหรับอัตราส่วนภาพวิดีโอ (ความกว้าง / ความสูง)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO_CONFIG,
   "ค่าปรับแต่ง"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO_CORE_PROVIDED,
   "Core เป็นตัวกำหนด"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO_CUSTOM,
   "กำหนดเอง"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO_FULL,
   "เต็มจอ"
   )
#if defined(DINGUX)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_IPU_KEEP_ASPECT,
   "คงอัตราส่วนภาพ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_DINGUX_IPU_KEEP_ASPECT,
   "คงอัตราส่วนพิกเซลแบบ 1:1 เมื่อปรับขนาดเนื้อหาผ่าน IPU ภายใน หากปิดใช้งาน ภาพจะถูกดึงให้เต็มหน้าจอ"
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_CUSTOM_X,
   "กำหนดอัตราส่วนภาพเอง (ตำแหน่ง X)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_CUSTOM_X,
   "ระยะออฟเซ็ตช่องมองภาพสำหรับกำหนดตำแหน่งแกน X"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_CUSTOM_Y,
   "อัตราส่วนภาพที่กำหนดเอง (ตำแหน่งแกน Y)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_CUSTOM_Y,
   "ระยะออฟเซ็ตช่องมองภาพ สำหรับกำหนดตำแหน่งแกน Y"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VIDEO_VIEWPORT_BIAS_X,
   "จุดยึดตำแหน่งช่องมองภาพบนแกน X"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_BIAS_X,
   "จุดยึดตำแหน่งช่องมองภาพบนแกน X"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VIDEO_VIEWPORT_BIAS_Y,
   "จุดยึดตำแหน่งช่องมองภาพบนแกน Y"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_BIAS_Y,
   "จุดยึดตำแหน่งช่องมองภาพบนแกน Y"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_BIAS_X,
   "ตำแหน่งแนวนอนของเนื้อหา เมื่อช่องมองภาพกว้างกว่าความกว้างของเนื้อหา โดย 0.0 คือชิดซ้ายสุด, 0.5 คือกึ่งกลาง และ 1.0 คือชิดขวาสุด"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_BIAS_Y,
   "ตำแหน่งแนวตั้งของเนื้อหา เมื่อช่องมองภาพสูงกว่าความสูงของเนื้อหา โดย 0.0 คือด้านบนสุด, 0.5 คือกึ่งกลาง และ 1.0 คือด้านล่างสุด"
   )
#if defined(RARCH_MOBILE)
MSG_HASH(
   MENU_ENUM_LABEL_VIDEO_VIEWPORT_BIAS_PORTRAIT_X,
   "จุดยึดตำแหน่งช่องมองภาพบนแกน X (แนวตั้ง)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_BIAS_PORTRAIT_X,
   "จุดยึดตำแหน่งช่องมองภาพบนแกน X (แนวตั้ง)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VIDEO_VIEWPORT_BIAS_PORTRAIT_Y,
   "จุดยึดตำแหน่งช่องมองภาพบนแกน Y (แนวตั้ง)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_BIAS_PORTRAIT_Y,
   "จุดยึดตำแหน่งช่องมองภาพบนแกน Y (แนวตั้ง)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_BIAS_PORTRAIT_X,
   "ตำแหน่งแนวนอนของเนื้อหา เมื่อช่องมองภาพกว้างกว่าความกว้างของเนื้อหา (แนวตั้ง) โดย 0.0 คือชิดซ้ายสุด, 0.5 คือกึ่งกลาง และ 1.0 คือชิดขวาสุด"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_BIAS_PORTRAIT_Y,
   "ตำแหน่งแนวตั้งของเนื้อหา เมื่อช่องมองภาพสูงกว่าความสูงของเนื้อหา (แนวตั้ง) โดย 0.0 คือด้านบนสุด, 0.5 คือกึ่งกลาง และ 1.0 คือด้านล่างสุด"
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_CUSTOM_WIDTH,
   "อัตราส่วนภาพที่กำหนดเอง (ความกว้าง)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_CUSTOM_WIDTH,
   "ความกว้างของช่องมองภาพที่กำหนดเอง ซึ่งจะถูกใช้เมื่อตั้งค่าอัตราส่วนภาพเป็น 'อัตราส่วนภาพที่กำหนดเอง'"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_CUSTOM_HEIGHT,
   "อัตราส่วนภาพที่กำหนดเอง (ความสูง)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_CUSTOM_HEIGHT,
   "ความสูงของช่องมองภาพที่กำหนดเอง ซึ่งจะถูกใช้เมื่อตั้งค่าอัตราส่วนภาพเป็น 'อัตราส่วนภาพที่กำหนดเอง'"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_CROP_OVERSCAN,
   "ตัดขอบส่วนเกิน (ต้องรีสตาร์ท)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_CROP_OVERSCAN,
   "ตัดส่วนขอบของภาพออกเล็กน้อย ซึ่งโดยปกติผู้พัฒนาจะเว้นว่างไว้หรือบางครั้งอาจมีพิกเซลขยะ ปรากฏอยู่"
   )

/* Settings > Video > HDR */

MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_HDR_ENABLE,
   "กำหนดโหมดการแสดงผลแบบ HDR หากหน้าจอรองรับ หมายเหตุ: การเลือกใช้ scRGB อาจทำให้ฟิลเตอร์ CRT shader ดูซอฟต์ลง เนื่องจากตัวจัดการหน้าจอของระบบปฏิบัติการจะแปลงค่าเป็น HDR10 หลังจากที่ใช้ฟิลเตอร์[...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_HDR_MODE_OFF,
   "ปิด"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_HDR_BRIGHTNESS_NITS,
   "ความสว่าง"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_HDR_BRIGHTNESS_NITS,
   "ความสว่างของเมนูในหน่วย cd/m2 (nits) เมื่อใช้งานหน้าจอแบบ HDR จะปรากฏให้เห็นเฉพาะเมื่อเปิดใช้งาน HDR ในส่วนของ การตั้งค่า > วิดีโอ > HDR เท่านั้น"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_HDR_PAPER_WHITE_NITS,
   "ความสว่าง"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_HDR_PAPER_WHITE_NITS,
   "กำหนดระดับความสว่าง HDR ในหน่วย nits โดยใช้ร่วมกับการตั้งค่าความสว่างทางกายภาพของหน้าจอ สำหรับจุดเริ่มต้นแนะนำให้ตั้งค่านี้ไว้ที่ 80 และปรับความสว่างของหน้าจอไปที่ระดับสูงสุด หรื[...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_HDR_EXPAND_GAMUT,
   "เร่งสี"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_HDR_EXPAND_GAMUT,
   "ใช้ช่วงสีทั้งหมดของหน้าจอเพื่อสร้างภาพที่สว่างและมีความอิ่มตัวของสีมากขึ้น หากต้องการสีสันที่ซื่อตรงต่อการออกแบบดั้งเดิมของเกม ให้ตั้งค่านี้เป็น แม่นยำ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_HDR_EXPAND_GAMUT_ACCURATE,
   "แม่นยำ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_HDR_EXPAND_GAMUT_EXPANDED,
   "ขยายเต็มจอ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_HDR_EXPAND_GAMUT_WIDE,
   "กว้าง"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_HDR_EXPAND_GAMUT_SUPER,
   "ใหญ่พิเศษ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_HDR_SCANLINES,
   "แสกนไลน์"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_HDR_SCANLINES,
   "เปิดใช้งาน HDR scanlines ซึ่งเป็นเหตุผลหลักในการใช้งาน HDR บน RetroArch เนื่องจากมวลรวมของเส้นสแกนไลน์ที่แม่นยำจะทำให้หน้าจอส่วนใหญ่ดับลง แต่ระบบ HDR จะช่วยชดเชยความสว่างที่สูญเสียไปในส่วนนั้[...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_HDR_SUBPIXEL_LAYOUT,
   "การจัดวางพิกเซลย่อย"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_HDR_SUBPIXEL_LAYOUT,
   "เลือกรูปแบบการจัดวางพิกเซลย่อยของหน้าจอคุณ ซึ่งจะมีผลต่อการแสดงผลของสแกนไลน์เท่านั้น หากคุณไม่ทราบว่าหน้าจอของคุณใช้รูปแบบใด สามารถตรวจสอบข้อมูล 'subpixel layout' ของรุ่นที่คุณใช้ได[...]"
   )


/* Settings > Video > Synchronization */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VSYNC,
   "ซิงค์แนวตั้ง (VSync)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_VSYNC,
   "ซิงค์การส่งสัญญาณภาพจากการ์ดจอ ให้ตรงกับอัตราการรีเฟรชของหน้าจอ (แนะนำ)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SWAP_INTERVAL,
   "ช่วงเวลาสลับ VSync"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SWAP_INTERVAL,
   "ใช้ช่วงเวลาสลับ VSync แบบกำหนดเอง ซึ่งจะลดอัตราการรีเฟรชของหน้าจอลงตาม x ที่ระบุ หากตั้งค่าเป็น 'อัตโนมัติ' ระบบจะเลือกตัวคูณตามเฟรมเรตที่ Core รายงาน ซึ่งช่วยให้จังหวะการแสดงผลเฟรม [...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SWAP_INTERVAL_AUTO,
   "อัตโนมัติ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ADAPTIVE_VSYNC,
   "VSync แบบปรับเปลี่ยนตามความเหมาะสม"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_ADAPTIVE_VSYNC,
   "'VSync' จะถูกเปิดใช้งานจนกว่าประสิทธิภาพจะลดลงต่ำกว่าอัตราการรีเฟรชเป้าหมาย สามารถช่วยลดอาการภาพกระตุกเมื่อประสิทธิภาพลดลงต่ำกว่าเวลาจริง และช่วยประหยัดพลังงานได้มากขึ้น ไม่ส[...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FRAME_DELAY,
   "การหน่วงเฟรม"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FRAME_DELAY,
   "ช่วยลดความหน่วงโดยแลกกับความเสี่ยงที่ภาพจะกระตุกมากขึ้น"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_FRAME_DELAY,
   "กำหนดจำนวนมิลลิวินาทีที่จะหยุดพักก่อนเริ่มการทำงานของ 'Core เป็นตัวกำหนด' หลังจากแสดงผลภาพ ช่วยลดความหน่วงโดยแลกกับความเสี่ยงที่จะเกิดภาพกระตุกมากขึ้น\nค่าตั้งแต่ 20 ขึ้นไปจะถูก[...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FRAME_DELAY_AUTO,
   "การหน่วงเฟรมอัตโนมัติ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FRAME_DELAY_AUTO,
   "ปรับค่า 'การหน่วงเฟรม' ที่ใช้งานอยู่โดยอัตโนมัติแบบไดนามิก"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_FRAME_DELAY_AUTO,
   "พยายามรักษาเป้าหมาย 'การหน่วงเฟรม' ที่ต้องการและลดการข้ามเฟรมให้เหลือน้อยที่สุด จุดเริ่มต้นคือ 3/4 ของเวลาเฟรมเมื่อ 'การหน่วงเฟรม' ถูกตั้งค่าเป็น 0 (อัตโนมัติ)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FRAME_DELAY_AUTOMATIC,
   "อัตโนมัติ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FRAME_DELAY_EFFECTIVE,
   "มีผล"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_HARD_SYNC,
   "การซิงค์ GPU อย่างเข้มงวด"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_HARD_SYNC,
   "ซิงค์ CPU และ GPU อย่างเข้มงวด ช่วยลดความหน่วงโดยแลกกับประสิทธิภาพที่ลดลง"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_HARD_SYNC_FRAMES,
   "เฟรมการซิงค์ GPU อย่างเข้มงวด"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_HARD_SYNC_FRAMES,
   "กำหนดจำนวนเฟรมที่ CPU สามารถทำงานล่วงหน้า GPU ได้ เมื่อใช้งาน 'การซิงค์ GPU อย่างเข้มงวด'"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_HARD_SYNC_FRAMES,
   "กำหนดจำนวนเฟรมที่ CPU สามารถทำงานล่วงหน้า GPU ได้ เมื่อใช้งาน 'การซิงค์ GPU อย่างเข้มงวด' โดยตั้งค่าได้สูงสุด 3\n 0: ซิงค์กับ GPU ทันที\n 1: ซิงค์กับเฟรมก่อนหน้า\n 2: และอื่นๆ ..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VRR_RUNLOOP_ENABLE,
   "ซิงค์กับเฟรมเรตที่แท้จริงของเนื้อหา (G-Sync, FreeSync)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VRR_RUNLOOP_ENABLE,
   "ไม่มีการเบี่ยงเบนจากจังหวะเวลาหลักที่ต้องการ ใช้สำหรับหน้าจอที่มีอัตรารีเฟรชแบบผันแปร (G-Sync, FreeSync, HDMI 2.1 VRR)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VRR_RUNLOOP_ENABLE,
   "ซิงค์กับเฟรมเรตที่แท้จริงของเนื้อหา ตัวเลือกนี้เทียบเท่ากับการบังคับความเร็ว x1 โดยที่ยังสามารถเร่งความเร็วไปข้างหน้าได้ ไม่มีการเบี่ยงเบนจากอัตรารีเฟรชที่ Core ต้องการ และไม่[...]"
   )

/* Settings > Audio */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_OUTPUT_SETTINGS,
   "ขาออก"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_OUTPUT_SETTINGS,
   "เปลี่ยนการตั้งค่าสัญญาณเสียงออก"
   )
#ifdef HAVE_MICROPHONE
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_SETTINGS,
   "ไมโครโฟน"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MICROPHONE_SETTINGS,
   "เปลี่ยนการตั้งค่าสัญญาณเสียงเข้า"
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_SYNCHRONIZATION_SETTINGS,
   "ซิงโครไนซ์"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_SYNCHRONIZATION_SETTINGS,
   "เปลี่ยนการตั้งค่าการซิงค์สัญญาณเสียง"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIDI_SETTINGS,
   "เปลี่ยนการตั้งค่าสัญญาณ MIDI"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_MIXER_SETTINGS,
   "เปลี่ยนการตั้งค่าตัวผสมสัญญาณเสียง"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SOUNDS,
   "เสียงเมนู"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SOUNDS,
   "เปลี่ยนเสียงเมนู"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_MUTE,
   "ปิดเสียง"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_MUTE,
   "ปิดสัญญาณเสียง"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_MIXER_MUTE,
   "ปิดเสียง Mixer"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_MIXER_MUTE,
   "ปิดเสียงตัวผสมสัญญาณเสียง"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_RESPECT_SILENT_MODE,
   "ให้ความสำคัญกับโหมดเงียบ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_RESPECT_SILENT_MODE,
   "ปิดเสียงทั้งหมดในโหมดเงียบ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_FASTFORWARD_MUTE,
   "ปิดเสียงขณะเร่งความเร็วไปข้างหน้า"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_FASTFORWARD_MUTE,
   "ปิดเสียงโดยอัตโนมัติเมื่อใช้การเร่งความเร็วไปข้างหน้า"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_FASTFORWARD_SPEEDUP,
   "เร่งความเร็วเสียงขณะ เร่งความเร็ว-ไปข้างหน้า"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_FASTFORWARD_SPEEDUP,
   "เร่งความเร็วเสียงขณะ เร่งความเร็ว-ไปข้างหน้า ช่วยป้องกันเสียงแตก แต่ระดับเสียงสูงต่ำจะเปลี่ยนไป"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_REWIND_MUTE,
   "ปิดเสียงขณะย้อนกลับ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_REWIND_MUTE,
   "ปิดเสียงโดยอัตโนมัติเมื่อมีการถอยหลัง"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_VOLUME,
   "ระดับการขยายเสียง (เดซิเบล)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_VOLUME,
   "ระดับความดังของเสียง (หน่วย dB) โดย 0 dB คือระดับเสียงปกติและไม่มีการเพิ่มการขยายเสียงใดๆ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_VOLUME,
   "ระดับความดังของเสียงในหน่วย dB โดยที่ 0 dB คือระดับเสียงปกติซึ่งไม่มีการเพิ่มการขยายเสียง สามารถควบคุมการขยายเสียงได้ในขณะใช้งานด้วยคำสั่งเพิ่มระดับเสียงเข้าหรือลดระดับเสียงเ[...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_MIXER_VOLUME,
   "ระดับการขยายเสียงของ Mixer (dB)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_MIXER_VOLUME,
   "ระดับการขยายเสียงรวมของ Mixer (dB) โดย 0 dB คือระดับเสียงปกติและไม่มีการเพิ่มการขยายเสียงใดๆ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_DSP_PLUGIN,
   "ปลั๊กอิน Audio DSP ที่ประมวลผลเสียงก่อนที่จะถูกส่งไปยังไดรเวอร์"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_DSP_PLUGIN_REMOVE,
   "ลบปลั๊กอิน DSP"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_DSP_PLUGIN_REMOVE,
   "ยกเลิกการโหลดปลั๊กอิน Audio DSP ที่กำลังใช้งานอยู่"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_WASAPI_EXCLUSIVE_MODE,
   "โหมด WASAPI Exclusive"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_WASAPI_EXCLUSIVE_MODE,
   "อนุญาตให้ไดรเวอร์ WASAPI เข้าควบคุมอุปกรณ์เสียงแบบเอกสิทธิ์ หากปิดใช้งาน จะใช้โหมดใช้งานร่วมแทน"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_WASAPI_FLOAT_FORMAT,
   "รูปแบบ Float ของ WASAPI"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_WASAPI_FLOAT_FORMAT,
   "ใช้รูปแบบ float สำหรับไดรเวอร์ WASAPI หากอุปกรณ์เสียงของคุณรองรับ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_WASAPI_SH_BUFFER_LENGTH,
   "ความยาวบัฟเฟอร์โหมดใช้งานร่วมของ WASAPI"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_WASAPI_SH_BUFFER_LENGTH,
   "ความยาวบัฟเฟอร์กลาง (หน่วยเป็นเฟรม) เมื่อใช้ไดรเวอร์ WASAPI ในโหมดใช้งานร่วม"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_ASIO_CONTROL_PANEL,
   "เปิดแผงควบคุม ASIO"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_ASIO_CONTROL_PANEL,
   "เปิดแผงควบคุมของไดรเวอร์ ASIO เพื่อกำหนดค่าการกำหนดเส้นทางอุปกรณ์ และการตั้งค่าบัฟเฟอร์"
   )

/* Settings > Audio > Output */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_ENABLE,
   "เสียง"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_ENABLE,
   "เปิดใช้งานเอาต์พุตเสียง"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_DEVICE,
   "อุปกรณ์"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_DEVICE,
   "แทนที่อุปกรณ์เสียงเริ่มต้นที่ไดรเวอร์เสียงใช้งาน โดยขึ้นอยู่กับไดรเวอร์"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DEVICE,
   "แทนที่อุปกรณ์เสียงค่าเริ่มต้นที่ไดรเวอร์เสียงใช้ โดยขึ้นอยู่กับไดรเวอร์"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DEVICE_ALSA,
   "ค่ากำหนดอุปกรณ์ PCM แบบกำหนดเองสำหรับไดรเวอร์ ALSA"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DEVICE_OSS,
   "ค่าพาธแบบกำหนดเองสำหรับไดรเวอร์ OSS (เช่น /dev/dsp)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DEVICE_JACK,
   "ค่าชื่อพอร์ตแบบกำหนดเองสำหรับไดรเวอร์ JACK (เช่น system:playback1,system:playback_2)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DEVICE_RSOUND,
   "ที่อยู่ IP แบบกำหนดเองของเซิร์ฟเวอร์ RSound สำหรับไดรเวอร์ RSound"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_LATENCY,
   "ความหน่วงเสียง (ms)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_LATENCY,
   "ความหน่วงเสียงสูงสุดในหน่วยมิลลิวินาที ไดรเวอร์จะพยายามรักษาความหน่วงจริงไว้ที่ 50% ของค่านี้ อาจไม่เป็นไปตามนี้หากไดรเวอร์เสียงไม่สามารถให้ค่าความหน่วงตามที่กำหนดได้"
   )

#ifdef HAVE_MICROPHONE
/* Settings > Audio > Input */
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_ENABLE,
   "ไมโครโฟน"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MICROPHONE_ENABLE,
   "เปิดใช้งานอินพุตเสียงใน Core ที่รองรับ โดยไม่มีภาระเพิ่มเติมหาก Core ไม่ได้ใช้ไมโครโฟน"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_DEVICE,
   "อุปกรณ์"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MICROPHONE_DEVICE,
   "แทนที่อุปกรณ์อินพุตค่าเริ่มต้นที่ไดรเวอร์ไมโครโฟนใช้ โดยขึ้นอยู่กับไดรเวอร์"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_MICROPHONE_DEVICE,
   "แทนที่อุปกรณ์อินพุตค่าเริ่มต้นที่ไดรเวอร์ไมโครโฟนใช้งาน โดยขึ้นอยู่กับไดรเวอร์"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_RESAMPLER_QUALITY,
   "คุณภาพตัวแปลงสัญญาณตัวอย่าง"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MICROPHONE_RESAMPLER_QUALITY,
   "ลดค่านี้เพื่อเน้นประสิทธิภาพ/ความหน่วงต่ำมากกว่าคุณภาพเสียง และเพิ่มค่าเพื่อให้ได้คุณภาพตัวแปลงสัญญาณตัวอย่างที่ดีขึ้นโดยแลกกับประสิทธิภาพ/ความหน่วงต่ำ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_INPUT_RATE,
   "อัตราอินพุตเริ่มต้น (Hz)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MICROPHONE_INPUT_RATE,
   "อัตราการสุ่มตัวอย่างอินพุตเสียง ใช้เมื่อ Core ไม่ได้ร้องขอค่าที่กำหนดไว้"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_LATENCY,
   "ความหน่วงอินพุตเสียง (ms)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MICROPHONE_LATENCY,
   "ความหน่วงอินพุตเสียงที่ต้องการในหน่วยมิลลิวินาที อาจไม่เป็นไปตามนี้หากไดรเวอร์ไมโครโฟนไม่สามารถให้ค่าความหน่วงตามที่กำหนดได้"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_WASAPI_EXCLUSIVE_MODE,
   "โหมด WASAPI Exclusive"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MICROPHONE_WASAPI_EXCLUSIVE_MODE,
   "อนุญาตให้ RetroArch ควบคุมอุปกรณ์ไมโครโฟนแต่เพียงผู้เดียว เมื่อใช้งานไดรเวอร์ไมโครโฟน WASAPI หากปิดใช้งาน RetroArch จะใช้งานในโหมดแชร์แทน"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_WASAPI_FLOAT_FORMAT,
   "รูปแบบ Float ของ WASAPI"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MICROPHONE_WASAPI_FLOAT_FORMAT,
   "ใช้อินพุต Floating-point สำหรับไดรเวอร์ WASAPI หากอุปกรณ์เสียงของคุณรองรับ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_WASAPI_SH_BUFFER_LENGTH,
   "ความยาวบัฟเฟอร์โหมดใช้งานร่วมของ WASAPI"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MICROPHONE_WASAPI_SH_BUFFER_LENGTH,
   "ความยาวบัฟเฟอร์กลาง (หน่วยเป็นเฟรม) เมื่อใช้ไดรเวอร์ WASAPI ในโหมดใช้งานร่วม"
   )
#endif

/* Settings > Audio > Resampler */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_RESAMPLER_QUALITY,
   "คุณภาพการสุ่มสัญญาณใหม่"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_RESAMPLER_QUALITY,
   "ลดค่านี้ลงเพื่อให้ความสำคัญกับประสิทธิภาพ/เวลาแฝงที่ต่ำกว่าเมื่อเทียบกับคุณภาพเสียง เพิ่มค่าเพื่อให้ได้คุณภาพเสียงที่ดีขึ้นโดยต้องแลกกับประสิทธิภาพ/เวลาแฝงที่ต่ำลง"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_OUTPUT_RATE,
   "อัตราสุ่มสัญญาณเสียงเอาต์พุต"
   )

/* Settings > Audio > Synchronization */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_SYNC,
   "ซิงโครไนซ์"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_SYNC,
   "ซิงโครไนซ์เสียง แนะนำ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_MAX_TIMING_SKEW,
   "ความคลาดเคลื่อนของเวลาสูงสุด"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_MAX_TIMING_SKEW,
   "การเปลี่ยนแปลงสูงสุดของอัตราอินพุตเสียง การเพิ่มค่านี้จะช่วยให้สามารถเปลี่ยนแปลงเวลาได้อย่างมาก แต่ต้องแลกมาด้วยระดับเสียงที่ไม่ถูกต้อง (เช่น การใช้งาน PAL cores บนจอแสดงผล NTSC)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_MAX_TIMING_SKEW,
   "ค่าความคลาดเคลื่อนของเวลาเสียงสูงสุด\nกำหนดการเปลี่ยนแปลงสูงสุดของอัตราอินพุต คุณอาจต้องการเพิ่มค่านี้เพื่อให้สามารถเปลี่ยนแปลงเวลาได้มาก เช่น การเรียกใช้คอร์ PAL บนจอแสดงผล [...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_RATE_CONTROL_DELTA,
   "การควบคุมอัตราเสียงแบบไดนามิก"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_RATE_CONTROL_DELTA,
   "ช่วยให้การซิงก์เสียงและวิดีโอราบรื่นขึ้น โดยแก้ไขความไม่สมบูรณ์ของจังหวะ หากปิดใช้งาน การซิงก์ที่ถูกต้องแทบจะไม่สามารถทำได้"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_RATE_CONTROL_DELTA,
   "การตั้งค่านี้เป็น 0 จะปิดการควบคุมอัตราเสียง ค่าที่ไม่ใช่ศูนย์จะควบคุมค่าเดลตาของการควบคุมอัตราเสียง\nกำหนดว่าความถี่อินพุตสามารถปรับได้แบบไดนามิกมากน้อยเพียงใด โดยอินพุตเ[...]"
   )

/* Settings > Audio > MIDI */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIDI_INPUT,
   "การควบคุม"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIDI_INPUT,
   "เลือกอุปกรณ์อินพุต"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_MIDI_INPUT,
   "ตั้งค่าอุปกรณ์อินพุต (ขึ้นอยู่กับไดรเวอร์) เมื่อเลือก 'ปิด' จะปิดการทำงานของ MIDI อินพุต สามารถพิมพ์ชื่ออุปกรณ์ได้เช่นกัน"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIDI_OUTPUT,
   "ขาออก"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIDI_OUTPUT,
   "เลือกอุปกรณ์เอาต์พุต"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_MIDI_OUTPUT,
   "ตั้งค่าอุปกรณ์เอาต์พุต (ขึ้นอยู่กับไดรเวอร์) เมื่อเลือก 'ปิด' จะปิดการทำงานของ MIDI เอาต์พุต สามารถพิมพ์ชื่ออุปกรณ์ได้เช่นกัน\nเมื่อเปิดใช้งาน MIDI เอาต์พุต และ Core รวมถึงเกม/แอป รองรับ MIDI [...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIDI_VOLUME,
   "ระดับเสียง"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIDI_VOLUME,
   "ตั้งค่าระดับเสียงเอาต์พุต (%)."
   )

/* Settings > Audio > Mixer Settings > Mixer Stream */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIXER_ACTION_PLAY,
   "เล่น"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIXER_ACTION_PLAY,
   "จะเริ่มเล่นสตรีมเสียง เมื่อเล่นเสร็จแล้ว จะลบสตรีมเสียงปัจจุบันออกจากหน่วยความจำ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIXER_ACTION_PLAY_LOOPED,
   "เล่น (วนซ้ำ)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIXER_ACTION_PLAY_LOOPED,
   "จะเริ่มเล่นสตรีมเสียง เมื่อเล่นเสร็จแล้ว จะวนซ้ำและเล่นแทร็กอีกครั้งตั้งแต่ต้น"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIXER_ACTION_PLAY_SEQUENTIAL,
   "เล่น (ตามลำดับ)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIXER_ACTION_PLAY_SEQUENTIAL,
   "จะเริ่มเล่นสตรีมเสียง เมื่อเล่นเสร็จแล้ว จะข้ามไปยังสตรีมเสียงถัดไปตามลำดับ และทำซ้ำพฤติกรรมนี้ เหมาะสำหรับโหมดการเล่นแบบอัลบั้ม"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIXER_ACTION_STOP,
   "หยุด"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIXER_ACTION_STOP,
   "การหยุดเล่นกระแสข้อมูลเสียงแต่จะไม่ลบข้อมูลออกจากหน่วยความจำ สามารถเริ่มเล่นใหม่ได้โดยการเลือก 'เล่น'"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIXER_ACTION_REMOVE,
   "ลบออก"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIXER_ACTION_REMOVE,
   "การหยุดเล่นสตรีมเสียงและลบข้อมูลออกจากหน่วยความจำทั้งหมด"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIXER_ACTION_VOLUME,
   "ระดับเสียง"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIXER_ACTION_VOLUME,
   "ปรับระดับความดังของสตรีมเสียง"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_STREAM_STATE_NONE,
   "สถานะ: ไม่พร้อมใช้งาน"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_STREAM_STATE_STOPPED,
   "สถานะ: หยุดการเล่น"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_STREAM_STATE_PLAYING,
   "สถานะ: กำลังเล่น"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_STREAM_STATE_PLAYING_LOOPED,
   "สถานะ: กำลังเล่น (วนซ้ำ)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_STREAM_STATE_PLAYING_SEQUENTIAL,
   "สถานะ: กำลังเล่น (ตามลำดับ)"
   )

/* Settings > Audio > Menu Sounds */

MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_ENABLE_MENU,
   "เล่นสตรีมเสียงพร้อมกันแม้จะอยู่ในเมนู"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SOUND_OK,
   "เปิดเสียง 'ตกลง'"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SOUND_CANCEL,
   "เปิดเสียง 'ยกเลิก'"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SOUND_NOTICE,
   "เปิดใช้งานเสียง 'แจ้งเตือน'"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SOUND_BGM,
   "เปิดใช้งานเสียง 'BGM'"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SOUND_SCROLL,
   "เปิดใช้งานเสียง 'เมื่อปัดหรือเลื่อน'"
   )

/* Settings > Input */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MAX_USERS,
   "จำนวนผู้ใช้สูงสุด"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_MAX_USERS,
   "จำนวนผู้ใช้สูงสุดที่รองรับโดย RetroArch"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_POLL_TYPE_BEHAVIOR,
   "พฤติกรรมการเรียกข้อมูล (จำเป็นต้องเริ่มระบบใหม่)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_POLL_TYPE_BEHAVIOR,
   "มีผลต่อการเรียกข้อมูลอินพุตใน RetroArch หากตั้งค่าเป็น 'เร็ว' หรือ 'ช้า' อาจช่วยลดความหน่วงได้ ทั้งนี้ขึ้นอยู่กับการกำหนดค่าของคุณ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_INPUT_POLL_TYPE_BEHAVIOR,
   "มีผลต่อการเรียกข้อมูลอินพุตภายใน RetroArch\nเร็ว - เรียกข้อมูลอินพุตก่อนที่จะมีการประมวลผลเฟรม\nปกติ - เรียกข้อมูลอินพุตเมื่อมีการร้องขอการเรียกข้อมูล\nช้า - เรียกข้อมูลอินพุตเมื่อมีก[...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_REMAP_BINDS_ENABLE,
   "เปลี่ยนการตั้งค่าปุ่มสำหรับ Core นี้"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_REMAP_BINDS_ENABLE,
   "แทนที่การตั้งค่าปุ่มอินพุตด้วยการเปลี่ยนการตั้งค่าปุ่มที่กำหนดไว้สำหรับ Core ปัจจุบัน แทนที่การตั้งค่าปุ่มอินพุตด้วยการเปลี่ยนการตั้งค่าปุ่มที่กำหนดไว้สำหรับ Core ปัจจุบัน"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_REMAP_SORT_BY_CONTROLLER_ENABLE,
   "เรียงลำดับการเปลี่ยนการตั้งค่าปุ่มตาม Gamepad"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_REMAP_SORT_BY_CONTROLLER_ENABLE,
   "การเปลี่ยนการตั้งค่าปุ่มจะมีผลกับ Gamepad ที่ใช้งานขณะบันทึกเท่านั้น"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_AUTODETECT_ENABLE,
   "กำหนดค่าอัตโนมัติ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_AUTODETECT_ENABLE,
   "กำหนดค่าคอนโทรลเลอร์ที่มีโปรไฟล์ให้โดยอัตโนมัติ ในรูปแบบเสียบปลั๊กแล้วใช้งานได้เลย"
   )
#if defined(HAVE_DINPUT) || defined(HAVE_WINRAWINPUT)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_NOWINKEY_ENABLE,
   "ปิดการใช้งานปุ่มลัดของ Windows (จำเป็นต้องเริ่มระบบใหม่)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_NOWINKEY_ENABLE,
   "คงการใช้งานคีย์ผสมของปุ่ม Windows ไว้ภายในแอปพลิเคชัน"
   )
#endif
#ifdef ANDROID
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_SELECT_PHYSICAL_KEYBOARD,
   "เลือกคีย์บอร์ดที่เชื่อมต่ออยู่"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_SELECT_PHYSICAL_KEYBOARD,
   "ใช้เครื่องมือนี้เป็นคีย์บอร์ดแทนที่จะเป็น Gamepad"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_INPUT_SELECT_PHYSICAL_KEYBOARD,
   "หาก RetroArch ระบุว่าคีย์บอร์ดฮาร์ดแวร์เป็น Gamepad ชนิดหนึ่ง การตั้งค่านี้สามารถใช้เพื่อบังคับให้ RetroArch ปฏิบัติกับอุปกรณ์ที่ระบุผิดนั้นเป็นคีย์บอร์ด\nสิ่งนี้จะมีประโยชน์หากคุณกำลังพยาย[...]"
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_SENSORS_ENABLE,
   "อินพุตเซนเซอร์เสริม"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_SENSORS_ENABLE,
   "เปิดใช้งานอินพุตจากเซนเซอร์ตรวจจับความเร่ง เซนเซอร์ตรวจจับลักษณะการหมุน และเซนเซอร์วัดความสว่าง หากฮาร์ดแวร์ปัจจุบันรองรับ อาจมีผลกระทบต่อประสิทธิภาพและ/หรือเพิ่มการใช้พล[...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_AUTO_MOUSE_GRAB,
   "ยึดเมาส์โดยอัตโนมัติ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_AUTO_MOUSE_GRAB,
   "เปิดใช้งานการยึดเมาส์เมื่อแอปพลิเคชันได้รับโฟกัส"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_AUTO_GAME_FOCUS,
   "เปิดใช้งานโหมด 'เน้นที่เกม' โดยอัตโนมัติ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_AUTO_GAME_FOCUS,
   "เปิดใช้งานโหมด 'เน้นที่เกม' ทุกครั้งที่เริ่มหรือกลับเข้าสู่เนื้อหา เมื่อตั้งค่าเป็น 'ตรวจจับ' ตัวเลือกนี้จะถูกเปิดใช้งานหาก Core ปัจจุบันมีการรองรับฟังก์ชันเรียกกลับของคีย์บอร์[...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_AUTO_GAME_FOCUS_OFF,
   "ปิด"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_AUTO_GAME_FOCUS_ON,
   "เปิด"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_AUTO_GAME_FOCUS_DETECT,
   "ตรวจจับ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PAUSE_ON_DISCONNECT,
   "หยุดเนื้อหาเมื่อคอนโทรลเลอร์ถูกตัดการเชื่อมต่อ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PAUSE_ON_DISCONNECT,
   "หยุดเนื้อหาเมื่อคอนโทรลเลอร์ใดก็ตามถูกตัดการเชื่อมต่อ และกลับมาเล่นต่อด้วยปุ่ม Start"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_BUTTON_AXIS_THRESHOLD,
   "เกณฑ์การตอบสนองของแกนปุ่มอินพุต"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_BUTTON_AXIS_THRESHOLD,
   "ค่าความอ่อนไหวของแกนที่ต้องขยับเพื่อให้ถือว่าเป็นการกดปุ่ม เมื่อใช้งานโหมด 'Analog to Digital'"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_DEADZONE,
   "ระยะตายของอนาล็อก (Analog Deadzone)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_ANALOG_DEADZONE,
   "ละเว้นการเคลื่อนที่ของแกนอนาล็อกที่น้อยกว่าค่าระยะตาย (Deadzone)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_SENSITIVITY,
   "ความไวของอนาล็อก"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_SENSOR_ACCELEROMETER_SENSITIVITY,
   "ความไวของเซนเซอร์ตรวจจับความเร่ง"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_SENSOR_GYROSCOPE_SENSITIVITY,
   "ความไวไจโรสโคป"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_ANALOG_SENSITIVITY,
   "ปรับความไวของก้านอะนาล็อก"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_SENSOR_ACCELEROMETER_SENSITIVITY,
   "ปรับความไวของตัวตรวจจับความเร่ง"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_SENSOR_ORIENTATION,
   "การปรับทิศทางเซ็นเซอร์"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_SENSOR_ORIENTATION,
   "หมุนแกนของตัวตรวจจับความเร่งและไจโรสโคปให้ตรงกับการวางอุปกรณ์"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SENSOR_ORIENTATION_AUTO,
   "อัตโนมัติ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_SENSOR_GYROSCOPE_SENSITIVITY,
   "ปรับความไวของไจโรสโคป"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_BIND_TIMEOUT,
   "หมดเวลาในการผูก"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_BIND_TIMEOUT,
   "จำนวนวินาทีที่จะรอก่อนดำเนินการไปยังการผูกถัดไป"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_BIND_HOLD,
   "กดค้างเพื่อผูก"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_BIND_HOLD,
   "จำนวนวินาทีที่ต้องกดอินพุตค้างไว้เพื่อทำการผูก"
   )
MSG_HASH(
   MSG_INPUT_BIND_PRESS,
   "กดแป้นพิมพ์ เมาส์ หรือคอนโทรลเลอร์"
   )
MSG_HASH(
   MSG_INPUT_BIND_RELEASE,
   "ปล่อยปุ่มและแป้นพิมพ์"
   )
MSG_HASH(
   MSG_INPUT_BIND_TIMEOUT,
   "หมดเวลา"
   )
MSG_HASH(
   MSG_INPUT_BIND_HOLD,
   "กดค้างไว้"
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TURBO_ENABLE,
   "ปุ่ม Turbo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TURBO_ENABLE,
   "การปิดใช้งานจะหยุดการทำงานของ ปุ่ม Turbo ทั้งหมด"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TURBO_PERIOD,
   "ช่วงเวลา Turbo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TURBO_PERIOD,
   "ระยะเวลาเป็นจำนวนเฟรมที่ใช้ในการกดปุ่ม Turbo ที่เปิดใช้งาน"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TURBO_DUTY_CYCLE,
   "อัตราส่วนหน้าที่ปุ่ม Turbo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TURBO_DUTY_CYCLE,
   "จำนวนเฟรมจากช่วงเวลาปุ่ม Turbo ที่ปุ่มถูกกดค้างไว้ หากตัวเลขนี้มีค่าเท่ากับหรือมากกว่าช่วงเวลาปุ่ม Turbo ปุ่มจะไม่ถูกปล่อยออกเลย"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TURBO_DUTY_CYCLE_HALF,
   "ครึ่งช่วงเวลา"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TURBO_MODE,
   "โหมด Turbo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TURBO_MODE,
   "เลือกพฤติกรรมทั่วไปของโหมด Turbo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TURBO_MODE_CLASSIC_TOGGLE,
   "Classic (เปิดปิด)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TURBO_MODE_SINGLEBUTTON,
   "ปุ่มเดียว (เปิดปิด)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TURBO_MODE_SINGLEBUTTON_HOLD,
   "ปุ่มเดียว (กดค้าง)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_TURBO_MODE_CLASSIC,
   "โหมด Classic การทำงานสองปุ่ม กดปุ่มค้างไว้แล้วแตะปุ่ม Turbo เพื่อเปิดใช้งานลำดับการกด-ปล่อย\nสามารถกำหนดการผูก Turbo ได้ใน การตั้งค่า/อินพุต/พอร์ต X ควบคุม"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_TURBO_MODE_CLASSIC_TOGGLE,
   "โหมด Classic เปิดปิด การทำงานสองปุ่ม กดปุ่มค้างไว้แล้วแตะปุ่ม Turbo เพื่อเปิดใช้งาน Turbo สำหรับปุ่มนั้น หากต้องการปิด Turbo: กดปุ่มค้างไว้แล้วกดปุ่ม Turbo อีกครั้ง\nสามารถกำหนดการผูก Turbo ได้ใน การต[...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_TURBO_MODE_SINGLEBUTTON,
   "โหมด เปิดปิด กดปุ่ม Turbo หนึ่งครั้งเพื่อเปิดใช้งานลำดับการกด-ปล่อยสำหรับปุ่มเริ่มต้นที่เลือก กดอีกครั้งเพื่อปิดการทำงาน\nสามารถกำหนดการผูก Turbo ได้ใน การตั้งค่า/อินพุต/พอร์ต X ควบคุม"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_TURBO_MODE_SINGLEBUTTON_HOLD,
   "โหมดกดค้าง ลำดับการกด-ปล่อยของปุ่มมาตรฐานที่เลือกไว้จะทำงานตราบเท่าที่กดปุ่ม Turbo ค้างไว้\nสามารถตั้งค่าปุ่ม Turbo ได้ที่ การตั้งค่า/อินพุต/การควบคุมพอร์ต X\nในการจำลองฟังก์ชันรัวปุ่[...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TURBO_BIND,
   "ปุ่ม Turbo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TURBO_BIND,
   "ปุ่ม Turbo ที่เชื่อมกับ RetroPad หากเว้นว่างไว้จะใช้ปุ่มที่เชื่อมไว้ตามพอร์ตที่กำหนด"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TURBO_BUTTON,
   "ปุ่ม Turbo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TURBO_BUTTON,
   "ปุ่ม Turbo เป้าหมายในโหมด 'ปุ่มเดียว'"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TURBO_ALLOW_DPAD,
   "อนุญาตทิศทาง D-Pad สำหรับ Turbo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TURBO_ALLOW_DPAD,
   "หากเปิดใช้งาน อินพุตทิศทางดิจิทัล (หรือที่เรียกว่า d-pad หรือ 'hatswitch') จะสามารถใช้ Turbo ได้"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TURBO_FIRE_SETTINGS,
   "ปุ่ม Turbo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TURBO_FIRE_SETTINGS,
   "เปลี่ยนการตั้งค่าปุ่ม Turbo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_HAPTIC_FEEDBACK_SETTINGS,
   "แรงสั่นสะเทือน/การสั่นสะเทือน"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_HAPTIC_FEEDBACK_SETTINGS,
   "เปลี่ยนการตั้งค่าแรงสั่นสะเทือนและการสั่นสะเทือน"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_SENSOR_SETTINGS,
   "เซนเซอร์จับความเคลื่อนไหว/แสง"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_SENSOR_SETTINGS,
   "เปลี่ยนการตั้งค่าเซนเซอร์ความเร่ง เซนเซอร์วัดการหมุน และความสว่างของแสง"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MENU_SETTINGS,
   "การควบคุมเมนู"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_MENU_SETTINGS,
   "เปลี่ยนการตั้งค่า การควบคุมเมนู"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_HOTKEY_BINDS,
   "ปุ่มลัด:"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_HOTKEY_BINDS,
   "เปลี่ยนการตั้งค่าและกำหนดปุ่มลัด เช่น ปุ่มเรียกเมนูระหว่างการเล่นเกม"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_RETROPAD_BINDS,
   "การกำหนดปุ่ม RetroPad"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_RETROPAD_BINDS,
   "เปลี่ยนวิธีตั้งค่าปุ่ม RetroPad เสมือนเข้ากับอุปกรณ์จริง หากอุปกรณ์ถูกตรวจพบและตั้งค่าอัตโนมัติถูกต้องแล้ว ปกติไม่จำเป็นต้องใช้เมนูนี้\nหมายเหตุ: สำหรับการเปลี่ยนปุ่มเฉพาะ core ให้ใ[...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_INPUT_RETROPAD_BINDS,
   "Libretro ใช้จอยจำลองที่เรียกว่า 'RetroPad' เป็นตัวกลางสื่อสารระหว่างหน้าจอหลัก (เช่น RetroArch) และ Core ต่างๆ เมนูนี้ใช้กำหนดว่า RetroPad เสมือนจะจับคู่กับอุปกรณ์ควบคุมจริงอย่างไร และอุปกรณ์เหล่านั้นจะ[...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_USER_BINDS,
   "การควบคุม Port %u"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_USER_BINDS,
   "เปลี่ยนวิธีตั้งค่าปุ่ม RetroPad เสมือนเข้ากับอุปกรณ์จริงสำหรับพอร์ตเสมือนนี้"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_USER_REMAPS,
   "เปลี่ยนการตั้งค่าปุ่มกดเฉพาะ Core"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ANDROID_INPUT_DISCONNECT_WORKAROUND,
   "วิธีแก้ปัญหาจอยหลุดบน Android"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ANDROID_INPUT_DISCONNECT_WORKAROUND,
   "วิธีแก้ปัญหาชั่วคราวสำหรับคอนโทรลเลอร์ที่หลุดแล้วเชื่อมต่อใหม่ ซึ่งส่งผลกระทบต่อการเล่น 2 คนที่ใช้คอนโทรลเลอร์รุ่นเดียวกัน"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONFIRM_QUIT,
   "ต้องกดปุ่มลัดเพื่อออกซ้ำสองครั้งจึงจะออกได้"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONFIRM_CLOSE,
   "ยืนยันการปิดเนื้อหา"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONFIRM_CLOSE,
   "ต้องกดปุ่มลัดเพื่อปิดเนื้อหาซ้ำสองครั้งจึงจะทำงาน"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONFIRM_RESET,
   "ยืนยันการรีเซ็ตเนื้อหา"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONFIRM_RESET,
   "ต้องกดปุ่มลัดเพื่อรีเซ็ตเนื้อหาซ้ำสองครั้งจึงจะทำงาน"
   )


/* Settings > Input > Haptic Feedback/Vibration */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIBRATE_ON_KEYPRESS,
   "สั่นเมื่อกดปุ่ม"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ENABLE_DEVICE_VIBRATION,
   "เปิดใช้งานการสั่นของอุปกรณ์ (สำหรับ Core ที่รองรับ)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_RUMBLE_GAIN,
   "แรงสั่นสะเทือน"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_RUMBLE_GAIN,
   "กำหนดระดับความแรงของเอฟเฟกต์การสั่น"
   )

/* Settings > Input > Menu Controls */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_UNIFIED_MENU_CONTROLS,
   "การควบคุมเมนูแบบรวม"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_UNIFIED_MENU_CONTROLS,
   "ใช้ปุ่มควบคุมเดียวกันทั้งในเมนูและในเกม มีผลกับการใช้งานคีย์บอร์ดด้วย"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_INPUT_SWAP_OK_CANCEL,
   "สลับปุ่มตกลงและปุ่มยกเลิก"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_INPUT_SWAP_OK_CANCEL,
   "สลับปุ่มสำหรับ ตกลง/ยกเลิก หากปิดใช้งานจะเป็นการเรียงปุ่มแบบญี่ปุ่น หากเปิดใช้งานจะเป็นการเรียงปุ่มแบบตะวันตก"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_INPUT_SWAP_SCROLL,
   "สลับปุ่มเลื่อน"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_INPUT_SWAP_SCROLL,
   "สลับปุ่มสำหรับการเลื่อน หากปิดใช้งานจะเลื่อนทีละ 10 รายการด้วยปุ่ม L/R และเลื่อนตามตัวอักษรด้วยปุ่ม L2/R2"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ALL_USERS_CONTROL_MENU,
   "ควบคุมเมนูได้ทุกผู้ใช้"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_ALL_USERS_CONTROL_MENU,
   "อนุญาตให้ผู้ใช้ทุกคนควบคุมเมนูได้ หากปิดใช้งาน จะมีเพียงผู้ใช้ที่ 1 เท่านั้นที่สามารถควบคุมเมนูได้"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SINGLECLICK_PLAYLISTS,
   "เพลย์ลิสต์แบบคลิกเดียวจบ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SINGLECLICK_PLAYLISTS,
   "ข้ามเมนู 'เล่น' เมื่อเข้าเล่นจากเพลย์ลิสต์ สามารถเข้าเมนู 'เล่น' ได้โดยการกดปุ่มทิศทางพร้อมกับกดปุ่มตกลงค้างไว้"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_ALLOW_TABS_BACK,
   "อนุญาตให้กดย้อนหลังจากแท็บได้"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_ALLOW_TABS_BACK,
   "กลับสู่เมนูหลักจากแท็บ/แถบด้านข้าง เมื่อกดปุ่มย้อนกลับ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SCROLL_FAST,
   "ความเร็วในการเลื่อน"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SCROLL_FAST,
   "ความเร็วสูงสุดของเคอร์เซอร์เมื่อกดทิศทางค้างเพื่อเลื่อนดูรายการต่างๆ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SCROLL_DELAY,
   "หน่วงเวลาการเลื่อน"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SCROLL_DELAY,
   "ความหน่วงเริ่มต้นเมื่อกดทิศทางค้างเพื่อเลื่อน (มิลลิวินาที)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DISABLE_INFO_BUTTON,
   "ปิดปุ่มข้อมูล"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_DISABLE_INFO_BUTTON,
   "ระงับฟังก์ชันข้อมูลเมนู"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DISABLE_SEARCH_BUTTON,
   "ปิดปุ่มค้นหา"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_DISABLE_SEARCH_BUTTON,
   "ระงับฟังก์ชันค้นหาเมนู"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DISABLE_LEFT_ANALOG_IN_MENU,
   "ปิดปุ่มอนาล็อกซ้ายในเมนู"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_DISABLE_LEFT_ANALOG_IN_MENU,
   "ระงับการใช้งานอนาล็อกซ้ายในเมนู"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DISABLE_RIGHT_ANALOG_IN_MENU,
   "ปิดปุ่มอนาล็อกขวาในเมนู"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_DISABLE_RIGHT_ANALOG_IN_MENU,
   "ระงับการใช้งานอนาล็อกขวาในเมนู โดยอนาล็อกขวาจะสลับภาพตัวอย่างในเพลย์ลิสต์แทน"
   )

/* Settings > Input > Hotkeys */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_ENABLE_HOTKEY,
   "ปุ่มเปิดใช้งานคีย์ลัด"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_ENABLE_HOTKEY,
   "เมื่อกำหนดแล้ว ต้องกดปุ่ม 'เปิดใช้งานคีย์ลัด' ค้างไว้ก่อนจึงจะใช้งานคีย์ลัดอื่นๆ ได้ ช่วยให้ตั้งค่าปุ่มบนคอนโทรลเลอร์เป็นคีย์ลัดได้โดยไม่กระทบต่อการควบคุมปกติ หากกำหนดปุ่มเ[...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_ENABLE_HOTKEY,
   "หากกำหนดคีย์ลัดนี้ให้กับคีย์บอร์ด ปุ่มกด หรือแกนควบคุม คีย์ลัดอื่นทั้งหมดจะถูกปิดการใช้งาน เว้นแต่จะกดคีย์ลัดนี้ค้างไว้พร้อมกัน\nซึ่งมีประโยชน์สำหรับการใช้งานที่เน้นคีย์บ[...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_HOTKEY_BLOCK_DELAY,
   "หน่วงเวลาเปิดใช้งานคีย์ลัด (เฟรม)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_HOTKEY_BLOCK_DELAY,
   "เพิ่มระยะเวลาการหน่วงเป็นจำนวนเฟรมก่อนที่การควบคุมปกติจะถูกระงับ หลังจากกดปุ่ม 'เปิดใช้งานคีย์ลัด' ที่กำหนดไว้\nช่วยให้ยังสามารถใช้งานการควบคุมปกติจากปุ่ม 'เปิดใช้งานคีย์ลั[...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_HOTKEY_DEVICE_MERGE,
   "รวมประเภทอุปกรณ์คีย์ลัดไว้ด้วยกัน"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_HOTKEY_DEVICE_MERGE,
   "ระงับคีย์ลัดทั้งหมดจากทั้งคีย์บอร์ดและคอนโทรลเลอร์ หากอุปกรณ์ประเภทใดประเภทหนึ่งมีการตั้งค่า 'เปิดใช้งานคีย์ลัด' ไว้\nช่วยให้การใช้งานคีย์ลัดจากทั้งสองอุปกรณ์ถูกควบคุมด้วย[...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_HOTKEY_FOLLOWS_PLAYER1,
   "คีย์ลัดตามผู้เล่น 1"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_HOTKEY_FOLLOWS_PLAYER1,
   "กำหนดให้คีย์ลัดยึดตามพอร์ตหลัก 1 (Core Port 1) แม้ว่าจะมีการเปลี่ยนพอร์ตดังกล่าวไปเป็นของผู้เล่นคนอื่นก็ตาม\nหมายเหตุ: คีย์ลัดบนคีย์บอร์ดจะไม่ทำงานหากพอร์ตหลัก 1 ถูกเปลี่ยนไปเป็นของผ[...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MENU_ENUM_TOGGLE_GAMEPAD_COMBO,
   "เปิด/ปิดเมนู (Controller Combo)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_MENU_ENUM_TOGGLE_GAMEPAD_COMBO,
   "ปุ่มกด Controller Combo เพื่อเปิด/ปิดเมนู"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_MENU_TOGGLE,
   "เปิด/ปิดเมนู"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_MENU_TOGGLE,
   "สลับการแสดงผลปัจจุบันระหว่างเมนูและเนื้อหาในแอปพลิเคชัน"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_QUIT_GAMEPAD_COMBO,
   "ออกจากแอป (Controller Combo)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_QUIT_GAMEPAD_COMBO,
   "ปุ่มกด Controller Combo เพื่อออกจาก RetroArch"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_QUIT_KEY,
   "ออก"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_QUIT_KEY,
   "ปิด RetroArch เพื่อให้แน่ใจว่าข้อมูลการเซฟและไฟล์การตั้งค่าทั้งหมดถูกเขียนลงในดิสก์เรียบร้อยแล้ว"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_CLOSE_CONTENT_KEY,
   "ปิดเนื้อหา"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_CLOSE_CONTENT_KEY,
   "ปิดเนื้อหาที่กำลังเล่นอยู่"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_RESET,
   "รีเซ็ตเนื้อหา"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_RESET,
   "เริ่มเนื้อหาใหม่ตั้งแต่ต้น"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_FAST_FORWARD_KEY,
   "เร่งความเร็ว (เปิด-ปิด)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_FAST_FORWARD_KEY,
   "สลับระหว่างเร่งความเร็วและความเร็วปกติ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_FAST_FORWARD_HOLD_KEY,
   "เร่งความเร็ว (กดค้าง)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_FAST_FORWARD_HOLD_KEY,
   "เปิดใช้งานการเร่งความเร็วเมื่อกดค้างไว้ และเนื้อหาจะรันที่ความเร็วปกติเมื่อปล่อยปุ่ม"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_SLOWMOTION_KEY,
   "สโลว์โมชั่น (เปิด-ปิด)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_SLOWMOTION_KEY,
   "สลับระหว่างสโลว์โมชั่นและความเร็วปกติ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_SLOWMOTION_HOLD_KEY,
   "สโลว์โมชั่น (กดค้าง)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_SLOWMOTION_HOLD_KEY,
   "เปิดใช้งานสโลว์โมชั่นเมื่อกดค้างไว้ และเนื้อหาจะรันที่ความเร็วปกติเมื่อปล่อยปุ่ม"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_REWIND,
   "ย้อนเวลา"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_REWIND,
   "ย้อนกลับเนื้อหาปัจจุบันเมื่อกดปุ่มค้างไว้ ต้องเปิดใช้งาน 'การสนับสนุนการย้อนกลับ'"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_PAUSE_TOGGLE,
   "หยุดชั่วคราว"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_PAUSE_TOGGLE,
   "สลับระหว่างสถานะหยุดชั่วคราวและไม่หยุดชั่วคราว"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_FRAMEADVANCE,
   "เฟรมถัดไป"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_FRAMEADVANCE,
   "เลื่อนเนื้อหาไปข้างหน้าทีละหนึ่งเฟรมเมื่อหยุดชั่วคราว"
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_MUTE,
   "ปิดเสียง"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_MUTE,
   "สลับการเปิด-ปิดเสียง"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_VOLUME_UP,
   "เพิ่มระดับเสียง"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_VOLUME_UP,
   "เพิ่มระดับเสียง"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_VOLUME_DOWN,
   "ลดระดับเสียง"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_VOLUME_DOWN,
   "ลดระดับเสียงเอาต์พุตลง"
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_LOAD_STATE_KEY,
   "โหลดสถานะ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_LOAD_STATE_KEY,
   "โหลดสถานะที่บันทึกไว้จากสล็อตที่เลือกอยู่ในปัจจุบัน"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_SAVE_STATE_KEY,
   "บันทึกสถานะ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_SAVE_STATE_KEY,
   "บันทึกสถานะลงในสล็อตที่เลือกอยู่ในปัจจุบัน"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_STATE_SLOT_PLUS,
   "สล็อตบันทึกสถานะถัดไป"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_STATE_SLOT_PLUS,
   "เพิ่มลำดับสล็อตบันทึกสถานะที่เลือกอยู่ในปัจจุบันขึ้นทีละหนึ่ง (สล็อตถัดไป)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_STATE_SLOT_MINUS,
   "สล็อตบันทึกสถานะก่อนหน้า"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_STATE_SLOT_MINUS,
   "ลดลำดับสล็อตบันทึกสถานะที่เลือกอยู่ในปัจจุบันลงทีละหนึ่ง (สล็อตก่อนหน้า)"
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_DISK_EJECT_TOGGLE,
   "นำแผ่นออก (เปิด-ปิด)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_DISK_EJECT_TOGGLE,
   "หากถาดวางแผ่นจำลองปิดอยู่ จะทำการเปิดถาดและนำแผ่นที่โหลดไว้ออก หากถาดเปิดอยู่ จะทำการใส่แผ่นที่เลือกไว้และปิดถาดลง"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_DISK_NEXT,
   "แผ่นถัดไป"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_DISK_NEXT,
   "เพิ่มลำดับแผ่นดิสก์ที่เลือกอยู่ในปัจจุบัน และจะทำการใส่แผ่นแบบดีเลย์ หากถาดใส่แผ่นจำลองปิดอยู่"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_DISK_PREV,
   "แผ่นก่อนหน้า"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_DISK_PREV,
   "ลดลำดับแผ่นดิสก์ที่เลือกอยู่ในปัจจุบัน และจะทำการใส่แผ่นแบบดีเลย์ หากถาดใส่แผ่นจำลองปิดอยู่"
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_SHADER_TOGGLE,
   "เปิด-ปิด การใช้งาน Shaders"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_SHADER_TOGGLE,
   "เปิด-ปิด การใช้งานเชเดอร์ที่เลือกอยู่ในปัจจุบัน"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_SHADER_HOLD,
   "เปิด-ปิด การใช้งาน Shaders"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_SHADER_HOLD,
   "เปิด/ปิดการใช้งาน Shader ปัจจุบัน ขณะที่กดปุ่มค้างไว้"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_SHADER_NEXT,
   "Shader ถัดไป"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_SHADER_NEXT,
   "โหลดและเรียกใช้ไฟล์ Shader preset ถัดไปที่อยู่ในโฟลเดอร์หลักของไดเรกทอรี 'Video Shaders'"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_SHADER_PREV,
   "Shader ก่อนหน้า"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_SHADER_PREV,
   "โหลดและเรียกใช้ไฟล์ Shader preset ก่อนหน้าที่อยู่ในโฟลเดอร์หลักของไดเรกทอรี 'Video Shaders'"
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_CHEAT_TOGGLE,
   "เปิด-ปิด สูตรโกง (Cheats)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_CHEAT_TOGGLE,
   "เปิด-ปิด การใช้งานสูตรโกงที่เลือกอยู่ในปัจจุบัน"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_CHEAT_INDEX_PLUS,
   "ลำดับสูตรโกงถัดไป"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_CHEAT_INDEX_PLUS,
   "เพิ่มลำดับสูตรโกงที่เลือกอยู่ในปัจจุบัน"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_CHEAT_INDEX_MINUS,
   "ลำดับสูตรโกงก่อนหน้า"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_CHEAT_INDEX_MINUS,
   "ลดลำดับสูตรโกงที่เลือกอยู่ในปัจจุบัน"
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_SCREENSHOT,
   "จับภาพหน้าจอ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_SCREENSHOT,
   "บันทึกภาพของเนื้อหาที่แสดงอยู่ในปัจจุบัน"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_RECORDING_TOGGLE,
   "เปิด-ปิด การบันทึก"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_RECORDING_TOGGLE,
   "เริ่ม-หยุด การบันทึกเซสชันปัจจุบันลงเป็นไฟล์วิดีโอในเครื่อง"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_STREAMING_TOGGLE,
   "เปิด-ปิด การสตรีม"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_STREAMING_TOGGLE,
   "เริ่ม-หยุด การสตรีมของเซสชันปัจจุบันไปยังแพลตฟอร์มวิดีโอออนไลน์"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_PLAY_REPLAY_KEY,
   "เล่นไฟล์ เล่นซ้ำ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_PLAY_REPLAY_KEY,
   "เล่นไฟล์ เล่นซ้ำ จากช่องที่เลือกไว้ปัจจุบัน"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_RECORD_REPLAY_KEY,
   "บันทึกไฟล์ เล่นซ้ำ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_RECORD_REPLAY_KEY,
   "บันทึกไฟล์เล่นซ้ำลงในช่องที่เลือกไว้ปัจจุบัน"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_HALT_REPLAY_KEY,
   "หยุดการบันทึก/เล่นซ้ำ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_HALT_REPLAY_KEY,
   "หยุดการบันทึก/เล่นไฟล์เล่นซ้ำปัจจุบัน"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_SAVE_REPLAY_CHECKPOINT_KEY,
   "บันทึกจุดเช็คพอยต์การเล่นซ้ำ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_SAVE_REPLAY_CHECKPOINT_KEY,
   "บันทึกจุดเช็คพอยต์ลงในการเล่นซ้ำที่กำลังเล่นอยู่ปัจจุบัน"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_PREV_REPLAY_CHECKPOINT_KEY,
   "ย้อนกลับไปยังจุดเช็คพอยต์การเล่นซ้ำก่อนหน้า"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_PREV_REPLAY_CHECKPOINT_KEY,
   "ย้อนกลับการเล่นซ้ำไปยังจุดเช็คพอยต์ก่อนหน้า ทั้งที่บันทึกโดยอัตโนมัติหรือบันทึกด้วยตัวเอง"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_NEXT_REPLAY_CHECKPOINT_KEY,
   "จุดเช็คพอยต์เล่นซ้ำถัดไป"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_NEXT_REPLAY_CHECKPOINT_KEY,
   "เร่งการเล่นซ้ำ ไปยังจุดเช็คพอยต์ที่บันทึกโดยอัตโนมัติหรือบันทึกด้วยตนเองถัดไป"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_REPLAY_SLOT_PLUS,
   "สล็อตเล่นซ้ำถัดไป"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_REPLAY_SLOT_PLUS,
   "เพิ่มลำดับหมายเลขสล็อตเล่นซ้ำที่เลือกไว้ในปัจจุบัน"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_REPLAY_SLOT_MINUS,
   "สล็อตเล่นซ้ำก่อนหน้า"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_REPLAY_SLOT_MINUS,
   "ลดหมายเลขสล็อตเล่นซ้ำที่เลือกไว้ในปัจจุบัน"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_TURBO_FIRE_TOGGLE,
   "ปุ่ม Turbo (เปิด-ปิด)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_TURBO_FIRE_TOGGLE,
   "สลับการ เปิด-ปิด ปุ่ม Turbo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_GRAB_MOUSE_TOGGLE,
   "จับเมาส์ (เปิด-ปิด)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_GRAB_MOUSE_TOGGLE,
   "ยึดหรือปล่อยเมาส์ เมื่อถูกยึด เคอร์เซอร์ของระบบจะถูกซ่อนและจำกัดไว้ในหน้าต่างแสดงผลของ RetroArch เพื่อปรับปรุงการป้อนข้อมูลเมาส์แบบสัมพัทธ์ให้ดีขึ้น"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_GAME_FOCUS_TOGGLE,
   "โฟกัสเกม (เปิด-ปิด)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_GAME_FOCUS_TOGGLE,
   "สลับ เปิด-ปิด โหมด 'โฟกัสเกม' เมื่อเนื้อหาถูกโฟกัส ปุ่มลัดจะถูกปิดใช้งาน (การป้อนข้อมูลคีย์บอร์ดทั้งหมดจะถูกส่งไปยัง Core ที่กำลังรันอยู่) และเมาส์จะถูกยึดไว้"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_FULLSCREEN_TOGGLE_KEY,
   "เต็มหน้าจอ (เปิด-ปิด)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_UI_COMPANION_TOGGLE,
   "เมนูเดสก์ท็อป (เปิด-ปิด)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_UI_COMPANION_TOGGLE,
   "เปิดหน้าต่างส่วนประสานงานผู้ใช้แบบเดสก์ท็อป WIMP (Windows, Icons, Menus, Pointer) ที่ใช้งานร่วมกัน"
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_VRR_RUNLOOP_TOGGLE,
   "ซิงค์กับอัตราเฟรมของเนื้อหาโดยตรง (เปิด-ปิด)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_VRR_RUNLOOP_TOGGLE,
   "สลับการ เปิด-ปิด การซิงค์กับอัตราเฟรมของเนื้อหาโดยตรง"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_RUNAHEAD_TOGGLE,
   "รันเฟรมล่วงหน้า (เปิด-ปิด)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_RUNAHEAD_TOGGLE,
   "เปิด-ปิด รันเฟรมล่วงหน้า"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_PREEMPT_TOGGLE,
   "เรนเดอร์เฟรมล่วงหน้า (เปิด-ปิด)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_PREEMPT_TOGGLE,
   "เปิด-ปิด เรนเดอร์เฟรมล่วงหน้า"
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_FPS_TOGGLE,
   "แสดง FPS (เปิด-ปิด)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_FPS_TOGGLE,
   "เปิด-ปิด การแสดงค่าเฟรมเรต (FPS)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_STATISTICS_TOGGLE,
   "แสดงสถิติทางเทคนิค (เปิด-ปิด)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_STATISTICS_TOGGLE,
   "เปิด-ปิด การแสดงสถิติทางเทคนิคบนหน้าจอ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_OSK,
   "คีย์บอร์ดเสมือน (เปิด/ปิด)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_OSK,
   "เปิด/ปิดการแสดงคีย์บอร์ดเสมือนบนหน้าจอ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_OVERLAY_NEXT,
   "โอเวอร์เลย์ถัดไป"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_OVERLAY_NEXT,
   "เปลี่ยนเป็นผังปุ่มถัดไปที่มีให้ใช้งาน ของโอเวอร์เลย์บนหน้าจอ ที่เปิดใช้อยู่ในปัจจุบัน"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_AI_SERVICE,
   "บริการ AI"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_AI_SERVICE,
   "จับภาพหน้าจอของเนื้อหาปัจจุบัน เพื่อแปลภาษา และ/หรือ อ่านออกเสียงข้อความใดๆ ที่ปรากฏบนหน้าจอ ทั้งนี้ต้องเปิดใช้งานและตั้งค่า 'บริการ AI' ไว้ก่อน"
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_NETPLAY_PING_TOGGLE,
   "ปิง Netplay (เปิด/ปิด)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_NETPLAY_PING_TOGGLE,
   "เปิด/ปิด การแสดงตัวนับค่าปิง สำหรับห้อง Netplay ที่ใช้งานอยู่ในปัจจุบัน"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_NETPLAY_HOST_TOGGLE,
   "โฮสต์ Netplay (เปิด/ปิด)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_NETPLAY_HOST_TOGGLE,
   "เปิด/ปิด การเป็นโฮสต์ Netplay"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_NETPLAY_GAME_WATCH,
   "โหมดการเล่น/รับชม Netplay (เปิด/ปิด)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_NETPLAY_GAME_WATCH,
   "สลับโหมด netplay ระหว่าง 'เล่น' และ 'รับชม'"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_NETPLAY_PLAYER_CHAT,
   "แชทผู้เล่น netplay"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_NETPLAY_PLAYER_CHAT,
   "ส่งข้อความแชทไปยังเซสชันการเล่นผ่านเครือข่าย Netplay ในปัจจุบัน"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_NETPLAY_FADE_CHAT_TOGGLE,
   "เปิด-ปิด การจางหายของแชท Netplay"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_NETPLAY_FADE_CHAT_TOGGLE,
   "สลับระหว่างการทำให้ข้อความแชทค่อยๆ จางหาย หรือแสดงค้างไว้ในการเล่นผ่านเครือข่าย Netplay"
   )

/* Settings > Input > Port # Controls */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DEVICE_TYPE,
   "ประเภทอุปกรณ์"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_DEVICE_TYPE,
   "ระบุประเภทของจอยคอนโทรลเลอร์ที่จำลองขึ้นมา"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ADC_TYPE,
   "Analog เป็น Digital"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_ADC_TYPE,
   "ใช้ก้านอะนาล็อกที่ระบุเพื่อป้อนข้อมูลแทนปุ่มทิศทาง (D-Pad) โดยโหมด 'บังคับ' (Forced) จะแทนที่การป้อนข้อมูลอะนาล็อกเดิมของตัวคอร์โดยตรง"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_INPUT_ADC_TYPE,
   "กำหนดก้านอะนาล็อกที่ระบุเพื่อป้อนข้อมูลแทนปุ่มทิศทาง (D-Pad)\nหากคอร์รองรับการใช้ก้านอะนาล็อกอยู่แล้ว การกำหนดค่านี้จะถูกปิดใช้งาน เว้นแต่จะเลือกตัวเลือก '(บังคับ)'\nหากเลือกแบบบั[...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DEVICE_INDEX,
   "หมายเลขอุปกรณ์"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_DEVICE_INDEX,
   "จอยคอนโทรลเลอร์ที่ระบบ RetroArch ตรวจพบทางกายภาพ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DEVICE_RESERVED_DEVICE_NAME,
   "อุปกรณ์ที่จองไว้สำหรับผู้เล่นนี้"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_DEVICE_RESERVED_DEVICE_NAME,
   "จอยคอนโทรลเลอร์นี้จะถูกจองไว้สำหรับผู้เล่นคนนี้ ตามโหมดการสำรองที่กำหนดไว้"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DEVICE_RESERVATION_NONE,
   "ไม่มีการจอง"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DEVICE_RESERVATION_PREFERRED,
   "ที่ต้องการ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DEVICE_RESERVATION_RESERVED,
   "จองแล้ว"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DEVICE_RESERVATION_TYPE,
   "ประเภทการจองอุปกรณ์"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_DEVICE_RESERVATION_TYPE,
   "ที่ต้องการ: หากพบอุปกรณ์ที่ระบุไว้ จะถูกจองไว้ให้ผู้เล่นคนนี้ จองแล้ว: จะไม่มีจอยคอนโทรลเลอร์ตัวอื่นถูกจองไว้ให้ผู้เล่นคนนี้แทนที่ได้"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_REMAP_PORT,
   "พอร์ตที่จับคู่ไว้"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_REMAP_PORT,
   "ระบุว่า Core พอร์ตใดจะได้รับสัญญาณอินพุตจากพอร์ตจอยคอนโทรลเลอร์ %u ของ Frontend"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_BIND_ALL,
   "ตั้งค่าการควบคุมทั้งหมด"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_BIND_ALL,
   "กำหนดทิศทางและปุ่มทั้งหมด ตามลำดับที่ปรากฏในเมนูนี้ทีละปุ่ม"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_BIND_DEFAULT_ALL,
   "คืนค่าการควบคุมเริ่มต้น"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_BIND_DEFAULTS,
   "ล้างค่าการตั้งค่าปุ่มอินพุตให้เป็นค่าเริ่มต้น"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_SAVE_AUTOCONFIG,
   "บันทึกโปรไฟล์จอยคอนโทรลเลอร์"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_SAVE_AUTOCONFIG,
   "บันทึกไฟล์การตั้งค่าอัตโนมัติ ซึ่งจะถูกนำมาใช้โดยอัตโนมัติเมื่อตรวจพบจอยคอนโทรลเลอร์นี้ในครั้งต่อไป"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_INDEX,
   "หมายเลขเมาส์"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_MOUSE_INDEX,
   "ลำดับเมาส์ ทางกายภาพ ที่ RetroArch ตรวจพบ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_B,
   "ปุ่ม B (ล่าง)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_Y,
   "ปุ่ม Y (ซ้าย)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_SELECT,
   "ปุ่ม Select"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_START,
   "ปุ่ม Start"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_UP,
   "ปุ่ม D-Pad (ขึ้น)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_DOWN,
   "ปุ่ม D-Pad (ลง)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_LEFT,
   "ปุ่ม D-Pad (ซ้าย)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_RIGHT,
   "ปุ่ม D-Pad (ขวา)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_A,
   "ปุ่ม A (ขวา)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_X,
   "ปุ่ม X (บน)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_L,
   "ปุ่ม L (ไหล่หน้า)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_R,
   "ปุ่ม R (ไหล่หน้า)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_L2,
   "ปุ่ม L2 (ทริกเกอร์)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_R2,
   "ปุ่ม R2 (ทริกเกอร์)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_L3,
   "ปุ่ม L3 (แกนโยกซ้าย)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_R3,
   "ปุ่ม R3 (แกนโยกขวา)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_X_PLUS,
   "ปุ่มแกนโยกซ้าย X+ (ขวา)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_X_MINUS,
   "ปุ่มแกนโยกซ้าย X- (ซ้าย)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_Y_PLUS,
   "ปุ่มแกนโยกซ้าย Y+ (ลง)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_Y_MINUS,
   "ปุ่มแกนโยกซ้าย Y- (บน)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_X_PLUS,
   "ปุ่มแกนโยกขวา X+ (ขวา)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_X_MINUS,
   "ปุ่มแกนโยกขวา X- (ซ้าย)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_Y_PLUS,
   "ปุ่มแกนโยกขวา Y+ (ลง)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_Y_MINUS,
   "ปุ่มแกนโยกขวา Y- (บน)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_TRIGGER,
   "ปุ่มไกปืน"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_RELOAD,
   "ปืน รีโหลดกระสุน"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_AUX_A,
   "ปุ่ม Gun Aux A"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_AUX_B,
   "ปุ่ม Gun Aux B"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_AUX_C,
   "ปุ่ม Gun Aux C"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_DPAD_UP,
   "ปืน ปุ่ม D-Pad (ขึ้น)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_DPAD_DOWN,
   "ปืน ปุ่ม D-Pad (ลง)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_DPAD_LEFT,
   "ปืน ปุ่ม D-Pad (ซ้าย)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_DPAD_RIGHT,
   "ปืน ปุ่ม D-Pad (ขวา)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TURBO,
   "ปุ่ม Turbo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_HOLD,
   "กดค้างไว้"
   )

/* Settings > Latency */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RUN_AHEAD_UNSUPPORTED,
   "[รันเฟรมล่วงหน้า ไม่สามารถใช้งานได้]"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RUN_AHEAD_UNSUPPORTED,
   "Core ปัจจุบันไม่รองรับการรันเฟรมล่วงหน้า เนื่องจากไม่รองรับการบันทึกสถานะแบบ Deterministic"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RUNAHEAD_MODE,
   "รันเฟรมล่วงหน้า"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RUN_AHEAD_FRAMES,
   "จำนวนเฟรมที่จะรันล่วงหน้า"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RUN_AHEAD_FRAMES,
   "จำนวนเฟรมที่จะรันเฟรมล่วงหน้า หากตั้งค่าเกินจำนวนเฟรมที่หน่วงจริงภายในเกม จะทำให้เกิดปัญหาในการเล่นอย่างเช่นอาการภาพสั่นค้างหรือกระตุกได้"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RUNAHEAD_MODE,
   "ประมวลผลการทำงานของ Core เพิ่มเติมเพื่อลดความหน่วง โดย Single Instance จะรันเฟรมไปยังอนาคตแล้วโหลดสถานะปัจจุบันใหม่ ส่วน Second Instance จะรัน Core แยกเฉพาะวิดีโอไว้ที่เฟรมอนาคตเพื่อเลี่ยงปัญหาด้านเ[...]"
   )
#if !(defined(HAVE_DYNAMIC) || defined(HAVE_DYLIB))
MSG_HASH(
   MENU_ENUM_SUBLABEL_RUNAHEAD_MODE_NO_SECOND_INSTANCE,
   "ประมวลผลการทำงานของ Core เพิ่มเติมเพื่อลดความหน่วง โดย Single Instance จะรันเฟรมไปยังอนาคตแล้วโหลดสถานะปัจจุบันใหม่ ส่วน Preemptive Frames จะรันเฟรมที่ผ่านมาใหม่พร้อมกับอินพุตใหม่เมื่อจำเป็นเพื่อเ[...]"
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RUNAHEAD_MODE_SINGLE_INSTANCE,
   "โหมด Single Instance"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RUNAHEAD_MODE_SECOND_INSTANCE,
   "โหมด Second Instance"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RUNAHEAD_MODE_PREEMPTIVE_FRAMES,
   "โหมดจัดการเฟรมที่ถูกเรนเดอร์ล่วงหน้า"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RUN_AHEAD_HIDE_WARNINGS,
   "ซ่อนการแจ้งเตือนรันเฟรมล่วงหน้า"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RUN_AHEAD_HIDE_WARNINGS,
   "ซ่อนข้อความแจ้งเตือนที่ปรากฏขึ้นเมื่อใช้งานรันเฟรมล่วงหน้าในกรณีที่ Core ไม่รองรับการบันทึกสถานะ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PREEMPT_FRAMES,
   "จำนวนเฟรมที่ถูกเรนเดอร์ล่วงหน้า"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PREEMPT_FRAMES,
   "จำนวนเฟรมที่จะรันซ้ำ หากตั้งค่าเกินจำนวนเฟรมที่หน่วงจริงภายในเกม จะทำให้เกิดปัญหาในการเล่นอย่างเช่นอาการภาพสั่นค้างหรือกระตุกได้"
   )

/* Settings > Core */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHARED_CONTEXT,
   "ใช้ Context ร่วมกับฮาร์ดแวร์"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHARED_CONTEXT,
   "กำหนดบริบทส่วนตัวให้กับ Core ที่มีการเรนเดอร์ด้วยฮาร์ดแวร์ เพื่อหลีกเลี่ยงการต้องคาดการณ์การเปลี่ยนแปลงสถานะของฮาร์ดแวร์ในระหว่างเฟรม"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DRIVER_SWITCH_ENABLE,
   "อนุญาตให้ Core สลับไดรเวอร์วิดีโอได้"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DRIVER_SWITCH_ENABLE,
   "อนุญาตให้ Core สามารถสลับไปใช้ไดรเวอร์วิดีโออื่นที่ต่างจากตัวที่กำลังใช้งานอยู่ได้"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DUMMY_ON_CORE_SHUTDOWN,
   "โหลด Dummy Core เมื่อปิดคอร์"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DUMMY_ON_CORE_SHUTDOWN,
   "โหลด Dummy Core เพื่อป้องกันไม่ให้ RetroArch ปิดตัวลง เมื่อใช้งานคอร์ที่มีฟีเจอร์คำสั่งปิดระบบ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_DUMMY_ON_CORE_SHUTDOWN,
   "บาง Core อาจมีฟีเจอร์ปิดเครื่อง หากปิดตัวเลือกนี้ไว้ การสั่งปิดเครื่องจะทำให้ RetroArch ปิดตัวลง\nหากเปิดไว้จะโหลด dummy Core แทนเพื่อให้ยังอยู่ในเมนูและ RetroArch ไม่ปิดลง"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_SET_SUPPORTS_NO_CONTENT_ENABLE,
   "เริ่มใช้งาน Core โดยอัตโนมัติ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_OPTION_CATEGORY_ENABLE,
   "หมวดหมู่ตัวเลือก Core"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_OPTION_CATEGORY_ENABLE,
   "อนุญาตให้ Core แสดงตัวเลือกในรูปแบบหมวดหมู่เมนูย่อย หมายเหตุ: ต้องทำการโหลด Core ใหม่เพื่อให้การเปลี่ยนแปลงมีผล"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_CACHE_ENABLE,
   "แคชไฟล์ข้อมูล Core"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_INFO_CACHE_ENABLE,
   "เก็บแคชข้อมูล Core ที่ติดตั้งไว้ในเครื่องอย่างถาวร ช่วยลดเวลาการโหลดได้อย่างมากบนแพลตฟอร์มที่มีการเข้าถึงดิสก์ได้ช้า"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_SAVESTATE_BYPASS,
   "ข้ามฟีเจอร์บันทึกสถานะจากข้อมูล Core"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_INFO_SAVESTATE_BYPASS,
   "กำหนดว่าจะละเว้นขีดความสามารถในการบันทึกสถานะที่ระบุไว้ในข้อมูล Core หรือไม่ เพื่อให้สามารถทดลองใช้งานฟีเจอร์ที่เกี่ยวข้องได้ (เช่นรันเฟรมล่วงหน้า, ย้อนเวลา และอื่นๆ)"
   )
#ifndef HAVE_DYNAMIC
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ALWAYS_RELOAD_CORE_ON_RUN_CONTENT,
   "โหลด Core ใหม่เสมอเมื่อเริ่มคอนเทนต์"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ALWAYS_RELOAD_CORE_ON_RUN_CONTENT,
   "รีสตาร์ท RetroArch เมื่อเริ่มคอนเทนต์ แม้ว่า Core ที่ต้องการจะถูกโหลดไว้แล้วก็ตาม วิธีนี้อาจช่วยเพิ่มความเสถียรของระบบ แต่ต้องแลกมาด้วยเวลาการโหลดที่เพิ่มขึ้น"
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ALLOW_ROTATE,
   "อนุญาตให้หมุนหน้าจอ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_ALLOW_ROTATE,
   "อนุญาตให้ Core ตั้งค่าการหมุนหน้าจอ เมื่อปิดการใช้งาน การร้องขอการหมุนหน้าจอจะถูกละเพิกเฉย มีประโยชน์สำหรับเครื่องที่ตั้งค่าหมุนหน้าจอด้วยตนเองอยู่แล้ว"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_MANAGER_LIST,
   "จัดการ Core"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_MANAGER_LIST,
   "จัดการงานบำรุงรักษาแบบออฟไลน์สำหรับ Core ที่ติดตั้งไว้ (สำรองข้อมูล, เรียกคืนข้อมูล, ลบ และอื่นๆ) และดูข้อมูล Core"
   )
#ifdef HAVE_MIST
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_MANAGER_STEAM_LIST,
   "จัดการ Core"
   )

MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_MANAGER_STEAM_LIST,
   "ติดตั้งหรือถอนการติดตั้ง Core ที่เผยแพร่ผ่าน Steam"
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_STEAM_INSTALL,
   "ติดตั้ง Core"
)

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_STEAM_UNINSTALL,
   "ถอนการติดตั้ง Core"
)

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_CORE_MANAGER_STEAM,
   "แสดง 'จัดการ Core'"
)
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_CORE_MANAGER_STEAM,
   "แสดง 'จัดการ Core' ในเมนูหลัก"
)

MSG_HASH(
   MSG_CORE_STEAM_INSTALLING,
   "กำลังติดตั้ง Core:"
)

MSG_HASH(
   MSG_CORE_STEAM_UNINSTALLED,
   "Core จะถูกถอนการติดตั้งเมื่อปิด RetroArch"
)

MSG_HASH(
   MSG_CORE_STEAM_CURRENTLY_DOWNLOADING,
   "ขณะนี้กำลังดาวน์โหลด Core"
)
#endif
/* Settings > Configuration */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONFIG_SAVE_ON_EXIT,
   "บันทึกการตั้งค่าเมื่อปิดโปรแกรม"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONFIG_SAVE_ON_EXIT,
   "บันทึกการเปลี่ยนแปลงไปยังไฟล์การตั้งค่าเมื่อปิดโปรแกรม"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_CONFIG_SAVE_ON_EXIT,
   "บันทึกการเปลี่ยนแปลงไปยังไฟล์การตั้งค่าเมื่อออกจากโปรแกรม มีประโยชน์สำหรับการเปลี่ยนแปลงที่ทำผ่านเมนู ทั้งนี้จะทำการเขียนทับไฟล์การตั้งค่าเดิม โดยที่บรรทัด #include และคอมเมนต[...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONFIG_SAVE_MINIMAL,
   "บันทึกการตั้งค่าแบบขั้นต่ำ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONFIG_SAVE_MINIMAL,
   "บันทึกเฉพาะการตั้งค่าที่ต่างจากค่าเริ่มต้น"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_CONFIG_SAVE_MINIMAL,
   "เมื่อเปิดใช้งาน จะบันทึกเฉพาะค่าการตั้งค่าที่ถูกเปลี่ยนจากค่าเริ่มต้นเท่านั้น  ส่งผลให้ไฟล์การตั้งค่ามีขนาดเล็กลงและจัดการได้ง่ายขึ้น"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_SAVE_ON_EXIT,
   "บันทึกไฟล์การปรับปุ่มเมื่อออกจากโปรแกรม"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_SAVE_ON_EXIT,
   "บันทึกการเปลี่ยนแปลงไปยังไฟล์การปรับปุ่มอินพุตที่ใช้งานอยู่ เมื่อปิดคอนเทนต์หรือออกจาก RetroArch"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GAME_SPECIFIC_OPTIONS,
   "โหลดตัวเลือก Core เฉพาะคอนเทนต์โดยอัตโนมัติ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GAME_SPECIFIC_OPTIONS,
   "โหลดตัวเลือก Core ที่ปรับแต่งไว้โดยค่าเริ่มต้นเมื่อเริ่มต้นโปรแกรม"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUTO_OVERRIDES_ENABLE,
   "โหลดไฟล์เขียนทับโดยอัตโนมัติ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUTO_OVERRIDES_ENABLE,
   "โหลดการตั้งค่าที่ปรับแต่งไว้โดยอัตโนมัติเมื่อเริ่มต้นโปรแกรม"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUTO_REMAPS_ENABLE,
   "โหลดไฟล์การปรับปุ่มโดยอัตโนมัติ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUTO_REMAPS_ENABLE,
   "โหลดการควบคุมที่ปรับแต่งไว้โดยอัตโนมัติเมื่อเริ่มต้นโปรแกรม"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INITIAL_DISK_CHANGE_ENABLE,
   "โหลดไฟล์หมายเลขแผ่นดิสก์เริ่มต้นโดยอัตโนมัติ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INITIAL_DISK_CHANGE_ENABLE,
   "เปลี่ยนไปใช้แผ่นดิสก์ล่าสุดที่เคยใช้ เมื่อเริ่มต้นคอนเทนต์แบบหลายแผ่น"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUTO_SHADERS_ENABLE,
   "โหลดค่า Shader Presets โดยอัตโนมัติ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GLOBAL_CORE_OPTIONS,
   "ใช้ไฟล์ตัวเลือก Core แบบ Global"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GLOBAL_CORE_OPTIONS,
   "บันทึกตัวเลือกทั้งหมดของ Core ลงในไฟล์การตั้งค่ารวม (retroarch-core-options.cfg)  เมื่อปิดการใช้งาน ตัวเลือกของแต่ละ Core จะถูกบันทึกลงในโฟลเดอร์/ไฟล์เฉพาะของ Core นั้น ๆ ภายในไดเรกทอรี 'Configs' ของ RetroArch"
   )

/* Settings > Saving */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SORT_SAVEFILES_ENABLE,
   "บันทึกไฟล์เซฟ โดยจัดแยกเข้าโฟลเดอร์ตามชื่อ Core "
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SORT_SAVEFILES_ENABLE,
   "จัดเรียงไฟล์เซฟลงในโฟลเดอร์ที่ตั้งชื่อตาม Core ที่ใช้ "
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SORT_SAVESTATES_ENABLE,
   "บันทึกสถานะ: แยกโฟลเดอร์ตามชื่อ Core"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SORT_SAVESTATES_ENABLE,
   "จัดเก็บไฟล์บันทึกสถานะลงในโฟลเดอร์ที่ตั้งชื่อตาม Core ที่ใช้งาน"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SORT_SAVEFILES_BY_CONTENT_ENABLE,
   "บันทึกไฟล์เซฟ: จัดเรียงเข้าโฟลเดอร์ตามไดเรกทอรีของคอนเทนต์"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SORT_SAVEFILES_BY_CONTENT_ENABLE,
   "จัดเรียงไฟล์เซฟ ลงในโฟลเดอร์ที่ตั้งชื่อตามไดเรกทอรีที่คอนเทนต์นั้นถูกเก็บอยู่"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SORT_SAVESTATES_BY_CONTENT_ENABLE,
   "บันทึกสถานะ: แยกโฟลเดอร์ตามไดเรกทอรีของเนื้อหา"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SORT_SAVESTATES_BY_CONTENT_ENABLE,
   "จัดเรียงการบันทึกสถานะลงในโฟลเดอร์ตามชื่อไดเรกทอรีที่เนื้อหานั้นจัดเก็บอยู่"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BLOCK_SRAM_OVERWRITE,
   "ไฟล์เซฟ: ไม่เขียนทับ SaveRAM เมื่อโหลดสถานะที่บันทึกไว้"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BLOCK_SRAM_OVERWRITE,
   "บล็อกไม่ให้ SaveRAM ถูกเขียนทับเมื่อโหลดสถานะที่บันทึกไว้ แต่อาจส่งผลให้ตัวเกมทำงานผิดปกติได้ในบางกรณี"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUTOSAVE_INTERVAL,
   "บันทึกไฟล์: SaveRAM ช่วงเวลาบันทึกอัตโนมัติ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUTOSAVE_INTERVAL,
   "บันทึก SaveRAM อัตโนมัติเป็นช่วงเวลา (วินาที) ทุกๆ ระยะเวลาที่กำหนดลงในหน่วยความจำถาวร"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUTOSAVE_INTERVAL,
   "บันทึก SaveRAM อัตโนมัติเป็นช่วงเวลา (วินาที) ทุกๆ ระยะเวลาที่กำหนด โดยปกติจะถูกปิดไว้จนกว่าจะมีการตั้งค่า หากตั้งค่าเป็น 0 จะเป็นการปิดการบันทึกอัตโนมัติ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REPLAY_CHECKPOINT_INTERVAL,
   "เล่นซ้ำ: ช่วงเวลาการบันทึกจุดเช็คพอยต์ออกมาเป็นวินาที"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REPLAY_CHECKPOINT_INTERVAL,
   "บันทึกจุดตรวจสอบสถานะเกมโดยอัตโนมัติระหว่างการบันทึกการเล่นซ้ำตามช่วงเวลาที่กำหนด (วินาที)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_REPLAY_CHECKPOINT_INTERVAL,
   "บันทึกสถานะเกมโดยอัตโนมัติระหว่างการบันทึกการเล่นซ้ำตามช่วงเวลาที่กำหนด โดยปกติจะถูกปิดใช้งานไว้ เว้นแต่จะมีการตั้งค่า โดยช่วงเวลามีหน่วยเป็นวินาที หากตั้งค่าเป็น 0 จะเป็น[...]"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REPLAY_CHECKPOINT_DESERIALIZE,
   "กำหนดว่าจะทำการอ่านค่าจุดเช็คพอยต์ที่เก็บไว้ในการเล่นซ้ำ ระหว่างการเล่นกลับตามปกติหรือไม่"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REPLAY_CHECKPOINT_DESERIALIZE,
   "การเล่นซ้ำ: การอ่านค่าจุดเช็คพอยต์"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_REPLAY_CHECKPOINT_DESERIALIZE,
   "กำหนดว่าจะทำการอ่านค่าจุดเช็คพอยต์ที่เก็บไว้ในการเล่นซ้ำระหว่างการเล่นกลับตามปกติหรือไม่ โดยทั่วไปควรตั้งค่าเป็นเปิดใช้งาน แต่บางคอร์อาจทำงานผิดปกติเมื่อมีการอ่านค่าข้อ[...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVESTATE_AUTO_INDEX,
   "บันทึกสถานะ: เพิ่มหมายเลขแบบอัตโนมัติ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVESTATE_AUTO_INDEX,
   "ก่อนที่จะทำการบันทึกสถานะ หมายเลขบันทึกสถานะจะถูกเพิ่มขึ้นโดยอัตโนมัติ และเมื่อโหลดเนื้อหา หมายเลขจะถูกตั้งค่าไปยังหมายเลขที่สูงสุดที่มีอยู่เดิม"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REPLAY_AUTO_INDEX,
   "เล่นซ้ำ: เพิ่มหมายเลขโดยอัตโนมัติ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REPLAY_AUTO_INDEX,
   "ก่อนที่จะทำการเล่นซ้ำ หมายเลขเล่นซ้ำ จะถูกเพิ่มขึ้นโดยอัตโนมัติ  เมื่อโหลดคอนเทนต์ หมายเลขนี้จะถูกตั้งค่าเป็นหมายเลขสูงสุดที่มีอยู่แล้ว"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVESTATE_MAX_KEEP,
   "บันทึกสถานะ: จำนวนหมายเลขที่เพิ่มขึ้นอัตโนมัติสูงสุดที่จะเก็บไว้"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVESTATE_MAX_KEEP,
   "จำกัดจำนวนของบันทึกสถานะที่จะถูกสร้างขึ้นเมื่อเปิดใช้งาน 'เพิ่มหมายเลขแบบอัตโนมัติ' หากจำนวนเกินขีดจำกัดขณะบันทึกสถานะใหม่ บันทึกสถานะเดิมที่มีหมายเลขต่ำสุดจะถูกลบออก หาก[...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REPLAY_MAX_KEEP,
   "เล่นซ้ำ: จำนวนการเพิ่มหมายเลขสูงสุดที่เก็บไว้"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REPLAY_MAX_KEEP,
   "จำกัดจำนวนการ เล่นซ้ำ ที่จะถูกสร้างขึ้นเมื่อเปิดใช้งาน 'เพิ่มหมายเลขโดยอัตโนมัติ' หากจำนวนเกินขีดจำกัดขณะที่บันทึกการ เล่นซ้ำ ใหม่ ไฟล์การ เล่นซ้ำ เดิมที่มี หมายเลข ต่ำสุดจะถู[...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVESTATE_AUTO_SAVE,
   "บันทึกสถานะ: บันทึกอัตโนมัติ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVESTATE_AUTO_SAVE,
   "ทำการบันทึกสถานะโดยอัตโนมัติเมื่อปิดเนื้อหา บันทึกสถานะนี้จะถูกโหลดขึ้นมาเมื่อเริ่มโปรแกรมหากเปิดใช้งาน 'โหลดอัตโนมัติ' ไว้"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVESTATE_AUTO_LOAD,
   "บันทึกสถานะ: โหลดอัตโนมัติ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVESTATE_AUTO_LOAD,
   "โหลดบันทึกสถานะอัตโนมัติโดยจะเริ่มทำงานเมื่อเปิดโปรแกรมครั้งแรก (Startup) หากมีการทำบันทึกสถานะอัตโนมัติเอาไว้ก่อนหน้านี้"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVESTATE_THUMBNAIL_ENABLE,
   "บันทึกสถานะ: ภาพตัวอย่าง"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVESTATE_THUMBNAIL_ENABLE,
   "แสดงภาพตัวอย่างของบันทึกสถานะ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVE_FILE_COMPRESSION,
   "บันทึกไฟล์: การบีบอัด"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVE_FILE_COMPRESSION,
   "เขียนไฟล์ SaveRAM ในรูปแบบไฟล์เก็บถาวร ช่วยลดขนาดไฟล์ได้อย่างมากแต่ต้องแลกกับการเพิ่มเวลาในการบันทึก/โหลด (เพียงเล็กน้อย)\nใช้ได้เฉพาะกับ Core ที่รองรับการบันทึกผ่านอินเทอร์เฟซ SaveRAM มา[...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVESTATE_FILE_COMPRESSION,
   "บันทึกสถานะ: การบีบอัดไฟล์"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVESTATE_FILE_COMPRESSION,
   "เขียนไฟล์บันทึกสถานะในรูปแบบไฟล์บีบอัด ช่วยลดขนาดไฟล์ลงได้อย่างมากแต่ต้องแลกมาด้วยการใช้เวลาในการบันทึกและโหลดที่นานขึ้น"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVEFILES_IN_CONTENT_DIR_ENABLE,
   "บันทึกไฟล์: เขียนลงในโฟลเดอร์ของเนื้อหา"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVEFILES_IN_CONTENT_DIR_ENABLE,
   "ใช้โฟลเดอร์ของเนื้อหาเป็นโฟลเดอร์สำหรับบันทึกไฟล์"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVESTATES_IN_CONTENT_DIR_ENABLE,
   "บันทึกสถานะ: เขียนลงในโฟลเดอร์ของเนื้อหา"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVESTATES_IN_CONTENT_DIR_ENABLE,
   "ใช้โฟลเดอร์ของเนื้อหาเป็นโฟลเดอร์สำหรับบันทึกสถานะ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEMFILES_IN_CONTENT_DIR_ENABLE,
   "ไฟล์ระบบอยู่ในโฟลเดอร์ของเนื้อหา"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SYSTEMFILES_IN_CONTENT_DIR_ENABLE,
   "ใช้โฟลเดอร์ของเนื้อหาเป็นโฟลเดอร์ System/BIOS"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SORT_SCREENSHOTS_BY_CONTENT_ENABLE,
   "จับภาพหน้าจอ: จัดแยกโฟลเดอร์ตามโฟลเดอร์ของเนื้อหา"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SORT_SCREENSHOTS_BY_CONTENT_ENABLE,
   "จัดเก็บภาพหน้าจอแยกตามโฟลเดอร์ที่ตั้งชื่อตามโฟลเดอร์ของเนื้อหา"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCREENSHOTS_IN_CONTENT_DIR_ENABLE,
   "จับภาพหน้าจอ: เขียนลงในโฟลเดอร์ของเนื้อหา"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SCREENSHOTS_IN_CONTENT_DIR_ENABLE,
   "ใช้โฟลเดอร์ของเนื้อหาเป็นโฟลเดอร์สำหรับเก็บภาพหน้าจอ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_GPU_SCREENSHOT,
   "จับภาพหน้าจอ: ใช้ GPU"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_GPU_SCREENSHOT,
   "การจับภาพหน้าจอจะรวมผลลัพธ์จาก GPU Shader หากมีการใช้งานอยู่"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_RUNTIME_LOG,
   "บันทึกบันทึกเวลาการใช้งาน (แยกตาม Core)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_RUNTIME_LOG,
   "บันทึกข้อมูลระยะเวลาที่ใช้เล่นเนื้อหาแต่ละรายการ โดยแยกการเก็บสถิติตาม Core ที่ใช้งาน"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_RUNTIME_LOG_AGGREGATE,
   "บันทึกบันทึกเวลาการใช้งาน (รวมทั้งหมด)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_RUNTIME_LOG_AGGREGATE,
   "บันทึกข้อมูลระยะเวลาที่ใช้เล่นเนื้อหาแต่ละรายการ โดยเก็บสถิติเป็นยอดรวมสะสมจากทุก Core ที่ใช้งาน"
   )

/* Settings > Logging */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOG_VERBOSITY,
   "ความละเอียดการบันทึก"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOG_VERBOSITY,
   "บันทึก Events log ลงหน้าต่างคำสั่งหรือไฟล์"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FRONTEND_LOG_LEVEL,
   "ระดับการบันทึก Log ของ Frontend"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FRONTEND_LOG_LEVEL,
   "กำหนดระดับการบันทึก Log สำหรับ Frontend หากระดับของ Log ที่ส่งมาต่ำกว่าค่านี้ ข้อมูลดังกล่าวจะถูกละเว้น"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LIBRETRO_LOG_LEVEL,
   "ระดับการบันทึก Log ของ Core"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LIBRETRO_LOG_LEVEL,
   "กำหนดระดับการบันทึก Log สำหรับ Core หากระดับของ Log ที่ส่งมาจาก Core ต่ำกว่าค่านี้ ข้อมูลดังกล่าวจะถูกละเว้น"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_LIBRETRO_LOG_LEVEL,
   "กำหนดระดับการบันทึก Log สำหรับ libretro core (GET_LOG_INTERFACE) หากระดับของ Log ที่ส่งมาจาก libretro core ต่ำกว่าระดับ libretro_log ข้อมูลดังกล่าวจะถูกละเว้น ทั้งนี้ Log ระดับ DEBUG จะถูกละเว้นเสมอ เว้นแต่จะเปิดใช้งานโหมดละเ[...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOG_TO_FILE,
   "บันทึกไฟล์ Log"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOG_TO_FILE,
   "เปลี่ยนเส้นทางข้อความบันทึก log ระบบไปยังไฟล์ จำเป็นต้องเปิดใช้งาน 'ความละเอียดการบันทึก' ก่อน"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOG_TO_FILE_TIMESTAMP,
   "บันทึกเวลาในไฟล์ Log"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOG_TO_FILE_TIMESTAMP,
   "เมื่อบันทึกลงไฟล์ ให้บันทึกแยกเป็นไฟล์ใหม่ตามเวลาที่เริ่มใช้งาน หากปิดไว้ ไฟล์บันทึกเดิมจะถูกเขียนทับทุกครั้งที่เริ่ม RetroArch ใหม่"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PERFCNT_ENABLE,
   "ตัวนับประสิทธิภาพ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PERFCNT_ENABLE,
   "ตัวนับประสิทธิภาพสำหรับ RetroArch และ Core ต่างๆ ข้อมูลตัวนับสามารถช่วยระบุจุดคอขวดของระบบและช่วยในการปรับแต่งประสิทธิภาพให้ดียิ่งขึ้น"
   )

/* Settings > File Browser */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHOW_HIDDEN_FILES,
   "แสดงไฟล์และไดเรกทอรีที่ซ่อนอยู่"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SHOW_HIDDEN_FILES,
   "แสดงไฟล์และไดเรกทอรีที่ซ่อนอยู่ในการเลือกดูไฟล์"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NAVIGATION_BROWSER_FILTER_SUPPORTED_EXTENSIONS_ENABLE,
   "ตัวกรองนามสกุลไฟล์ที่ไม่รู้จัก"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NAVIGATION_BROWSER_FILTER_SUPPORTED_EXTENSIONS_ENABLE,
   "กรองไฟล์ที่แสดงในการเลือกดูไฟล์ โดยเลือกเฉพาะนามสกุลไฟล์ที่รองรับเท่านั้น"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FILTER_BY_CURRENT_CORE,
   "กรองตาม Core ปัจจุบัน"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FILTER_BY_CURRENT_CORE,
   "กรองไฟล์ที่แสดงในการเลือกดูไฟล์ โดยเลือกตาม Core ที่ใช้งานอยู่ในปัจจุบัน"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_USE_LAST_START_DIRECTORY,
   "จำไดเรกทอรีเริ่มต้นที่ใช้งานล่าสุด"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_USE_LAST_START_DIRECTORY,
   "เปิดการเลือกดูไฟล์ที่ตำแหน่งล่าสุดที่เคยใช้งาน เมื่อโหลดเนื้อหาจากไดเรกทอรีเริ่มต้น หมายเหตุ: ตำแหน่งจะถูกรีเซ็ตเป็นค่าเริ่มต้นเมื่อเริ่มการทำงาน RetroArch ใหม่"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_SUGGEST_ALWAYS,
   "แนะนำ Core เสมอ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_SUGGEST_ALWAYS,
   "แนะนำ Core ที่ใช้งานได้เสมอ แม้ว่าจะมีการโหลด Core ไว้เองแล้วก็ตาม"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_USE_BUILTIN_PLAYER,
   "ใช้เครื่องเล่นสื่อภายในตัว"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_USE_BUILTIN_PLAYER,
   "แสดงไฟล์ที่เครื่องเล่นสื่อรองรับในการเลือกดูไฟล์"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_USE_BUILTIN_IMAGE_VIEWER,
   "ใช้เครื่องเล่นรูปภาพภายในตัว"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_USE_BUILTIN_IMAGE_VIEWER,
   "แสดงไฟล์ที่เครื่องเล่นรูปภาพรองรับในการเลือกดูไฟล์"
   )

/* Settings > Frame Throttle */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REWIND_SETTINGS,
   "ย้อนเวลา"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REWIND_SETTINGS,
   "ปรับเปลี่ยนการตั้งค่าการย้อนกลับ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FRAME_TIME_COUNTER_SETTINGS,
   "ตัวนับเวลาเฟรม"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FRAME_TIME_COUNTER_SETTINGS,
   "ปรับเปลี่ยนการตั้งค่าที่มีผลต่อตัวนับเวลาเฟรม\nจะใช้งานได้เมื่อปิดการใช้งานวิดีโอแบบเธรดเท่านั้น"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FASTFORWARD_RATIO,
   "อัตราการเร่งความเร็ว"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FASTFORWARD_RATIO,
   "อัตราความเร็วสูงสุดที่จะให้เนื้อหาทำงานเมื่อใช้งานการเร่งความเร็ว (เช่น 5.0x สำหรับเนื้อหา 60 fps = จำกัดที่ 300 fps) หากตั้งค่าเป็น 0.0x จะไม่จำกัดอัตราการเร่งความเร็ว (ไม่มีการจำกัด FPS)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FASTFORWARD_RATIO,
   "อัตราความเร็วสูงสุดที่จะให้เนื้อหาทำงานเมื่อใช้การเร่งความเร็ว (เช่น 5.0 สำหรับเนื้อหา 60 fps => จำกัดที่ 300 fps)\nRetroArch จะเข้าสู่โหมดพักการทำงานเพื่อให้แน่ใจว่าความเร็วจะไม่เกินอัตราสูงส[...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FASTFORWARD_FRAMESKIP,
   "ข้ามเฟรมเมื่อเร่งความเร็ว"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FASTFORWARD_FRAMESKIP,
   "ข้ามเฟรมตามอัตราการเร่งความเร็ว สิ่งนี้ช่วยประหยัดพลังงานและเปิดโอกาสให้ใช้การจำกัดเฟรมจากซอฟต์แวร์ภายนอกได้"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SLOWMOTION_RATIO,
   "อัตราการเล่นแบบสโลว์โมชั่น"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SLOWMOTION_RATIO,
   "อัตราที่คอนเทนต์จะเล่นเมื่อใช้โหมดสโลว์โมชั่น"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_ENUM_THROTTLE_FRAMERATE,
   "จำกัดอัตราเฟรมเรตของเมนู"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_ENUM_THROTTLE_FRAMERATE,
   "ตรวจสอบให้แน่ใจว่ามีการจำกัดอัตราเฟรมเรตในขณะที่อยู่ในเมนู"
   )

/* Settings > Frame Throttle > Rewind */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REWIND_ENABLE,
   "สนับสนุนการย้อนกลับ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REWIND_ENABLE,
   "ย้อนกลับไปยังจุดก่อนหน้าในการเล่นเกมล่าสุด ซึ่งจะส่งผลกระทบต่อประสิทธิภาพการทำงานอย่างมากในขณะเล่น"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REWIND_GRANULARITY,
   "จำนวนเฟรมในการย้อนกลับ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REWIND_GRANULARITY,
   "จำนวนเฟรมที่จะย้อนกลับต่อหนึ่งขั้นตอน ค่าที่สูงขึ้นจะช่วยเพิ่มความเร็วในการย้อนกลับออกไปอีก"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REWIND_BUFFER_SIZE,
   "ขนาดบัฟเฟอร์การย้อนกลับ (MB)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REWIND_BUFFER_SIZE,
   "ขนาดหน่วยความจำ (MB) ที่จะสำรองไว้สำหรับบัฟเฟอร์การย้อนกลับ การเพิ่มค่านี้จะช่วยให้สามารถจัดเก็บประวัติการย้อนกลับได้ยาวนานขึ้น"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REWIND_BUFFER_SIZE_STEP,
   "ขนาดขั้นในการปรับบัฟเฟอร์การย้อนกลับ (MB)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REWIND_BUFFER_SIZE_STEP,
   "ทุกครั้งที่คุณปรับค่า ขนาดบัฟเฟอร์การย้อนกลับ มันจะเปลี่ยนขึ้นหรือลงตามจำนวนที่กำหนดไว้ใน ขนาดขั้นในการปรับขยาย เสมอ"
   )

/* Settings > Frame Throttle > Frame Time Counter */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FRAME_TIME_COUNTER_RESET_AFTER_FASTFORWARDING,
   "รีเซ็ตหลังจากเร่งความเร็ว"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FRAME_TIME_COUNTER_RESET_AFTER_FASTFORWARDING,
   "รีเซ็ตตัวนับเวลาเฟรม หลังจากหยุดการเร่งความเร็ว"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FRAME_TIME_COUNTER_RESET_AFTER_LOAD_STATE,
   "รีเซ็ตหลังจากโหลดสถานะ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FRAME_TIME_COUNTER_RESET_AFTER_LOAD_STATE,
   "รีเซ็ตตัวนับเวลาเฟรม หลังจากโหลดสถานะ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FRAME_TIME_COUNTER_RESET_AFTER_SAVE_STATE,
   "รีเซ็ตหลังจากบันทึกสถานะ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FRAME_TIME_COUNTER_RESET_AFTER_SAVE_STATE,
   "รีเซ็ตตัวนับเวลาเฟรมหลังจากทำการบันทึกสถานะ"
   )

/* Settings > Recording */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_QUALITY,
   "คุณภาพการบันทึกวิดีโอ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_CONFIG_TYPE_RECORDING_CUSTOM,
   "กำหนดเอง"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_CONFIG_TYPE_RECORDING_LOW_QUALITY,
   "ตํ่า"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_CONFIG_TYPE_RECORDING_MED_QUALITY,
   "ปานกลาง"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_CONFIG_TYPE_RECORDING_HIGH_QUALITY,
   "สูง"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_CONFIG_TYPE_RECORDING_LOSSLESS_QUALITY,
   "ไม่สูญเสียคุณภาพ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_CONFIG_TYPE_RECORDING_WEBM_FAST,
   "WebM เร็ว"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_CONFIG_TYPE_RECORDING_WEBM_HIGH_QUALITY,
   "WebM คุณภาพสูง"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RECORD_CONFIG,
   "การตั้งค่าการบันทึกวิดีโอแบบกำหนดเอง"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_THREADS,
   "จำนวนเธรดในการบันทึกวิดีโอ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_POST_FILTER_RECORD,
   "ใช้การบันทึกวิดีโอหลังฟิลเตอร์"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_POST_FILTER_RECORD,
   "บันทึกภาพหลังจากใช้งานฟิลเตอร์ (แต่ไม่รวมเชดเดอร์) วิดีโอที่ได้จะมีความสวยงามเหมือนกับที่คุณเห็นบนหน้าจอของคุณ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_GPU_RECORD,
   "ใช้การบันทึกวิดีโอด้วย GPU"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_GPU_RECORD,
   "บันทึกวิดีโอจากผลลัพธ์ของเชดเดอร์บน GPU หากมีการใช้งาน"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STREAMING_MODE,
   "โหมดการสตรีม"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_STREAMING_MODE_LOCAL,
   "ในเครื่อง"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_STREAMING_MODE_CUSTOM,
   "กำหนดเอง"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_STREAM_QUALITY,
   "คุณภาพการสตรีม"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_CONFIG_TYPE_STREAMING_CUSTOM,
   "กำหนดเอง"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_CONFIG_TYPE_STREAMING_LOW_QUALITY,
   "ตํ่า"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_CONFIG_TYPE_STREAMING_MED_QUALITY,
   "ปานกลาง"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_CONFIG_TYPE_STREAMING_HIGH_QUALITY,
   "สูง"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STREAM_CONFIG,
   "การตั้งค่าการสตรีมแบบกำหนดเอง"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STREAMING_TITLE,
   "ชื่อสตรีม"
   )

/* Settings > On-Screen Display */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ONSCREEN_OVERLAY_SETTINGS,
   "Overlay ซ้อนทับบนหน้าจอ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ONSCREEN_OVERLAY_SETTINGS,
   "ปรับ bezels และตัวควบคุมบนหน้าจอ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ONSCREEN_VIDEO_LAYOUT_SETTINGS,
   "เลย์เอาต์วิดีโอ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ONSCREEN_VIDEO_LAYOUT_SETTINGS,
   "ปรับเลย์เอาต์วิดีโอ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ONSCREEN_NOTIFICATIONS_SETTINGS,
   "การแจ้งเตือนบนหน้าจอ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ONSCREEN_NOTIFICATIONS_SETTINGS,
   "ปรับแต่งการแจ้งเตือนบนหน้าจอ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ONSCREEN_NOTIFICATIONS_VIEWS_SETTINGS,
   "การมองเห็นการแจ้งเตือน"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ONSCREEN_NOTIFICATIONS_VIEWS_SETTINGS,
   "เปิด-ปิด การมองเห็นของการแจ้งเตือนแต่ละประเภท"
   )

/* Settings > On-Screen Display > On-Screen Overlay */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_ENABLE,
   "แสดงผลการซ้อนทับบนหน้าจอ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_ENABLE,
   "การซ้อนทับบนหน้าจอใช้สำหรับขอบภาพและตัวควบคุมบนหน้าจอ"
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_BEHIND_MENU,
   "แสดงการซ้อนทับไว้หลังเมนู"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_BEHIND_MENU,
   "แสดง Overlay ไว้ด้านหลังแทนที่จะอยู่ด้านหน้าเมนู"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_HIDE_IN_MENU,
   "ซ่อน Overlay ในเมนู"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_HIDE_IN_MENU,
   "ซ่อน Overlay ขณะที่อยู่ในเมนู และแสดงอีกครั้งเมื่อออกจากเมนู"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_HIDE_WHEN_GAMEPAD_CONNECTED,
   "ซ่อน Overlay เมื่อเชื่อมต่อจอยคอนโทรลเลอร์"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_HIDE_WHEN_GAMEPAD_CONNECTED,
   "ซ่อน Overlay เมื่อมีการเชื่อมต่อคอนโทรลเลอร์ในพอร์ต 1 และแสดงอีกครั้งเมื่อตัดการเชื่อมต่อคอนโทรลเลอร์"
   )
#if defined(ANDROID)
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_HIDE_WHEN_GAMEPAD_CONNECTED_ANDROID,
   "ซ่อน Overlay เมื่อมีการเชื่อมต่อคอนโทรลเลอร์ในพอร์ต 1 โดย Overlay จะไม่ถูกเรียกคืนให้โดยอัตโนมัติเมื่อตัดการเชื่อมต่อคอนโทรลเลอร์"
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_SHOW_INPUTS,
   "แสดง Input บน Overlay"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_SHOW_INPUTS,
   "แสดงปุ่มที่กดบน Overlay หน้าจอ โดย 'สัมผัส' จะเน้นองค์ประกอบบน Overlay ที่ถูกกด/คลิก ส่วน 'Physical (คอนโทรลเลอร์)' จะเน้นอินพุตจริงที่ส่งไปยัง Core ซึ่งมักจะมาจากคอนโทรลเลอร์หรือคีย์บอร์ดที่เชื่อม[...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_SHOW_INPUTS_TOUCHED,
   "สัมผัส"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_SHOW_INPUTS_PHYSICAL,
   "Physical (คอนโทรลเลอร์)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_SHOW_INPUTS_PORT,
   "แสดงอินพุตจากพอร์ต"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_SHOW_INPUTS_PORT,
   "เลือกพอร์ตของอุปกรณ์อินพุตที่จะตรวจสอบ เมื่อตั้งค่า 'แสดงการกดปุ่มบน Overlay' เป็น 'Physical (คอนโทรลเลอร์)'"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_SHOW_MOUSE_CURSOR,
   "แสดงตัวชี้เมาส์พร้อมกับ Overlay"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_SHOW_MOUSE_CURSOR,
   "แสดงตัวชี้เมาส์เมื่อใช้งาน Overlay บนหน้าจอ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_AUTO_ROTATE,
   "หมุน Overlay อัตโนมัติ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_AUTO_ROTATE,
   "หาก Overlay ปัจจุบันรองรับ จะทำการหมุน Layout โดยอัตโนมัติเพื่อให้สอดคล้องกับการวางแนวของหน้าจอหรืออัตราส่วนภาพ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_AUTO_SCALE,
   "ปรับขนาด Overlay อัตโนมัติ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_AUTO_SCALE,
   "ปรับขนาดของ Overlay และระยะห่างขององค์ประกอบ UI โดยอัตโนมัติเพื่อให้สอดคล้องกับอัตราส่วนภาพของหน้าจอ ซึ่งจะให้ผลลัพธ์ที่ดีที่สุดกับการใช้งาน Overlay รูปแบบคอนโทรลเลอร์"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_DPAD_DIAGONAL_SENSITIVITY,
   "ความไวในแนวทแยงของ D-Pad"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_DPAD_DIAGONAL_SENSITIVITY,
   "ปรับขนาดของพื้นที่แนวทแยง กำหนดเป็น 100% เพื่อให้มีความสมมาตรแบบ 8 ทิศทาง"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_ABXY_DIAGONAL_SENSITIVITY,
   "ความไวในการกดปุ่ม ABXY พร้อมกัน"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_ABXY_DIAGONAL_SENSITIVITY,
   "ปรับขนาดของพื้นที่ทับซ้อนในปุ่มกดหลักรูปเพชร กำหนดเป็น 100% เพื่อให้มีความสมมาตรแบบ 8 ทิศทาง"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_ANALOG_RECENTER_ZONE,
   "โซนการคืนค่ากึ่งกลางของอนาล็อก"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_ANALOG_RECENTER_ZONE,
   "อินพุตของอนาล็อกจะอ้างอิงจากจุดสัมผัสแรก หากมีการกดภายในโซนนี้"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_AUTOLOAD_PREFERRED,
   "โหลด Overlay ที่ต้องการโดยอัตโนมัติ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_AUTOLOAD_PREFERRED,
   "เลือกโหลด Overlay ตามชื่อของระบบก่อนที่จะใช้ค่าเริ่มต้น จะถูกข้ามหากมีการตั้งค่าการเขียนทับไว้สำหรับชุดค่าล่วงหน้านั้นๆ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_OPACITY,
   "ความโปร่งใสของ Overlay"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_OPACITY,
   "ความโปร่งใสขององค์ประกอบ UI ทั้งหมดบน Overlay"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_PRESET,
   "เลือก Overlay จากเบราว์เซอร์ไฟล์"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_SCALE_LANDSCAPE,
   "ขนาด Overlay (แนวนอน)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_SCALE_LANDSCAPE,
   "มาตราส่วนขององค์ประกอบ UI ทั้งหมดบน Overlay เมื่อใช้งานในแนวนอน"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_ASPECT_ADJUST_LANDSCAPE,
   "การปรับสัดส่วนภาพของ Overlay (แนวนอน)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_ASPECT_ADJUST_LANDSCAPE,
   "ใช้ค่าการแก้ไขสัดส่วนภาพกับ Overlay เมื่อใช้งานในแนวนอน โดยค่าบวกจะเพิ่มความกว้างและค่าลบจะลดความกว้างของ Overlay ออกไป โดยมีผลต่อพื้นที่การแสดงผลจริง"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_X_SEPARATION_LANDSCAPE,
   "การแยกส่วนของ Overlay ในแนวนอน"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_X_SEPARATION_LANDSCAPE,
   "หากชุดค่าล่วงหน้าปัจจุบันรองรับ ให้ปรับระยะห่างระหว่างองค์ประกอบ UI ในซีกซ้ายและซีกขวาของ Overlay เมื่อใช้งานในแนวนอน โดยค่าบวกจะเพิ่มการแยกของทั้งสองซีกออกจากกัน และค่าลบจะลดระ[...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_Y_SEPARATION_LANDSCAPE,
   "การแยกส่วนของ Overlay ในแนวตั้ง (แนวนอน)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_Y_SEPARATION_LANDSCAPE,
   "หากชุดค่าล่วงหน้าปัจจุบันรองรับ ให้ปรับระยะห่างระหว่างองค์ประกอบ UI ในซีกบนและซีกล่างของ Overlay เมื่อใช้งานในแนวนอน โดยค่าบวกจะเพิ่มการแยกของทั้งสองซีกออกจากกัน และค่าลบจะลดระย[...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_X_OFFSET_LANDSCAPE,
   "การขยับตำแหน่ง Overlay X (แนวนอน)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_X_OFFSET_LANDSCAPE,
   "ระยะขยับของ Overlay เมื่อใช้งานในแนวนอน โดยค่าบวกจะขยับ Overlay ไปทางขวา และค่าลบจะขยับไปทางซ้าย"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_Y_OFFSET_LANDSCAPE,
   "การขยับตำแหน่ง Overlay Y (แนวนอน)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_Y_OFFSET_LANDSCAPE,
   "ระยะขยับของ Overlay ในแนวตั้ง เมื่อใช้งานในแนวนอน โดยค่าบวกจะขยับ Overlay ขึ้นด้านบน และค่าลบจะขยับลงด้านล่าง"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_SCALE_PORTRAIT,
   "ขนาดของ Overlay (แนวตั้ง)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_SCALE_PORTRAIT,
   "ขนาดขององค์ประกอบ UI ทั้งหมดบน Overlay เมื่อใช้งานในแนวตั้ง"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_ASPECT_ADJUST_PORTRAIT,
   "การปรับอัตราส่วนภาพของ Overlay (แนวตั้ง)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_ASPECT_ADJUST_PORTRAIT,
   "ปรับค่าแก้ไขอัตราส่วนภาพของ Overlay เมื่อใช้งานในแนวตั้ง โดยค่าบวกจะเพิ่มความสูง (และค่าลบจะลดความสูง) ของ Overlay ที่แสดงผลจริง"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_X_SEPARATION_PORTRAIT,
   "การแยกองค์ประกอบ Overlay ในแนวนอน (แนวตั้ง)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_X_SEPARATION_PORTRAIT,
   "หากรองรับโดยค่าที่ตั้งไว้ปัจจุบัน จะเป็นการปรับระยะห่างระหว่างองค์ประกอบ UI ในส่วนซีกซ้ายและซีกขวาของ Overlay เมื่อใช้งานในแนวตั้ง โดยค่าบวกจะเพิ่มระยะห่าง (และค่าลบจะลดระยะห่าง) [...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_Y_SEPARATION_PORTRAIT,
   "การแยกองค์ประกอบ Overlay ในแนวตั้ง (แนวตั้ง)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_Y_SEPARATION_PORTRAIT,
   "หากค่าพรีเซ็ตปัจจุบันรองรับ ให้ปรับระยะห่างระหว่างองค์ประกอบ UI ในส่วนบนและส่วนล่างของการซ้อนทับเมื่อใช้งานในแนวตั้ง โดยค่าที่เป็นบวกจะเพิ่มระยะห่าง (ในขณะที่ค่าที่เป็นลบจะ[...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_X_OFFSET_PORTRAIT,
   "(แนวตั้ง) ระยะห่างแนวนอนของการซ้อนทับ X"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_X_OFFSET_PORTRAIT,
   "(แนวตั้ง) ระยะห่างแนวนอนของการซ้อนทับเมื่อใช้งานในแนวตั้ง โดยค่าที่เป็นบวกจะเลื่อนการซ้อนทับไปทางขวา และค่าที่เป็นลบจะเลื่อนไปทางซ้าย"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_Y_OFFSET_PORTRAIT,
   "(แนวตั้ง) ระยะห่างแนวตั้งของการซ้อนทับ Y"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_Y_OFFSET_PORTRAIT,
   "(แนวตั้ง) ระยะห่างแนวตั้งของการซ้อนทับเมื่อใช้งานในแนวตั้ง โดยค่าที่เป็นบวกจะเลื่อนการซ้อนทับขึ้นด้านบน และค่าที่เป็นลบจะเลื่อนลงด้านล่าง"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OSK_OVERLAY_SETTINGS,
   "การซ้อนทับแป้นพิมพ์"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OSK_OVERLAY_SETTINGS,
   "เลือกและปรับแต่งการซ้อนทับแป้นพิมพ์"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_POINTER_ENABLE,
   "เปิดใช้งานการซ้อนทับสำหรับไลต์กัน เมาส์ และพอยเทอร์"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_POINTER_ENABLE,
   "ใช้การสัมผัสใดๆ ที่ไม่ได้เป็นการกดปุ่มตัวควบคุมเพื่อสร้างอินพุตสำหรับอุปกรณ์ชี้ตำแหน่งให้กับคอร์ (Core)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_LIGHTGUN_SETTINGS,
   "การซ้อนทับไลต์กัน"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_LIGHTGUN_SETTINGS,
   "กำหนดค่าอินพุตไลต์กันที่ส่งมาจากการซ้อนทับบนหน้าจอ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_MOUSE_SETTINGS,
   "การซ้อนทับเมาส์"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_MOUSE_SETTINGS,
   "กำหนดค่าอินพุตเมาส์ที่ส่งมาจากการซ้อนทับบนหน้าจอ หมายเหตุ: การแตะด้วย 1, 2 และ 3 นิ้ว จะเป็นการส่งคำสั่งคลิกซ้าย, ขวา และกลาง ตามลำดับ"
   )

/* Settings > On-Screen Display > On-Screen Overlay > Keyboard Overlay */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OSK_OVERLAY_PRESET,
   "พรีเซ็ตการซ้อนทับแป้นพิมพ์"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OSK_OVERLAY_PRESET,
   "เลือกพรีเซ็ตการซ้อนทับแป้นพิมพ์จากตัวเลือกไฟล์"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OSK_OVERLAY_AUTO_SCALE,
   "ปรับขนาดการซ้อนทับแป้นพิมพ์อัตโนมัติ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OSK_OVERLAY_AUTO_SCALE,
   "ปรับขนาดการซ้อนทับแป้นพิมพ์ให้เป็นไปตามอัตราส่วนภาพดั้งเดิม หากปิดการใช้งานจะเป็นการยืดให้เต็มหน้าจอ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OSK_OVERLAY_OPACITY,
   "ความโปร่งใสของการซ้อนทับแป้นพิมพ์"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OSK_OVERLAY_OPACITY,
   "ระดับความโปร่งใสขององค์ประกอบ UI ทั้งหมดของการซ้อนทับแป้นพิมพ์"
   )

/* Settings > On-Screen Display > On-Screen Overlay > Overlay Lightgun */

MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_LIGHTGUN_PORT,
   "ตั้งค่าพอร์ตของ Core เพื่อรับสัญญาณอินพุตจากไลต์กันบนหน้าจอซ้อนทับ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_LIGHTGUN_PORT_ANY,
   "ทั้งหมด"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_LIGHTGUN_TRIGGER_ON_TOUCH,
   "เหนี่ยวไกเมื่อสัมผัส"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_LIGHTGUN_TRIGGER_ON_TOUCH,
   "ส่งสัญญาณอินพุตการเหนี่ยวไกไปพร้อมกับอินพุตพอยเตอร์"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_LIGHTGUN_TRIGGER_DELAY,
   "ความหน่วงการเหนี่ยวไก (เฟรม)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_LIGHTGUN_TRIGGER_DELAY,
   "หน่วงเวลาการเหนี่ยวไกเพื่อให้พอยเตอร์มีเวลาเคลื่อนที่ การหน่วงเวลานี้ยังใช้เพื่อรอการนับจำนวนการสัมผัสแบบมัลติทัชที่ถูกต้องด้วย"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_LIGHTGUN_TWO_TOUCH_INPUT,
   "อินพุตแบบสัมผัส 2 จุด"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_LIGHTGUN_TWO_TOUCH_INPUT,
   "เลือกอินพุตที่จะส่งเมื่อมีพอยเตอร์สองจุดบนหน้าจอ ควรตั้งค่าความหน่วงการเหนี่ยวไกให้ไม่เป็นศูนย์เพื่อแยกแยะจากอินพุตอื่นๆ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_LIGHTGUN_THREE_TOUCH_INPUT,
   "อินพุตแบบสัมผัส 3 จุด"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_LIGHTGUN_THREE_TOUCH_INPUT,
   "เลือกอินพุตที่จะส่งเมื่อมีพอยเตอร์สามจุดบนหน้าจอ ควรตั้งค่าความหน่วงการเหนี่ยวไกให้ไม่เป็นศูนย์เพื่อแยกแยะจากอินพุตอื่นๆ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_LIGHTGUN_FOUR_TOUCH_INPUT,
   "อินพุตแบบสัมผัส 4 จุด"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_LIGHTGUN_FOUR_TOUCH_INPUT,
   "เลือกอินพุตที่จะส่งเมื่อมีพอยเตอร์สี่จุดบนหน้าจอ ควรตั้งค่าความหน่วงการเหนี่ยวไกให้ไม่เป็นศูนย์เพื่อแยกแยะจากอินพุตอื่นๆ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_LIGHTGUN_ALLOW_OFFSCREEN,
   "อนุญาตให้นอกหน้าจอ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_LIGHTGUN_ALLOW_OFFSCREEN,
   "อนุญาตให้นอกหน้าจอ หากปิดใช้งานจะจำกัดขอบเขตการเล็งที่อยู่นอกหน้าจอไว้ที่บริเวณขอบจอแทน"
   )

/* Settings > On-Screen Display > On-Screen Overlay > Overlay Mouse */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_MOUSE_SPEED,
   "ความเร็วเมาส์"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_MOUSE_SPEED,
   "ปรับความเร็วการเคลื่อนที่ของพอยเตอร์"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_MOUSE_HOLD_TO_DRAG,
   "กดค้างเพื่อลาก"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_MOUSE_HOLD_TO_DRAG,
   "กดหน้าจอค้างไว้เพื่อเริ่มการกดปุ่มค้างย้อนกลับ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_MOUSE_HOLD_MSEC,
   "ระยะเวลาการกดค้าง (มิลลิวินาที)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_MOUSE_HOLD_MSEC,
   "ปรับระยะเวลาที่ต้องกดค้างไว้เพื่อให้ระบบรับรู้เป็นการกดค้าง"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_MOUSE_DTAP_TO_DRAG,
   "แตะสองครั้งเพื่อลาก"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_MOUSE_DTAP_TO_DRAG,
   "แตะสองครั้งที่หน้าจอเพื่อเริ่มการกดปุ่มค้างในการแตะครั้งที่สอง (อาจทำให้การคลิกเมาส์มีความล่าช้าเพิ่มขึ้น)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_MOUSE_DTAP_MSEC,
   "ระยะเวลาการแตะสองครั้ง (มิลลิวินาที)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_MOUSE_DTAP_MSEC,
   "ปรับระยะเวลาที่อนุญาตให้แตะแต่ละครั้ง เพื่อตรวจจับการแตะสองครั้ง"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_MOUSE_SWIPE_THRESHOLD,
   "เกณฑ์การปัด"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_MOUSE_ALT_TWO_TOUCH_INPUT,
   "อินพุตสัมผัสสำรอง 2"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_MOUSE_ALT_TWO_TOUCH_INPUT,
   "ใช้การสัมผัสจุดที่สองเป็นปุ่มเมาส์ในระหว่างที่ควบคุมพอยเตอร์อยู่"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_MOUSE_SWIPE_THRESHOLD,
   "ปรับระยะการเคลื่อนที่ที่อนุญาตเมื่อตรวจจับการกดค้างหรือการแตะ โดยคำนวณเป็นเปอร์เซ็นต์ของมิติหน้าจอส่วนที่สั้นกว่า"
   )

/* Settings > On-Screen Display > On-Screen Notifications */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FONT_ENABLE,
   "การแจ้งเตือนบนหน้าจอ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FONT_ENABLE,
   "แสดงข้อความบนหน้าจอ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_WIDGET_SCALE_AUTO,
   "ปรับขนาดวิดเจ็ตกราฟิกโดยอัตโนมัติ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SAVE_STATE,
   "การแจ้งเตือนบันทึกสถานะ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_SAVE_STATE,
   "แสดงข้อความบนหน้าจอเมื่อทำการบันทึกและโหลดบันทึกสถานะ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SCREENSHOT_FLASH,
   "เอฟเฟกต์แสงแฟลชเมื่อถ่ายภาพหน้าจอ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_SCREENSHOT_FLASH,
   "แสดงเอฟเฟกต์แสงแฟลชสีขาวบนหน้าจอตามระยะเวลาที่กำหนดเมื่อถ่ายภาพหน้าจอ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SCREENSHOT_FLASH_NORMAL,
   "เปิด (ปกติ)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SCREENSHOT_FLASH_FAST,
   "เปิด (เร็ว)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_REFRESH_RATE,
   "แจ้งเตือนอัตราการรีเฟรชหน้าจอ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_BGCOLOR_GREEN,
   "กำหนดค่าสีเขียวของสีพื้นหลัง OSD โดยค่าที่ใช้งานได้จะอยู่ระหว่าง 0 ถึง 255"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_BGCOLOR_BLUE,
   "สีพื้นหลังการแจ้งเตือน (สีน้ำเงิน)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_BGCOLOR_BLUE,
   "กำหนดค่าสีน้ำเงินของสีพื้นหลัง OSD โดยค่าที่ใช้งานได้จะอยู่ระหว่าง 0 ถึง 255"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_BGCOLOR_OPACITY,
   "ความโปร่งใสของพื้นหลังการแจ้งเตือน"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_BGCOLOR_OPACITY,
   "กำหนดความโปร่งใสของสีพื้นหลัง OSD โดยค่าที่ใช้งานได้จะอยู่ระหว่าง 0.0 ถึง 1.0"
   )

/* Settings > User Interface */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SETTINGS,
   "รูปลักษณ์"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SETTINGS,
   "ปรับแต่งการตั้งค่ารูปลักษณ์หน้าจอเมนู"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_VIEWS_SETTINGS,
   "การแสดงผลรายการเมนู"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_VIEWS_SETTINGS,
   "เปิด/ปิด การแสดงผลรายการเมนูต่างๆ ใน RetroArch"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PAUSE_LIBRETRO,
   "หยุดเนื้อหาเมื่อเปิดใช้งานเมนู"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PAUSE_LIBRETRO,
   "หยุดเนื้อหาชั่วคราว หากมีการเปิดใช้งานเมนู"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PAUSE_NONACTIVE,
   "หยุดเนื้อหา เมื่อไม่ได้ใช้งานเมนู"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SAVESTATE_RESUME,
   "กลับมาเล่นเนื้อหาต่อหลังจากใช้งานบันทึกสถานะ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SAVESTATE_RESUME,
   "ปิดเมนูและกลับเข้าสู่เนื้อหาโดยอัตโนมัติหลังจากบันทึกหรือโหลดสถานะ การปิดใช้งานส่วนนี้อาจช่วยเพิ่มประสิทธิภาพการบันทึกสถานะให้ดีขึ้นบนอุปกรณ์ที่มีความเร็วต่ำมาก"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UI_COMPANION_ENABLE,
   "ส่วนเสริม UI"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UI_COMPANION_START_ON_BOOT,
   "เริ่มใช้งานส่วนเสริม UI เมื่อบูต"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_UI_COMPANION_START_ON_BOOT,
   "เริ่มใช้งานไดรเวอร์ส่วนเสริม ส่วนติดต่อผู้ใช้ เมื่อบูต (หากมี)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DESKTOP_MENU_ENABLE,
   "เมนูเดสก์ท็อป (ต้องรีสตาร์ท)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UI_COMPANION_TOGGLE,
   "เปิดเมนูเดสก์ท็อปเมื่อเริ่มต้นระบบ"
   )
#ifdef _3DS
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_BOTTOM_SETTINGS,
   "รูปลักษณ์หน้าจอด้านล่าง 3DS"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_BOTTOM_SETTINGS,
   "ปรับแต่งการตั้งค่ารูปลักษณ์หน้าจอด้านล่าง"
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_APPICON_SETTINGS,
   "ไอคอนแอป"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_APPICON_SETTINGS,
   "เปลี่ยนไอคอนแอป"
   )

/* Settings > User Interface > Menu Item Visibility */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_VIEWS_SETTINGS,
   "ทางลัด"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_VIEWS_SETTINGS,
   "เปิด/ปิด การแสดงรายการ ในเมนูทางลัด"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_VIEWS_SETTINGS,
   "ตั้งค่า"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_VIEWS_SETTINGS,
   "เปิด/ปิด การแสดงผลรายการต่างๆ ในเมนูการตั้งค่า"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_LOAD_CORE,
   "แสดง 'โหลด Core'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_LOAD_CORE,
   "แสดงตัวเลือก 'Load Core' ในเมนูหลัก"
   )
#ifdef HAVE_LAKKA
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_ADD_CONTENT_ENTRY_DISPLAY_MAIN_TAB,
   "เมนูหลัก"
   )

/* Settings > User Interface > Menu Item Visibility > Quick Menu */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_SAVESTATE_SUBMENU,
   "แสดงเมนูย่อย 'บันทึกสถานะ'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_SAVESTATE_SUBMENU,
   "แสดงตัวเลือกการบันทึกสถานะในเมนูย่อย"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_SAVE_LOAD_STATE,
   "แสดง 'บันทึก/โหลดสถานะ'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_SAVE_LOAD_STATE,
   "แสดงตัวเลือกสำหรับการบันทึก/โหลดสถานะ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_UNDO_SAVE_LOAD_STATE,
   "แสดง เลิกทำ 'บันทึก/โหลดสถานะ'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_UNDO_SAVE_LOAD_STATE,
   "แสดงตัวเลือกสำหรับการเลิกทำ บันทึก/โหลดสถานะ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_SHADERS,
   "แสดงตัวเลือก 'เชดเดอร์'"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_ADD_TO_FAVORITES,
   "แสดงตัวเลือก 'เพิ่มในรายการโปรด'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_ADD_TO_FAVORITES,
   "แสดงตัวเลือก 'เพิ่มในรายการโปรด'"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_ADD_TO_PLAYLIST,
   "แสดงตัวเลือก 'เพิ่มลงในเพลย์ลิสต์'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_ADD_TO_PLAYLIST,
   "แสดงตัวเลือก 'เพิ่มลงในเพลย์ลิสต์'"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_SET_CORE_ASSOCIATION,
   "แสดงตัวเลือก 'กำหนด Core'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_SET_CORE_ASSOCIATION,
   "แสดงตัวเลือก 'กำหนด Core' เมื่อไม่ได้รันเนื้อหาอยู่"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_RESET_CORE_ASSOCIATION,
   "แสดงตัวเลือก 'ล้างการกำหนด Core'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_RESET_CORE_ASSOCIATION,
   "แสดงตัวเลือก 'ล้างการกำหนด Core' เมื่อไม่ได้รันเนื้อหาอยู่"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_DOWNLOAD_THUMBNAILS,
   "แสดงตัวเลือก 'ดาวน์โหลดรูปตัวอย่าง'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_DOWNLOAD_THUMBNAILS,
   "แสดงตัวเลือก 'ดาวน์โหลดรูปตัวอย่าง' เมื่อไม่ได้รันเนื้อหาอยู่"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_INFORMATION,
   "แสดงตัวเลือก 'ข้อมูล'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_INFORMATION,
   "แสดงตัวเลือก 'ข้อมูล'"
   )

/* Settings > User Interface > Views > Settings */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_DRIVERS,
   "แสดงตัวเลือก 'ไดรเวอร์'"
   )


/* Settings > User Interface > Appearance */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_THUMBNAILS,
   "ภาพตัวอย่างหลัก"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_THUMBNAILS,
   "ประเภทของภาพตัวอย่างที่จะแสดง"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_THUMBNAIL_UPSCALE_THRESHOLD,
   "ขีดจำกัด การขยายขนาดภาพตัวอย่าง"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_THUMBNAIL_UPSCALE_THRESHOLD,
   "ขยายขนาดภาพตัวอย่างที่มีความกว้าง/ความสูงต่ำกว่าค่าที่กำหนดโดยอัตโนมัติ เพื่อเพิ่มคุณภาพของภาพ แต่อาจส่งผลต่อประสิทธิภาพการทำงานเล็กน้อย"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_THUMBNAIL_BACKGROUND_ENABLE,
   "พื้นหลังภาพตัวอย่าง"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_THUMBNAIL_BACKGROUND_ENABLE,
   "เปิดใช้งานการเติมพื้นที่ว่างในภาพตัวอย่างด้วยสีพื้นหลัง เพื่อให้ภาพทั้งหมดมีขนาดการแสดงผลที่เท่ากัน ช่วยให้เมนูดูสวยงามและเป็นระเบียบเมื่อมีภาพตัวอย่างที่มีสัดส่วนแตกต่[...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_TICKER_TYPE,
   "ภาพเคลื่อนไหวข้อความวิ่ง"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_TICKER_TYPE,
   "เลือกรูปแบบการเลื่อนข้อความแนวนอน สำหรับข้อความเมนูที่ยาว"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_TICKER_SPEED,
   "ความเร็วข้อความวิ่ง"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_TICKER_SPEED,
   "ความเร็วของภาพเคลื่อนไหว เมื่อเลื่อนข้อความเมนูที่ยาว"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_TICKER_SMOOTH,
   "ข้อความเลื่อนแบบสมูท"
   )

/* Settings > AI Service */

MSG_HASH(
   MENU_ENUM_SUBLABEL_AI_SERVICE_MODE,
   "แสดงคำแปลแบบภาพซ้อนทับบนหน้าจอ (โหมดรูปภาพ), เล่นเป็นเสียงอ่านจากข้อความ (เสียงพูด), หรือใช้โปรแกรมอ่านหน้าจอของระบบอย่างเช่น NVDA (โปรแกรมอ่านหน้าจอ)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_BACKEND,
   "ผู้ให้บริการ AI"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AI_SERVICE_BACKEND,
   "เลือกว่าจะใช้ผู้ให้บริการรายใดในการแปลภาษา โดยแบบ HTTP จะใช้เซิร์ฟเวอร์ภายนอกตาม URL ที่ตั้งค่าไว้ ส่วนแบบ Apple จะใช้ระบบ OCR และการแปลภาษาภายในเครื่อง (macOS/iOS)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_URL,
   "URL ผู้ให้บริการ AI"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AI_SERVICE_URL,
   "ที่อยู่ URL http:// ของผู้ให้บริการแปลภาษาที่จะใช้งาน"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_ENABLE,
   "เปิดใช้งาน ผู้ให้บริการ AI"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AI_SERVICE_ENABLE,
   "เปิดใช้งานบริการ AI ให้ทำงาน เมื่อมีการกด ปุ่มลัด บริการ AI"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_PAUSE,
   "พักการทำงานระหว่างแปลภาษา"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AI_SERVICE_PAUSE,
   "หยุด Core ในขณะที่กำลังแปลหน้าจอ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_SOURCE_LANG,
   "ภาษาต้นทาง"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AI_SERVICE_SOURCE_LANG,
   "ภาษาต้นทางที่ต้องการแปล หากตั้งค่าเป็น 'ค่าพื้นฐาน' ระบบจะพยายามตรวจหาภาษาโดยอัตโนมัติ การระบุภาษาที่ชัดเจนจะช่วยให้แปลได้แม่นยำยิ่งขึ้น"
   )

/* Settings > Accessibility */


/* Settings > Power Management */

/* Settings > Achievements */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_ENABLE,
   "ความสำเร็จ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_HARDCORE_MODE_ENABLE,
   "ปิดการใช้งานสูตรโกง, การย้อนกลับ, สโลว์โมชัน และการโหลดบันทึกสถานะ, ความสำเร็จ ที่ได้รับในโหมดฮาร์ดคอร์จะถูกทำเครื่องหมายไว้เป็นพิเศษ เพื่อให้คุณสามารถแสดงให้ผู้อื่นเห็นถึง[...]"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_UNLOCK_SOUND_ENABLE,
   "ข้อความวิ่งแบบสมูท"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_AUTO_SCREENSHOT,
   "บันทึกภาพหน้าจออัตโนมัติ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_AUTO_SCREENSHOT,
   "บันทึกภาพหน้าจออัตโนมัติ เมื่อปลดล็อกความสำเร็จ"
   )
MSG_HASH( /* suggestion for translators: translate as 'Play Again Mode' */
   MENU_ENUM_LABEL_VALUE_CHEEVOS_START_ACTIVE,
   "โหมดเล่นซ้ำ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_START_ACTIVE,
   "เริ่มเซสชันโดยเปิดใช้งานความสำเร็จทั้งหมด (รวมถึงอันที่เคยปลดล็อกไปแล้วก่อนหน้านี้)"
   )

/* Settings > Achievements > Appearance */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_APPEARANCE_SETTINGS,
   "รูปลักษณ์"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_APPEARANCE_SETTINGS,
   "เปลี่ยนตำแหน่งและระยะขอบของการแจ้งเตือนความสำเร็จบนหน้าจอ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_APPEARANCE_ANCHOR,
   "ตำแหน่ง"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_APPEARANCE_ANCHOR,
   "ตั้งค่ามุมหรือขอบของหน้าจอ ที่จะใช้สำหรับการแสดงการแจ้งเตือนความสำเร็จ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_APPEARANCE_ANCHOR_TOPLEFT,
   "ด้านบนซ้าย"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_APPEARANCE_ANCHOR_TOPCENTER,
   "ด้านบนกลาง"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_APPEARANCE_ANCHOR_TOPRIGHT,
   "ด้านบนขวา"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_APPEARANCE_ANCHOR_BOTTOMLEFT,
   "ด้านล่างซ้าย"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_APPEARANCE_ANCHOR_BOTTOMCENTER,
   "ตรงกลางด้านล่าง"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_APPEARANCE_ANCHOR_BOTTOMRIGHT,
   "ด้านล่างขวา"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_APPEARANCE_PADDING_AUTO,
   "เติมช่องว่างให้ตรงกัน"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_APPEARANCE_PADDING_AUTO,
   "กำหนดว่าต้องการให้การแจ้งเตือนความสำเร็จ จัดวางในตำแหน่งเดียวกับการแจ้งเตือนบนหน้าจอประเภทอื่นๆ หรือไม่ หากปิดใช้งานจะสามารถตั้งค่าระยะขอบและตำแหน่งด้วยตนเองได้"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_APPEARANCE_PADDING_H,
   "ระยะขอบแนวนอน กำหนดเอง"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_APPEARANCE_PADDING_H,
   "ระยะห่างจากขอบจอซ้าย/ขวา ซึ่งสามารถใช้ชดเชยส่วนที่ล้นขอบจอได้"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_APPEARANCE_PADDING_V,
   "ระยะขอบแนวตั้ง กำหนดเอง"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_APPEARANCE_PADDING_V,
   "ระยะห่างจากขอบจอบน/ล่าง ซึ่งสามารถใช้ชดเชยส่วนที่ล้นขอบจอได้"
   )

/* Settings > Achievements > Visibility */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_VISIBILITY_SETTINGS,
   "การแสดงผล"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_VISIBILITY_SETTINGS,
   "เปลี่ยนการแสดงข้อความและองค์ประกอบต่าง ๆ บนหน้าจอ ไม่ส่งผลต่อการปิดการทำงานของระบบ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_VISIBILITY_SUMMARY,
   "รายการสรุปเมื่อเริ่มระบบ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_VISIBILITY_SUMMARY,
   "แสดงข้อมูลเกี่ยวกับเกมที่กำลังโหลดและลำดับความคืบหน้าปัจจุบันของผู้ใช้\n'ทุกเกมที่ระบุได้' จะแสดงข้อมูลสรุปสำหรับเกมที่ไม่มีรายการความสำเร็จ เผยแพร่อยู่"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_VISIBILITY_SUMMARY_ALLGAMES,
   "ทุกเกมที่ระบุได้"
   )

/* Settings > Network */


/* Settings > Network > Updater */


/* Settings > Playlists */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HISTORY_LIST_ENABLE,
   "ประวัติ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_ENTRY_RENAME,
   "อนุญาตให้เปลี่ยนชื่อรายการได้"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_ENTRY_RENAME,
   "อนุญาตให้เปลี่ยนชื่อรายการในเพลย์ลิสต์ได้"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_FUZZY_ARCHIVE_MATCH,
   "เมื่อค้นหาไฟล์บีบอัดในเพลย์ลิสต์ ให้จับคู่เฉพาะ ชื่อไฟล์บีบอัด แทนการใช้ [ชื่อไฟล์บีบอัด]+[ชื่อไฟล์ภายใน] เปิดใช้งานตัวเลือกนี้เพื่อป้องกันไม่ให้เกิดรายการประวัติการใช้งานที[...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCAN_WITHOUT_CORE_MATCH,
   "สแกนโดยไม่จับคู่กับ Core"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SCAN_WITHOUT_CORE_MATCH,
   "อนุญาตให้สแกนเนื้อหาและเพิ่มลงในเพลย์ลิสต์ได้ แม้ว่าจะยังไม่มีการติดตั้ง Core ที่รองรับเนื้อหานั้นก็ตาม"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCAN_SERIAL_AND_CRC,
   "ตรวจสอบ CRC เมื่อพบรายการซ้ำขณะสแกน"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SCAN_SERIAL_AND_CRC,
   "บางครั้งไฟล์ ISO อาจมีรหัส Serial ซ้ำกัน โดยเฉพาะในเกมของ PSP/PSN การอาศัยเพียงรหัส Serial อย่างเดียวอาจทำให้ระบบสแกนจัดหมวดหมู่เกมไปไว้ในระบบที่ผิดพลาด การเปิดใช้งานส่วนนี้จะเพิ่มการตรวจส[...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LIST,
   "จัดการ เพลย์ลิสต์"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_MANAGER_LIST,
   "จัดระเบียบ เพลย์ลิสต์"
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
   "เพลย์ลิสต์"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVESTATE_DIRECTORY,
   "บันทึกสถานะ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVESTATE_DIRECTORY,
   "บันทึกสถานะ และการเล่นย้อนหลัง จะถูกจัดเก็บไว้ในโฟลเดอร์นี้ หากไม่ได้ตั้งค่าไว้ ระบบจะพยายามบันทึกไปยังโฟลเดอร์ที่เนื้อหานั้นตั้งอยู่โดยอัตโนมัติ"
   )

#ifdef HAVE_MIST
/* Settings > Steam */



#endif

/* Music */

/* Music > Quick Menu */


/* Netplay */


/* Netplay > Host */


/* Import Content */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_LIST,
   "สแกนเนื้อหา"
   )

/* Import Content > Scan File */


/* Import Content > Content Scan */

MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_DAT_FILE,
   "เลือกไฟล์ Logiqx หรือ MAME List XML DAT เพื่อเปิดใช้งานการตั้งชื่อเนื้อหาอาร์เขตที่สแกนโดยอัตโนมัติ (MAME, FinalBurn Neo และอื่น ๆ)"
   )

/* Explore tab */

/* Playlist > Playlist Item */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RUN,
   "เล่น"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RUN,
   "เริ่มเกม"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RENAME_ENTRY,
   "เปลี่ยนชื่อ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RENAME_ENTRY,
   "เปลี่ยนชื่อรายการ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DELETE_ENTRY,
   "ลบออก"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_TO_FAVORITES_PLAYLIST,
   "เพิ่มในรายการโปรด"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SET_CORE_ASSOCIATION,
   "กำหนด Core"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RESET_CORE_ASSOCIATION,
   "ล้าง Core ที่เลือกไว้"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INFORMATION,
   "ข้อมูล"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DOWNLOAD_PL_ENTRY_THUMBNAILS,
   "ดาวน์โหลดรูปตัวอย่าง"
   )

/* Playlist Item > Set Core Association */


/* Playlist Item > Information */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_INFO_LABEL,
   "ชื่อ"
   )
MSG_HASH( /* FIXME Unused? */
   MENU_ENUM_LABEL_VALUE_CONTENT_INFO_RUNTIME,
   "เล่นไปแล้ว"
   )

/* Quick Menu */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVE_STATE,
   "บันทึกสถานะ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVE_STATE,
   "บันทึกสถานะไปยังสล็อตที่เลือกไว้ในปัจจุบัน"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_SAVE_STATE,
   "บันทึกสถานะไปยังสล็อตที่เลือกไว้ในปัจจุบัน หมายเหตุ: โดยทั่วไปแล้วไฟล์บันทึกสถานะ จะไม่สามารถนำไปใช้ข้ามระบบได้ และอาจใช้งานไม่ได้กับเวอร์ชันอื่นของ Core นี้"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOAD_STATE,
   "โหลดสถานะจากสล็อตที่เลือกไว้ในปัจจุบัน"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_LOAD_STATE,
   "โหลดสถานะจากสล็อตที่เลือกไว้ในปัจจุบัน หมายเหตุ: อาจใช้งานไม่ได้หากสถานะถูกบันทึกไว้ด้วยเวอร์ชันอื่นของ Core นี้"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UNDO_SAVE_STATE,
   "เลิกบันทึกสถานะ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_UNDO_SAVE_STATE,
   "หากมีการบันทึกทับสถานะเดิม ระบบจะย้อนกลับไปใช้การบันทึกสถานะก่อนหน้า ให้โดยอัตโนมัติ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAY_REPLAY,
   "เล่นไฟล์ เล่นซ้ำ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAY_REPLAY,
   "เล่นไฟล์ เล่นซ้ำ จากช่องที่เลือกไว้ปัจจุบัน"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RECORD_REPLAY,
   "บันทึกไฟล์ เล่นซ้ำ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RECORD_REPLAY,
   "บันทึกไฟล์เล่นซ้ำลงในช่องที่เลือกไว้ปัจจุบัน"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HALT_REPLAY,
   "หยุดการบันทึก/เล่นซ้ำ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_TO_FAVORITES,
   "เพิ่มในรายการโปรด"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVESTATE_LIST,
   "บันทึกสถานะ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVESTATE_LIST,
   "เข้าถึงตัวเลือกการบันทึกสถานะ"
   )
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

MSG_HASH(
   MENU_ENUM_SUBLABEL_ACHIEVEMENT_PAUSE,
   "ระงับโหมดฮาร์ดคอร์สำหรับ ความสำเร็จ ในเซสชันปัจจุบัน การดำเนินการนี้จะเปิดใช้งานสูตรโกง, การย้อนกลับ, สโลว์โมชัน และการโหลดบันทึกสถานะ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACHIEVEMENT_RESUME,
   "กลับเข้าสู่โหมดฮาร์ดคอร์สำหรับ ความสำเร็จ ในเซสชันปัจจุบัน การดำเนินการนี้จะปิดใช้งานสูตรโกง, การย้อนกลับ, สโลว์โมชัน และการโหลดบันทึกสถานะ พร้อมทั้งเริ่มเกมใหม่ทันที"
   )

/* Quick Menu > Information */


/* Miscellaneous UI Items */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_FAVORITES_AVAILABLE,
   "ไม่มีรายการโปรดที่ใช้งานได้"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_IMAGES_AVAILABLE,
   "ไม่มีรูปภาพที่ใช้งานได้"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_CORE,
   "ไม่มี Core"
   )

/* Settings Options */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_RUNTIME_PER_CORE,
   "แยกตาม Core"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_TYPE_RUN_NEXT_IF_GT,
   "ใช้สูตรถัดไปหาก ค่า > หน่วยความจำ"
   )

/* RGUI: Settings > User Interface > Appearance */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_ASPECT_RATIO,
   "อัตราส่วนภาพ"
   )

/* RGUI: Settings Options */


/* XMB: Settings > User Interface > Appearance */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LEFT_THUMBNAILS,
   "รูปตัวอย่างรอง"
   )

/* XMB: Settings Options */


/* Ozone: Settings > User Interface > Appearance */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_TRUNCATE_PLAYLIST_NAME,
   "ตัดชื่อรายการเพลย์ลิสต์ให้สั้นลง (ต้องรีสตาร์ท)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_TRUNCATE_PLAYLIST_NAME,
   "เอาชื่อผู้ผลิตออกจากเพลย์ลิสต์ เช่น จาก 'Sony - PlayStation' จะกลายเป็น 'PlayStation'"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LEFT_THUMBNAILS_OZONE,
   "รูปตัวอย่างรอง"
   )



/* MaterialUI: Settings > User Interface > Appearance */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_THUMBNAIL_VIEW_LANDSCAPE,
   "มุมมองรูปตัวอย่างแนวนอน"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_MENU_THUMBNAIL_VIEW_LANDSCAPE,
   "กำหนดโหมดการแสดงรูปตัวอย่างในเพลย์ลิสต์ เมื่อใช้งานหน้าจอในแนวตั้ง"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_DUAL_THUMBNAIL_LIST_VIEW_ENABLE,
   "แสดงรูปตัวอย่างรอง ในมุมมองรายการ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_DUAL_THUMBNAIL_LIST_VIEW_ENABLE,
   "แสดงรูปตัวอย่างลำดับรอง เมื่อใช้โหมดมุมมองเพลย์ลิสต์แบบ 'รายการ' โดยการตั้งค่านี้จะมีผลก็ต่อเมื่อหน้าจอมีความกว้างเพียงพอที่จะแสดงรูปตัวอย่างสองรูปพร้อมกันเท่านั้น"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_BACKGROUND_ENABLE,
   "พื้นหลังภาพตัวอย่าง"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_THUMBNAIL_BACKGROUND_ENABLE,
   "เพิ่มระยะขอบในพื้นที่ว่างของรูปตัวอย่างด้วยสีพื้นหลัง เพื่อให้รูปภาพทั้งหมดมีขนาดการแสดงผลที่สม่ำเสมอ ช่วยให้เมนูดูสวยงามยิ่งขึ้นเมื่อแสดงรูปตัวอย่างที่มีขนาดแตกต่างกัน[...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_THUMBNAILS_MATERIALUI,
   "ภาพตัวอย่างหลัก"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_THUMBNAILS_MATERIALUI,
   "ประเภทหลักของรูปตัวอย่างสำหรับแต่ละรายการในเพลย์ลิสต์ โดยปกติจะใช้เป็นไอคอนของเนื้อหา"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LEFT_THUMBNAILS_MATERIALUI,
   "รูปตัวอย่างรอง"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LEFT_THUMBNAILS_MATERIALUI,
   "ประเภทรูปตัวอย่างเสริม สำหรับแต่ละรายการในเพลย์ลิสต์ โดยการใช้งานจะขึ้นอยู่กับโหมดมุมมองรูปตัวอย่างที่เลือกใช้ในขณะนั้น"
   )

/* MaterialUI: Settings Options */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_BLUE,
   "น้ำเงิน"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_BLUE_GREY,
   "เทาฟ้า"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_DARK_BLUE,
   "น้ำเงินเข้ม"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_GREEN,
   "เขียว"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_NVIDIA_SHIELD,
   "เขียวมรกต"
   )

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
   "เพลย์ลิสต์"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_TAB_FILE_BROWSER,
   "ตัวจัดการไฟล์"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_INFORMATION,
   "ข้อมูล"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_STOP,
   "หยุด"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_RENAME_PLAYLIST,
   "เปลี่ยนชื่อเพลย์ลิสต์"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_COULD_NOT_RENAME_FILE,
   "ไม่สามารถเปลี่ยนชื่อไฟล์ได้"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_REMOVE,
   "ลบออก"
   )

/* Unsorted */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_SETTINGS,
   "Overlay ซ้อนทับบนหน้าจอ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOAD_CONTENT_HISTORY,
   "ประวัติ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_USER,
   "ผู้ใช้"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WAITABLE_SWAPCHAINS,
   "ซิงค์ CPU และ GPU อย่างเข้มงวด ช่วยลดความหน่วงโดยแลกกับประสิทธิภาพที่ลดลง"
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


MSG_HASH(
   MSG_AUTO_SAVE_STATE_TO,
   "บันทึกสถานะอัตโนมัติไปยัง"
   )
MSG_HASH(
   MSG_CORE_DOES_NOT_SUPPORT_SAVESTATES,
   "Core ไม่รองรับการบันทึกสถานะ"
   )
MSG_HASH(
   MSG_CORE_DOES_NOT_SUPPORT_SAVESTATE_UNDO,
   "Core ไม่รองรับการยกเลิกบันทึกสถานะ"
   )
MSG_HASH(
   MSG_FAILED_TO_SAVE_STATE_TO,
   "ล้มเหลวในการบันทึกสถานะไปยัง"
   )
MSG_HASH(
   MSG_FAILED_TO_UNDO_SAVE_STATE,
   "ล้มเหลวในการเลิกทำ บันทึกสถานะ"
   )
MSG_HASH(
   MSG_FOUND_AUTO_SAVESTATE_IN,
   "พบการบันทึกสถานะอัตโนมัติใน"
   )
MSG_HASH(
   MSG_INPUT_RENAME_ENTRY,
   "เปลี่ยนชื่อหัวข้อ"
   )

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
