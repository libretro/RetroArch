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
   MENU_ENUM_SUBLABEL_VIDEO_SCANLINE_SYNC,
   "ซิงโครไนซ์การแสดงผลวิดีโอเข้ากับตำแหน่งของเส้นสแกน ช่วยลดความหน่วง แต่ต้องแลกมาด้วยความเสี่ยงที่จะเกิดภาพขาดมากขึ้นทจำเป็นต้องปิดการใช้งาน VSync ก่อน"
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
   MENU_ENUM_LABEL_VALUE_MENU_WIDGETS_ENABLE,
   "วิดเจ็ตกราฟิก"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_WIDGETS_ENABLE,
   "ใช้แอนิเมชัน การแจ้งเตือน ตัวบ่งชี้ และตัวควบคุมที่ตกแต่งแล้ว"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_WIDGET_SCALE_AUTO,
   "ปรับขนาดวิดเจ็ตกราฟิกโดยอัตโนมัติ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_WIDGET_SCALE_AUTO,
   "ปรับขนาดการแจ้งเตือน ตัวบ่งชี้ และตัวควบคุมที่ตกแต่งแล้วโดยอัตโนมัติตามสเกลเมนูปัจจุบัน"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_WIDGET_SCALE_FACTOR_FULLSCREEN,
   "การแทนค่าสเกลวิดเจ็ตกราฟิก (เต็มหน้าจอ)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_WIDGET_SCALE_FACTOR_FULLSCREEN,
   "ใช้การแทนค่า x การปรับสเกลแบบกำหนดเองเมื่อวาดวิดเจ็ตแสดงผลในโหมดเต็มหน้าจอ ใช้ได้เฉพาะเมื่อ 'ปรับสเกลวิดเจ็ตกราฟิกโดยอัตโนมัติ' ถูกปิด สามารถใช้เพื่อเพิ่มหรือลดขนาดของการแจ[...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_WIDGET_SCALE_FACTOR_WINDOWED,
   "การแทนค่าสเกลวิดเจ็ตกราฟิก (หน้าต่าง)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_WIDGET_SCALE_FACTOR_WINDOWED,
   "ใช้การแทนค่า x การปรับสเกลแบบกำหนดเองเมื่อวาดวิดเจ็ตแสดงผลในโหมดหน้าต่าง ใช้ได้เฉพาะเมื่อ 'ปรับสเกลวิดเจ็ตกราฟิกโดยอัตโนมัติ' ถูกปิด สามารถใช้เพื่อเพิ่มหรือลดขนาดของการแจ้ง[...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FPS_SHOW,
   "แสดงเฟรมเรต"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FPS_SHOW,
   "แสดงจำนวนเฟรมต่อวินาทีปัจจุบัน"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FPS_UPDATE_INTERVAL,
   "ช่วงเวลาอัปเดตเฟรมเรต (เป็นจำนวนเฟรม)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FPS_UPDATE_INTERVAL,
   "การแสดงเฟรมเรตจะถูกอัปเดตตามช่วงเวลาที่ตั้งไว้เป็นจำนวนเฟรม"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FRAMECOUNT_SHOW,
   "แสดงจำนวนเฟรมที่เรนเดอร์แล้ว"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FRAMECOUNT_SHOW,
   "แสดงจำนวนเฟรมปัจจุบันบนหน้าจอ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STATISTICS_SHOW,
   "การแสดงสถิติ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_STATISTICS_SHOW,
   "แสดงสถิติทางเทคนิคบนหน้าจอ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MEMORY_SHOW,
   "แสดงการใช้หน่วยความจำ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MEMORY_SHOW,
   "แสดงหน่วยความจำที่ใช้และจำนวนทั้งหมดของระบบ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MEMORY_UPDATE_INTERVAL,
   "ช่วงเวลาอัปเดตการใช้หน่วยความจำ (เป็นจำนวนเฟรม)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MEMORY_UPDATE_INTERVAL,
   "การแสดงการใช้หน่วยความจำจะถูกอัปเดตตามช่วงเวลาที่ตั้งไว้เป็นจำนวนเฟรม"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_PING_SHOW,
   "แสดงค่า Ping ของ Netplay"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_PING_SHOW,
   "แสดงค่า Ping ของห้อง Netplay ปัจจุบัน"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_LOAD_CONTENT_ANIMATION,
   "การแจ้งเตือนเริ่มต้น \"โหลดเนื้อหา\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_LOAD_CONTENT_ANIMATION,
   "แสดงแอนิเมชันการตอบสนองสั้น ๆ เมื่อโหลดเนื้อหา"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_AUTOCONFIG,
   "การแจ้งเตือนการเชื่อมต่ออินพุต (Autoconfig)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_AUTOCONFIG_FAILS,
   "การแจ้งเตือนความล้มเหลวของการเชื่อมต่ออินพุต (Autoconfig)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_CHEATS_APPLIED,
   "การแจ้งเตือนรหัสโกง"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_CHEATS_APPLIED,
   "แสดงข้อความบนหน้าจอเมื่อมีการใช้รหัสโกง"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_PATCH_APPLIED,
   "การแจ้งเตือนการแพตช์"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_PATCH_APPLIED,
   "แสดงข้อความบนหน้าจอเมื่อมีการทำ soft‑patching กับ ROMs"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_AUTOCONFIG,
   "แสดงข้อความบนหน้าจอเมื่อเชื่อมต่อหรือยกเลิกการเชื่อมต่ออุปกรณ์อินพุต"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_AUTOCONFIG_FAILS,
   "แสดงข้อความบนหน้าจอเมื่อเชื่อมต่อหรือยกเลิกการเชื่อมต่ออุปกรณ์อินพุต"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_REMAP_LOAD,
   "การแจ้งเตือนเมื่อโหลดการปรับแมปอินพุต"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_REMAP_LOAD,
   "แสดงข้อความบนหน้าจอเมื่อโหลดไฟล์รีแมปอินพุต"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_CONFIG_OVERRIDE_LOAD,
   "การแจ้งเตือนเมื่อโหลดการแทนที่การตั้งค่าคอนฟิก"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_CONFIG_OVERRIDE_LOAD,
   "แสดงข้อความบนหน้าจอเมื่อโหลดไฟล์การเขียนทับการตั้งค่า"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SET_INITIAL_DISK,
   "การแจ้งเตือนเมื่อคืนค่าแผ่นดิสก์เริ่มต้น"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_SET_INITIAL_DISK,
   "แสดงข้อความบนหน้าจอเมื่อคืนค่าแผ่นดิสก์ล่าสุดที่ใช้ของคอนเทนต์หลายแผ่น (multi‑disc) ที่โหลดผ่านเพลย์ลิสต์ M3U โดยอัตโนมัติเมื่อเริ่มต้นๆ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_DISK_CONTROL,
   "การแจ้งเตือนการควบคุมแผ่นดิสก์"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_DISK_CONTROL,
   "แสดงข้อความบนหน้าจอเมื่อใส่หรือเอาแผ่นดิสก์ออก"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SAVE_STATE,
   "การแจ้งเตือนบันทึกสถานะ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_SAVE_STATE,
   "แสดงข้อความบนหน้าจอเมื่อทำการบันทึกและโหลดบันทึกสถานะ"
   )
MSG_HASH( /* FIXME: Rename config key and msg hash */
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_FAST_FORWARD,
   "การแจ้งเตือนการจำกัดเฟรม"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_FAST_FORWARD,
   "แสดงตัวบ่งชี้บนหน้าจอเมื่อเปิดใช้งานการเดินหน้าเร็ว การเล่นช้า หรือการย้อนกลับ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SCREENSHOT,
   "การแจ้งเตือนจับภาพหน้าจอ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_SCREENSHOT,
   "แสดงข้อความบนหน้าจอเมื่อถ่ายภาพหน้าจอ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SCREENSHOT_DURATION,
   "การแจ้งเตือนเมื่อบันทึกภาพหน้าจอ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_SCREENSHOT_DURATION,
   "กำหนดระยะเวลาของข้อความแจ้งเตือนบันทึกภาพหน้าจอบนหน้าจอ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SCREENSHOT_DURATION_NORMAL,
   "ปกติ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SCREENSHOT_DURATION_FAST,
   "เร็ว"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SCREENSHOT_DURATION_VERY_FAST,
   "เร็วมาก"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SCREENSHOT_DURATION_INSTANT,
   "ทันที"
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
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_REFRESH_RATE,
   "แสดงข้อความบนหน้าจอเมื่อกำหนดอัตราการรีเฟรช"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_NETPLAY_EXTRA,
   "การแจ้งเตือน Netplay เพิ่มเติม"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_NETPLAY_EXTRA,
   "แสดงข้อความบนหน้าจอของ Netplay ที่ไม่จำเป็น"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_WHEN_MENU_IS_ALIVE,
   "การแจ้งเตือนเฉพาะเมนู"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_WHEN_MENU_IS_ALIVE,
   "แสดงการแจ้งเตือนเฉพาะเมื่อเมนูเปิดอยู่"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FONT_PATH,
   "แบบอักษรการแจ้งเตือน"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FONT_PATH,
   "เลือกแบบอักษรสำหรับการแจ้งเตือนบนหน้าจอ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FONT_SIZE,
   "ขนาดการแจ้งเตือน"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FONT_SIZE,
   "ระบุขนาดแบบอักษรเป็นพอยต์ เมื่อใช้วิดเจ็ต ขนาดนี้จะมีผลเฉพาะกับการแสดงสถิติบนหน้าจอ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_POS_X,
   "ตำแหน่งการแจ้งเตือน (แนวนอน)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_POS_X,
   "ระบุตำแหน่งแกน X แบบกำหนดเองสำหรับข้อความบนหน้าจอ โดยค่า 0 คือขอบด้านซ้าย"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_POS_Y,
   "ตำแหน่งการแจ้งเตือน (แนวตั้ง)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_POS_Y,
   "ระบุตำแหน่งแกน Y แบบกำหนดเองสำหรับข้อความบนหน้าจอ โดยค่า 0 คือขอบด้านล่าง"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_COLOR_RED,
   "สีการแจ้งเตือน (แดง)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_COLOR_RED,
   "ตั้งค่าค่าแดงของสีข้อความ OSD ค่าใช้ได้อยู่ระหว่าง 0 ถึง 255"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_COLOR_GREEN,
   "สีการแจ้งเตือน (เขียว)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_COLOR_GREEN,
   "ตั้งค่าค่าเขียวของสีข้อความ OSD ค่าใช้ได้อยู่ระหว่าง 0 ถึง 255"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_COLOR_BLUE,
   "สีการแจ้งเตือน (น้ำเงิน)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_COLOR_BLUE,
   "ตั้งค่าค่าน้ำเงินของสีข้อความ OSD ค่าใช้ได้อยู่ระหว่าง 0 ถึง 255"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_BGCOLOR_ENABLE,
   "พื้นหลังการแจ้งเตือน"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_BGCOLOR_ENABLE,
   "เปิดใช้งานสีพื้นหลังสำหรับ OSD"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_BGCOLOR_RED,
   "สีพื้นหลังการแจ้งเตือน (แดง)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_BGCOLOR_RED,
   "ตั้งค่าค่าแดงของสีพื้นหลัง OSD ค่าใช้ได้อยู่ระหว่าง 0 ถึง 255"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_BGCOLOR_GREEN,
   "สีพื้นหลังการแจ้งเตือน (เขียว)"
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
   MENU_ENUM_SUBLABEL_PAUSE_NONACTIVE,
   "หยุดเนื้อหาเมื่อ RetroArch ไม่ใช่หน้าต่างที่กำลังใช้งาน"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUIT_ON_CLOSE_CONTENT,
   "ออกเมื่อปิดเนื้อหา"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUIT_ON_CLOSE_CONTENT,
   "ออกจากโปรแกรมโดยอัตโนมัติเมื่อปิดเนื้อหา ‘CLI’ จะออกก็ต่อเมื่อมีการเปิดเนื้อหาผ่านบรรทัดคำสั่งเท่านั้น"
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
   MENU_ENUM_LABEL_VALUE_MENU_INSERT_DISK_RESUME,
   "กลับมาเล่นเนื้อหาต่อหลังจากเปลี่ยนแผ่นดิสก์"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_INSERT_DISK_RESUME,
   "ปิดเมนูโดยอัตโนมัติและกลับมาเล่นเนื้อหาต่อหลังจากใส่หรือโหลดแผ่นดิสก์ใหม่"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NAVIGATION_WRAPAROUND,
   "การนำทางแบบวนรอบ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NAVIGATION_WRAPAROUND,
   "การนำทางแบบวนรอบจะทำให้รายการเลื่อนไปยังจุดเริ่มต้นหรือจุดสิ้นสุดโดยอัตโนมัติ เมื่อถึงขอบเขตของรายการทั้งในแนวนอนหรือแนวตั้ง"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHOW_ADVANCED_SETTINGS,
   "แสดงการตั้งค่าขั้นสูง"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SHOW_ADVANCED_SETTINGS,
   "แสดงการตั้งค่าขั้นสูงสำหรับผู้ใช้ระดับเชี่ยวชาญ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_ENABLE_KIOSK_MODE,
   "โหมดล็อกการแก้ไข"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_ENABLE_KIOSK_MODE,
   "ปกป้องการตั้งค่าโดยการซ่อนการตั้งค่าทั้งหมดที่เกี่ยวข้องกับการปรับแต่ง"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_KIOSK_MODE_PASSWORD,
   "ตั้งรหัสผ่านสำหรับการปิดโหมดล็อกการแก้ไข"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_KIOSK_MODE_PASSWORD,
   "เมื่อมีการตั้งรหัสผ่านขณะเปิดใช้งานโหมดล็อกการแก้ไข จะสามารถปิดโหมดนี้ได้ภายหลังจากเมนู โดยไปที่เมนูหลัก เลือก “ปิดโหมดล็อกการแก้ไข” และกรอกรหัสผ่าน"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MOUSE_ENABLE,
   "รองรับการใช้งานเมาส์"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MOUSE_ENABLE,
   "อนุญาตให้ควบคุมเมนูด้วยเมาส์"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_POINTER_ENABLE,
   "รองรับการใช้งานระบบสัมผัส"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_POINTER_ENABLE,
   "อนุญาตให้ควบคุมเมนูด้วยหน้าจอสัมผัส"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_THREADED_DATA_RUNLOOP_ENABLE,
   "งานแบบแยกเธรด"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_THREADED_DATA_RUNLOOP_ENABLE,
   "ทำงานบนเธรดแยกต่างหาก"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SCREENSAVER_TIMEOUT,
   "เวลาภาพพักหน้าจอเมนู"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SCREENSAVER_TIMEOUT,
   "เมื่อเมนูทำงานอยู่ ภาพพักหน้าจอจะปรากฏขึ้นหลังจากไม่มีการใช้งานตามเวลาที่กำหนด"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SCREENSAVER_ANIMATION,
   "แอนิเมชันภาพพักหน้าจอเมนู"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SCREENSAVER_ANIMATION,
   "เปิดใช้งานเอฟเฟกต์แอนิเมชันขณะเมนูภาพพักหน้าจอทำงาน มีผลกระทบต่อประสิทธิภาพเล็กน้อย"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SCREENSAVER_ANIMATION_SNOW,
   "หิมะ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SCREENSAVER_ANIMATION_STARFIELD,
   "ทุ่งดวงดาว"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SCREENSAVER_ANIMATION_VORTEX,
   "วังวน"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SCREENSAVER_ANIMATION_SPEED,
   "ความเร็วแอนิเมชันภาพพักหน้าจอเมนู"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SCREENSAVER_ANIMATION_SPEED,
   "ปรับความเร็วของเอฟเฟกต์แอนิเมชันหน้าจอพักเมนู"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DISABLE_COMPOSITION,
   "ปิดการทำงานของ Desktop Composition"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_DISABLE_COMPOSITION,
   "ตัวจัดการหน้าต่างใช้การทำงานแบบ Composition เพื่อเพิ่มเอฟเฟกต์ภาพ ตรวจจับหน้าต่างที่ไม่ตอบสนอง และอื่น ๆ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DISABLE_COMPOSITION,
   "บังคับปิดการทำงานของ Composition การปิดใช้งานนี้ใช้ได้เฉพาะบน Windows Vista/7 ในตอนนี้"
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
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_LOAD_CONTENT,
   "แสดง 'โหลดเนื้อหา'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_LOAD_CONTENT,
   "แสดงตัวเลือก 'โหลดเนื้อหา' ในเมนูหลัก"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_LOAD_DISC,
   "แสดง 'โหลดแผ่นดิสก์'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_LOAD_DISC,
   "แสดงตัวเลือก 'โหลดแผ่นดิสก์' ในเมนูหลัก"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_DUMP_DISC,
   "แสดง 'ดัมพ์แผ่นดิสก์'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_DUMP_DISC,
   "แสดงตัวเลือก 'ดัมพ์แผ่นดิสก์' ในเมนูหลัก"
   )
#ifdef HAVE_LAKKA
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_EJECT_DISC,
   "แสดง 'นำแผ่นดิสก์ออก'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_EJECT_DISC,
   "แสดงตัวเลือก 'นำแผ่นดิสก์ออก' ในเมนูหลัก"
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_ONLINE_UPDATER,
   "แสดง 'ตัวอัปเดตออนไลน์'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_ONLINE_UPDATER,
   "แสดงตัวเลือก 'ตัวอัปเดตออนไลน์' ในเมนูหลัก"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_CORE_UPDATER,
   "แสดง 'ตัวดาวน์โหลด Core'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_CORE_UPDATER,
   "แสดง 'การอัปเดต Core (และไฟล์ข้อมูล Core)' ในตัวเลือก 'ตัวอัปเดตออนไลน์'"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_INFORMATION,
   "แสดง 'ข้อมูล'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_INFORMATION,
   "แสดงตัวเลือก 'ข้อมูล' ในเมนูหลัก"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_CONFIGURATIONS,
   "แสดง 'ไฟล์การตั้งค่า'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_CONFIGURATIONS,
   "แสดงตัวเลือก 'ไฟล์การตั้งค่า' ในเมนูหลัก"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_HELP,
   "แสดง 'วิธีใช้'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_HELP,
   "แสดงตัวเลือก 'วิธีใช้' ในเมนูหลัก"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_QUIT_RETROARCH,
   "แสดง 'ออกจาก RetroArch'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_QUIT_RETROARCH,
   "แสดงตัวเลือก 'ออกจาก RetroArch' ในเมนูหลัก"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_RESTART_RETROARCH,
   "แสดง 'เริ่มใหม่ RetroArch'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_RESTART_RETROARCH,
   "แสดงตัวเลือก 'เริ่มใหม่ RetroArch' ในเมนูหลัก"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_SETTINGS,
   "แสดง 'การตั้งค่า'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_SETTINGS,
   "แสดงเมนู 'การตั้งค่า'"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_SETTINGS_PASSWORD,
   "ตั้งรหัสผ่านสำหรับการเปิดใช้งาน 'การตั้งค่า'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_SETTINGS_PASSWORD,
   "เมื่อซ่อนแท็บการตั้งค่าโดยกำหนดรหัสผ่าน จะสามารถกู้คืนได้ภายหลังจากเมนู โดยไปที่แท็บเมนูหลัก เลือก 'เปิดใช้งานแท็บการตั้งค่า' แล้วใส่รหัสผ่าน"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_FAVORITES,
   "แสดง 'รายการโปรด'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_FAVORITES,
   "แสดงเมนู 'รายการโปรด'"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_FAVORITES_FIRST,
   "แสดง 'รายการโปรด' ก่อน"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_FAVORITES_FIRST,
   "แสดง 'รายการโปรด' ก่อน 'ประวัติ'"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_IMAGES,
   "แสดง 'รูปภาพ'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_IMAGES,
   "แสดงเมนู 'รูปภาพ'"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_MUSIC,
   "แสดง 'เพลง'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_MUSIC,
   "แสดงเมนู 'เพลง'"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_VIDEO,
   "แสดง 'วิดีโอ'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_VIDEO,
   "แสดงเมนู 'วิดีโอ'"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_NETPLAY,
   "แสดง 'ออนไลน์ Netplay'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_NETPLAY,
   "แสดงเมนู 'เล่นออนไลน์ Netplay'"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_HISTORY,
   "แสดง 'ประวัติ'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_HISTORY,
   "แสดงเมนูประวัติล่าสุด"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_ADD_ENTRY,
   "แสดง 'นำเข้าเนื้อหา'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_ADD_ENTRY,
   "แสดงรายการ 'นำเข้าเนื้อหา' ในเมนูหลักหรือเพลย์ลิสต์"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_ADD_CONTENT_ENTRY_DISPLAY_MAIN_TAB,
   "เมนูหลัก"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_ADD_CONTENT_ENTRY_DISPLAY_PLAYLISTS_TAB,
   "เมนูเพลย์ลิสต์"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_PLAYLISTS,
   "แสดง 'เพลย์ลิสต์'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_PLAYLISTS,
   "แสดงเพลย์ลิสต์ในเมนูหลัก ไม่ถูกนำมาใช้ใน GLUI หากเปิดใช้งานแท็บเพลย์ลิสต์และแถบนำทางแล้ว"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_PLAYLIST_TABS,
   "แสดงแท็บเพลย์ลิสต์"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_PLAYLIST_TABS,
   "แสดงแท็บเพลย์ลิสต์ ไม่ส่งผลต่อ RGUI ต้องเปิดใช้งานแถบนำทางใน GLUI"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_EXPLORE,
   "แสดง 'เรียกดู'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_EXPLORE,
   "แสดงตัวเลือกการเรียกดูเนื้อหา"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_CONTENTLESS_CORES,
   "แสดง 'Core ที่ไม่มีคอนเทนต์'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_CONTENTLESS_CORES,
   "ระบุประเภทของ Core (ถ้ามี) ที่จะแสดงในเมนู 'Core ที่ไม่มีคอนเทนต์' เมื่อตั้งเป็น 'กำหนดเอง' จะสามารถเลือก เปิด-ปิด การมองเห็นของแต่ละ Core ได้ผ่านเมนู 'จัดการ Core'"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHOW_CONTENTLESS_CORES_ALL,
   "ทั้งหมด"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHOW_CONTENTLESS_CORES_SINGLE_PURPOSE,
   "ใช้ครั้งเดียว"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHOW_CONTENTLESS_CORES_CUSTOM,
   "กำหนดเอง"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_ENABLE,
   "แสดงวันที่และเวลา"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_TIMEDATE_ENABLE,
   "แสดงวันที่และ/หรือเวลาปัจจุบันภายในเมนู"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_STYLE,
   "รูปแบบของวันที่และเวลา"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_TIMEDATE_STYLE,
   "เปลี่ยนรูปแบบการแสดงวันที่และ/หรือเวลาปัจจุบันภายในเมนู"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_DATE_SEPARATOR,
   "ตัวคั่นวันที่"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_TIMEDATE_DATE_SEPARATOR,
   "ระบุตัวอักษรที่จะใช้เป็นตัวคั่นระหว่าง ปี/เดือน/วัน เมื่อมีการแสดงวันที่ปัจจุบันภายในเมนู"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BATTERY_LEVEL_ENABLE,
   "แสดงระดับแบตเตอรี่"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BATTERY_LEVEL_ENABLE,
   "แสดงระดับแบตเตอรี่ปัจจุบันภายในเมนู"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_ENABLE,
   "แสดงชื่อ Core"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_ENABLE,
   "แสดงชื่อ Core ปัจจุบันภายในเมนู"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_SUBLABELS,
   "แสดงคำอธิบายใต้เมนู"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_SUBLABELS,
   "แสดงข้อมูลเพิ่มเติมสำหรับรายการเมนูต่างๆ"
   )
MSG_HASH( /* FIXME Not RGUI specific */
   MENU_ENUM_LABEL_VALUE_RGUI_SHOW_START_SCREEN,
   "แสดงหน้าจอเริ่มต้น"
   )
MSG_HASH( /* FIXME Not RGUI specific */
   MENU_ENUM_SUBLABEL_RGUI_SHOW_START_SCREEN,
   "แสดงหน้าจอเริ่มต้นในเมนู โดยจะถูกตั้งค่าเป็น \"ปิด\" โดยอัตโนมัติหลังจากเริ่มใช้งานโปรแกรมครั้งแรก"
   )

/* Settings > User Interface > Menu Item Visibility > Quick Menu */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_RESUME_CONTENT,
   "แสดง 'เล่นต่อ'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_RESUME_CONTENT,
   "แสดงตัวเลือก เล่นต่อ ในเมนู"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_RESTART_CONTENT,
   "แสดง 'รีเซ็ต'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_RESTART_CONTENT,
   "แสดง ตัวเลือกรีเซ็ตเนื้อหา"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_CLOSE_CONTENT,
   "แสดง 'ปิดเนื้อหา'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_CLOSE_CONTENT,
   "แสดง 'ตัวเลือกปิดเนื้อหา'"
   )
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
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_REPLAY,
   "แสดง 'การควบคุมการเล่นซ้ำ'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_REPLAY,
   "แสดงตัวเลือกสำหรับการบันทึกหรือการเล่นไฟล์เล่นซ้ำย้อนหลัง"
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
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_OPTIONS,
   "แสดง 'ตัวเลือก Core'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_OPTIONS,
   "แสดงตัวเลือก 'ตัวเลือก Core'"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_CORE_OPTIONS_FLUSH,
   "แสดง 'ตัวเลือกการ Flush ลง Disk'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_CORE_OPTIONS_FLUSH,
   "แสดง 'ตัวเลือกการ Flush ลง Disk' ในเมนู 'ตัวเลือก > จัดการตัวเลือก Core'"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_CONTROLS,
   "แสดง 'การควบคุม'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_CONTROLS,
   "แสดงตัวเลือก 'การควบคุม'"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_TAKE_SCREENSHOT,
   "แสดง 'จับภาพหน้าจอ'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_TAKE_SCREENSHOT,
   "แสดงตัวเลือก 'จับภาพหน้าจอ'"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_START_RECORDING,
   "แสดง 'เริ่มการบันทึก'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_START_RECORDING,
   "แสดงตัวเลือก 'เริ่มการบันทึก'"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_START_STREAMING,
   "แสดง 'เริ่มการสตรีม'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_START_STREAMING,
   "แสดงตัวเลือก 'เริ่มการสตรีม'"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_OVERLAYS,
   "แสดง 'โอเวอร์เลย์บนหน้าจอ'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_OVERLAYS,
   "แสดงตัวเลือก 'โอเวอร์เลย์บนหน้าจอ'"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_VIDEO_LAYOUT,
   "แสดง 'เลย์เอาต์วิดีโอ'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_VIDEO_LAYOUT,
   "แสดงตัวเลือก 'เลย์เอาต์วิดีโอ'"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_LATENCY,
   "แสดง 'ความหน่วง'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_LATENCY,
   "แสดงตัวเลือก 'ความหน่วง'"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_REWIND,
   "แสดง 'ย้อนกลับ'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_REWIND,
   "แสดงตัวเลือก 'ย้อนกลับ'"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_SAVE_CORE_OVERRIDES,
   "แสดง 'บันทึกการเขียนทับ Core'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_SAVE_CORE_OVERRIDES,
   "แสดงตัวเลือก 'บันทึกการเขียนทับ Core' ในเมนู 'การเขียนทับ'"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_SAVE_CONTENT_DIR_OVERRIDES,
   "แสดง 'บันทึกการเขียนทับโฟลเดอร์เนื้อหา'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_SAVE_CONTENT_DIR_OVERRIDES,
   "แสดงตัวเลือก 'บันทึกการเขียนทับโฟลเดอร์เนื้อหา' ในเมนู 'การเขียนทับ'"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_SAVE_GAME_OVERRIDES,
   "แสดง 'บันทึกการเขียนทับเกม'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_SAVE_GAME_OVERRIDES,
   "แสดงตัวเลือก 'บันทึกการเขียนทับเกม' ในเมนู 'การเขียนทับ'"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_CHEATS,
   "แสดง 'สูตรโกง'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_CHEATS,
   "แสดงตัวเลือก 'สูตรโกง'"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_SHADERS,
   "แสดง 'เชดเดอร์'"
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
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_DRIVERS,
   "แสดงการตั้งค่า 'ไดรเวอร์'"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_VIDEO,
   "แสดง 'วิดีโอ'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_VIDEO,
   "แสดงการตั้งค่า 'วิดีโอ'"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_AUDIO,
   "แสดง 'เสียง'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_AUDIO,
   "แสดงการตั้งค่า 'เสียง'"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_INPUT,
   "แสดง 'อินพุต'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_INPUT,
   "แสดงการตั้งค่า 'อินพุต'"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_LATENCY,
   "แสดง 'ความหน่วง'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_LATENCY,
   "แสดงการตั้งค่า 'ความหน่วง'"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_CORE,
   "แสดง 'Core'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_CORE,
   "แสดงการตั้งค่า 'Core'"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_CONFIGURATION,
   "แสดง 'การตั้งค่า'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_CONFIGURATION,
   "แสดงตั้งค่า 'การตั้งค่า'"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_SAVING,
   "แสดง 'การบันทึก'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_SAVING,
   "แสดงการตั้งค่า 'การบันทึก'"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_LOGGING,
   "แสดง 'บันทึก Log'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_LOGGING,
   "ตั้งค่า การบันทึก 'Log'"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_FILE_BROWSER,
   "ตัวเรียกดูไฟล์"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_FILE_BROWSER,
   "แสดงการตั้งค่า 'ตัวเรียกดูไฟล์'"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_FRAME_THROTTLE,
   "แสดง 'การควบคุมเฟรม'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_FRAME_THROTTLE,
   "แสดงการตั้งค่า 'การควบคุมเฟรม'"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_RECORDING,
   "แสดง 'การบันทึกวิดีโอ'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_RECORDING,
   "แสดงการตั้งค่า 'การบันทึกวิดีโอ'"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_ONSCREEN_DISPLAY,
   "แสดง 'การแสดงผลบนหน้าจอ'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_ONSCREEN_DISPLAY,
   "แสดงการตั้งค่า 'การแสดงผลบนหน้าจอ'"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_USER_INTERFACE,
   "แสดง 'ส่วนติดต่อผู้ใช้' UI'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_USER_INTERFACE,
   "แสดงการตั้งค่า 'User Interface'"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_AI_SERVICE,
   "แสดง 'บริการ AI'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_AI_SERVICE,
   "แสดงการตั้งค่า 'บริการ AI'"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_ACCESSIBILITY,
   "แสดง 'การช่วยเหลือพิเศษ'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_ACCESSIBILITY,
   "แสดงการตั้งค่า 'การช่วยเหลือพิเศษ'"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_POWER_MANAGEMENT,
   "แสดง 'การจัดการพลังงาน'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_POWER_MANAGEMENT,
   "แสดงการตั้งค่า 'การจัดการพลังงาน'"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_ACHIEVEMENTS,
   "แสดง 'ความสำเร็จ'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_ACHIEVEMENTS,
   "แสดงการตั้งค่า 'ความสำเร็จ'"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_NETWORK,
   "แสดง 'เครือข่าย'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_NETWORK,
   "แสดงการตั้งค่า 'เครือข่าย'"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_PLAYLISTS,
   "แสดง 'เพลย์ลิสต์'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_PLAYLISTS,
   "แสดงการตั้งค่า 'เพลย์ลิสต์'"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_USER,
   "แสดง 'ผู้ใช้'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_USER,
   "แสดงการตั้งค่า 'ผู้ใช้'"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_DIRECTORY,
   "แสดง 'ไดเรกทอรี'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_DIRECTORY,
   "แสดงการตั้งค่า 'ไดเรกทอรี'"
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_STEAM,
   "แสดง 'Steam'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_STEAM,
   "แสดงการตั้งค่า 'Steam'"
   )

/* Settings > User Interface > Appearance */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SCALE_FACTOR,
   "อัปสเกล"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SCALE_FACTOR,
   "ปรับสเกลขนาดขององค์ประกอบอินเทอร์เฟซผู้ใช้ในเมนู"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_WALLPAPER,
   "ภาพพื้นหลัง"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_WALLPAPER,
   "เลือกรูปภาพเพื่อตั้งค่าเป็นพื้นหลังของเมนู รูปภาพที่เลือกเองหรือรูปภาพแบบไดนามิกจะแทนที่ 'ธีมสี' ในเมนู"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_WALLPAPER_OPACITY,
   "ความโปร่งใสของภาพพื้นหลัง"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_WALLPAPER_OPACITY,
   "ปรับความโปร่งใสของภาพพื้นหลัง"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_FRAMEBUFFER_OPACITY,
   "ความโปร่งใส"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_FRAMEBUFFER_OPACITY,
   "ปรับความโปร่งใสของพื้นหลังเมนูเริ่มต้น"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_USE_PREFERRED_SYSTEM_COLOR_THEME,
   "ใช้ธีมสีระบบที่ตั้งค่าไว้เป็นค่าเริ่มต้น"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_USE_PREFERRED_SYSTEM_COLOR_THEME,
   "ใช้ธีมสีของระบบปฏิบัติการ (ถ้ามี) เขียนทับการตั้งค่าธีม"
   )
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
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_TICKER_SMOOTH,
   "ใช้แอนิเมชันเลื่อนแบบนุ่มนวลเมื่อแสดงข้อความเมนูยาว มีผลกระทบต่อประสิทธิภาพเล็กน้อย"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_REMEMBER_SELECTION,
   "จำการเลือกไว้เมื่อเปลี่ยนแท็บ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_REMEMBER_SELECTION,
   "จำตำแหน่งเคอร์เซอร์ก่อนหน้าในแท็บ RGUI ไม่มีแท็บ แต่เพลย์ลิสต์และการตั้งค่าทำงานเหมือนแท็บ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_REMEMBER_SELECTION_ALWAYS,
   "เสมอ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_REMEMBER_SELECTION_PLAYLISTS,
   "เฉพาะสำหรับเพลย์ลิสต์"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_REMEMBER_SELECTION_MAIN,
   "เฉพาะสำหรับเมนูหลักและการตั้งค่า"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_STARTUP_PAGE,
   "หน้าเริ่มต้น"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_STARTUP_PAGE,
   "หน้าเมนูเริ่มต้นเมื่อเปิดโปรแกรม"
   )

/* Settings > AI Service */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_MODE,
   "เอาต์พุตของบริการ AI"
   )
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
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_TARGET_LANG,
   "ภาษาเป้าหมาย"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AI_SERVICE_TARGET_LANG,
   "ภาษาที่บริการจะใช้แปล 'ค่าเริ่มต้น' คือภาษาอังกฤษ ในเมนู"
   )

/* Settings > Accessibility */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACCESSIBILITY_ENABLED,
   "เปิดใช้งานการช่วยการเข้าถึง"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACCESSIBILITY_ENABLED,
   "เปิดใช้งานฟังก์ชันอ่านออกเสียง (Text-to-Speech) เพื่อช่วยในการนำทางเมนู"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACCESSIBILITY_NARRATOR_SPEECH_SPEED,
   "ความเร็วในการอ่านออกเสียง"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACCESSIBILITY_NARRATOR_SPEECH_SPEED,
   "ความเร็วในการอ่านออกเสียง"
   )

/* Settings > Power Management */

/* Settings > Achievements */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_ENABLE,
   "ความสำเร็จ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_ENABLE,
   "สะสมความสำเร็จ (Achievements) ในเกมคลาสสิก ดูข้อมูลเพิ่มเติมได้ที่ '[https://retroachievements.org]'"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_HARDCORE_MODE_ENABLE,
   "โหมดฮาร์ดคอร์"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_HARDCORE_MODE_ENABLE,
   "ปิดการใช้งานสูตรโกง, การย้อนกลับ, สโลว์โมชัน และการโหลดบันทึกสถานะ, ความสำเร็จ ที่ได้รับในโหมดฮาร์ดคอร์จะถูกทำเครื่องหมายไว้เป็นพิเศษ เพื่อให้คุณสามารถแสดงให้ผู้อื่นเห็นถึง[...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_LEADERBOARDS_ENABLE,
   "ตารางคะแนนผู้นำ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_RICHPRESENCE_ENABLE,
   "สถานะออนไลน์แบบละเอียด"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_RICHPRESENCE_ENABLE,
   "ส่งข้อมูลบริบทของเกมไปยังเว็บไซต์ RetroAchievements เป็นระยะ จะไม่มีผลหากเปิดใช้งาน 'โหมดฮาร์ดคอร์'"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_BADGES_ENABLE,
   "เหรียญตราความสำเร็จ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_BADGES_ENABLE,
   "แสดงเหรียญตราในรายการความสำเร็จ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_TEST_UNOFFICIAL,
   "ทดสอบความสำเร็จที่ไม่เป็นทางการ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_TEST_UNOFFICIAL,
   "ใช้งานความสำเร็จที่ไม่เป็นทางการ และ/หรือ ฟีเจอร์เวอร์ชันเบต้า เพื่อจุดประสงค์ในการทดสอบ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_UNLOCK_SOUND_ENABLE,
   "ปลดล็อคเสียง"
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
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_VISIBILITY_SUMMARY_HASCHEEVOS,
   "เกมที่มีระบบความสำเร็จ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_VISIBILITY_UNLOCK,
   "การแจ้งเตือนการปลดล็อก"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_VISIBILITY_MASTERY,
   "การแจ้งเตือน Mastery"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_VISIBILITY_MASTERY,
   "แสดงการแจ้งเตือนเมื่อมีการปลดล็อกความสำเร็จทั้งหมดของเกม"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_CHALLENGE_INDICATORS,
   "ตัวบ่งชี้ความท้าทายที่กำลังทำงาน"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_CHALLENGE_INDICATORS,
   "แสดงตัวบ่งชี้บนหน้าจอในขณะที่สามารถปลดล็อกความสำเร็จบางอย่างได้"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_VISIBILITY_PROGRESS_TRACKER,
   "ตัวบ่งชี้ความคืบหน้า"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_VISIBILITY_PROGRESS_TRACKER,
   "แสดงตัวบ่งชี้บนหน้าจอเมื่อมีความคืบหน้าในการทำความสำเร็จบางอย่าง"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_VISIBILITY_LBOARD_START,
   "ข้อความเริ่มตารางคะแนนผู้นำ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_VISIBILITY_LBOARD_START,
   "แสดงรายละเอียดของตารางคะแนนผู้นำเมื่อเริ่มทำงาน"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_VISIBILITY_LBOARD_SUBMIT,
   "ข้อความส่งคะแนนตารางคะแนนผู้นำ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_VISIBILITY_LBOARD_SUBMIT,
   "แสดงข้อความพร้อมคะแนนที่ส่งไปเมื่อการพยายามทำคะแนนในตารางผู้นำสิ้นสุดลง"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_VISIBILITY_LBOARD_CANCEL,
   "ข้อความล้มเหลวของตารางคะแนนผู้นำ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_VISIBILITY_LBOARD_CANCEL,
   "แสดงข้อความเมื่อการทำคะแนนในตารางผู้นำล้มเหลว"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_VISIBILITY_LBOARD_TRACKERS,
   "ตัวติดตามตารางคะแนนผู้นำ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_VISIBILITY_LBOARD_TRACKERS,
   "แสดงตัวติดตามบนหน้าจอพร้อมค่าคะแนนปัจจุบันของตารางคะแนนผู้นำที่กำลังทำงานอยู่"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_VISIBILITY_ACCOUNT,
   "ข้อความการเข้าสู่ระบบ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_VISIBILITY_ACCOUNT,
   "แสดงข้อความที่เกี่ยวข้องกับการเข้าสู่ระบบบัญชี RetroAchievements"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_VERBOSE_ENABLE,
   "ข้อความรายละเอียดครบถ้วน"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_VERBOSE_ENABLE,
   "แสดงข้อความการวินิจฉัยและข้อผิดพลาดเพิ่มเติม"
   )

/* Settings > Network */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_PUBLIC_ANNOUNCE,
   "ประกาศ Netplay ต่อสาธารณะ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_PUBLIC_ANNOUNCE,
   "กำหนดว่าจะประกาศห้องเล่น Netplay ต่อสาธารณะหรือไม่ หากไม่ได้ตั้งค่าไว้ ผู้เล่นคนอื่นจะต้องเชื่อมต่อด้วยตนเองแทนการใช้ล็อบบี้สาธารณะ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_USE_MITM_SERVER,
   "ใช้งานเซิร์ฟเวอร์รีเลย์"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_USE_MITM_SERVER,
   "ส่งต่อการเชื่อมต่อ Netplay ผ่านเซิร์ฟเวอร์ตัวกลาง มีประโยชน์ในกรณีที่โฮสต์ (ผู้สร้างห้อง) อยู่หลังไฟร์วอลล์ หรือมีปัญหาเกี่ยวกับ NAT/UPnP"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_MITM_SERVER,
   "ตำแหน่งของเซิร์ฟเวอร์รีเลย์"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_MITM_SERVER,
   "เลือกเซิร์ฟเวอร์รีเลย์ที่ต้องการใช้งาน โดยปกติแล้วตำแหน่งที่อยู่ใกล้ทางภูมิศาสตร์มากกว่ามักจะมีค่าความหน่วง (Latency) ที่ต่ำกว่า"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_CUSTOM_MITM_SERVER,
   "ที่อยู่เซิร์ฟเวอร์รีเลย์แบบกำหนดเอง"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_CUSTOM_MITM_SERVER,
   "กรอกที่อยู่ของเซิร์ฟเวอร์รีเลย์แบบกำหนดเองของคุณที่นี่ รูปแบบ: address หรือ address|port"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_MITM_SERVER_LOCATION_1,
   "อเมริกาเหนือ (ชายฝั่งตะวันออก, สหรัฐอเมริกา)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_MITM_SERVER_LOCATION_2,
   "ยุโรปตะวันตก"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_MITM_SERVER_LOCATION_3,
   "อเมริกาใต้ (ตะวันออกเฉียงใต้, บราซิล)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_MITM_SERVER_LOCATION_4,
   "เอเชียตะวันออกเฉียงใต้"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_MITM_SERVER_LOCATION_CUSTOM,
   "กำหนดเอง"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_IP_ADDRESS,
   "ที่อยู่เซิฟเวอร์"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_IP_ADDRESS,
   "ที่อยู่ของโฮสต์ที่ต้องการเชื่อมต่อ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_TCP_UDP_PORT,
   "พอร์ตของที่อยู่ IP โฮสต์ สามารถเป็นได้ทั้งพอร์ต TCP หรือ UDP ในเมนู"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_MAX_CONNECTIONS,
   "จำนวนการเชื่อมต่อสูงสุดพร้อมกัน"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_MAX_CONNECTIONS,
   "จำนวนการเชื่อมต่อสูงสุดที่โฮสต์จะยอมรับก่อนที่จะปฏิเสธการเชื่อมต่อใหม่"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_MAX_PING,
   "ตัวจำกัดค่า Ping"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_MAX_PING,
   "ค่าความหน่วงการเชื่อมต่อ (ping) สูงสุดที่โฮสต์จะยอมรับ กำหนดค่าเป็น 0 หากไม่ต้องการจำกัด"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_PASSWORD,
   "รหัสผ่านเซิฟเวอร์"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_PASSWORD,
   "รหัสผ่านที่ใช้โดยไคลเอนต์ในการเชื่อมต่อกับโฮสต์"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SPECTATE_PASSWORD,
   "รหัสผ่านสำหรับผู้ชมเท่านั้นของเซิร์ฟเวอร์"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_SPECTATE_PASSWORD,
   "รหัสผ่านที่ใช้โดยไคลเอนต์ในการเชื่อมต่อกับโฮสต์ในฐานะผู้ชม"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_START_AS_SPECTATOR,
   "โหมดผู้ชม Netplay"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_START_AS_SPECTATOR,
   "เริ่มต้นการเล่นออนไลน์ ในโหมดผู้ชม"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_NETPLAY_START_AS_SPECTATOR,
   "เริ่มต้น Netplay ในโหมดผู้ชมหรือไม่  ถ้าตั้งค่าเป็น true Netplay จะเริ่มต้นในโหมดผู้ชม  สามารถเปลี่ยนโหมดได้ภายหลังเสมอ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_FADE_CHAT,
   "แชทแบบเฟด"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_FADE_CHAT,
   "ทำให้ข้อความแชทค่อย ๆ จางลงเรื่อยๆ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_CHAT_COLOR_NAME,
   "สีแชท (ชื่อเล่น)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_CHAT_COLOR_NAME,
   "รูปแบบ: #RRGGBB หรือ RRGGBB"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_CHAT_COLOR_MSG,
   "สีแชท (ข้อความ)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_CHAT_COLOR_MSG,
   "รูปแบบ: #RRGGBB หรือ RRGGBB"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_ALLOW_PAUSING,
   "อนุญาตให้หยุดชั่วคราว"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_ALLOW_PAUSING,
   "อนุญาตให้ผู้เล่นหยุดเกมชั่วคราวระหว่าง Netplay"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_ALLOW_SLAVES,
   "อนุญาตให้ไคลเอนต์โหมด Slave ใช้งาน"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_ALLOW_SLAVES,
   "อนุญาตให้เชื่อมต่อในโหมด Slave ได้  ไคลเอนต์โหมด Slave ต้องการพลังประมวลผลน้อยมากทั้งสองฝั่ง แต่จะได้รับผลกระทบจากความหน่วงของเครือข่ายอย่างมาก"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_REQUIRE_SLAVES,
   "ไม่อนุญาตให้ไคลเอนต์ที่ไม่ใช่โหมด Slave ใช้งาน"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_REQUIRE_SLAVES,
   "ไม่อนุญาตให้เชื่อมต่อที่ไม่ใช่โหมด Slave  ไม่แนะนำ ยกเว้นสำหรับเครือข่ายที่เร็วมากและเครื่องที่มีประสิทธิภาพต่ำมาก"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_CHECK_FRAMES,
   "ตรวจสอบเฟรม Netplay"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_CHECK_FRAMES,
   "ความถี่ (เฟรม) ที่ Netplay จะตรวจสอบความสอดคล้องกันระหว่างเครื่อง Host และ Client"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_NETPLAY_CHECK_FRAMES,
   "ความถี่ในหน่วยเฟรมที่ Netplay จะตรวจสอบความสอดคล้องกันระหว่างเครื่อง Host และ Client สำหรับ Core ส่วนใหญ่ ค่านี้จะไม่มีผลที่มองเห็นได้และสามารถข้ามไปได้เลย แต่สำหรับ Core ประเภท Nondeterministic ค่านี้จะ[...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_INPUT_LATENCY_FRAMES_MIN,
   "เฟรมความหน่วงของอินพุต"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_INPUT_LATENCY_FRAMES_MIN,
   "จำนวนเฟรมของความหน่วงอินพุตที่ Netplay ใช้เพื่อซ่อนความหน่วงของเครือข่าย ช่วยลดอาการกระตุกและลดการใช้งาน CPU แต่ต้องแลกมาด้วยความล่าช้าในการตอบสนอง ที่สังเกตเห็นได้ชัดเจน"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_NETPLAY_INPUT_LATENCY_FRAMES_MIN,
   "จำนวนเฟรมของความหน่วงอินพุตที่ Netplay ใช้เพื่อซ่อนความหน่วงของเครือข่าย\nเมื่ออยู่ในโหมด Netplay ตัวเลือกนี้จะหน่วงการรับข้อมูลอินพุตจากเครื่องโลคอล เพื่อให้เฟรมที่กำลังรันอยู่ใกล[...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_INPUT_LATENCY_FRAMES_RANGE,
   "ช่วงเฟรมความหน่วงของอินพุต"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_INPUT_LATENCY_FRAMES_RANGE,
   "ช่วงของจำนวนเฟรมความหน่วงของอินพุตที่อาจถูกนำมาใช้เพื่อซ่อนความหน่วงของเครือข่าย ช่วยลดอาการกระตุกและลดภาระการทำงานของ CPU แต่ต้องแลกมาด้วยความล่าช้าในการตอบสนองที่ไม่สาม[...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_NETPLAY_INPUT_LATENCY_FRAMES_RANGE,
   "ช่วงของจำนวนเฟรมความหน่วงของอินพุตที่ Netplay อาจนำมาใช้เพื่อซ่อนความหน่วงของเครือข่าย\nหากตั้งค่าไว้ Netplay จะปรับจำนวนเฟรมของความหน่วงอินพุตแบบไดนามิก เพื่อรักษาสมดุลระหว่างเวลา[...]"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_NAT_TRAVERSAL,
   "เมื่อเป็นโฮสต์ จะพยายามรับการเชื่อมต่อจากอินเทอร์เน็ตสาธารณะ โดยใช้ UPnP หรือเทคโนโลยีที่ใกล้เคียงกันเพื่อทะลุผ่าน LAN ออกไป"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_DIGITAL,
   "การแชร์อินพุตดิจิทัล"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_REQUEST_DEVICE_I,
   "ร้องขออุปกรณ์ %u"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_REQUEST_DEVICE_I,
   "คำร้องขอเพื่อเล่นด้วยอุปกรณ์อินพุตที่กำหนด"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_CMD_ENABLE,
   "คำสั่งเครือข่าย"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_CMD_PORT,
   "พอร์ตคำสั่งเครือข่าย"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_REMOTE_PORT,
   "พอร์ตพื้นฐานของ Network RetroPad"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_USER_REMOTE_ENABLE,
   "ผู้เล่น %d Network RetroPad"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STDIN_CMD_ENABLE,
   "คำสั่ง stdin"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_STDIN_CMD_ENABLE,
   "อินเตอร์เฟซคำสั่ง stdin"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_ON_DEMAND_THUMBNAILS,
   "ดาวน์โหลดรูปตัวอย่างแบบตามคำขอ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETWORK_ON_DEMAND_THUMBNAILS,
   "ดาวน์โหลดรูปตัวอย่างที่ขาดหายไปโดยอัตโนมัติขณะเลือกดูเพลย์ลิสต์ ซึ่งจะส่งผลกระทบต่อประสิทธิภาพการทำงานอย่างรุนแรง"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATER_SETTINGS,
   "ตั้งค่าตัวอัปเดต"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_UPDATER_SETTINGS,
   "เข้าถึงการตั้งค่าตัวอัปเดต Core"
   )

/* Settings > Network > Updater */

MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_UPDATER_BUILDBOT_URL,
   "URL ไปยังไดเรกทอรีตัวอัปเดตคอร์บน Buildbot ของ Libretro"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BUILDBOT_ASSETS_URL,
   "URL ไปยังไดเรกทอรีตัวอัปเดตทรัพยากรบน libretro buildbot"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_UPDATER_AUTO_EXTRACT_ARCHIVE,
   "สกัดไฟล์เก็บถาวรที่ดาวน์โหลดมาโดยอัตโนมัติ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_UPDATER_AUTO_EXTRACT_ARCHIVE,
   "หลังจากดาวน์โหลดเสร็จสิ้น จะทำการแตกไฟล์ที่อยู่ในไฟล์เก็บถาวร (Archive) โดยอัตโนมัติ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_UPDATER_SHOW_EXPERIMENTAL_CORES,
   "แสดง 'Core ทดลอง'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_UPDATER_SHOW_EXPERIMENTAL_CORES,
   "รวม 'Core ทดลอง' ไว้ในรายการตัวดาวน์โหลด Core โดยปกติแล้วสิ่งเหล่านี้มีไว้เพื่อการพัฒนา/ทดสอบเท่านั้น และไม่แนะนำให้ใช้งานทั่วไป"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_UPDATER_AUTO_BACKUP,
   "สำรองข้อมูล Core เมื่อทำการอัปเดต"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_UPDATER_AUTO_BACKUP,
   "สร้างสำรองข้อมูลของ Core ที่ติดตั้งไว้โดยอัตโนมัติเมื่อทำการอัปเดตออนไลน์ ช่วยให้สามารถย้อนกลับไปยัง Core ที่ใช้งานได้ตามปกติ หากการอัปเดตทำให้เกิดข้อผิดพลาดหรือประสิทธิภาพลดลง"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_UPDATER_AUTO_BACKUP_HISTORY_SIZE,
   "ขนาดประวัติการสำรองข้อมูล Core"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_UPDATER_AUTO_BACKUP_HISTORY_SIZE,
   "ระบุจำนวนไฟล์สำรองข้อมูลที่สร้างขึ้นโดยอัตโนมัติเพื่อเก็บไว้สำหรับแต่ละ Core ที่ติดตั้ง เมื่อครบตามจำนวนที่กำหนด การสร้างไฟล์สำรองใหม่ผ่านการอัปเดตออนไลน์จะลบไฟล์สำรองที่เก[...]"
   )

/* Settings > Playlists */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HISTORY_LIST_ENABLE,
   "ประวัติ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_HISTORY_LIST_ENABLE,
   "เก็บรายการประวัติของเกม รูปภาพ เพลง และวิดีโอที่เพิ่งใช้งานล่าสุดไว้ในเพลย์ลิสต์"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_HISTORY_SIZE,
   "ขนาดประวัติการใช้งานล่าสุด"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_HISTORY_SIZE,
   "จำกัดจำนวนรายการในเพลย์ลิสต์ประวัติการใช้งานล่าสุดสำหรับเกม รูปภาพ เพลง และวิดีโอ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_FAVORITES_SIZE,
   "ขนาดประวัติรายการโปรด"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_FAVORITES_SIZE,
   "จำกัดจำนวนรายการในเพลย์ลิสต์ 'รายการโปรด' เมื่อครบตามจำนวนที่กำหนด จะไม่สามารถเพิ่มรายการใหม่ได้จนกว่าจะลบรายการเก่าออก การตั้งค่าเป็น -1 จะทำให้สามารถเพิ่มรายการได้ 'ไม่จำกั[...]"
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
   MENU_ENUM_LABEL_VALUE_PLAYLIST_ENTRY_REMOVE,
   "อนุญาตให้ลบรายการออก"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_ENTRY_REMOVE,
   "อนุญาตให้ลบรายการออกจากเพลย์ลิสต์ได้"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SORT_ALPHABETICAL,
   "เรียงลำดับเพลย์ลิสต์ตามตัวอักษร"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_SORT_ALPHABETICAL,
   "เรียงลำดับเพลย์ลิสต์เนื้อหาตามตัวอักษร โดยไม่รวมเพลย์ลิสต์ 'ประวัติการใช้งาน', 'รูปภาพ', 'เพลง' และ 'วิดีโอ'"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_USE_OLD_FORMAT,
   "บันทึกเพลย์ลิสต์โดยใช้รูปแบบเก่า"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_USE_OLD_FORMAT,
   "บันทึกเพลย์ลิสต์โดยใช้รูปแบบข้อความ (Plain-text) ซึ่งเป็นรูปแบบเก่าที่เลิกใช้แล้ว หากปิดการใช้งานนี้ เพลย์ลิสต์จะถูกจัดรูปแบบโดยใช้ JSON แทน"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_COMPRESSION,
   "บีบอัดเพลย์ลิสต์"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_COMPRESSION,
   "บีบอัดข้อมูลเพลย์ลิสต์เมื่อบันทึกลงดิสก์ ช่วยลดขนาดไฟล์และเวลาในการโหลด โดยแลกกับการใช้งาน CPU ที่เพิ่มขึ้น (เพียงเล็กน้อย) สามารถใช้ได้กับเพลย์ลิสต์ทั้งรูปแบบเก่าและรูปแบบให[...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SHOW_INLINE_CORE_NAME,
   "แสดง Core ที่เกี่ยวข้องในเพลย์ลิสต์"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_SHOW_INLINE_CORE_NAME,
   "กำหนดว่าจะให้แสดงแท็ก Core ในเพลย์ลิสต์เมื่อใด (ถ้ามี)การตั้งค่านี้จะถูกละเว้นเมื่อเปิดใช้งานคำอธิบายย่อยในเพลย์ลิสต์"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SHOW_SUBLABELS,
   "แสดงคำอธิบายย่อยในเพลย์ลิสต์"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_SHOW_SUBLABELS,
   "แสดงข้อมูลเพิ่มเติมสำหรับแต่ละรายการในเพลย์ลิสต์ เช่น Core ที่เชื่อมโยงอยู่และเวลาที่ใช้เล่น (ถ้ามี) ซึ่งอาจส่งผลต่อประสิทธิภาพการทำงานที่แตกต่างกันออกไป"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SHOW_HISTORY_ICONS,
   "แสดงไอคอนเฉพาะเนื้อหาในประวัติและรายการที่ชื่นชอบ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_SHOW_HISTORY_ICONS,
   "แสดงไอคอนเฉพาะสำหรับแต่ละรายการในเพลย์ลิสต์ประวัติและรายการที่ชื่นชอบ ซึ่งอาจส่งผลต่อประสิทธิภาพการทำงานที่แตกต่างกันออกไป"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SUBLABEL_RUNTIME,
   "เวลาที่ใช้เล่น:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SUBLABEL_LAST_PLAYED,
   "เล่นล่าสุด:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SUBLABEL_PLAY_COUNT,
   "จำนวนครั้งที่เล่น:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_SECONDS_SINGLE,
   "วินาที"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_SECONDS_PLURAL,
   "วินาที"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_MINUTES_SINGLE,
   "นาที"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_MINUTES_PLURAL,
   "นาที"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_HOURS_SINGLE,
   "ชั่วโมง"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_HOURS_PLURAL,
   "ชั่วโมง"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_DAYS_SINGLE,
   "วัน"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_DAYS_PLURAL,
   "วัน"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_WEEKS_SINGLE,
   "สัปดาห์"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_WEEKS_PLURAL,
   "สัปดาห์"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_MONTHS_SINGLE,
   "เดือน"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_MONTHS_PLURAL,
   "เดือน"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_YEARS_SINGLE,
   "ปี"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_YEARS_PLURAL,
   "ปี"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_AGO,
   "ที่ผ่านมา"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SHOW_ENTRY_IDX,
   "แสดงหมายเลขรายการในเพลย์ลิสต์"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_SHOW_ENTRY_IDX,
   "แสดงหมายเลขรายการเมื่อเรียกดูเพลย์ลิสต์ รูปแบบการแสดงผลจะขึ้นอยู่กับไดรเวอร์เมนูที่เลือกใช้ในปัจจุบัน"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SUBLABEL_RUNTIME_TYPE,
   "เวลาที่ใช้เล่นในคำอธิบายย่อยของเพลย์ลิสต์"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_SUBLABEL_RUNTIME_TYPE,
   "เลือกประเภทของ log เวลาที่ใช้เล่นที่จะแสดงในคำอธิบายย่อยของเพลย์ลิสต์\nต้องเปิดใช้งาน log เวลาที่ใช้เล่นที่เกี่ยวข้องผ่านเมนูตัวเลือกการบันทึกก่อน"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SUBLABEL_LAST_PLAYED_STYLE,
   "รูปแบบวันที่และเวลา 'เล่นล่าสุด'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_SUBLABEL_LAST_PLAYED_STYLE,
   "ตั้งค่ารูปแบบของวันที่และเวลาที่แสดงสำหรับข้อมูล 'เล่นล่าสุด' ตัวเลือกแบบ '(AM/PM)' อาจส่งผลต่อประสิทธิภาพการทำงานเล็กน้อยในบางแพลตฟอร์ม"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_FUZZY_ARCHIVE_MATCH,
   "ตรวจสอบไฟล์บีบอัดแบบง่ายๆ"
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
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_PORTABLE_PATHS,
   "รายการเกมแบบพกพา"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_PORTABLE_PATHS,
   "เมื่อเปิดใช้งาน และมีการเลือกไดเรกทอรี 'ตัวเลือกไฟล์' ไว้ด้วย ค่าปัจจุบันของพารามิเตอร์ 'ตัวเลือกไฟล์' จะถูกบันทึกไว้ในเพลย์ลิสต์ เมื่อมีการโหลดเพลย์ลิสต์นี้ในระบบอื่นที่เปิด[...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_USE_FILENAME,
   "ใช้ชื่อไฟล์สำหรับการจับคู่รูปภาพตัวอย่าง"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_USE_FILENAME,
   "เมื่อเปิดใช้งาน จะค้นหารูปภาพตัวอย่างจากชื่อไฟล์ของรายการ แทนที่จะค้นหาจากชื่อที่แสดงในรายการครับ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_ALLOW_NON_PNG,
   "อนุญาตให้ใช้ไฟล์ภาพทุกรูปแบบที่รองรับสำหรับรูปภาพตัวอย่าง"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_ALLOW_NON_PNG,
   "เมื่อเปิดใช้งาน จะสามารถเพิ่มรูปภาพตัวอย่างจากไฟล์ภาพทุกประเภทที่ RetroArch รองรับ (เช่น jpeg) แต่อาจส่งผลต่อประสิทธิภาพการทำงานเล็กน้อย"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANAGE,
   "จัดการ"
   )

/* Settings > Playlists > Playlist Management */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_DEFAULT_CORE,
   "Core เริ่มต้น"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_MANAGER_DEFAULT_CORE,
   "ระบุ Core ที่จะใช้เมื่อเรียกใช้เนื้อหาผ่านรายการในเพลย์ลิสต์ที่ยังไม่มีการเชื่อมโยงกับ Core ใดๆ ไว้ก่อนหน้า"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_RESET_CORES,
   "รีเซ็ตการเชื่อมโยง Core"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_MANAGER_RESET_CORES,
   "ลบการเชื่อมโยง Core ที่มีอยู่เดิมสำหรับทุกรายการในเพลย์ลิสต์"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE,
   "โหมดการแสดงชื่อรายการ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE,
   "เปลี่ยนวิธีแสดงชื่อรายการเนื้อหาในเพลย์ลิสต์นี้"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_SORT_MODE,
   "วิธีการจัดเรียงตัวเลือก"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_MANAGER_SORT_MODE,
   "กำหนดวิธีการจัดเรียงรายการในเพลย์ลิสต์นี้"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_CLEAN_PLAYLIST,
   "ลบเพลย์ลิสต์"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_MANAGER_CLEAN_PLAYLIST,
   "ตรวจสอบการเชื่อมโยง Core และลบรายการที่ไม่ถูกต้องหรือรายการที่ซ้ำกันออก"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_REFRESH_PLAYLIST,
   "รีเฟรชเพลย์ลิสต์"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_MANAGER_REFRESH_PLAYLIST,
   "เพิ่มเนื้อหาใหม่และลบรายการที่ไม่ถูกต้องออก โดยการทำซ้ำการสแกนเนื้อหาที่ใช้ล่าสุดในการสร้างหรือแก้ไขเพลย์ลิสต์นั้น ๆ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DELETE_PLAYLIST,
   "ลบเพลย์ลิสต์"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DELETE_PLAYLIST,
   "ลบเพลย์ลิสต์ออกจากระบบไฟล์"
   )

/* Settings > User */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PRIVACY_SETTINGS,
   "ความเป็นส่วนตัว"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PRIVACY_SETTINGS,
   "เปลี่ยนการตั้งค่าความเป็นส่วนตัว"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACCOUNTS_LIST,
   "บัญชีผู้ใช้"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACCOUNTS_LIST,
   "จัดการบัญชีผู้ใช้ที่กำหนดค่าไว้ในปัจจุบัน"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_NICKNAME,
   "ชื่อผู้ใช้"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_NICKNAME,
   "ระบุชื่อผู้ใช้ของคุณที่นี่ ซึ่งจะถูกนำไปใช้สำหรับการเล่นผ่านระบบออนไลน์ (Netplay) และฟังก์ชันอื่น ๆ ที่เกี่ยวข้อง"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_USER_LANGUAGE,
   "ภาษา"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_USER_LANGUAGE,
   "ตั้งค่าภาษาของส่วนประสานผู้ใช้ (UI)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_USER_LANGUAGE,
   "ปรับเปลี่ยนภาษาของเมนูและข้อความทั้งหมดที่แสดงบนหน้าจอตามภาษาที่คุณเลือกไว้ที่นี่ จำเป็นต้องรีสตาร์ทเพื่อให้การเปลี่ยนแปลงมีผล\nความสมบูรณ์ของการแปลจะแสดงอยู่ถัดจากแต่ล[...]"
   )

/* Settings > User > Privacy */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CAMERA_ALLOW,
   "อนุญาตกล้อง"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CAMERA_ALLOW,
   "อนุญาตให้ Core เข้าถึงกล้อง"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DISCORD_ALLOW,
   "อนุญาตให้แอป Discord แสดงข้อมูลเกี่ยวกับเนื้อหาที่กำลังเล่น\nใช้งานได้เฉพาะกับโปรแกรมบนเดสก์ท็อปเท่านั้น"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOCATION_ALLOW,
   "อนุญาตตำแหน่งที่ตั้ง"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOCATION_ALLOW,
   "อนุญาตให้ Core เข้าถึงตำแหน่งที่ตั้งของคุณ"
   )

/* Settings > User > Accounts */

MSG_HASH(
   MENU_ENUM_SUBLABEL_ACCOUNTS_RETRO_ACHIEVEMENTS,
   "สะสมความสำเร็จ ในเกมคลาสสิก ดูข้อมูลเพิ่มเติมได้ที่  'https://retroachievements.org'"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_ACCOUNTS_RETRO_ACHIEVEMENTS,
   "รายละเอียดการเข้าสู่ระบบสำหรับบัญชี RetroAchievements ของคุณ เข้าไปที่ retroachievements.org เพื่อสมัครบัญชีฟรี\nหลังจากลงทะเบียนเสร็จแล้ว คุณต้องระบุชื่อผู้ใช้และรหัสผ่านลงใน RetroArch"
   )

/* Settings > User > Accounts > RetroAchievements */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACCOUNTS_CHEEVOS_USERNAME,
   "ชื่อผู้ใช้"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_USERNAME,
   "ระบุชื่อผู้ใช้บัญชี RetroAchievements ของคุณ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACCOUNTS_CHEEVOS_PASSWORD,
   "รหัสผ่าน"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_PASSWORD,
   "ระบุรหัสผ่านของบัญชี RetroAchievements ของคุณ ความยาวสูงสุด: 255 ตัวอักษร"
   )

/* Settings > User > Accounts > YouTube */


/* Settings > User > Accounts > Twitch */


/* Settings > User > Accounts > Facebook Gaming */


/* Settings > Directory */

MSG_HASH(
   MENU_ENUM_SUBLABEL_SYSTEM_DIRECTORY,
   "ไบออส (BIOS), บูต ROM (Boot ROM) และไฟล์เฉพาะของระบบอื่น ๆ จะถูกจัดเก็บไว้ในโฟลเดอร์นี้"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_ASSETS_DIRECTORY,
   "ดาวน์โหลด"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_ASSETS_DIRECTORY,
   "ไฟล์ที่ดาวน์โหลดมาจะถูกจัดเก็บไว้ในโฟลเดอร์นี้"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ASSETS_DIRECTORY,
   "ทรัพยากร"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ASSETS_DIRECTORY,
   "โฟลเดอร์นี้ใช้สำหรับจัดเก็บทรัพยากรเมนูที่ RetroArch เรียกใช้งาน"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DYNAMIC_WALLPAPERS_DIRECTORY,
   "พื้นหลังแบบไดนามิก"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DYNAMIC_WALLPAPERS_DIRECTORY,
   "โฟลเดอร์นี้ใช้สำหรับจัดเก็บรูปภาพพื้นหลังที่ใช้งานภายในเมนู"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_THUMBNAILS_DIRECTORY,
   "ภาพตัวอย่าง"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_THUMBNAILS_DIRECTORY,
   "โฟลเดอร์นี้ใช้สำหรับจัดเก็บภาพตัวอย่างที่เป็นหน้าปกเกม ภาพจับภาพหน้าจอ และหน้าจอชื่อเกม"
   )
MSG_HASH( /* FIXME Not RGUI specific */
   MENU_ENUM_LABEL_VALUE_RGUI_BROWSER_DIRECTORY,
   "เปิดไดเรกทอรีเริ่มต้น"
   )
MSG_HASH( /* FIXME Not RGUI specific */
   MENU_ENUM_SUBLABEL_RGUI_BROWSER_DIRECTORY,
   "ตั้งค่าโฟลเดอร์เริ่มต้นสำหรับตัวเรียกดูไฟล์"
   )
MSG_HASH( /* FIXME Not RGUI specific */
   MENU_ENUM_LABEL_VALUE_RGUI_CONFIG_DIRECTORY,
   "ไฟล์การตั้งค่า"
   )
MSG_HASH( /* FIXME Not RGUI specific */
   MENU_ENUM_SUBLABEL_RGUI_CONFIG_DIRECTORY,
   "ไฟล์การตั้งค่าเริ่มต้นจะถูกจัดเก็บไว้ในโฟลเดอร์นี้"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LIBRETRO_DIR_PATH,
   "ไฟล์ Libretro core จะถูกจัดเก็บไว้ในโฟลเดอร์นี้"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LIBRETRO_INFO_PATH,
   "ข้อมูล Core"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LIBRETRO_INFO_PATH,
   "ไฟล์ข้อมูลแอปพลิเคชันหรือข้อมูล Core จะถูกจัดเก็บไว้ในโฟลเดอร์นี้"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_DATABASE_DIRECTORY,
   "ฐานข้อมูล"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_DATABASE_DIRECTORY,
   "ไฟล์ Database จะถูกจัดเก็บไว้ในโฟลเดอร์นี้"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_DATABASE_PATH,
   "ไฟล์สูตรโกง"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_DATABASE_PATH,
   "ไฟล์สูตรโกงจะถูกจัดเก็บไว้ในโฟลเดอร์นี้"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FILTER_DIR,
   "ตัวกรองวิดีโอ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FILTER_DIR,
   "ไฟล์วิดีโอฟิลเตอร์ที่ประมวลผลด้วย CPU จะถูกจัดเก็บไว้ในโฟลเดอร์นี้"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_FILTER_DIR,
   "ตัวกรองเสียง"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_FILTER_DIR,
   "ไฟล์ออดิโอ DSP ฟิลเตอร์จะถูกจัดเก็บไว้ในโฟลเดอร์นี้"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_DIR,
   "วิดีโอเชดเดอร์"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_DIR,
   "ไฟล์วิดีโอเชดเดอร์ที่ประมวลผลด้วย GPU จะถูกจัดเก็บไว้ในโฟลเดอร์นี้"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RECORDING_OUTPUT_DIRECTORY,
   "การบันทึก"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RECORDING_OUTPUT_DIRECTORY,
   "ไฟล์ที่บันทึกไว้จะถูกจัดเก็บไว้ในโฟลเดอร์นี้"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RECORDING_CONFIG_DIRECTORY,
   "การตั้งค่าการบันทึก"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RECORDING_CONFIG_DIRECTORY,
   "ไดเรกทอรีสำหรับเก็บการตั้งค่าการบันทึก"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_DIRECTORY,
   "โอเวอร์เลย์"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_DIRECTORY,
   "ไดเรกทอรีสำหรับเก็บไฟล์โอเวอร์เลย์"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OSK_OVERLAY_DIRECTORY,
   "โอเวอร์เลย์คีย์บอร์ด"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OSK_OVERLAY_DIRECTORY,
   "ไดเรกทอรีสำหรับเก็บไฟล์โอเวอร์เลย์คีย์บอร์ด"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_LAYOUT_DIRECTORY,
   "เลย์เอาต์วิดีโอ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_LAYOUT_DIRECTORY,
   "ไดเรกทอรีสำหรับเก็บไฟล์เลย์เอาต์วิดีโอ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCREENSHOT_DIRECTORY,
   "ภาพหน้าจอ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SCREENSHOT_DIRECTORY,
   "ไดเรกทอรีสำหรับเก็บภาพหน้าจอ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_JOYPAD_AUTOCONFIG_DIR,
   "โปรไฟล์คอนโทรลเลอร์"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_JOYPAD_AUTOCONFIG_DIR,
   "ไดเรกทอรีสำหรับเก็บไฟล์โปรไฟล์ที่ใช้สำหรับกำหนดค่าคอนโทรลเลอร์โดยอัตโนมัติ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_REMAPPING_DIRECTORY,
   "การเปลี่ยนปุ่มกดใหม่"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_REMAPPING_DIRECTORY,
   "ไดเรกทอรีสำหรับเก็บไฟล์การเปลี่ยนปุ่มกดใหม่"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_DIRECTORY,
   "เพลย์ลิสต์"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_DIRECTORY,
   "ไดเรกทอรีสำหรับเก็บไฟล์เพลย์ลิสต์"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_FAVORITES_DIRECTORY,
   "เพลย์ลิสต์ที่ชอบ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_FAVORITES_DIRECTORY,
   "ไดเรกทอรีสำหรับบันทึกเพลย์ลิสต์ที่ชื่นชอบ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_HISTORY_DIRECTORY,
   "เพลย์ลิสต์ประวัติการใช้งาน"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_HISTORY_DIRECTORY,
   "บันทึกเพลย์ลิสต์ประวัติการใช้งานลงในโฟลเดอร์นี้"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_IMAGE_HISTORY_DIRECTORY,
   "เพลย์ลิสต์รูปภาพ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_IMAGE_HISTORY_DIRECTORY,
   "บันทึกเพลย์ลิสต์ประวัติรูปภาพลงในโฟลเดอร์นี้"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_MUSIC_HISTORY_DIRECTORY,
   "เพลย์ลิสต์เพลง"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_MUSIC_HISTORY_DIRECTORY,
   "บันทึกเพลย์ลิสต์เพลงลงในโฟลเดอร์นี้"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_VIDEO_HISTORY_DIRECTORY,
   "เพลย์ลิสต์วิดีโอ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_VIDEO_HISTORY_DIRECTORY,
   "บันทึกเพลย์ลิสต์วิดีโอลงในโฟลเดอร์นี้"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RUNTIME_LOG_DIRECTORY,
   "บันทึกการทำงาน"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RUNTIME_LOG_DIRECTORY,
   "บันทึกการทำงาน (Runtime Logs) จะถูกจัดเก็บไว้ในโฟลเดอร์นี้"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVEFILE_DIRECTORY,
   "บันทึกไฟล์"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVEFILE_DIRECTORY,
   "บันทึกไฟล์เซฟทั้งหมดลงในไดเรกทอรีนี้ หากไม่ได้กำหนดไว้ จะพยายามบันทึกไว้ภายในไดเรกทอรีที่ไฟล์เนื้อหาเปิดทำงานอยู่"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_SAVEFILE_DIRECTORY,
   "บันทึกไฟล์เซฟทั้งหมด (*.srm) ลงในไดเรกทอรีนี้ รวมถึงไฟล์ที่เกี่ยวข้อง เช่น .rt, .psrm และอื่นๆ โดยตัวเลือกจากบรรทัดคำสั่ง (Command Line) จะมีความสำคัญเหนือกว่าค่าที่ตั้งไว้นี้"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVESTATE_DIRECTORY,
   "บันทึกสถานะ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVESTATE_DIRECTORY,
   "บันทึกสถานะ และการเล่นย้อนหลัง จะถูกจัดเก็บไว้ในโฟลเดอร์นี้ หากไม่ได้ตั้งค่าไว้ ระบบจะพยายามบันทึกไปยังโฟลเดอร์ที่เนื้อหานั้นตั้งอยู่โดยอัตโนมัติ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CACHE_DIRECTORY,
   "แคช"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CACHE_DIRECTORY,
   "เนื้อหาที่ถูกบีบอัดจะถูกแตกไฟล์ชั่วคราวลงในไดเรกทอรีนี้"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOG_DIR,
   "บันทึกเหตุการณ์ของระบบ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOG_DIR,
   "บันทึกเหตุการณ์ของระบบจะถูกจัดเก็บไว้ในไดเรกทอรีนี้"
   )

#ifdef HAVE_MIST
/* Settings > Steam */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STEAM_RICH_PRESENCE_ENABLE,
   "เปิดใช้งาน Rich Presence"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_STEAM_RICH_PRESENCE_ENABLE,
   "แชร์สถานะการเล่นของคุณใน RetroArch บน Steam"
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STEAM_RICH_PRESENCE_FORMAT,
   "รูปแบบเนื้อหาของ Rich Presence"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_STEAM_RICH_PRESENCE_FORMAT,
   "ตัดสินใจว่าข้อมูลใดที่เกี่ยวข้องกับเนื้อหาจะถูกแชร์ออกไป"
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STEAM_RICH_PRESENCE_FORMAT_CONTENT,
   "เนื้อหา"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STEAM_RICH_PRESENCE_FORMAT_CORE,
   "ชื่อ Core"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STEAM_RICH_PRESENCE_FORMAT_SYSTEM,
   "ชื่อระบบ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STEAM_RICH_PRESENCE_FORMAT_CONTENT_SYSTEM,
   "เนื้อหา (ชื่อ ระบบ)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STEAM_RICH_PRESENCE_FORMAT_CONTENT_CORE,
   "เนื้อหา (ชื่อ Core)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STEAM_RICH_PRESENCE_FORMAT_CONTENT_SYSTEM_CORE,
   "เนื้อหา (ชื่อระบบ - ชื่อ Core)"
   )
#endif

/* Music */

/* Music > Quick Menu */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_TO_MIXER,
   "เพิ่มลงใน Mixer"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ADD_TO_MIXER,
   "เพิ่มแทร็กเสียงนี้ลงในช่องสตรีมเสียงที่ว่างอยู่\nหากไม่มีช่องว่างที่พร้อมใช้งานในขณะนี้ ข้อมูลนี้จะถูกข้ามไป"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_TO_MIXER_AND_PLAY,
   "เพิ่มลงใน Mixer และเล่น"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ADD_TO_MIXER_AND_PLAY,
   "เพิ่มแทร็กเสียงนี้ลงในช่องสตรีมเสียงที่ว่างอยู่และเล่นทันที\nหากไม่มีช่องว่างที่พร้อมใช้งานในขณะนี้ ข้อมูลนี้จะถูกข้ามไป"
   )

/* Netplay */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_ENABLE_CLIENT,
   "เชื่อมต่อกับ Netplay Host"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_ENABLE_CLIENT,
   "กรอกที่อยู่เซิร์ฟเวอร์ Netplay และเชื่อมต่อในโหมด Client"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_DISCONNECT,
   "ตัดการเชื่อมต่อจาก Netplay Host"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_DISCONNECT,
   "ตัดการเชื่อมต่อ Netplay ที่กำลังใช้งานอยู่"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_LOBBY_FILTERS,
   "ตัวกรองล็อบบี้"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHOW_ONLY_CONNECTABLE,
   "เฉพาะห้องที่เชื่อมต่อได้เท่านั้น"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHOW_ONLY_INSTALLED_CORES,
   "เฉพาะ Core ที่ติดตั้งแล้วเท่านั้น"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHOW_PASSWORDED,
   "ห้องที่มีรหัสผ่าน"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_REFRESH_ROOMS,
   "รีเฟรชรายชื่อ Netplay Host"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_REFRESH_ROOMS,
   "สแกนหา Netplay Host"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_REFRESH_LAN,
   "รีเฟรชรายชื่อ Netplay LAN"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_REFRESH_LAN,
   "สแกนหา Netplay Host บน LAN"
   )

/* Netplay > Host */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_ENABLE_HOST,
   "เริ่ม Netplay Host"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_ENABLE_HOST,
   "เริ่ม Netplay ในโหมดโฮสต์ (เซิร์ฟเวอร์)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_DISABLE_HOST,
   "หยุด Netplay Host"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_KICK,
   "เตะผู้เล่นออก"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_KICK,
   "เตะผู้เล่นออกจากห้องที่คุณกำลังเป็นโฮสต์อยู่ขณะนี้"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_BAN,
   "แบนผู้เล่น"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_BAN,
   "แบนผู้เล่นออกจากห้องที่คุณกำลังเป็นโฮสต์อยู่ขณะนี้"
   )

/* Import Content */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCAN_DIRECTORY,
   "สแกนไดเรกทอรี"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SCAN_DIRECTORY,
   "สแกนไดเรกทอรีเพื่อหาเนื้อหาที่ตรงกับฐานข้อมูล"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_SCAN_THIS_DIRECTORY,
   "เลือกสิ่งนี้เพื่อสแกนไดเรกทอรีปัจจุบันเพื่อหาเนื้อหา"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCAN_FILE,
   "สแกนไฟล์"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SCAN_FILE,
   "สแกนไฟล์เพื่อหาเนื้อหาที่ตรงกับฐานข้อมูล"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_LIST,
   "สแกนเนื้อหา"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_LIST,
   "การสแกนที่กำหนดค่าได้ โดยอ้างอิงตามชื่อไฟล์ของเนื้อหา และ/หรือ ข้อมูลที่ตรงกับฐานข้อมูล"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCAN_ENTRY,
   "สแกน"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCAN_METHOD,
   "วิธีการสแกน"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SCAN_METHOD,
   "อัตโนมัติหรือกำหนดเองพร้อมตัวเลือกโดยละเอียด"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCAN_USE_DB,
   "ตรวจสอบฐานข้อมูล"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SCAN_USE_DB,
   "แบบเข้มงวดจะเพิ่มเฉพาะรายการที่ตรงกับฐานข้อมูลเท่านั้น แบบผ่อนปรนจะเพิ่มไฟล์ที่มีนามสกุลถูกต้องด้วยแม้จะไม่พบข้อมูลที่ตรงกับ CRC/serial แบบกำหนดเองจะตรวจสอบกับไฟล์ XML ที่ผู้ใช้ร[...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCAN_DB_SELECT,
   "ฐานข้อมูลที่ใช้เปรียบเทียบ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SCAN_DB_SELECT,
   "จำกัดการค้นหาให้เฉพาะเจาะจงเพียงฐานข้อมูลเดียว หรือเลือกฐานข้อมูลแรกที่พบข้อมูลที่ตรงกัน เพื่อเพิ่มความเร็วในการสแกน"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCAN_TARGET_PLAYLIST,
   "เพลย์ลิสต์ที่ต้องการอัปเดต"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SCAN_TARGET_PLAYLIST,
   "ผลลัพธ์จะถูกเพิ่มลงในเพลย์ลิสต์นี้ ในกรณีที่เป็นแบบอัตโนมัติ - ทั้งหมด เพลย์ลิสต์ของหลายระบบอาจถูกอัปเดตพร้อมกัน \nหากเลือกแบบกำหนดเองโดยไม่อ้างอิงฐานข้อมูล รายการที่เพิ่มเ[...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCAN_SINGLE_FILE,
   "สแกนไฟล์เดียว"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SCAN_SINGLE_FILE,
   "สแกนไฟล์เพียงไฟล์เดียวแทนที่จะเป็นไดเรกทอรี \nเลือกตำแหน่งเนื้อหาอีกครั้งหลังจากเปลี่ยนรายการนี้"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCAN_OMIT_DB_REF,
   "ข้ามการอ้างอิงฐานข้อมูลจากเพลย์ลิสต์"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SCAN_OMIT_DB_REF,
   "ในกรณีที่มีการกำหนดชื่อเพลย์ลิสต์เอง ให้ใช้ชื่อเพลย์ลิสต์นั้นในการค้นหาภาพตัวอย่างเสมอ แม้ว่าจะมีการค้นพบข้อมูลที่ตรงกับฐานข้อมูลก็ตาม"
   )

/* Import Content > Scan File */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_TO_MIXER_AND_COLLECTION,
   "เพิ่มลงใน Mixer"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_TO_MIXER_AND_COLLECTION_AND_PLAY,
   "เพิ่มลงใน Mixer และเล่น"
   )

/* Import Content > Content Scan */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_DIR,
   "ตำแหน่งเนื้อหา"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_DIR,
   "เลือกไดเรกทอรี (หรือไฟล์) เพื่อสแกนหาเนื้อหา"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_SYSTEM_NAME,
   "เพลย์ลิสต์เป้าหมาย"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_SYSTEM_NAME,
   "ชื่อของไฟล์เพลย์ลิสต์ที่สร้างขึ้น ซึ่งจะถูกใช้ในการระบุรูปภาพตัวอย่างของเพลย์ลิสต์ด้วย หากตั้งค่าเป็นอัตโนมัติจะใช้ชื่อเดียวกับฐานข้อมูลที่ตรงกันหรือชื่อไดเรกทอรีของเนื[...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_SYSTEM_NAME_CUSTOM,
   "ชื่อเพลย์ลิสต์ที่กำหนดเอง"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_SYSTEM_NAME_CUSTOM,
   "ชื่อเพลย์ลิสต์ที่กำหนดเองสำหรับเนื้อหาที่สแกนแล้ว"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_CORE_NAME,
   "Core เริ่มต้น"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_CORE_NAME,
   "เลือก Core เริ่มต้นสำหรับใช้ในการเปิดเนื้อหาที่สแกนพบแล้ว"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_FILE_EXTS,
   "นามสกุลไฟล์"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_FILE_EXTS,
   "รายการประเภทไฟล์ที่จะรวมอยู่ในการสแกน โดยคั่นด้วยเว้นวรรค หากว่างไว้จะรวมไฟล์ทุกประเภท หรือหากมีการระบุ Core จะรวมไฟล์ทั้งหมดที่ Core นั้นรองรับ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_SEARCH_RECURSIVELY,
   "สแกนแบบเรียกซ้ำ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_SEARCH_RECURSIVELY,
   "เมื่อเปิดใช้งาน ไดเรกทอรีย่อยทั้งหมดของ 'ไดเรกทอรีเนื้อหา' ที่ระบุไว้จะถูกรวมอยู่ในการสแกนด้วย"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_SEARCH_ARCHIVES,
   "สแกนภายในไฟล์บีบอัด"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_SEARCH_ARCHIVES,
   "เมื่อเปิดใช้งาน จะมีการค้นหาเนื้อหาที่ถูกต้องหรือรองรับภายในไฟล์บีบอัด (.zip, .7z และอื่นๆ) ซึ่งอาจส่งผลกระทบอย่างมากต่อประสิทธิภาพในการสแกน"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_DAT_FILE,
   "ไฟล์ Arcade DAT"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_DAT_FILE,
   "เลือกไฟล์ Logiqx หรือ MAME List XML DAT เพื่อเปิดใช้งานการตั้งชื่อเนื้อหาอาร์เขตที่สแกนโดยอัตโนมัติ (MAME, FinalBurn Neo และอื่น ๆ)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_DAT_FILE_FILTER,
   "ตัวกรองไฟล์ Arcade DAT"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_DAT_FILE_FILTER,
   "เมื่อใช้งานไฟล์ Arcade DAT เนื้อหาจะถูกเพิ่มลงในเพลย์ลิสต์ก็ต่อเมื่อพบรายการที่ตรงกันในไฟล์ DAT เท่านั้น"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_OVERWRITE,
   "เขียนทับเพลย์ลิสต์ที่มีอยู่เดิม"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_OVERWRITE,
   "เมื่อเปิดใช้งาน เพลย์ลิสต์ที่มีอยู่เดิมจะถูกลบออกก่อนที่จะเริ่มการสแกนเนื้อหา เมื่อปิดใช้งาน รายการในเพลย์ลิสต์ที่มีอยู่จะถูกเก็บรักษาไว้ และจะมีการเพิ่มเฉพาะเนื้อหาที่ย[...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_VALIDATE_ENTRIES,
   "ตรวจสอบรายการที่มีอยู่เดิม"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_VALIDATE_ENTRIES,
   "เมื่อเปิดใช้งาน รายการในเพลย์ลิสต์ที่มีอยู่เดิมจะได้รับการตรวจสอบก่อนที่จะเริ่มการสแกนเนื้อหาใหม่ โดยรายการที่อ้างอิงถึงเนื้อหาที่สูญหาย และ/หรือไฟล์ที่มีนามสกุลไม่ถูกต้[...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_START,
   "เริ่มสแกน"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_START,
   "สแกนเนื้อหาที่เลือกไว้"
   )

/* Explore tab */
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_INITIALISING_LIST,
   "กำลังเริ่มต้นรายการ..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_CATEGORY_RELEASE_YEAR,
   "ปีที่วางขาย"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_CATEGORY_PLAYER_COUNT,
   "จำนวนผู้เล่น"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_CATEGORY_REGION,
   "ภูมิภาค"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_CATEGORY_TAG,
   "แท็ก"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_SEARCH_NAME,
   "ค้นหาชื่อ ..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_SHOW_ALL,
   "แสดงทั้งหมด"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_ADDITIONAL_FILTER,
   "ตัวกรองเพิ่มเติม"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_ALL,
   "ทั้งหมด"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_ADD_ADDITIONAL_FILTER,
   "ตัวกรองเพิ่มเติม"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_ITEMS_COUNT,
   "%u รายการ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_DEVELOPER,
   "ตามผู้พัฒนา"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_PUBLISHER,
   "ตามผู้จำหน่าย"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_RELEASE_YEAR,
   "ตามปีที่เปิดตัว"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_PLAYER_COUNT,
   "ตามจำนวนผู้เล่น"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_GENRE,
   "ตามประเภทเนื้อหา"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_ACHIEVEMENTS,
   "ตามความสำเร็จ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_CATEGORY,
   "ตามประเภท"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_LANGUAGE,
   "ตามภาษา"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_REGION,
   "ตามภูมิภาค"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_CONSOLE_EXCLUSIVE,
   "ตามเฉพาะคอนโซล"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_PLATFORM_EXCLUSIVE,
   "ตามเฉพาะแพลตฟอร์ม"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_RUMBLE,
   "ตามการสั่น"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_SCORE,
   "ตามคะแนน"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_MEDIA,
   "ตามสื่อ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_CONTROLS,
   "ตามการควบคุม"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_ARTSTYLE,
   "ตาม Artstyle"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_GAMEPLAY,
   "ตามเกมเพลย์"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_NARRATIVE,
   "ตามเนื้อเรื่อง"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_PACING,
   "ตามจังหวะดำเนินเรื่อง"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_PERSPECTIVE,
   "ตามมุมมองของผู้เล่น"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_SETTING,
   "ตามฉากหลัง"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_VISUAL,
   "ตามงานภาพ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_VEHICULAR,
   "ตามพาหนะ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_ORIGIN,
   "ตามแหล่งที่มา"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_FRANCHISE,
   "ตาม​แฟรนไชส์"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_TAG,
   "ตามแท็ก"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_SYSTEM_NAME,
   "ตามชื่อระบบ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_EXPLORE_RANGE_FILTER,
   "ตั้งค่าตัวกรองช่วงข้อมูล"
   )
MSG_HASH(
   MENU_ENUM_LABEL_EXPLORE_VIEW,
   "มุมมอง"
   )
MSG_HASH(
   MENU_ENUM_LABEL_EXPLORE_SAVE_VIEW,
   "บันทึกเป็นมุมมอง"
   )
MSG_HASH(
   MENU_ENUM_LABEL_EXPLORE_DELETE_VIEW,
   "ลบมุมมองนี้"
   )
MSG_HASH(
   MENU_ENUM_LABEL_EXPLORE_NEW_VIEW,
   "ป้อนชื่อของมุมมองใหม่"
   )
MSG_HASH(
   MENU_ENUM_LABEL_EXPLORE_VIEW_EXISTS,
   "มีมุมมองชื่อนี้อยู่แล้ว"
   )
MSG_HASH(
   MENU_ENUM_LABEL_EXPLORE_VIEW_SAVED,
   "บันทึกมุมมองแล้ว"
   )
MSG_HASH(
   MENU_ENUM_LABEL_EXPLORE_VIEW_DELETED,
   "ลบมุมมองแล้ว"
   )

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
   MENU_ENUM_SUBLABEL_DELETE_ENTRY,
   "ลบรายการนี้ออกจากเพลย์ลิสต์"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_TO_FAVORITES_PLAYLIST,
   "เพิ่มในรายการโปรด"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ADD_TO_FAVORITES_PLAYLIST,
   "เพิ่มเนื้อหาไปที่ 'รายการโปรด'"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_TO_PLAYLIST,
   "เพิ่มลงในเพลย์ลิสต์"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ADD_TO_PLAYLIST,
   "เพิ่มเนื้อหาลงในเพลย์ลิสต์"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CREATE_NEW_PLAYLIST,
   "สร้างเพลย์ลิสต์ใหม่"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CREATE_NEW_PLAYLIST,
   "สร้างเพลย์ลิสต์ใหม่และเพิ่มรายการปัจจุบันลงไป"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SET_CORE_ASSOCIATION,
   "กำหนด Core"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SET_CORE_ASSOCIATION,
   "กำหนดคอร์ที่เกี่ยวข้องกับเนื้อหานี้"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RESET_CORE_ASSOCIATION,
   "ล้าง Core ที่เลือกไว้"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RESET_CORE_ASSOCIATION,
   "รีเซ็ต Core ที่เกี่ยวข้องกับเนื้อหานี้"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INFORMATION,
   "ข้อมูล"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INFORMATION,
   "ดูข้อมูลเพิ่มเติมเกี่ยวกับเนื้อหาชิ้นนี้"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DOWNLOAD_PL_ENTRY_THUMBNAILS,
   "ดาวน์โหลดรูปตัวอย่าง"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DOWNLOAD_PL_ENTRY_THUMBNAILS,
   "ดาวน์โหลดรูปภาพสกรีนช็อต/หน้าปก/ภาพหน้าจอชื่อเกม สำหรับเนื้อหาปัจจุบัน และอัปเดตไฟล์เดิมที่มีอยู่เดิม"
   )

/* Playlist Item > Set Core Association */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DETECT_CORE_LIST_OK_CURRENT_CORE,
   "Core ปัจจุบัน"
   )

/* Playlist Item > Information */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_INFO_LABEL,
   "ชื่อ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_INFO_PATH,
   "เส้นทางไฟล์"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_INFO_ENTRY_IDX,
   "รายการ: %lu/%lu"
   )
MSG_HASH( /* FIXME Unused? */
   MENU_ENUM_LABEL_VALUE_CONTENT_INFO_RUNTIME,
   "เล่นไปแล้ว"
   )
MSG_HASH( /* FIXME Unused? */
   MENU_ENUM_LABEL_VALUE_CONTENT_INFO_LAST_PLAYED,
   "เล่นล่าสุด"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_INFO_DATABASE,
   "ฐานข้อมูล"
   )

/* Quick Menu */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RESUME_CONTENT,
   "ดำเนินการต่อ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RESUME_CONTENT,
   "กลับไปยังเนื้อหาและออกจาก เมนูด่วน"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RESTART_CONTENT,
   "รีเซ็ต"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RESTART_CONTENT,
   "เปิดใช้งานการ Soft Reset ส่วนปุ่ม Start ของ RetroPad จะเป็นการ Trigger การ Hard Reset"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOSE_CONTENT,
   "ปิดเนื้อหา"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOSE_CONTENT,
   "ปิดเนื้อหา การเปลี่ยนแปลงที่ยังไม่ได้บันทึกอาจสูญหายได้"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TAKE_SCREENSHOT,
   "จับภาพหน้าจอ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_TAKE_SCREENSHOT,
   "บันทึกภาพหน้าจอ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STATE_SLOT,
   "ช่องบันทึก"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_STATE_SLOT,
   "เปลี่ยนช่องบันทึกที่เลือกอยู่ในขณะนี้"
   )
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
   MENU_ENUM_LABEL_VALUE_LOAD_STATE,
   "โหลดสถานะ"
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
   MENU_ENUM_LABEL_VALUE_UNDO_LOAD_STATE,
   "เลิกโหลดสถานะ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_UNDO_LOAD_STATE,
   "หากมีการโหลดสถานะ เนื้อหาจะย้อนกลับไปยังสถานะก่อนที่จะทำการโหลดนั้น"
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
   MENU_ENUM_LABEL_VALUE_REPLAY_SLOT,
   "ช่องบันทึกรีเพลย์"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REPLAY_SLOT,
   "เปลี่ยนช่องบันทึกที่เลือกอยู่ในขณะนี้"
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
   MENU_ENUM_SUBLABEL_HALT_REPLAY,
   "หยุดการบันทึก/เล่นย้อนหลัง รีเพลย์ ปัจจุบัน"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_TO_FAVORITES,
   "เพิ่มในรายการโปรด"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ADD_TO_FAVORITES,
   "เพิ่มเนื้อหาไปยัง 'รายการโปรด'"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_START_RECORDING,
   "เริ่มการบันทึก"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_START_RECORDING,
   "เริ่มการบันทึกวิดีโอ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_STOP_RECORDING,
   "หยุดการบันทึก"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_STOP_RECORDING,
   "หยุดการบันทึกวิดีโอ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_START_STREAMING,
   "เริ่มสตรีมมิ่ง"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_START_STREAMING,
   "เริ่มสตรีมไปยังปลายทางที่เลือกไว้"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_STOP_STREAMING,
   "หยุดสตรีมมิ่ง"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_STOP_STREAMING,
   "สิ้นสุดการสตรีม"
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
   MENU_ENUM_LABEL_VALUE_CORE_OPTIONS,
   "ตัวเลือก Core"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_OPTIONS,
   "เปลี่ยนตัวเลือกสำหรับเนื้อหาดังกล่าว"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INPUT_REMAPPING_OPTIONS,
   "การควบคุม"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_INPUT_REMAPPING_OPTIONS,
   "เปลี่ยนตัวเลือกสำหรับเนื้อหาดังกล่าว"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_CHEAT_OPTIONS,
   "สูตรโกง"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_CHEAT_OPTIONS,
   "ตั้งค่าสูตรโกง"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISK_OPTIONS,
   "การควบคุมแผ่น"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DISK_OPTIONS,
   "จัดการรูปภาพแผ่นดิสก์"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SHADER_OPTIONS,
   "ตั้งค่า Shader เพื่อเพิ่มคุณภาพของการแสดงผลทางภาพ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_OVERRIDE_OPTIONS,
   "แทนที่"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_OVERRIDE_OPTIONS,
   "ตัวเลือกสำหรับการเขียนทับการกำหนดค่าส่วนกลาง"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_LIST,
   "ความสำเร็จ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACHIEVEMENT_LIST,
   "ดูความสำเร็จ และการตั้งค่าที่เกี่ยวข้อง"
   )

/* Quick Menu > Options */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_OPTION_OVERRIDE_LIST,
   "จัดการตัวเลือก Core"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_OPTION_OVERRIDE_LIST,
   "บันทึกหรือลบการแทนที่ตัวเลือกสำหรับเนื้อหาปัจจุบัน"
   )

/* Quick Menu > Options > Manage Core Options */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GAME_SPECIFIC_CORE_OPTIONS_CREATE,
   "บันทึกตัวเลือกสำหรับเกม"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GAME_SPECIFIC_CORE_OPTIONS_CREATE,
   "บันทึกตัวเลือก Core สำหรับเนื้อหาปัจจุบันเท่านั้น"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GAME_SPECIFIC_CORE_OPTIONS_REMOVE,
   "ลบตัวเลือกสำหรับเกม"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GAME_SPECIFIC_CORE_OPTIONS_REMOVE,
   "ลบตัวเลือก Core สำหรับเนื้อหาปัจจุบันเท่านั้น"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FOLDER_SPECIFIC_CORE_OPTIONS_CREATE,
   "บันทึกตัวเลือกสำหรับโฟลเดอร์เนื้อหา"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FOLDER_SPECIFIC_CORE_OPTIONS_CREATE,
   "บันทึกตัวเลือก Core สำหรับเนื้อหาทั้งหมดที่โหลดจากโฟลเดอร์เดียวกับไฟล์ปัจจุบัน"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FOLDER_SPECIFIC_CORE_OPTIONS_REMOVE,
   "ลบตัวเลือกสำหรับโฟลเดอร์เนื้อหา"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FOLDER_SPECIFIC_CORE_OPTIONS_REMOVE,
   "ลบตัวเลือก Core สำหรับเนื้อหาทั้งหมดที่โหลดจากโฟลเดอร์เดียวกับไฟล์ปัจจุบัน"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_OPTION_OVERRIDE_INFO,
   "ไฟล์ตัวเลือกที่ใช้งานอยู่"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_OPTION_OVERRIDE_INFO,
   "ไฟล์ตัวเลือกปัจจุบันที่ใช้งานอยู่"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_OPTIONS_RESET,
   "รีเซ็ตตัวเลือก Core ทั้งหมด"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_OPTIONS_RESET,
   "ตั้งค่าตัวเลือกทั้งหมดของ Core ปัจจุบันเป็นค่าเริ่มต้น"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_OPTIONS_FLUSH,
   "เขียนตัวเลือกลงดิสก์"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_OPTIONS_FLUSH,
   "บังคับให้การตั้งค่าปัจจุบันถูกเขียนลงในไฟล์ตัวเลือกที่ใช้งานอยู่ เพื่อให้มั่นใจว่าตัวเลือกจะถูกเก็บรักษาไว้ในกรณีที่เกิดบั๊กในคอร์ซึ่งทำให้ส่วนหน้า (frontend) ปิดตัวลงอย่างไม่[...]"
   )

/* Quick Menu > Controls */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_MANAGER_LIST,
   "จัดการไฟล์การเปลี่ยนปุ่ม"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_FILE_MANAGER_LIST,
   "โหลด บันทึก หรือลบไฟล์การเปลี่ยนปุ่ม สำหรับเนื้อหาปัจจุบัน"
   )

/* Quick Menu > Controls > Manage Remap Files */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_INFO,
   "ไฟล์การเปลี่ยนปุ่มที่ใช้งานอยู่"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_FILE_INFO,
   "ไฟล์การเปลี่ยนปุ่มปัจจุบันที่ใช้งานอยู่"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_LOAD,
   "โหลดไฟล์การเปลี่ยนปุ่ม"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_FILE_LOAD,
   "โหลดและแทนที่การกำหนดปุ่มปัจจุบัน"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_SAVE_AS,
   "บันทึกไฟล์การเปลี่ยนปุ่มเป็น..."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_FILE_SAVE_AS,
   "บันทึกการกำหนดปุ่มปัจจุบันเป็นไฟล์การเปลี่ยนปุ่มใหม่"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_SAVE_CORE,
   "บันทึกไฟล์การเปลี่ยนปุ่มของ Core"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_FILE_SAVE_CORE,
   "บันทึกไฟล์การเปลี่ยนปุ่มของ Core ซึ่งจะมีผลกับเนื้อหาทั้งหมดที่เปิดด้วย Core นี้"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_REMOVE_CORE,
   "ลบไฟล์การเปลี่ยนปุ่มของ Core"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_FILE_REMOVE_CORE,
   "ลบไฟล์การเปลี่ยนปุ่ม ที่มีผลกับเนื้อหาทั้งหมดที่เปิดด้วย Core นี้"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_SAVE_CONTENT_DIR,
   "บันทึกไฟล์การเปลี่ยนปุ่มของโฟลเดอร์เนื้อหา"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_FILE_SAVE_CONTENT_DIR,
   "บันทึกไฟล์การเปลี่ยนปุ่ม ซึ่งจะมีผลกับเนื้อหาทั้งหมดที่โหลดมาจากโฟลเดอร์เดียวกันกับไฟล์ปัจจุบัน"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_REMOVE_CONTENT_DIR,
   "ลบไฟล์การเปลี่ยนปุ่มของโฟลเดอร์เนื้อหาเกม"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_FILE_REMOVE_CONTENT_DIR,
   "ลบไฟล์การเปลี่ยนปุ่ม ที่มีผลกับเนื้อหาทั้งหมดที่โหลดมาจากโฟลเดอร์เดียวกันกับไฟล์ปัจจุบัน"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_SAVE_GAME,
   "บันทึกไฟล์การเปลี่ยนปุ่มของเกม"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_FILE_SAVE_GAME,
   "บันทึกไฟล์การเปลี่ยนปุ่มซึ่งจะมีผลกับเนื้อหาปัจจุบันเท่านั้น"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_REMOVE_GAME,
   "ลบไฟล์การเปลี่ยนปุ่มของเกม"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_FILE_REMOVE_GAME,
   "ลบไฟล์การเปลี่ยนปุ่มที่มีผลกับเนื้อหาปัจจุบันเท่านั้น"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_RESET,
   "รีเซ็ตการกำหนดปุ่มอินพุต"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_FILE_RESET,
   "ตั้งค่าตัวเลือกการเปลี่ยนปุ่มอินพุตทั้งหมดเป็นค่าเริ่มต้น"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_FLUSH,
   "อัปเดตไฟล์การเปลี่ยนปุ่มอินพุต"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_FILE_FLUSH,
   "เขียนทับไฟล์การเปลี่ยนปุ่มที่ใช้งานอยู่ด้วยตัวเลือกการเปลี่ยนปุ่มอินพุตปัจจุบัน"
   )

/* Quick Menu > Controls > Manage Remap Files > Load Remap File */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE,
   "ไฟล์การเปลี่ยนปุ่ม"
   )

/* Quick Menu > Cheats */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_START_OR_CONT,
   "เริ่มหรือดำเนินการค้นหาสูตรโกงต่อ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_START_OR_CONT,
   "สแกนหน่วยความจำเพื่อสร้างสูตรโกงใหม่"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_FILE_LOAD,
   "โหลดไฟล์สูตรโกง (แทนที่)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_FILE_LOAD,
   "โหลดไฟล์สูตรโกงและแทนที่ตัวโกงที่มีอยู่เดิม"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_FILE_LOAD_APPEND,
   "โหลดไฟล์สูตรโกง (เพิ่มต่อท้าย)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_FILE_LOAD_APPEND,
   "โหลดไฟล์สูตรโกงและเพิ่มเข้าไปในรายการสูตรโกงที่มีอยู่เดิม"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_RELOAD_CHEATS,
   "โหลดสูตรโกงเฉพาะเกมใหม่อีกครั้ง"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_RELOAD_CHEATS,
   "โหลดสูตรโกงเฉพาะเกมใหม่อีกครั้ง"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_FILE_SAVE_AS,
   "บันทึกไฟล์สูตรโกงเป็น"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_FILE_SAVE_AS,
   "บันทึกสูตรโกงปัจจุบันเป็นไฟล์สูตรโกง"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_ADD_NEW_TOP,
   "เพิ่มสูตรโกงใหม่ไปที่ด้านบนสุด"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_ADD_NEW_TOP,
   "เพิ่มสูตรโกงไปที่ด้านบนสุดของรายการ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_ADD_NEW_BOTTOM,
   "เพิ่มสูตรโกงใหม่ไปที่ด้านล่างสุด"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_ADD_NEW_BOTTOM,
   "เพิ่มสูตรโกงต่อท้ายรายการที่มีอยู่เดิม"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_DELETE_ALL,
   "ลบสูตรโกงทั้งหมด"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_DELETE_ALL,
   "ล้างรายการสูตรโกง"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_APPLY_AFTER_LOAD,
   "ใช้สูตรโกงอัตโนมัติเมื่อโหลดเกม"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_APPLY_AFTER_LOAD,
   "ใช้สูตรโกงโดยอัตโนมัติเมื่อโหลดเกม"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_APPLY_AFTER_TOGGLE,
   "ใช้หลังจากเปิด/ปิดสูตรโกง"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_APPLY_AFTER_TOGGLE,
   "ปรับใช้ Cheat ทันทีหลังจากสลับเปิดใช้งาน"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_APPLY_CHANGES,
   "ปรับใช้การเปลี่ยนแปลง"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_APPLY_CHANGES,
   "การเปลี่ยนแปลง Cheat จะมีผลทันที"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT,
   "สูตรโกง"
   )

/* Quick Menu > Cheats > Start or Continue Cheat Search */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_START_OR_RESTART,
   "เริ่มหรือเริ่มใหม่การค้นหา Cheat"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_START_OR_RESTART,
   "กด ซ้าย หรือ ขวา เพื่อเปลี่ยนขนาด Bit"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_EXACT,
   "ค้นหาค่าในหน่วยความจำ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_SEARCH_EXACT,
   "กด ซ้าย หรือ ขวา เพื่อเปลี่ยนค่า"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_SEARCH_EXACT_VAL,
   "เท่ากับ %u (%X)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_LT,
   "ค้นหาค่าในหน่วยความจำ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_SEARCH_LT_VAL,
   "น้อยกว่าก่อนหน้านี้"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_LTE,
   "ค้นหาค่าในหน่วยความจำ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_SEARCH_LTE_VAL,
   "น้อยกว่าหรือเท่ากับก่อนหน้านี้"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_GT,
   "ค้นหาค่าในหน่วยความจำ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_SEARCH_GT_VAL,
   "มากกว่าก่อนหน้านี้"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_GTE,
   "ค้นหาค่าในหน่วยความจำ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_SEARCH_GTE_VAL,
   "มากกว่าหรือเท่ากับก่อนหน้านี้"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_EQ,
   "ค้นหาค่าในหน่วยความจำ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_SEARCH_EQ_VAL,
   "เท่ากับก่อนหน้านี้"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_NEQ,
   "ค้นหาค่าในหน่วยความจำ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_SEARCH_NEQ_VAL,
   "ไม่เท่ากับก่อนหน้านี้"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_EQPLUS,
   "ค้นหาค่าในหน่วยความจำ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_SEARCH_EQPLUS,
   "กด ซ้าย หรือ ขวา เพื่อเปลี่ยนค่า"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_SEARCH_EQPLUS_VAL,
   "เท่ากับก่อนหน้า +%u (%X)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_EQMINUS,
   "ค้นหาค่าในหน่วยความจำ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_SEARCH_EQMINUS,
   "กด ซ้าย หรือ ขวา เพื่อเปลี่ยนค่า"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_SEARCH_EQMINUS_VAL,
   "เท่ากับก่อนหน้า -%u (%X)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_ADD_MATCHES,
   "เพิ่ม %u รายการที่ตรงกันลงในรายการ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_DELETE_MATCH,
   "ลบรายการที่ตรงกัน #"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_COPY_MATCH,
   "สร้างรายการที่ตรงกัน #"
   )

/* Quick Menu > Cheats > Load Cheat File (Replace) */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_FILE,
   "ไฟล์สูตรโกง (แทนที่)"
   )

/* Quick Menu > Cheats > Load Cheat File (Append) */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_FILE_APPEND,
   "ไฟล์สูตรโกง (เขียนต่อท้าย)"
   )

/* Quick Menu > Cheats > Cheat Details */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_DETAILS_SETTINGS,
   "รายละเอียดสูตรโกง"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_IDX,
   "หมายเลข"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_IDX,
   "ลำดับของสูตรโกงในรายการ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_STATE,
   "เปิดการใช้งาน"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_DESC,
   "รายละเอียด"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_HANDLER,
   "ตัวดำเนินการ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_MEMORY_SEARCH_SIZE,
   "ขนาดการค้นหาหน่วยความจำ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_TYPE,
   "ประเภท"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_VALUE,
   "ค่า"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_ADDRESS,
   "ที่อยู่หน่วยความจำ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_BROWSE_MEMORY,
   "เรียกดูที่อยู่: %08X"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_ADDRESS_BIT_POSITION,
   "Maskที่อยู่หน่วยความจำ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_ADDRESS_BIT_POSITION,
   "ที่อยู่ bitmask เมื่อขนาดการค้นหาหน่วยความจำ < 8-bit"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_REPEAT_COUNT,
   "จำนวนรอบของการทำงาน"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_REPEAT_COUNT,
   "จำนวนรอบที่สูตรโกงจะทำงาน ใช้ร่วมกับตัวเลือก 'รอบการทำงาน' อีกสองตัวเพื่อส่งผลต่อพื้นที่ขนาดใหญ่ในหน่วยความจำ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_REPEAT_ADD_TO_ADDRESS,
   "เพิ่มที่อยู่หน่วยความจำในแต่ละรอบของการทำงาน"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_REPEAT_ADD_TO_ADDRESS,
   "หลังจากการทำงานแต่ละรอบ 'ที่อยู่หน่วยความจำ' จะเพิ่มขึ้นตามจำนวนเท่าของ 'ขนาดการค้นหาหน่วยความจำ' โดยคำนวณจากค่านี้"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_REPEAT_ADD_TO_VALUE,
   "เพิ่มค่าที่อยู่หน่วยความจำในแต่ละรอบของการทำงาน"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_REPEAT_ADD_TO_VALUE,
   "หลังจากการทำงานแต่ละรอบ 'ค่า' จะเพิ่มขึ้นตามจำนวนนี้"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_RUMBLE_TYPE,
   "สั่นเมื่อหน่วยความจำมีการเปลี่ยนแปลง"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_RUMBLE_VALUE,
   "ค่าการสั่น"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_RUMBLE_PORT,
   "พอร์ตการสั่น"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_RUMBLE_PRIMARY_STRENGTH,
   "ความแรงหลักของการสั่น"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_RUMBLE_PRIMARY_DURATION,
   "ระยะเวลาหลักของการสั่น (ms)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_RUMBLE_SECONDARY_STRENGTH,
   "ความแรงรองของการสั่น"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_RUMBLE_SECONDARY_DURATION,
   "ระยะเวลารองของการสั่น (ms)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_CODE,
   "รหัส"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_ADD_NEW_AFTER,
   "เพิ่มสูตรโกงใหม่หลังจากนี้"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_ADD_NEW_BEFORE,
   "เพิ่มสูตรโกงใหม่ก่อนหน้าหน้าชุดนี้"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_COPY_AFTER,
   "คัดลอกสูตรโกงนี้ไว้หลังจากนี้"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_COPY_BEFORE,
   "คัดลอกสูตรโกงนี้ไว้ก่อนหน้าชุดนี้"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_DELETE,
   "ลบสูตรโกงนี้ออก"
   )

/* Quick Menu > Disc Control */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISK_INDEX,
   "ลำดับแผ่นปัจจุบัน"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DISK_INDEX,
   "เลือกแผ่นดิสก์ปัจจุบันจากรายการภาพที่พร้อมใช้งาน โดยถาดดิสก์เสมือนสามารถปิดอยู่ได้ตามปกติ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISK_TRAY_EJECT,
   "เอาแผ่นออก"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DISK_TRAY_EJECT,
   "เปิดถาดดิสก์เสมือน"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISK_TRAY_INSERT,
   "ใส่แผ่นดิสก์"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DISK_TRAY_INSERT,
   "ปิดถาดดิสก์เสมือน"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISK_IMAGE_APPEND,
   "โหลดแผ่นดิสก์ใหม่"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DISK_IMAGE_APPEND,
   "เลือกแผ่นดิสก์ใหม่จากระบบไฟล์และเพิ่มเข้าไปในรายการลำดับแผ่น\nหมายเหตุ: นี่เป็นฟีเจอร์แบบเก่า ขอแนะนำให้ใช้ไฟล์เพลย์ลิสต์ M3U สำหรับเกมที่มีหลายแผ่นแทน"
   )

/* Quick Menu > Shaders */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADERS_ENABLE,
   "วิดีโอเชดเดอร์"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADERS_ENABLE,
   "เปิดใช้งานไปป์ไลน์เชดเดอร์วิดีโอ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHADER_WATCH_FOR_CHANGES,
   "ติดตามการเปลี่ยนแปลงของไฟล์เชดเดอร์"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SHADER_WATCH_FOR_CHANGES,
   "ใช้การเปลี่ยนแปลงที่ทำกับไฟล์เชดเดอร์บนดิสก์โดยอัตโนมัติ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_SHADER_WATCH_FOR_CHANGES,
   "ติดตามการเปลี่ยนแปลงของไฟล์เชดเดอร์ เมื่อมีการบันทึกการเปลี่ยนแปลงของไฟล์เชดเดอร์บนดิสก์ ไฟล์จะถูกคอมไพล์ใหม่และนำไปใช้กับเนื้อหาโดยอัตโนมัติ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_REMEMBER_LAST_DIR,
   "จดจำโฟลเดอร์เชดเดอร์ที่ใช้งานล่าสุด"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_REMEMBER_LAST_DIR,
   "เปิดตัวเลือกไฟล์ในโฟลเดอร์ที่ใช้งานล่าสุด เมื่อโหลดค่าเชดเดอร์ที่ตั้งไว้ล่วงหน้าและระดับการประมวลผลเชดเดอร์"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET,
   "โหลดค่าที่ตั้งไว้ล่วงหน้า"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET,
   "โหลดค่าเชดเดอร์ที่ตั้งไว้ล่วงหน้า ระบบประมวลผลเชดเดอร์จะถูกตั้งค่าโดยอัตโนมัติ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_SHADER_PRESET,
   "โหลดค่าเชดเดอร์ที่ตั้งไว้ล่วงหน้าโดยตรง เมนูเชดเดอร์จะได้รับการอัปเดตตามความเหมาะสมปัจจัยการปรับขนาดที่แสดงในเมนูจะเชื่อถือได้ก็ต่อเมื่อค่าที่ตั้งไว้ล่วงหน้านั้นใช้วิธ[...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_PREPEND,
   "เพิ่มพรีเซ็ตไว้ข้างหน้า"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_PREPEND,
   "เพิ่มพรีเซ็ตไว้ข้างหน้าพรีเซ็ตที่โหลดอยู่ในปัจจุบัน"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_APPEND,
   "เพิ่มค่าล่วงหน้าต่อท้าย"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_APPEND,
   "พิ่มพรีเซ็ตต่อท้ายพรีเซ็ตที่โหลดอยู่ในปัจจุบัน"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_MANAGER,
   "จัดการพรีเซ็ต"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_MANAGER,
   "บันทึกหรือเอาพรีเซ็ตเชดเดอร์ออก"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_FILE_INFO,
   "ไฟล์พรีเซ็ตที่ใช้งานอยู่"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_FILE_INFO,
   "พรีเซ็ตเชดเดอร์ที่กำลังใช้งานอยู่ในปัจจุบัน"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHADER_APPLY_CHANGES,
   "ปรับใช้การเปลี่ยนแปลง"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SHADER_APPLY_CHANGES,
   "การเปลี่ยนแปลงการตั้งค่าเชเดอร์จะมีผลทันที ใช้ตัวเลือกนี้หากคุณมีการเปลี่ยนจำนวนรอบของเชเดอร์, การกรองแสง, การปรับขนาด FBO และอื่นๆ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_SHADER_APPLY_CHANGES,
   "หลังจากเปลี่ยนการตั้งค่าเชดเดอร์ เช่น จำนวนการประมวลผลเชดเดอร์ การกรอง หรือการปรับขนาด FBO ให้ใช้ตัวเลือกนี้เพื่อใช้การเปลี่ยนแปลง\nการเปลี่ยนการตั้งค่าเชดเดอร์เหล่านี้เป็นกา[...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PARAMETERS,
   "พารามิเตอร์เชดเดอร์"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PARAMETERS,
   "แก้ไขเชดเดอร์ที่ใช้งานอยู่โดยตรง การเปลี่ยนแปลงจะไม่ถูกบันทึกลงในไฟล์พรีเซ็ต"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_NUM_PASSES,
   "การประมวลผลเชดเดอร์"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_NUM_PASSES,
   "เพิ่มหรือลดจำนวนการประมวลผลในเชดเดอร์พายป์ไลน์ เชดเดอร์แต่ละตัวสามารถถูกกำหนดให้ทำงานในแต่ละขั้นตอนของพายป์ไลน์ และสามารถปรับขนาดและการกรองได้"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_SHADER_NUM_PASSES,
   "RetroArch อนุญาตให้ผสมและจับคู่เชดเดอร์ต่าง ๆ กับจำนวนการประมวลผลเชดเดอร์ที่กำหนดเอง พร้อมตัวกรองฮาร์ดแวร์และตัวปรับสเกลที่กำหนดเอง\nตัวเลือกนี้ใช้เพื่อระบุจำนวนการประมวลผลเชดเ[...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHADER,
   "เชดเดอร์"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_SHADER_PASS,
   "เส้นทางไปยังเชดเดอร์ เชดเดอร์ทั้งหมดต้องเป็นชนิดเดียวกัน (เช่น Cg, GLSL หรือ Slang) ตั้งค่าไดเรกทอรีเชดเดอร์เพื่อกำหนดตำแหน่งที่เบราว์เซอร์จะเริ่มค้นหาเชดเดอร์"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FILTER,
   "ตัวกรอง"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_SHADER_FILTER_PASS,
   "ตัวกรองฮาร์ดแวร์สำหรับการประมวลผลนี้ หากตั้งค่าเป็น ‘เริ่มต้น’ ตัวกรองจะถูกกำหนดเป็น ‘เชิงเส้น’ หรือ ‘ใกล้เคียงที่สุด’ โดยขึ้นอยู่กับการตั้งค่า ‘การกรองแบบไบไลเนียร์’ ในการ[...]"
  )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCALE,
   "อัตราส่วน"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_SHADER_SCALE_PASS,
   "การปรับสเกลสำหรับการประมวลผลนี้ ค่าการปรับสเกลจะสะสม เช่น 2x สำหรับการประมวลผลแรก และ 2x สำหรับการประมวลผลที่สอง จะได้ผลรวมเป็น 4x\nหากมีการปรับสเกลสำหรับการประมวลผลสุดท้าย ผลลัพ[...]"
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_REFERENCE,
   "พรีเซ็ตแบบง่าย"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_SAVE_REFERENCE,
   "บันทึกพรีเซ็ตเชดเดอร์ที่มีการเชื่อมโยงไปยังพรีเซ็ตต้นฉบับที่ถูกโหลด และรวมเฉพาะการเปลี่ยนแปลงของพารามิเตอร์ที่คุณได้แก้ไขไว้เท่านั้น"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_CURRENT,
   "บันทึกพรีเซ็ตปัจจุบัน"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_SAVE_CURRENT,
   "บันทึกพรีเซ็ตเชดเดอร์ปัจจุบัน"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_AS,
   "บันทึกพรีเซ็ตเป็น"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_SAVE_AS,
   "บันทึกการตั้งค่าเชดเดอร์ปัจจุบันเป็นพรีเซ็ตเชดเดอร์ใหม่"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_GLOBAL,
   "บันทึกพรีเซ็ตทั่วโลก"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_SAVE_GLOBAL,
   "บันทึกการตั้งค่าเชดเดอร์ปัจจุบันเป็นการตั้งค่าเริ่มต้นทั่วโลก"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_CORE,
   "บันทึกพรีเซ็ต"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_SAVE_CORE,
   "บันทึกการตั้งค่า Shader ปัจจุบันเป็นค่าเริ่มต้นสำหรับ Core นี้"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_PARENT,
   "บันทึกพรีเซ็ตไดเรกทอรีเนื้อหา"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_SAVE_PARENT,
   "บันทึกการตั้งค่า Shader ปัจจุบันเป็นค่าเริ่มต้นสำหรับไฟล์ทั้งหมดในไดเรกทอรีเนื้อหาปัจจุบัน"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_GAME,
   "บันทึกพรีเซ็ตเกม"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_SAVE_GAME,
   "บันทึกการตั้งค่า Shader ปัจจุบันเป็นค่าเริ่มต้นสำหรับเนื้อหา"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_PRESETS_FOUND,
   "ไม่พบพรีเซ็ต Shader อัตโนมัติ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_REMOVE_GLOBAL,
   "ลบพรีเซ็ต Global"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_REMOVE_GLOBAL,
   "ลบพรีเซ็ต ทั่วโลก ที่ใช้กับเนื้อหาและ Core ทั้งหมด"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_REMOVE_CORE,
   "ลบพรีเซ็ต Core"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_REMOVE_CORE,
   "ลบพรีเซ็ต Core ที่ใช้กับเนื้อหาทั้งหมดที่ทำงานด้วย Core ที่โหลดอยู่ในปัจจุบัน"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_REMOVE_PARENT,
   "ลบพรีเซ็ตไดเรกทอรีเนื้อหา"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_REMOVE_PARENT,
   "ลบพรีเซ็ตไดเรกทอรีเนื้อหา ที่ใช้กับเนื้อหาทั้งหมดภายในไดเรกทอรีที่กำลังทำงานอยู่"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_REMOVE_GAME,
   "ลบค่าพรีเซ็ตสำหรับเกมนี้ออก"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_REMOVE_GAME,
   "ลบค่าที่ตั้งไว้ล่วงหน้าสำหรับเกม ใช้สำหรับเกมที่ระบุไว้เท่านั้น"
   )

/* Quick Menu > Shaders > Shader Parameters */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_SHADER_PARAMETERS,
   "ไม่มีพารามิเตอร์เชดเดอร์"
   )

/* Quick Menu > Overrides */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERRIDE_FILE_INFO,
   "ไฟล์เขียนทับที่ใช้งานอยู่"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERRIDE_FILE_INFO,
   "ไฟล์เขียนทับที่กำลังใช้งานอยู่"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERRIDE_FILE_LOAD,
   "โหลดไฟล์เขียนทับ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERRIDE_FILE_LOAD,
   "โหลดและแทนที่การกำหนดค่าปัจจุบัน"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERRIDE_FILE_SAVE_AS,
   "บันทึกการเขียนทับเป็น"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERRIDE_FILE_SAVE_AS,
   "บันทึกการกำหนดค่าปัจจุบันเป็นไฟล์เขียนทับใหม่"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVE_CURRENT_CONFIG_OVERRIDE_CORE,
   "บันทึกการเขียนทับ Core"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVE_CURRENT_CONFIG_OVERRIDE_CORE,
   "บันทึกไฟล์การกำหนดค่าการเขียนทับซึ่งจะใช้กับเนื้อหาทั้งหมดที่โหลดด้วย Core นี้ จะมีความสำคัญเหนือกว่าการกำหนดค่าหลัก"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMOVE_CURRENT_CONFIG_OVERRIDE_CORE,
   "ลบการเขียนทับ Core"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMOVE_CURRENT_CONFIG_OVERRIDE_CORE,
   "ลบไฟล์การกำหนดค่าการเขียนทับซึ่งจะใช้กับเนื้อหาทั้งหมดที่โหลดด้วย Core นี้"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVE_CURRENT_CONFIG_OVERRIDE_CONTENT_DIR,
   "บันทึกการเขียนทับไดเรกทอรีเนื้อหา"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVE_CURRENT_CONFIG_OVERRIDE_CONTENT_DIR,
   "บันทึกไฟล์การกำหนดค่าการเขียนทับ ซึ่งจะใช้กับเนื้อหาทั้งหมดที่โหลดจากไดเรกทอรีเดียวกันกับไฟล์ปัจจุบัน จะมีความสำคัญเหนือกว่าการกำหนดค่าหลัก"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMOVE_CURRENT_CONFIG_OVERRIDE_CONTENT_DIR,
   "ลบการเขียนทับไดเรกทอรีเนื้อหา"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMOVE_CURRENT_CONFIG_OVERRIDE_CONTENT_DIR,
   "ลบไฟล์การกำหนดค่าการเขียนทับ ซึ่งจะใช้กับเนื้อหาทั้งหมดที่โหลดจากไดเรกทอรีเดียวกันกับไฟล์ปัจจุบัน"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVE_CURRENT_CONFIG_OVERRIDE_GAME,
   "บันทึกการเขียนทับเกม"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVE_CURRENT_CONFIG_OVERRIDE_GAME,
   "บันทึกไฟล์การกำหนดค่าการเขียนทับ ซึ่งจะใช้สำหรับเนื้อหาปัจจุบันเท่านั้น จะมีความสำคัญเหนือกว่าการกำหนดค่าหลัก"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMOVE_CURRENT_CONFIG_OVERRIDE_GAME,
   "ลบการเขียนทับเกม"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMOVE_CURRENT_CONFIG_OVERRIDE_GAME,
   "ลบไฟล์การกำหนดค่าการเขียนทับซึ่งจะใช้สำหรับเนื้อหาปัจจุบันเท่านั้น"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERRIDE_UNLOAD,
   "ยกเลิกการโหลดการเขียนทับ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERRIDE_UNLOAD,
   "รีเซ็ตตัวเลือกทั้งหมดเป็นค่าการกำหนดค่าทั่วโลก"
   )

/* Quick Menu > Achievements */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_ACHIEVEMENTS_TO_DISPLAY,
   "ไม่มีความสำเร็จที่จะแสดง"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_PAUSE_CANCEL,
   "ยกเลิกการหยุดความสำเร็จชั่วคราวในโหมด Hardcore"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACHIEVEMENT_PAUSE_CANCEL,
   "เปิดโหมดความสำเร็จ Hardcore ทิ้งไว้สำหรับเซสชันปัจจุบัน"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_RESUME_CANCEL,
   "ยกเลิกการกลับมาใช้โหมดความสำเร็จ Hardcore ต่อ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACHIEVEMENT_RESUME_CANCEL,
   "ปล่อยให้โหมดความสำเร็จ Hardcore ถูกปิดใช้งานสำหรับเซสชันปัจจุบัน"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_RESUME_REQUIRES_RELOAD,
   "ปิดการใช้งานโหมดความสำเร็จ Hardcore ต่อไป"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACHIEVEMENT_RESUME_REQUIRES_RELOAD,
   "คุณต้องโหลด Core ใหม่อีกครั้งเพื่อกลับเข้าสู่โหมดความสำเร็จ Hardcore ต่อไป"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_PAUSE,
   "หยุดความสำเร็จชั่วคราวในโหมด Hardcore"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACHIEVEMENT_PAUSE,
   "ระงับโหมดฮาร์ดคอร์สำหรับ ความสำเร็จ ในเซสชันปัจจุบัน การดำเนินการนี้จะเปิดใช้งานสูตรโกง, การย้อนกลับ, สโลว์โมชัน และการโหลดบันทึกสถานะ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_RESUME,
   "กลับมาใช้โหมดความสำเร็จ Hardcore ต่อ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACHIEVEMENT_RESUME,
   "กลับเข้าสู่โหมดฮาร์ดคอร์สำหรับ ความสำเร็จ ในเซสชันปัจจุบัน การดำเนินการนี้จะปิดใช้งานสูตรโกง, การย้อนกลับ, สโลว์โมชัน และการโหลดบันทึกสถานะ พร้อมทั้งเริ่มเกมใหม่ทันที"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_SERVER_UNREACHABLE,
   "เซิร์ฟเวอร์ RetroAchievements ไม่สามารถติดต่อได้"
)
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACHIEVEMENT_SERVER_UNREACHABLE,
   "มีหนึ่งความสำเร็จหรือมากกว่านั้นที่ไม่สามารถส่งไปยังเซิร์ฟเวอร์ได้ จะมีการพยายามส่งใหม่อีกครั้งตราบเท่าที่คุณยังเปิดแอปทิ้งไว้"
)
MSG_HASH(
   MENU_ENUM_LABEL_CHEEVOS_SERVER_DISCONNECTED,
   "เซิร์ฟเวอร์ RetroAchievements ไม่สามารถติดต่อได้ จะพยายามใหม่อีกครั้งจนกว่าจะสำเร็จหรือจนกว่าแอปจะถูกปิด"
)
MSG_HASH(
   MENU_ENUM_LABEL_CHEEVOS_SERVER_RECONNECTED,
   "คำขอที่ค้างอยู่ทั้งหมดได้รับการซิงค์กับเซิร์ฟเวอร์ RetroAchievements สำเร็จแล้ว"
)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_IDENTIFYING_GAME,
   "กำลังระบุตัวตนของเกม"
)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_FETCHING_GAME_DATA,
   "กำลังดึงข้อมูลเกม"
)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_STARTING_SESSION,
   "กำลังเริ่มเซสชัน"
)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOT_LOGGED_IN,
   "ไม่ได้เข้าสู่ระบบ"
)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_ERROR,
   "เครือข่ายขัดข้อง"
)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UNKNOWN_GAME,
   "เกมที่ไม่รู้จัก"
)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CANNOT_ACTIVATE_ACHIEVEMENTS_WITH_THIS_CORE,
   "ไม่สามารถเปิดใช้งานความสำเร็จกับ Core นี้ได้"
)

/* Quick Menu > Information */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_DETAIL,
   "ข้อมูลในฐานข้อมูล"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RDB_ENTRY_DETAIL,
   "แสดงข้อมูลฐานข้อมูลสำหรับเนื้อหาปัจจุบัน"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_ENTRIES_TO_DISPLAY,
   "ไม่มีข้อมูลที่จะแสดง"
   )

/* Miscellaneous UI Items */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_CORES_AVAILABLE,
   "ไม่มี Core ที่พร้อมใช้งาน"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_CORE_OPTIONS_AVAILABLE,
   "ตอนนี้ไม่มีตัวเลือกสำหรับ Core"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_CORE_INFORMATION_AVAILABLE,
   "ตอนนี้ไม่มีข้อมูล Core"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_CORE_BACKUPS_AVAILABLE,
   "ไม่พบการสำรองข้อมูลพรีเซ็ต Core"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_FAVORITES_AVAILABLE,
   "ไม่มีรายการโปรดที่ใช้งานได้"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_HISTORY_AVAILABLE,
   "ไม่พบประวัติ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_IMAGES_AVAILABLE,
   "ไม่มีรูปภาพที่ใช้งานได้"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_MUSIC_AVAILABLE,
   "ไม่พบเพลง"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_VIDEOS_AVAILABLE,
   "ไม่พบวิดีโอ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_INFORMATION_AVAILABLE,
   "ไม่พบข้อมูล"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_PLAYLIST_ENTRIES_AVAILABLE,
   "ไม่พบรายการเพลย์ลิสต์"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_SETTINGS_FOUND,
   "ไม่พบการตั้งค่า"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_BT_DEVICES_FOUND,
   "ไม่พบอุปกรณ์บลูทูธ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_NETWORKS_FOUND,
   "ไม่พบเครือข่าย"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_CORE,
   "ไม่มี Core"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SEARCH,
   "ค้นหา"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CYCLE_THUMBNAILS,
   "วนภาพตัวอย่าง"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RANDOM_SELECT,
   "สุ่มเลือก"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_BACK,
   "ย้อนกลับ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_OK,
   "ตกลง"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PARENT_DIRECTORY,
   "ไดเรกทอรีหลัก"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_PARENT_DIRECTORY,
   "ย้อนกลับไปยังไดเรกทอรีหลัก"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DIRECTORY_NOT_FOUND,
   "ไม่พบไดเรกทอรี"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_ITEMS,
   "ไม่พบรายการ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SELECT_FILE,
   "เลือกไฟล์"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ROTATION_NORMAL,
   "ปกติ"
   )

/* Settings Options */

MSG_HASH( /* FIXME Should be MENU_LABEL_VALUE */
   MSG_UNKNOWN_COMPILER,
   "ไม่ทราบคอมไพเลอร์"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_DIGITAL_OR,
   "แชร์"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_DIGITAL_XOR,
   "แย่งสิทธิ์ยึดควบคุม"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_DIGITAL_VOTE,
   "โหวต"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_ANALOG,
   "การแชร์สัญญาณอินพุตแบบอนาล็อก"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_ANALOG_MAX,
   "สูงสุด"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_ANALOG_AVERAGE,
   "เฉลี่ย"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_NONE,
   "ไม่แชร์อินพุต"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_NO_PREFERENCE,
   "ไม่กำหนดวิธีเฉพาะ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_TICKER_TYPE_BOUNCE,
   " เด้งไปทางซ้าย/ขวา"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_TICKER_TYPE_LOOP,
   "เลื่อนทางซ้าย"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_IMAGE_MODE,
   "โหมดภาพ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_SPEECH_MODE,
   "โหมดเสียง"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_NARRATOR_MODE,
   "โหมดผู้บรรยาย"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_ENTRY_REMOVE_ENABLE_HIST_FAV,
   "ประวัติและรายการโปรด"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_ENTRY_REMOVE_ENABLE_ALL,
   "เพลย์ลิสต์ทั้งหมด"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_ENTRY_REMOVE_ENABLE_NONE,
   "ปิด"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_INLINE_CORE_DISPLAY_HIST_FAV,
   "ประวัติและรายการโปรด"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_INLINE_CORE_DISPLAY_ALWAYS,
   "ตลอดเวลา"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_INLINE_CORE_DISPLAY_NEVER,
   "ไม่เลย"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_RUNTIME_PER_CORE,
   "แยกตาม Core"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_RUNTIME_AGGREGATE,
   "รวม"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE_CHARGED,
   "ชาร์จ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE_CHARGING,
   "กำลังชาร์จ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE_DISCHARGING,
   "เลิกชาร์จ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE_NO_SOURCE,
   "ไม่มีแหล่ง"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_USE_THIS_DIRECTORY,
   "เลือกสิ่งนี้เพื่อตั้งค่าเป็นไดเรกทอรี"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RETROPAD_WITH_ANALOG,
   "RetroPad แบบมีอนาล็อก"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NONE,
   "ไม่แชร์อินพุต"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UNKNOWN,
   "ไม่ทราบ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DOWN_Y_L_R,
   "ลง + Y + L1 + R1"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HOLD_START,
   "กด Start ค้างไว้ (2 วินาที)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HOLD_SELECT,
   "กด Select ค้างไว้ (2 วินาที)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DOWN_SELECT,
   "ลง + Select"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_CHANGES,
   "เปลี่ยน"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_DOES_NOT_CHANGE,
   "ไม่เปลี่ยน"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_INCREASE,
   "เพิ่มขึ้น"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_DECREASE,
   "ลดลง"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_EQ_VALUE,
   "= ค่าการสั่น"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_NEQ_VALUE,
   "!= ค่าการสั่น"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_LT_VALUE,
   "< ค่าการสั่น"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_GT_VALUE,
   "> ค่าการสั่น"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_INCREASE_BY_VALUE,
   "เพิ่มขึ้นตามค่าการสั่น"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_DECREASE_BY_VALUE,
   "ลดลงตามค่าการสั่น"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_PORT_16,
   "ทั้งหมด"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_TYPE_SET_TO_VALUE,
   "เลือกค่า"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_TYPE_INCREASE_VALUE,
   "เพิ่มตามค่า"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_TYPE_DECREASE_VALUE,
   "ลดตามค่า"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_TYPE_RUN_NEXT_IF_EQ,
   "รันสูตรโกงถัดไปถ้าค่า = หน่วยความจำ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_TYPE_RUN_NEXT_IF_NEQ,
   "รันสูตรโกงถัดไปถ้าค่า != หน่วยความจำ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_TYPE_RUN_NEXT_IF_LT,
   "รันสูตรโกงถัดไปถ้าค่า < หน่วยความจำ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_TYPE_RUN_NEXT_IF_GT,
   "ใช้สูตรถัดไปหาก ค่า > หน่วยความจำ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_HANDLER_TYPE_EMU,
   "โปรแกรมจำลอง"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_MEMORY_SIZE_1,
   "1-Bit, ค่าสูงสุด = 0x01"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_MEMORY_SIZE_2,
   "2-Bit, ค่าสูงสุด = 0x03"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_MEMORY_SIZE_4,
   "4-Bit, ค่าสูงสุด = 0x0F"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_MEMORY_SIZE_8,
   "8-Bit, ค่าสูงสุด = 0xFF"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_MEMORY_SIZE_16,
   "16-Bit, ค่าสูงสุด = 0xFFFF"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_MEMORY_SIZE_32,
   "32-Bit, ค่าสูงสุด = 0xFFFFFFFF"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_SORT_MODE_DEFAULT,
   "ค่าเริ่มต้นของระบบ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_SORT_MODE_ALPHABETICAL,
   "ตามอักษร"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_SORT_MODE_OFF,
   "ไม่แชร์อินพุต"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_DEFAULT,
   "แสดงป้ายกำกับเต็ม"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_REMOVE_PARENS,
   "ลบ () เนื้อหา"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_REMOVE_BRACKETS,
   "ลบ [] เนื้อหา"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_REMOVE_PARENS_AND_BRACKETS,
   "ลบ () และ []"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_KEEP_REGION,
   "คงภูมิภาคเดิมไว้"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_KEEP_DISC_INDEX,
   "คงหมายเลขแผ่นดิสก์ไว้"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_KEEP_REGION_AND_DISC_INDEX,
   "คงภูมิภาคและหมายเลขแผ่นดิสก์ไว้"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_THUMBNAIL_MODE_DEFAULT,
   "ค่าเริ่มต้นของระบบ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_THUMBNAIL_MODE_BOXARTS,
   "ปกกล่องเกม"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_THUMBNAIL_MODE_SCREENSHOTS,
   "ภาพหน้าจอ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_THUMBNAIL_MODE_TITLE_SCREENS,
   "หน้าเริ่มเกม"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_THUMBNAIL_MODE_LOGOS,
   "โลโก้เนื้อหา"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCROLL_NORMAL,
   "ปกติ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCROLL_FAST,
   "เร็ว"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ON,
   "เปิด"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OFF,
   "ปิด"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_YES,
   "ใช่"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO,
   "ไม่"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TRUE,
   "จริง"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FALSE,
   "เท็จ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ENABLED,
   "เปิดการใช้งาน"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISABLED,
   "ปิดใช้งาน"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_LOCKED_ENTRY,
   "ถูกล็อก"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_UNLOCKED_ENTRY,
   "ปลดล็อก"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_UNLOCKED_ENTRY_HARDCORE,
   "ฮาร์ดคอร์"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_UNOFFICIAL_ENTRY,
   "ไม่เป็นทางการ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_UNSUPPORTED_ENTRY,
   "ไม่รองรับ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_RECENTLY_UNLOCKED_ENTRY,
   "ปลดล็อกล่าสุด"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_ALMOST_THERE_ENTRY,
   "เกือบสำเร็จแล้ว"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_ACTIVE_CHALLENGES_ENTRY,
   "ความท้าที่กำลังดำเนินอยู่"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_TRACKERS_ONLY,
   "เฉพาะตัวติดตามเท่านั้น"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_NOTIFICATIONS_ONLY,
   "เฉพาะการแจ้งเตือนเท่านั้น"
)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DONT_CARE,
   "ค่าเริ่มต้น"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LINEAR,
   "เส้นตรง"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NEAREST,
   "ใกล้ที่สุด"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MAIN,
   "หลัก"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT,
   "เนื้อหา"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCAN_METHOD_AUTO,
   "อัตโนมัติเต็มรูปแบบ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCAN_METHOD_CUSTOM,
   "กำหนดเอง"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCAN_USE_DB_STRICT,
   "เข้มงวด"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCAN_USE_DB_LOOSE,
   "ไม่เข้มงวด"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCAN_USE_DB_CUSTOM_DAT,
   "กำหนดเอง DAT (เข้มงวด)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCAN_USE_DB_CUSTOM_DAT_LOOSE,
   "กำหนดเอง DAT (ไม่เข้มงวด)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCAN_USE_DB_NONE,
   "ไม่แชร์อินพุต"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LEFT_ANALOG,
   "อนาล็อกซ้าย"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LEFT_ANALOG_FORCED,
   "อนาล็อกซ้าย (บังคับ)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RIGHT_ANALOG,
   "อนาล็อกขวา"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RIGHT_ANALOG_FORCED,
   "อนาล็อกขวา (บังคับ)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LEFTRIGHT_ANALOG,
   "อนาล็อกซ้าย + อนาล็อกขวา"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LEFTRIGHT_ANALOG_FORCED,
   "อนาล็อกซ้าย + อนาล็อกขวา (บังคับ)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TWINSTICK_ANALOG,
   "อนาล็อกแบบสองแท่ง"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TWINSTICK_ANALOG_FORCED,
   "อนาล็อกแบบสองแท่ง (บังคับ)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_KEY,
   "คีย์ %s"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_LEFT,
   "เมาส์ 1"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_RIGHT,
   "เมาส์ 2"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_MIDDLE,
   "เมาส์ 3"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_BUTTON4,
   "เมาส์ 4"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_BUTTON5,
   "เมาส์ 5"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_WHEEL_UP,
   "เลื่อนลูกกลิ้งเมาส์ขึ้น"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_WHEEL_DOWN,
   "เลื่อนลูกกลิ้งเมาส์ลง"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_HORIZ_WHEEL_UP,
   "เลื่อนลูกกลิ้งเมาส์ซ้าย"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_HORIZ_WHEEL_DOWN,
   "เลื่อนลูกกลิ้งเมาส์ขวา"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_POLL_TYPE_BEHAVIOR_EARLY,
   "เร็ว"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_POLL_TYPE_BEHAVIOR_NORMAL,
   "ปกติ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_POLL_TYPE_BEHAVIOR_LATE,
   "ล่าช้า"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_AGO,
   "เมื่อไม่นานมานี้"
   )

/* RGUI: Settings > User Interface > Appearance */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_BACKGROUND_FILLER_THICKNESS_ENABLE,
   "ความหนาของส่วนเติมพื้นหลัง"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_BACKGROUND_FILLER_THICKNESS_ENABLE,
   "เพิ่มความหยาบของลวดลายตารางหมากรุกที่พื้นหลังเมนู"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_BORDER_FILLER_ENABLE,
   "ส่วนเติมขอบ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_BORDER_FILLER_THICKNESS_ENABLE,
   "ความหนาของส่วนเติมขอบ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_BORDER_FILLER_THICKNESS_ENABLE,
   "เพิ่มความหยาบของลวดลายตารางหมากรุกที่ขอบเมนู"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_BORDER_FILLER_ENABLE,
   "แสดงขอบเมนู"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_FULL_WIDTH_LAYOUT,
   "ใช้รูปแบบความกว้างเต็มหน้าจอ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_FULL_WIDTH_LAYOUT,
   "ปรับขนาดและตำแหน่งของรายการเมนูเพื่อให้ใช้พื้นที่หน้าจอที่มีอยู่ให้เกิดประโยชน์สูงสุด ปิดการใช้งานนี้เพื่อใช้รูปแบบสองคอลัมน์ที่มีความกว้างคงที่แบบคลาสสิก"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_LINEAR_FILTER,
   "ตัวกรองแบบ Linear"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_LINEAR_FILTER,
   "เพิ่มความเบลอเล็กน้อยให้กับเมนูเพื่อช่วยลดความคมของขอบพิกเซลให้ดูนุ่มนวลขึ้น"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_INTERNAL_UPSCALE_LEVEL,
   "การปรับขนาดภายใน"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_INTERNAL_UPSCALE_LEVEL,
   "ปรับขนาดอินเทอร์เฟซเมนูให้ใหญ่ขึ้นก่อนที่จะแสดงผลบนหน้าจอ เมื่อใช้งานร่วมกับการเปิด 'ตัวกรองแบบ Linear สำหรับเมนู' จะช่วยขจัดความผิดเพี้ยนจากการขยายภาพ (พิกเซลไม่เท่ากัน) ในขณะที[...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_ASPECT_RATIO,
   "อัตราส่วนภาพ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_ASPECT_RATIO,
   "เลือกอัตราส่วนภาพของเมนู โดยอัตราส่วนแบบจอกว้างจะช่วยเพิ่มความละเอียดในแนวนอนของอินเทอร์เฟซเมนู (อาจต้องเริ่มระบบใหม่หากปิดการใช้งาน 'ล็อกอัตราส่วนภาพของเมนู')"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_ASPECT_RATIO_LOCK,
   "ล็อคอัตราส่วนภาพ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_ASPECT_RATIO_LOCK,
   "เพื่อให้แน่ใจว่าเมนูจะแสดงผลด้วยอัตราส่วนภาพที่ถูกต้องเสมอ หากปิดการใช้งานส่วนนี้ เมนูทางลัด จะถูกยืดออกเพื่อให้พอดีกับเนื้อหาที่กำลังโหลดอยู่ในขณะนั้น"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME,
   "ธีมสี"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RGUI_MENU_COLOR_THEME,
   "เลือกธีมสีที่แตกต่างออกไป การเลือก 'กำหนดเอง' จะช่วยให้สามารถใช้ไฟล์ค่าล่วงหน้าของธีมเมนูได้"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_THEME_PRESET,
   "ไฟล์ค่าล่วงหน้าของธีมที่กำหนดเอง"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RGUI_MENU_THEME_PRESET,
   "เลือกไฟล์ค่าล่วงหน้าของธีมเมนูจากโปรแกรมเลือกไฟล์"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_TRANSPARENCY,
   "ความโปร่งใส"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_TRANSPARENCY,
   "เปิดใช้งานการแสดงผลเนื้อหาที่พื้นหลังในขณะที่เมนูทางลัด (Quick Menu) ทำงานอยู่ การปิดใช้งานความโปร่งใสอาจทำให้สีของธีมเปลี่ยนไป"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_SHADOWS,
   "เอฟเฟกต์เงา"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_SHADOWS,
   "เปิดใช้งานเงาสำหรับข้อความเมนู ขอบ และภาพตัวอย่าง (Thumbnails) ส่งผลต่อประสิทธิภาพเล็กน้อย"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_PARTICLE_EFFECT,
   "แอนิเมชันพื้นหลัง"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_PARTICLE_EFFECT,
   "เปิดใช้งานเอฟเฟกต์แอนิเมชันอนุภาค ที่พื้นหลัง ส่งผลต่อประสิทธิภาพอย่างมาก"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_PARTICLE_EFFECT_SPEED,
   "ความเร็วแอนิเมชันพื้นหลัง"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_PARTICLE_EFFECT_SPEED,
   "ปรับความเร็วของเอฟเฟกต์แอนิเมชันอนุภาค ที่พื้นหลัง"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_PARTICLE_EFFECT_SCREENSAVER,
   "แอนิเมชันพื้นหลังของโหมดพักหน้าจอ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_PARTICLE_EFFECT_SCREENSAVER,
   "แสดงผลเอฟเฟกต์แอนิเมชันอนุภาค ที่พื้นหลังในขณะที่โหมดพักหน้าจอ ของเมนูทำงานอยู่"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_INLINE_THUMBNAILS,
   "แสดงภาพตัวอย่างของเพลย์ลิสต์"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_INLINE_THUMBNAILS,
   "เปิดใช้งานการแสดงภาพตัวอย่างขนาดเล็ก ในขณะที่ดูเพลย์ลิสต์ สามารถสลับการแสดงผลได้ด้วยปุ่ม RetroPad Select เมื่อปิดการใช้งาน คุณยังสามารถสลับไปดูภาพตัวอย่างแบบเต็มหน้าจอได้ด้วยปุ่ม Retro[...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_THUMBNAILS_RGUI,
   " รูปตัวอย่างด้านบน"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_THUMBNAILS_RGUI,
   "ประเภทของรูปตัวอย่างที่แสดงด้านขวาบนของเพลย์ลิสต์ สามารถสลับเปลี่ยนได้ด้วยการดันอนาล็อกขวา ขึ้น/ซ้าย"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LEFT_THUMBNAILS_RGUI,
   "รูปตัวอย่างด้านล่าง"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LEFT_THUMBNAILS_RGUI,
   "ประเภทของรูปตัวอย่างที่แสดงด้านขวาล่างของเพลย์ลิสต์ สามารถสลับเปลี่ยนได้ด้วยการดันอนาล็อกขวา ลง/ขวา"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_SWAP_THUMBNAILS,
   "สลับรูปตัวอย่าง"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_SWAP_THUMBNAILS,
   "สลับตำแหน่งการแสดงผลของ 'รูปตัวอย่างด้านบน' และ 'รูปตัวอย่างด้านล่าง'"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_THUMBNAIL_DOWNSCALER,
   "วิธีการย่อขนาดรูปตัวอย่าง"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_THUMBNAIL_DOWNSCALER,
   "รูปแบบการสุ่มตัวอย่างใหม่ที่ใช้เมื่อย่อขนาดรูปตัวอย่างที่มีขนาดใหญ่ให้พอดีกับหน้าจอ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_THUMBNAIL_DELAY,
   "หน่วงเวลาการแสดงรูปตัวอย่าง (ms)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_THUMBNAIL_DELAY,
   "กำหนดค่าการหน่วงเวลาระหว่างการเลือกรายชื่อในเพลย์ลิสต์กับการโหลดรูปตัวอย่างที่เกี่ยวข้อง การตั้งค่านี้ไว้อย่างน้อย 256 ms จะช่วยให้เลื่อนดูรายชื่อได้อย่างรวดเร็วโดยไม่กระตุ[...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_EXTENDED_ASCII,
   "การรองรับ ASCII แบบขยาย"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_EXTENDED_ASCII,
   "เปิดใช้งานการแสดงผลตัวอักษร ASCII แบบที่ไม่ใช่มาตรฐาน จำเป็นต้องใช้เพื่อความเข้ากันได้กับบางภาษาในแถบตะวันตกที่ไม่ใช่ภาษาอังกฤษ มีผลกระทบต่อประสิทธิภาพการทำงานในระดับปานกลา[...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_SWITCH_ICONS,
   "สลับไอค่อน"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_SWITCH_ICONS,
   "ใช้ไอคอนแทนข้อความ เปิด/ปิด เพื่อแสดงรายการตั้งค่าเมนูแบบ 'สวิตช์สลับ'"
   )

/* RGUI: Settings Options */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_THUMB_SCALE_POINT,
   "จุดที่ใกล้ที่สุด (เร็ว)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_THUMB_SCALE_SINC,
   "Sinc/Lanczos3 (ช้า)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_UPSCALE_NONE,
   "ไม่แชร์อินพุต"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_UPSCALE_AUTO,
   "อัตโนมัติ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_16_9_CENTRE,
   "16:9 (จัดวางกึ่งกลาง)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_16_10_CENTRE,
   "16:10 (จัดวางกึ่งกลาง)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_21_9_CENTRE,
   "21:9 (จัดวางกึ่งกลาง)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_3_2_CENTRE,
   "3:2 (จัดวางกึ่งกลาง)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_5_3_CENTRE,
   "5:3 (จัดวางกึ่งกลาง)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_AUTO,
   "อัตโนมัติ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_LOCK_NONE,
   "ปิด"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_LOCK_FIT_SCREEN,
   "ปรับให้พอดีหน้าจอ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_LOCK_FILL_SCREEN,
   "เต็มหน้าจอ (ยืด)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CUSTOM,
   "กำหนดเอง"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_PARTICLE_EFFECT_NONE,
   "ปิด"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_PARTICLE_EFFECT_SNOW,
   "หิมะ (ตกปรอยๆ)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_PARTICLE_EFFECT_SNOW_ALT,
   "หิมะ (ตกหนัก)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_PARTICLE_EFFECT_RAIN,
   "ฝนตก"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_PARTICLE_EFFECT_VORTEX,
   "วังวน"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_PARTICLE_EFFECT_STARFIELD,
   "ทุ่งดวงดาว"
   )

/* XMB: Settings > User Interface > Appearance */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LEFT_THUMBNAILS,
   "รูปตัวอย่างรอง"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LEFT_THUMBNAILS,
   "ประเภทของภาพตัวอย่างที่จะแสดงทางด้านซ้าย"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ICON_THUMBNAILS,
   "ภาพตัวอย่างไอคอน"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ICON_THUMBNAILS,
   "ประเภทของภาพตัวอย่างไอคอนเพลย์ลิสต์ที่จะแสดงผล"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DYNAMIC_WALLPAPER,
   "พื้นหลังแบบไดนามิก"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DYNAMIC_WALLPAPER,
   "โหลดวอลเปเปอร์ใหม่แบบไดนามิกตามบริบทที่ใช้งานอยู่"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_HORIZONTAL_ANIMATION,
   "แอนิเมชันแนวนอน"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_HORIZONTAL_ANIMATION,
   "เปิดใช้งานแอนิเมชันแนวนอนสำหรับเมนู ซึ่งจะส่งผลต่อประสิทธิภาพการทำงานของเครื่อง"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_XMB_ANIMATION_HORIZONTAL_HIGHLIGHT,
   "แอนิเมชันไฮไลต์ไอคอนแนวนอน"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_XMB_ANIMATION_HORIZONTAL_HIGHLIGHT,
   "แอนิเมชันที่เกิดขึ้นเมื่อเลื่อนเปลี่ยนระหว่างแท็บต่างๆ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_XMB_ANIMATION_MOVE_UP_DOWN,
   "แอนิเมชันเคลื่อนที่ ขึ้น/ลง"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_XMB_ANIMATION_MOVE_UP_DOWN,
   "แอนิเมชันที่เกิดขึ้นเมื่อเคลื่อนที่ขึ้นหรือลง"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_XMB_ANIMATION_OPENING_MAIN_MENU,
   "แอนิเมชันเปิด/ปิดเมนูหลัก"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_XMB_ANIMATION_OPENING_MAIN_MENU,
   "แอนิเมชันที่เกิดขึ้นเมื่อเปิดเมนูย่อย"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_ALPHA_FACTOR,
   "ค่าอัลฟ่าของธีมสี"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_CURRENT_MENU_ICON,
   "ไอคอนเมนูปัจจุบัน"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_XMB_CURRENT_MENU_ICON,
   "ไอคอนเมนูปัจจุบันสามารถซ่อนได้ โดยจะแสดงอยู่ใต้เมนูแนวนอนหรือในชื่อส่วนหัวแทน"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_CURRENT_MENU_ICON_NONE,
   "ไม่แชร์อินพุต"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_CURRENT_MENU_ICON_NORMAL,
   "ปกติ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_CURRENT_MENU_ICON_TITLE,
   "ชื่อ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_FONT,
   "แบบอักษร"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_XMB_FONT,
   "เลือกฟอนต์หลักอื่นเพื่อใช้ในเมนู"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_FONT_COLOR_RED,
   "สีตัวอักษร (แดง)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_FONT_COLOR_GREEN,
   "สีตัวอักษร (เขียว)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_FONT_COLOR_BLUE,
   "สีตัวอักษร (น้ำเงิน)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_LAYOUT,
   "การจัดวาง"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_XMB_LAYOUT,
   "เลือกรูปแบบเลย์เอาต์ที่แตกต่างออกไปสำหรับอินเทอร์เฟซ XMB"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_THEME,
   "ชุดไอคอน"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_XMB_THEME,
   "เลือกธีมไอคอนที่แตกต่างออกไปสำหรับ RetroArch"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_SWITCH_ICONS,
   "สลับไอค่อน"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_XMB_SWITCH_ICONS,
   "ใช้ไอคอนแทนข้อความ เปิด/ปิด เพื่อแสดงรายการตั้งค่าเมนูแบบ 'สวิตช์สลับ'"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_SHADOWS_ENABLE,
   "เอฟเฟกต์เงา"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_XMB_SHADOWS_ENABLE,
   "วาดเงาตกกระทบสำหรับไอคอน รูปตัวอย่างเกม และตัวอักษร ซึ่งจะส่งผลกระทบต่อประสิทธิภาพเพียงเล็กน้อย"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_XMB_RIBBON_ENABLE,
   "เลือกเอฟเฟกต์ภาพพื้นหลังแบบเคลื่อนไหว ซึ่งอาจใช้ทรัพยากร GPU สูงขึ้นอยู่กับเอฟเฟกต์ที่เลือก หากประสิทธิภาพไม่เป็นที่น่าพอใจ ให้ปิดการใช้งานหรือเปลี่ยนกลับไปใช้เอฟเฟกต์ที่เร[...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME,
   "ธีมสี"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_XMB_MENU_COLOR_THEME,
   "เลือกธีมสีพื้นหลังอื่น"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_VERTICAL_THUMBNAILS,
   "การจัดวางรูปตัวอย่างเกมในแนวตั้ง"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_XMB_VERTICAL_THUMBNAILS,
   "แสดงรูปตัวอย่างเกมด้านซ้ายไว้ใต้รูปด้านขวา บริเวณฝั่งขวาของหน้าจอ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_XMB_THUMBNAIL_SCALE_FACTOR,
   "อัตราส่วนรูปตัวอย่างเกม"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_XMB_THUMBNAIL_SCALE_FACTOR,
   "ลดขนาดการแสดงผลรูปตัวอย่างเกมโดยการย่อความกว้างสูงสุดที่อนุญาตให้น้อยลง"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_XMB_VERTICAL_FADE_FACTOR,
   "ค่าการจางหายในแนวตั้ง"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_XMB_SHOW_TITLE_HEADER,
   "แสดงส่วนหัวชื่อเรื่อง"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_XMB_TITLE_MARGIN,
   "ระยะขอบชื่อเรื่อง"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_XMB_TITLE_MARGIN_HORIZONTAL_OFFSET,
   "ระยะห่างของขอบชื่อเรื่องในแนวนอน"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MAIN_MENU_ENABLE_SETTINGS,
   "เปิดการใช้งานแถบการตั้งค่า"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_XMB_MAIN_MENU_ENABLE_SETTINGS,
   "แสดงแถบการตั้งค่าซึ่งประกอบด้วยการตั้งค่าต่าง ๆ ของโปรแกรม"
   )

/* XMB: Settings Options */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHADER_PIPELINE_RIBBON,
   "ริบบิ้น"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHADER_PIPELINE_RIBBON_SIMPLIFIED,
   "ริบบิ้น (แบบย่อ)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHADER_PIPELINE_SIMPLE_SNOW,
   "หิมะแบบเรียบง่าย"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHADER_PIPELINE_SNOW,
   "หิมะ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHADER_PIPELINE_BOKEH,
   "โบเก้"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHADER_PIPELINE_SNOWFLAKE,
   "เกล็ดหิมะ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_CUSTOM,
   "กำหนดเอง"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_MONOCHROME,
   "ขาวดำ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_MONOCHROME_INVERTED,
   "ขาวดำแบบย้อนกลับ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_AUTOMATIC,
   "อัตโนมัติ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_AUTOMATIC_INVERTED,
   "สลับสีอัตโนมัติแบบย้อนกลับ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_DARK,
   "มืด"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_LIGHT,
   "สว่าง"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_SUNBEAM,
   "ลำแสงอาทิตย์"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_PLAIN,
   "ภาพพื้นหลัง"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_UNDERSEA,
   "ใต้ทะเล"
   )

/* Ozone: Settings > User Interface > Appearance */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_FONT,
   "แบบอักษร"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_FONT,
   "เลือกแบบอักษรหลักแบบอื่นเพื่อใช้ในเมนู"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_FONT_SCALE,
   "ขนาดตัวอักษร"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_FONT_SCALE,
   "กำหนดว่าควรจะมีการปรับขนาดตัวอักษรในเมนูแยกต่างหากหรือไม่ และควรปรับขนาดแบบพร้อมกันทั้งหมดหรือแยกปรับตามส่วนต่างๆ ของเมนู"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_FONT_SCALE_GLOBAL,
   "ทั่วโลก"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_FONT_SCALE_SEPARATE,
   "แยกค่าปรับแต่งจากกัน"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_FONT_SCALE_FACTOR_GLOBAL,
   "ปรับขนาดตัวอักษร"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_FONT_SCALE_FACTOR_GLOBAL,
   "ปรับขนาดตัวอักษรแบบเชิงเส้นทั่วทั้งเมนู"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_FONT_SCALE_FACTOR_TITLE,
   "ปรับขนาดตัวอักษรของชื่อ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_FONT_SCALE_FACTOR_TITLE,
   "ปรับขนาดตัวอักษรสำหรับข้อความชื่อเรื่องในส่วนหัวของเมนู"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_FONT_SCALE_FACTOR_SIDEBAR,
   "ปรับขนาดตัวอักษรของแถบด้านข้างซ้าย"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_FONT_SCALE_FACTOR_SIDEBAR,
   "ปรับขนาดตัวอักษรสำหรับข้อความในแถบด้านข้างซ้าย"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_FONT_SCALE_FACTOR_LABEL,
   "ปรับขนาดตัวอักษรของป้ายกำกับ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_FONT_SCALE_FACTOR_LABEL,
   "ปรับขนาดตัวอักษรสำหรับป้ายกำกับของตัวเลือกเมนูและรายการเพลย์ลิสต์ รวมถึงมีผลต่อขนาดตัวอักษรในกล่องช่วยเหลือด้วย"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_FONT_SCALE_FACTOR_SUBLABEL,
   "ปรับขนาดตัวอักษรของป้ายกำกับย่อย"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_FONT_SCALE_FACTOR_SUBLABEL,
   "ปรับขนาดตัวอักษรสำหรับป้ายกำกับย่อยของตัวเลือกเมนูและรายการเพลย์ลิสต์"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_FONT_SCALE_FACTOR_TIME,
   "ปรับขนาดตัวอักษรของวันที่และเวลา"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_FONT_SCALE_FACTOR_TIME,
   "ปรับขนาดตัวอักษรของตัวบ่งชี้วันที่และเวลาที่มุมบนขวาของเมนู"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_FONT_SCALE_FACTOR_FOOTER,
   "ปรับขนาดตัวอักษรของส่วนท้ายเมนู"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_FONT_SCALE_FACTOR_FOOTER,
   "ปรับขนาดตัวอักษรของข้อความในส่วนท้ายเมนู รวมถึงมีผลต่อขนาดตัวอักษรในแถบด้านข้างรูปภาพตัวอย่างทางด้านขวาด้วย"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLLAPSE_SIDEBAR,
   "พับแถบด้านข้างลง"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_COLLAPSE_SIDEBAR,
   "ทำให้แถบด้านข้างพับเก็บอยู่เสมอ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_TRUNCATE_PLAYLIST_NAME,
   "ตัดชื่อรายการเพลย์ลิสต์ให้สั้นลง (ต้องรีสตาร์ท)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_TRUNCATE_PLAYLIST_NAME,
   "เอาชื่อผู้ผลิตออกจากเพลย์ลิสต์ เช่น จาก 'Sony - PlayStation' จะกลายเป็น 'PlayStation'"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_SORT_AFTER_TRUNCATE_PLAYLIST_NAME,
   "เรียงลำดับเพลย์ลิสต์หลังจากตัดชื่อให้สั้นลง (ต้องเริ่มโปรแกรมใหม่)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_SORT_AFTER_TRUNCATE_PLAYLIST_NAME,
   "เพลย์ลิสต์จะถูกเรียงลำดับใหม่ตามตัวอักษร หลังจากตัดส่วนที่เป็นชื่อผู้ผลิตออกจากชื่อเพลย์ลิสต์แล้ว"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LEFT_THUMBNAILS_OZONE,
   "รูปตัวอย่างรอง"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LEFT_THUMBNAILS_OZONE,
   "แทนที่แผงข้อมูลเมตาของเนื้อหาด้วยรูปภาพตัวอย่างอื่น"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_SCROLL_CONTENT_METADATA,
   "ใช้ข้อความวิ่งสำหรับ Content Metadata"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_SCROLL_CONTENT_METADATA,
   "เมื่อเปิดใช้งาน ข้อมูลเมตาแต่ละรายการของเนื้อหาที่แสดงบนแถบด้านข้างขวาของเพลย์ลิสต์ (เช่น Core ที่ใช้, เวลาที่เล่น) จะแสดงผลเพียงบรรทัดเดียว โดยข้อความที่ยาวเกินความกว้างของแถบ[...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_THUMBNAIL_SCALE_FACTOR,
   "ปรับขนาดรูปภาพตัวอย่าง"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_THUMBNAIL_SCALE_FACTOR,
   "ปรับขนาดของแถบรูปภาพตัวอย่าง"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_PADDING_FACTOR,
   "ปรับระยะขอบ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_PADDING_FACTOR,
   "ปรับขนาดระยะขอบแนวนอน"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_HEADER_ICON,
   "ไอคอนส่วนหัว"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_HEADER_ICON,
   "แสดง 'โลโก้ส่วนหัวสามารถซ่อนได้ ปรับเปลี่ยนตามการนำทาง หรือกำหนดคงที่เป็นแบบคลาสสิก Invader'"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_HEADER_SEPARATOR,
   "เส้นแบ่งส่วนหัวชื่อเรื่อง"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_HEADER_SEPARATOR,
   "ความกว้างสำรองสำหรับเส้นแบ่งส่วนหัวและส่วนท้ายชื่อเรื่อง"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_HEADER_ICON_NONE,
   "ไม่แชร์อินพุต"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_HEADER_ICON_FIXED,
   "คงที่"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_HEADER_SEPARATOR_NONE,
   "ไม่แชร์อินพุต"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_HEADER_SEPARATOR_NORMAL,
   "ปกติ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_HEADER_SEPARATOR_MAXIMUM,
   "สูงสุด"
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_MENU_COLOR_THEME,
   "ธีมสี"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_MENU_COLOR_THEME,
   "เลือกธีมสีอื่น"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_BASIC_WHITE,
   "สีขาวพื้นฐาน"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_BASIC_BLACK,
   "สีดำพื้นฐาน"
   )


/* MaterialUI: Settings > User Interface > Appearance */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_ICONS_ENABLE,
   "ไอคอน"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_ICONS_ENABLE,
   "แสดงไอคอนทางด้านซ้ายของรายการเมนูออกาไนเซอร์"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_SWITCH_ICONS,
   "สลับไอค่อน"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_SWITCH_ICONS,
   "ใช้ไอคอนแทนข้อความ เปิด/ปิด เพื่อแสดงรายการตั้งค่าเมนูแบบ 'สวิตช์สลับ'"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_PLAYLIST_ICONS_ENABLE,
   "ไอคอนเพลย์ลิสต์ (ต้องรีสตาร์ท)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_PLAYLIST_ICONS_ENABLE,
   "แสดงไอคอนเฉพาะของแต่ละระบบในเพลย์ลิสต์"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_LANDSCAPE_LAYOUT_OPTIMIZATION,
   "ปรับเค้าโครงแนวนอนให้เหมาะสม"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_LANDSCAPE_LAYOUT_OPTIMIZATION,
   "ปรับเค้าโครงเมนูโดยอัตโนมัติเพื่อให้เหมาะสมกับหน้าจอมากขึ้น เมื่อใช้งานในโหมดแสดงผลแนวนอน"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_SHOW_NAV_BAR,
   "แสดงแถบนำทาง"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_SHOW_NAV_BAR,
   "แสดงแถบทางลัดสำหรับนำทางบนหน้าจออย่างถาวร ช่วยให้สลับระหว่างหมวดหมู่เมนูได้อย่างรวดเร็ว แนะนำสำหรับอุปกรณ์หน้าจอสัมผัส"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_AUTO_ROTATE_NAV_BAR,
   "หมุนแถบนำทางโดยอัตโนมัติ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_AUTO_ROTATE_NAV_BAR,
   "ย้ายแถบนำทางไปยังด้านขวาของหน้าจอโดยอัตโนมัติ เมื่อใช้งานในโหมดแสดงผลแนวนอน"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME,
   "ธีมสี"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_MENU_COLOR_THEME,
   "เลือกธีมสีพื้นหลังอื่น"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_TRANSITION_ANIMATION,
   "แอนิเมชันการเปลี่ยนผ่านหน้าจอ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_MENU_TRANSITION_ANIMATION,
   "เปิดใช้งานเอฟเฟกต์แอนิเมชันที่ลื่นไหล เมื่อมีการนำทางระหว่างระดับต่างๆ ของเมนู"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_THUMBNAIL_VIEW_PORTRAIT,
   "มุมมองภาพตัวอย่างแนวตั้ง"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_MENU_THUMBNAIL_VIEW_PORTRAIT,
   "กำหนดโหมดการแสดงผลภาพตัวอย่างของเพลย์ลิสต์ เมื่อใช้งานในโหมดแสดงผลแนวตั้ง"
   )
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
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_TRANSITION_ANIM_AUTO,
   "อัตโนมัติ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_TRANSITION_ANIM_NONE,
   "ปิด"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_PORTRAIT_DISABLED,
   "ปิด"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_PORTRAIT_LIST_SMALL,
   "รายการ (ขนาดเล็ก)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_PORTRAIT_LIST_MEDIUM,
   "รายการ (ขนาดกลาง)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_PORTRAIT_DUAL_ICON,
   "ไอคอนคู่"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_LANDSCAPE_DISABLED,
   "ปิด"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_LANDSCAPE_LIST_SMALL,
   "รายการ (ขนาดเล็ก)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_LANDSCAPE_LIST_MEDIUM,
   "รายการ (ขนาดกลาง)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_LANDSCAPE_LIST_LARGE,
   "รายการ (ขนาดใหญ่)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_LANDSCAPE_DESKTOP,
   "เดสก์ท็อป"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_LANDSCAPE_LAYOUT_OPTIMIZATION_DISABLED,
   "ปิด"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_LANDSCAPE_LAYOUT_OPTIMIZATION_ALWAYS,
   "เปิด"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_LANDSCAPE_LAYOUT_OPTIMIZATION_EXCLUDE_THUMBNAIL_VIEWS,
   "ยกเว้นมุมมองภาพตัวอย่าง"
   )

/* Qt (Desktop Menu) */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_INFO,
   "ข้อมูล"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_FILE,
   "&ไฟล์"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_FILE_LOAD_CORE,
   "&โหลด Core..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_FILE_UNLOAD_CORE,
   "&เลิกโหลด Core"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_FILE_EXIT,
   "อ&อก"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_EDIT,
   "&แก้ไข"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_EDIT_SEARCH,
   "&ค้นหา"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW,
   "&มุมมอง"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_CLOSED_DOCKS,
   "ปิด Docks"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_SHADER_PARAMS,
   "พารามิเตอร์เชดเดอร์"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS,
   "&ตั้งค่า..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_SAVE_DOCK_POSITIONS,
   "จำตำแหน่ง dock"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_SAVE_GEOMETRY,
   "จำรูปทรงหน้าต่าง"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_SAVE_LAST_TAB,
   "จำแท็บเปิดเนื้อหาล่าสุด"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_THEME,
   "ธีม:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_THEME_DARK,
   "มืด"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_THEME_CUSTOM,
   "กำหนดเอง..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_TITLE,
   "ตั้งค่า"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_TOOLS,
   "&เครื่องมือ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_HELP,
   "&ช่วยเหลือ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_HELP_ABOUT,
   "เกี่ยวกับ RetroArch"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_HELP_DOCUMENTATION,
   "เอกสารประกอบ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_LOAD_CUSTOM_CORE,
   "โหลด Core แบบกำหนดเอง..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_LOAD_CORE,
   "โหลด Core"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_LOADING_CORE,
   "กำลังโหลด Core..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_NAME,
   "ชื่อ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CORE_VERSION,
   "เวอร์ชั่น"
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
   MENU_ENUM_LABEL_VALUE_QT_TAB_FILE_BROWSER_TOP,
   "บน"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_TAB_FILE_BROWSER_UP,
   "ขึ้น"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_DOCK_CONTENT_BROWSER,
   "เปิดหาเนื้อหา"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_THUMBNAIL_BOXART,
   "ภาพหน้าปกบนกล่อง"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_THUMBNAIL_SCREENSHOT,
   "ภาพหน้าจอ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_THUMBNAIL_TITLE_SCREEN,
   "หน้าจอชื่อเรื่อง"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_THUMBNAIL_LOGO,
   "โลโก้"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_ALL_PLAYLISTS,
   "เพลย์ลิสต์ทั้งหมด"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CORE_INFO,
   "ข้อมูล Core"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_INFORMATION,
   "ข้อมูล"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_WARNING,
   "คำเตือน"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_ERROR,
   "ผิดพลาด"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_NETWORK_ERROR,
   "เครือข่ายขัดข้อง"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_RESTART_TO_TAKE_EFFECT,
   "กรุณารีสตาร์ทโปรแกรม เพื่อให้การเปลี่ยนแปลงมีผล"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_ITEMS_COUNT,
   "%1 รายการ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_DROP_IMAGE_HERE,
   "วางภาพที่นี่"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_DONT_SHOW_AGAIN,
   "อย่าแสดงอีกครั้ง"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_STOP,
   "หยุด"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_ASSOCIATE_CORE,
   "เชื่อมโยง Core"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_HIDDEN_PLAYLISTS,
   "เพลย์ลิสต์ที่ซ่อนอยู่"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_HIDE,
   "ซ่อน"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_HIGHLIGHT_COLOR,
   "สีไฮไลต์:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CHOOSE,
   "&เลือก..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_SELECT_COLOR,
   "เลือก สี"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_SELECT_THEME,
   "เลือกธีม"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CUSTOM_THEME,
   "ธีมที่กำหนดเอง"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_FILE_PATH_IS_BLANK,
   "เส้นทางไฟล์ว่างเปล่า"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_FILE_IS_EMPTY,
   "ไฟล์ว่างเปล่า"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_FILE_READ_OPEN_FAILED,
   "ไม่สามารถเปิดไฟล์เพื่ออ่านได้"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_FILE_WRITE_OPEN_FAILED,
   "ไม่สามารถเปิดไฟล์เพื่อเขียนได้"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_FILE_DOES_NOT_EXIST,
   "ไม่มีไฟล์อยู่"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_SUGGEST_LOADED_CORE_FIRST,
   "แนะนำ Core ที่โหลดอยู่ก่อน:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_ZOOM,
   "ขยาย"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_VIEW,
   "มุมมอง"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_VIEW_TYPE_ICONS,
   "ไอคอน"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_VIEW_TYPE_LIST,
   "รายการ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_SEARCH_CLEAR,
   "ล้าง"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_PROGRESS,
   "ความคืบหน้า:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_ALL_PLAYLISTS_LIST_MAX_COUNT,
   "จำกัดจำนวนรายการสูงสุดใน \"เพลย์ลิสต์ทั้งหมด\":"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_ALL_PLAYLISTS_GRID_MAX_COUNT,
   "จำกัดจำนวนรายการตารางสูงสุดใน \"เพลย์ลิสต์ทั้งหมด\":"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_SHOW_HIDDEN_FILES,
   "แสดงไฟล์และโฟลเดอร์ที่ซ่อนอยู่:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_NEW_PLAYLIST,
   "สร้างเพลย์ลิสต์ใหม่"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_ENTER_NEW_PLAYLIST_NAME,
   "โปรดป้อนชื่อเพลย์ลิสต์ใหม่:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_DELETE_PLAYLIST,
   "ลบเพลย์ลิสต์"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_RENAME_PLAYLIST,
   "เปลี่ยนชื่อเพลย์ลิสต์"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CONFIRM_DELETE_PLAYLIST,
   "คุณแน่ใจหรือไม่ว่าต้องการลบเพลย์ลิสต์ \"%1\"?"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_QUESTION,
   "คำถาม"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_COULD_NOT_DELETE_FILE,
   "ไม่สามารถลบไฟล์ได้"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_COULD_NOT_RENAME_FILE,
   "ไม่สามารถเปลี่ยนชื่อไฟล์ได้"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_GATHERING_LIST_OF_FILES,
   "กำลังรวบรวมรายชื่อไฟล์..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_ADDING_FILES_TO_PLAYLIST,
   "กำลังเพิ่มไฟล์ลงในเพลย์ลิสต์..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY,
   "รายการในเพลย์ลิสต์"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY_NAME,
   "ชื่อ:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY_PATH,
   "เส้นทาง:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY_DATABASE,
   "ฐานข้อมูล:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY_EXTENSIONS,
   "นามสกุล:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY_EXTENSIONS_PLACEHOLDER,
   "(เว้นวรรคคั่นระหว่างรายการ; ค่าเริ่มต้นคือรวมทั้งหมด)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY_FILTER_INSIDE_ARCHIVES,
   "กรองข้อมูลภายในไฟล์บีบอัด"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_FOR_THUMBNAILS,
   "(ใช้เพื่อค้นหาภาพตัวอย่าง)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CONFIRM_DELETE_PLAYLIST_ITEM,
   "คุณแน่ใจหรือไม่ว่าต้องการลบรายการ \"%1\"?"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CANNOT_ADD_TO_ALL_PLAYLISTS,
   "กรุณาเลือกเพลย์ลิสต์ก่อนเพียงหนึ่งรายการ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_DELETE,
   "ลบ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_ADD_ENTRY,
   "เพิ่มรายการ..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_ADD_FILES,
   "เพิ่มไฟล์..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_ADD_FOLDER,
   "เพิ่มโฟลเดอร์..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_EDIT,
   "แก้ไข"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_SELECT_FILES,
   "เลือกไฟล์"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_SELECT_FOLDER,
   "เลือกโฟลเดอร์"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_COULD_NOT_UPDATE_PLAYLIST_ENTRY,
   "เกิดข้อผิดพลาดในการอัปเดตรายการในเพลย์ลิสต์"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_PLEASE_FILL_OUT_REQUIRED_FIELDS,
   "กรุณากรอกข้อมูลในช่องที่จำเป็นให้ครบถ้วน"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_UPDATE_RETROARCH_NIGHTLY,
   "อัปเดต RetroArch (รุ่นทดลอง Nightly)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_UPDATE_RETROARCH_FINISHED,
   "อัปเดต RetroArch สำเร็จแล้ว กรุณาเริ่มแอปพลิเคชันใหม่เพื่อให้การเปลี่ยนแปลงมีผล ออกแบบมาเพื่อแจ้งให้คุณทราบว่าขั้นตอนเสร็จสมบูรณ์"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_UPDATE_RETROARCH_FAILED,
   "การอัปเดตล้มเหลว"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_HELP_ABOUT_CONTRIBUTORS,
   "ผู้ร่วมพัฒนา"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CURRENT_SHADER,
   "เชดเดอร์ปัจจุบัน"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MOVE_DOWN,
   "เลื่อนลง"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MOVE_UP,
   "เลื่อนขึ้น"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_LOAD,
   "โหลด"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_SAVE,
   "บันทึกเกม"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_REMOVE,
   "ลบออก"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_REMOVE_PASSES,
   "ลบบัตรผ่าน"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_APPLY,
   "ยื่นสมัคร"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_SHADER_ADD_PASS,
   "เพิ่มบัตรผ่าน"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_SHADER_CLEAR_ALL_PASSES,
   "ยกเลิกบัตรผ่านทั้งหมด"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_SHADER_NO_PASSES,
   "ไม่มีการใช้ บัตรผ่าน Shader"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_RESET_PASS,
   "ล้างบัตรผ่าน"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_RESET_ALL_PASSES,
   "รีเซ็ตบัตรผ่านทั้งหมด"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_RESET_PARAMETER,
   "รีเซ็ตพารามิเตอร์"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_DOWNLOAD_THUMBNAIL,
   "ดาวน์โหลดรูปตัวอย่าง"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_DOWNLOAD_ALREADY_IN_PROGRESS,
   "กำลังดำเนินการดาวน์โหลดอยู่"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_STARTUP_PLAYLIST,
   "เริ่มเล่นจากเพลย์ลิสต์:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_THUMBNAIL_TYPE,
   "รูปตัวอย่าง"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_THUMBNAIL_CACHE_LIMIT,
   "ขีดจำกัดแคชของรูปภาพตัวอย่าง"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_THUMBNAIL_DROP_SIZE_LIMIT,
   "ขีดจำกัดขนาดรูปภาพตัวอย่างสำหรับการลากวาง:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_DOWNLOAD_ALL_THUMBNAILS,
   "ดาวน์โหลดรูปตัวอย่างทั้งหมด"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_DOWNLOAD_ALL_THUMBNAILS_ENTIRE_SYSTEM,
   "ทั้งระบบ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_DOWNLOAD_ALL_THUMBNAILS_THIS_PLAYLIST,
   "เพลย์ลิสต์นี้"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_THUMBNAIL_PACK_DOWNLOADED_SUCCESSFULLY,
   "ดาวน์โหลดรูปภาพตัวอย่างสำเร็จแล้ว"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_DOWNLOAD_PLAYLIST_THUMBNAIL_PROGRESS,
   "สำเร็จ: %1 ล้มเหลว: %2"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CORE_OPTIONS,
   "ตัวเลือก Core"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_RESET,
   "รีเซ็ต"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_RESET_ALL,
   "รีเซ็ตทั้งหมด"
   )

/* Unsorted */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_UPDATER_SETTINGS,
   "ตั้งค่าตัวอัปเดต Core"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACCOUNTS_CHEEVOS_SETTINGS,
   "บัญชี Cheevos"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACCOUNTS_LIST_END,
   "ปลายทางรายการบัญชี"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_COUNTERS,
   "ตัวนับ Core"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_DISK,
   "ไม่ได้เลือกแผ่น"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FRONTEND_COUNTERS,
   "ตัวนับ Frontend"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HORIZONTAL_MENU,
   "เมนูแนวนอน"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DESCRIPTOR_HIDE_UNBOUND,
   "ซ่อนคำอธิบายอินพุตของ Core ที่ไม่ได้ระบุไว้"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DESCRIPTOR_LABEL_SHOW,
   "แสดงป้ายคำอธิบายอินพุต"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_SETTINGS,
   "Overlay ซ้อนทับบนหน้าจอ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOAD_CONTENT_HISTORY,
   "ประวัติ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOAD_CONTENT_HISTORY,
   "เลือกเนื้อหาจากเพลย์ลิสต์ประวัติล่าสุด"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_LOAD_CONTENT_HISTORY,
   "เมื่อโหลดเนื้อหาแล้ว การจับคู่ระหว่างเนื้อหาและคอร์ libretro จะถูกบันทึกไว้ในประวัติ\nโดยไฟล์ประวัติจะถูกเก็บไว้ในโฟลเดอร์เดียวกับไฟล์กำหนดค่าของ RetroArch หากไม่มีการโหลดไฟล์กำหนดค่[...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MULTIMEDIA_SETTINGS,
   "มัลติมีเดีย"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SUBSYSTEM_SETTINGS,
   "เข้าถึงการตั้งค่า Subsystem สำหรับเนื้อหาปัจจุบัน"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SUBSYSTEM_CONTENT_INFO,
   "เนื้อหาปัจจุบัน: %s"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_NETPLAY_HOSTS_FOUND,
   "ไม่พบโฮสต์ Netplay"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_NETPLAY_CLIENTS_FOUND,
   "ไม่พบไคลเอนต์ Netplay"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_PERFORMANCE_COUNTERS,
   "ไม่มีตัวนับประสิทธิภาพ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_PLAYLISTS,
   "ไม่พบเพลย์ลิสต์"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BT_CONNECTED,
   "เชื่อมต่อแล้ว"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ONLINE,
   "ออนไลน์"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PORT,
   "พอร์ต"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PORT_DEVICE_NAME,
   "พอร์ต %d ชื่ออุปกรณ์: %s"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PORT_DEVICE_INFO,
   "ชื่ออุปกรณ์ที่แสดง: %s\nชื่อไฟล์กำหนดค่าอุปกรณ์: %s\nVID/PID ของอุปกรณ์: %d/%d"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SETTINGS,
   "ตั้งค่าสูตรโกง"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_SETTINGS,
   "เริ่มหรือดำเนินการค้นหาสูตรโกงต่อ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RUN_MUSIC,
   "เล่นในเครื่องเล่นสื่อ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SECONDS,
   "วินาที"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_START_CORE,
   "เริ่ม Core"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_START_CORE,
   "เริ่ม Core โดยไม่มีเนื้อหา"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SUPPORTED_CORES,
   "Core ที่แนะนำ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UNABLE_TO_READ_COMPRESSED_FILE,
   "ไม่สามารถอ่านไฟล์ที่บีบอัดได้"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_USER,
   "ผู้ใช้"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MAX_SWAPCHAIN_IMAGES,
   "จำนวนภาพ Swapchain สูงสุด"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MAX_SWAPCHAIN_IMAGES,
   "บอกให้ไดรเวอร์วิดีโอใช้โหมดการบัฟเฟอร์ที่กำหนดโดยเฉพาะ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_MAX_SWAPCHAIN_IMAGES,
   "จำนวนภาพสูงสุดในห่วงโซ่การสลับ สิ่งนี้สามารถสั่งให้ไดรเวอร์วิดีโอใช้โหมดการบัฟเฟอร์วิดีโอที่เฉพาะเจาะจงได้\nบัฟเฟอร์เดี่ยว - 1\nบัฟเฟอร์คู่ - 2\nบัฟเฟอร์สามชั้น - 3\nการตั้งค่าโหมด[...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WAITABLE_SWAPCHAINS,
   "ห่วงโซ่การสลับที่รอได้"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WAITABLE_SWAPCHAINS,
   "ซิงค์ CPU และ GPU อย่างเข้มงวด ช่วยลดความหน่วงโดยแลกกับประสิทธิภาพที่ลดลง"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MAX_FRAME_LATENCY,
   "ความหน่วงเฟรมสูงสุด"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MAX_FRAME_LATENCY,
   "สั่งให้ไดรเวอร์วิดีโอใช้โหมดการบัฟเฟอร์ที่กำหนดโดยเฉพาะ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_PARAMETERS,
   "ปรับเปลี่ยนค่าที่ตั้งไว้ล่วงหน้าของเชดเดอร์ที่กำลังใช้งานอยู่ในเมนู"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_TWO,
   "ค่าเชดเดอร์ที่ตั้งไว้ล่วงหน้า"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_PREPEND_TWO,
   "ค่าเชดเดอร์ที่ตั้งไว้ล่วงหน้า"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_APPEND_TWO,
   "ค่าเชดเดอร์ที่ตั้งไว้ล่วงหน้า"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BROWSE_URL_LIST,
   "เรียกดู URL"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BROWSE_URL,
   "เส้นทาง URL"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BROWSE_START,
   "เริ่ม"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_ROOM_NICKNAME,
   "ชื่อเล่น: %s"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_COMPAT_CONTENT_LOOK,
   "กำลังค้นหาคอนเทนต์ที่เข้ากันได้..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_COMPAT_CONTENT_NO_CORE,
   "ไม่พบ Core"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_COMPAT_CONTENT_NO_PLAYLISTS,
   "ไม่พบเพลย์ลิสต์"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_COMPAT_CONTENT_FOUND,
   "พบเนื้อหาที่เข้ากันได้"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_COMPAT_CONTENT_NOT_FOUND,
   "ไม่สามารถหาเนื้อหาที่ตรงกันได้จาก CRC หรือชื่อไฟล์"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STATUS,
   "สถานะ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_BGM_ENABLE,
   "ระบบ BGM"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HELP,
   "ช่วยเหลือ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLEAR_SETTING,
   "ล้าง"
   )

/* Discord Status */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISCORD_IN_MENU,
   "ในเมนู"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISCORD_IN_GAME,
   "ในเกม"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISCORD_IN_GAME_PAUSED,
   "ในเกม(หยุดชั่วคราว)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISCORD_STATUS_PLAYING,
   "กำลังเล่น"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISCORD_STATUS_PAUSED,
   "หยุดชั่วคราว"
   )

/* Notifications */

MSG_HASH( /* FIXME Should be MSG_ */
   MENU_ENUM_LABEL_VALUE_NETPLAY_START_WHEN_LOADED,
   "Netplay จะเริ่มเมื่อโหลดเนื้อหาแล้ว"
   )
MSG_HASH(
   MSG_NETPLAY_NEED_CONTENT_LOADED,
   "ต้องโหลดเนื้อหาก่อนเริ่มการเล่นออนไลน์ netplay"
   )
MSG_HASH( /* FIXME Should be MSG_ */
   MENU_ENUM_LABEL_VALUE_NETPLAY_LOAD_CONTENT_MANUALLY,
   "ไม่พบ core หรือไฟล์เนื้อหาที่เหมาะสม ให้โหลดด้วยตนเอง"
   )
MSG_HASH( /* FIXME Should be MSG_ */
   MENU_ENUM_LABEL_VALUE_VIDEO_DRIVER_FALLBACK,
   "ไดรเวอร์กราฟิกของคุณไม่สามารถทำงานร่วมกับวิดีโอไดรเวอร์ปัจจุบันใน RetroArch ได้ ระบบจะเปลี่ยนกลับไปใช้ไดรเวอร์ %s กรุณารีสตาร์ท RetroArch เพื่อให้การเปลี่ยนแปลงมีผล"
   )
MSG_HASH( /* FIXME Should be MSG_ */
   MENU_ENUM_LABEL_VALUE_SIDELOAD_CORE_SUCCESS,
   "การติดตั้งคอร์สำเร็จ"
   )
MSG_HASH( /* FIXME Should be MSG_ */
   MENU_ENUM_LABEL_VALUE_SIDELOAD_CORE_ERROR,
   "การติดตั้งคอร์ล้มเหลว"
   )
MSG_HASH(
   MSG_CHEAT_DELETE_ALL_INSTRUCTIONS,
   "กดขวาห้าครั้งเพื่อลบสูตรโกงทั้งหมด"
   )
MSG_HASH(
   MSG_AUDIO_MIXER_VOLUME,
   "ระดับเสียงมิกเซอร์ทั่วโลก"
   )
MSG_HASH(
   MSG_NETPLAY_LAN_SCAN_COMPLETE,
   "การสแกนเพื่อเล่น Netplay เสร็จสิ้น"
   )
MSG_HASH(
   MSG_SORRY_UNIMPLEMENTED_CORES_DONT_DEMAND_CONTENT_NETPLAY,
   "ขออภัย ยังไม่รองรับ: Core ที่ไม่ต้องการเนื้อหา ไม่สามารถเข้าร่วม Netplay ได้"
   )
MSG_HASH(
   MSG_NATIVE,
   "ดั้งเดิม"
   )
MSG_HASH(
   MSG_UNKNOWN_NETPLAY_COMMAND_RECEIVED,
   "ได้รับคำสั่ง netplay ที่ไม่รู้จัก"
   )
MSG_HASH(
   MSG_FILE_ALREADY_EXISTS_SAVING_TO_BACKUP_BUFFER,
   "ไฟล์มีอยู่แล้ว กำลังบันทึกไปยังบัฟเฟอร์สำรอง"
   )
MSG_HASH(
   MSG_GOT_CONNECTION_FROM,
   "ได้รับการเชื่อมต่อจาก: “%s”"
   )
MSG_HASH(
   MSG_GOT_CONNECTION_FROM_NAME,
   "ได้รับการเชื่อมต่อจาก: “%s (%s)”"
   )
MSG_HASH(
   MSG_PUBLIC_ADDRESS,
   "การแมปพอร์ต Netplay สำเร็จ"
   )
MSG_HASH(
   MSG_PRIVATE_OR_SHARED_ADDRESS,
   "เครือข่ายภายนอกมีที่อยู่แบบส่วนตัวหรือใช้ร่วมกัน แนะนำให้ใช้เซิร์ฟเวอร์รีเลย์"
   )
MSG_HASH(
   MSG_UPNP_FAILED,
   "การแมปพอร์ต Netplay แบบ UPnP ล้มเหลว"
   )
MSG_HASH(
   MSG_NO_ARGUMENTS_SUPPLIED_AND_NO_MENU_BUILTIN,
   "ไม่มีการส่งค่าเข้ามา และไม่มีเมนูในตัว กำลังแสดงความช่วยเหลือ…"
   )
MSG_HASH(
   MSG_SETTING_DISK_IN_TRAY,
   "กำลังตั้งค่าแผ่นในถาด"
   )
MSG_HASH(
   MSG_WAITING_FOR_CLIENT,
   "กำลังรอผู้ใช้เชื่อมต่อ…"
   )
MSG_HASH(
   MSG_ROOM_NOT_CONNECTABLE,
   "ห้องของคุณไม่สามารถเชื่อมต่อจากอินเทอร์เน็ตได้"
   )
MSG_HASH(
   MSG_NETPLAY_YOU_HAVE_LEFT_THE_GAME,
   "คุณออกจากเกมแล้ว"
   )
MSG_HASH(
   MSG_NETPLAY_YOU_HAVE_JOINED_AS_PLAYER_N,
   "คุณเข้าร่วม เป็นผู้เล่น %u"
   )
MSG_HASH(
   MSG_NETPLAY_YOU_HAVE_JOINED_WITH_INPUT_DEVICES_S,
   "คุณเข้าร่วมพร้อมกับอุปกรณ์นำเข้า %.*s"
   )
MSG_HASH(
   MSG_NETPLAY_PLAYER_S_LEFT,
   "ผู้เล่น %.*s ออกจากเกมแล้ว"
   )
MSG_HASH(
   MSG_NETPLAY_S_HAS_JOINED_AS_PLAYER_N,
   "%.*s เข้าร่วมเป็นผู้เล่น %u"
   )
MSG_HASH(
   MSG_NETPLAY_S_HAS_JOINED_WITH_INPUT_DEVICES_S,
   "%.*s เข้าร่วมพร้อมกับอุปกรณ์นำเข้า %.*s"
   )
MSG_HASH(
   MSG_NETPLAY_PLAYERS_INFO,
   "มีผู้เล่น %d คน"
   )
MSG_HASH(
   MSG_NETPLAY_SPECTATORS_INFO,
   "มีผู้เล่น %d คน (มีผู้ชม %d คน)"
   )
MSG_HASH(
   MSG_NETPLAY_NOT_RETROARCH,
   "การเชื่อมต่อแบบ Netplay ล้มเหลว เนื่องจากคู่เชื่อมต่อไม่ได้ใช้ RetroArch หรือใช้เวอร์ชันที่เก่ากว่า RetroArch"
   )
MSG_HASH(
   MSG_NETPLAY_OUT_OF_DATE,
   "คู่เชื่อมต่อ Netplay ใช้ RetroArch เวอร์ชันเก่า ไม่สามารถเชื่อมต่อได้"
   )
MSG_HASH(
   MSG_NETPLAY_DIFFERENT_VERSIONS,
   "คำเตือน: คู่เชื่อมต่อ Netplay ใช้ RetroArch เวอร์ชันที่แตกต่างกัน หากเกิดปัญหา ควรใช้เวอร์ชันเดียวกัน"
   )
MSG_HASH(
   MSG_NETPLAY_DIFFERENT_CORES,
   "คู่เชื่อมต่อ Netplay ใช้ Core ที่แตกต่างกัน ไม่สามารถเชื่อมต่อได้"
   )
MSG_HASH(
   MSG_NETPLAY_DIFFERENT_CORE_VERSIONS,
   "คำเตือน: คู่เชื่อมต่อ Netplay ใช้ Core เวอร์ชันที่แตกต่างกัน หากเกิดปัญหา ควรใช้เวอร์ชันเดียวกัน"
   )
MSG_HASH(
   MSG_NETPLAY_ENDIAN_DEPENDENT,
   "Core นี้ไม่รองรับการเล่น Netplay ระหว่างแพลตฟอร์มเหล่านี้"
   )
MSG_HASH(
   MSG_NETPLAY_PLATFORM_DEPENDENT,
   "Core นี้ไม่รองรับ Netplay ระหว่างแพลตฟอร์มที่ต่างกัน"
   )
MSG_HASH(
   MSG_NETPLAY_ENTER_PASSWORD,
   "ป้อนรหัสผ่านเซิร์ฟเวอร์ Netplay:"
   )
MSG_HASH(
   MSG_NETPLAY_ENTER_CHAT,
   "ป้อนข้อความแชท Netplay:"
   )
MSG_HASH(
   MSG_DISCORD_CONNECTION_REQUEST,
   "คุณต้องการอนุญาตการเชื่อมต่อจากผู้ใช้:"
   )
MSG_HASH(
   MSG_NETPLAY_INCORRECT_PASSWORD,
   "รหัสผ่านไม่ถูกต้อง"
   )
MSG_HASH(
   MSG_NETPLAY_SERVER_NAMED_HANGUP,
   "\"%s\" ตัดการเชื่อมต่อแล้ว"
   )
MSG_HASH(
   MSG_NETPLAY_SERVER_HANGUP,
   "ผู้ใช้ Netplay ตัดการเชื่อมต่อแล้ว"
   )
MSG_HASH(
   MSG_NETPLAY_CLIENT_HANGUP,
   "การเชื่อมต่อ Netplay ถูกตัดแล้ว"
   )
MSG_HASH(
   MSG_NETPLAY_CANNOT_PLAY_UNPRIVILEGED,
   "คุณไม่มีสิทธิ์ในการเล่น"
   )
MSG_HASH(
   MSG_NETPLAY_CANNOT_PLAY_NO_SLOTS,
   "ไม่มีช่องว่างสำหรับผู้เล่นใหม่แล้ว"
   )
MSG_HASH(
   MSG_NETPLAY_CANNOT_PLAY_NOT_AVAILABLE,
   "อุปกรณ์อินพุตที่ร้องขอไม่พร้อมใช้งาน"
   )
MSG_HASH(
   MSG_NETPLAY_CANNOT_PLAY,
   "ไม่สามารถเปลี่ยนเป็นโหมดการเล่นได้"
   )
MSG_HASH(
   MSG_NETPLAY_PEER_PAUSED,
   "เพื่อน Netplay \"%s\" หยุดชั่วคราว"
   )
MSG_HASH(
   MSG_NETPLAY_CHANGED_NICK,
   "เปลี่ยนชื่อเล่นของคุณเป็น \"%s\" แล้ว"
   )
MSG_HASH(
   MSG_NETPLAY_KICKED_CLIENT_S,
   "ผู้เล่นถูกเตะ: \"%s\""
   )
MSG_HASH(
   MSG_NETPLAY_FAILED_TO_KICK_CLIENT_S,
   "ไม่สามารถเตะผู้เล่นออกได้: \"%s\""
   )
MSG_HASH(
   MSG_NETPLAY_BANNED_CLIENT_S,
   "ผู้เล่นถูกแบน: \"%s\""
   )
MSG_HASH(
   MSG_NETPLAY_FAILED_TO_BAN_CLIENT_S,
   "ไม่สามารถแบนผู้เล่นได้: \"%s\""
   )
MSG_HASH(
   MSG_NETPLAY_STATUS_PLAYING,
   "กำลังเล่น"
   )
MSG_HASH(
   MSG_NETPLAY_STATUS_SPECTATING,
   "กำลังรับชม"
   )
MSG_HASH(
   MSG_NETPLAY_CLIENT_DEVICES,
   "อุปกรณ์"
   )
MSG_HASH(
   MSG_NETPLAY_CHAT_SUPPORTED,
   "รองรับการแชท"
   )
MSG_HASH(
   MSG_NETPLAY_SLOWDOWNS_CAUSED,
   "สาเหตุความล่าช้า"
   )

MSG_HASH(
   MSG_AUDIO_VOLUME,
   "ระดับเสียง"
   )
MSG_HASH(
   MSG_AUTODETECT,
   "ตรวจหาอัตโนมัติ"
   )
MSG_HASH(
   MSG_CAPABILITIES,
   "รองรับ"
   )
MSG_HASH(
   MSG_CONNECTING_TO_NETPLAY_HOST,
   "กำลังเชื่อมต่อไปยังโฮสต์ของ Netplay"
   )
MSG_HASH(
   MSG_CONNECTING_TO_PORT,
   "กำลังเชื่อมต่อไปยังพอร์ต"
   )
MSG_HASH(
   MSG_CONNECTION_SLOT,
   "ช่องการเชื่อมต่อ"
   )
MSG_HASH(
   MSG_FETCHING_CORE_LIST,
   "กำลังดึงรายการ Core..."
   )
MSG_HASH(
   MSG_CORE_LIST_FAILED,
   "ไม่สามารถดึงรายการ Core ได้!"
   )
MSG_HASH(
   MSG_LATEST_CORE_INSTALLED,
   "ติดตั้งเวอร์ชันล่าสุดแล้ว:"
   )
MSG_HASH(
   MSG_UPDATING_CORE,
   "กำลังอัปเดต Core:"
   )
MSG_HASH(
   MSG_DOWNLOADING_CORE,
   "กำลังดาวน์โหลด Core:"
   )
MSG_HASH(
   MSG_EXTRACTING_CORE,
   "กำลังแตกไฟล์ Core:"
   )
MSG_HASH(
   MSG_CORE_INSTALLED,
   "กำลังติดตั้ง Core:"
   )
MSG_HASH(
   MSG_CORE_INSTALL_FAILED,
   "ไม่สามารถติดตั้ง Core ได้!"
   )
MSG_HASH(
   MSG_SCANNING_CORES,
   "กำลังสแกนรายการ Core…"
   )
MSG_HASH(
   MSG_CHECKING_CORE,
   "กำลังตรวจสอบ Core:"
   )
MSG_HASH(
   MSG_ALL_CORES_UPDATED,
   "Core ทั้งหมดที่ติดตั้งอยู่เป็นเวอร์ชันล่าสุดแล้ว"
   )
MSG_HASH(
   MSG_ALL_CORES_SWITCHED_PFD,
   "Core ที่รองรับทั้งหมดถูกสลับไปใช้เวอร์ชันจาก Play Store แล้ว"
   )
MSG_HASH(
   MSG_NUM_CORES_UPDATED,
   "Core ถูกอัปเดตแล้ว: "
   )
MSG_HASH(
   MSG_NUM_CORES_LOCKED,
   "Core ถูกข้าม: "
   )
MSG_HASH(
   MSG_CORE_UPDATE_DISABLED,
   "Core update ถูกปิดใช้งาน - Core ถูกล็อก: "
   )
MSG_HASH(
   MSG_PLAYLIST_MANAGER_RESETTING_CORES,
   "กำลังรีเซ็ต Core: "
   )
MSG_HASH(
   MSG_PLAYLIST_MANAGER_CORES_RESET,
   "Core ถูกรีเซ็ตแล้ว: "
   )
MSG_HASH(
   MSG_PLAYLIST_MANAGER_CLEANING_PLAYLIST,
   "กำลังล้างเพลย์ลิสต์: "
   )
MSG_HASH(
   MSG_PLAYLIST_MANAGER_PLAYLIST_CLEANED,
   "เพลย์ลิสต์ถูกล้างเรียบร้อยแล้ว: "
   )
MSG_HASH(
   MSG_PLAYLIST_MANAGER_REFRESH_MISSING_CONFIG,
   "การรีเฟรชล้มเหลว - เพลย์ลิสต์ไม่มีบันทึกการสแกนที่ถูกต้อง: "
   )
MSG_HASH(
   MSG_PLAYLIST_MANAGER_REFRESH_INVALID_CONTENT_DIR,
   "การรีเฟรชล้มเหลว - ไม่มีไดเรกทอรีเนื้อหาที่ถูกต้องหรือหายไป: "
   )
MSG_HASH(
   MSG_PLAYLIST_MANAGER_REFRESH_INVALID_SYSTEM_NAME,
   "การรีเฟรชล้มเหลว - ไม่มีชื่อระบบที่ถูกต้องหรือหายไป: "
   )
MSG_HASH(
   MSG_PLAYLIST_MANAGER_REFRESH_INVALID_CORE,
   "การรีเฟรชล้มเหลว - Core ไม่ถูกต้อง: "
   )
MSG_HASH(
   MSG_PLAYLIST_MANAGER_REFRESH_INVALID_DAT_FILE,
   "การรีเฟรชล้มเหลว - ไม่มีไฟล์ DAT ของ arcade ที่ถูกต้องหรือหายไป: "
   )
MSG_HASH(
   MSG_PLAYLIST_MANAGER_REFRESH_DAT_FILE_TOO_LARGE,
   "การรีเฟรชล้มเหลว - ไฟล์ DAT ของ arcade มีขนาดใหญ่เกินไป (หน่วยความจำไม่เพียงพอ): "
   )
MSG_HASH(
   MSG_ADDED_TO_FAVORITES,
   "เพิ่มไปยังรายการโปรดแล้ว"
   )
MSG_HASH(
   MSG_ADD_TO_FAVORITES_FAILED,
   "ไม่สามารถเพิ่มรายการโปรดได้: เพลย์ลิสต์เต็ม"
   )
MSG_HASH(
   MSG_ADDED_TO_PLAYLIST,
   "เพิ่มในเพลย์ลิสต์แล้ว"
   )
MSG_HASH(
   MSG_ADD_TO_PLAYLIST_FAILED,
   "ไม่สามารถเพิ่มไปยังเพลย์ลิสต์ได้: เพลย์ลิสต์เต็ม"
   )
MSG_HASH(
   MSG_SET_CORE_ASSOCIATION,
   "Core ถูกตั้งค่าแล้ว:"
   )
MSG_HASH(
   MSG_RESET_CORE_ASSOCIATION,
   "การเชื่อม Core ของรายการในเพลย์ลิสต์ถูกรีเซ็ตแล้ว:"
   )
MSG_HASH(
   MSG_APPENDED_DISK,
   "ดิสก์ถูกเพิ่มแล้ว"
   )
MSG_HASH(
   MSG_FAILED_TO_APPEND_DISK,
   "ไม่สามารถเพิ่มดิสก์ได้"
   )
MSG_HASH(
   MSG_APPLICATION_DIR,
   "ไดเรกทอรีแอปพลิเคชัน"
   )
MSG_HASH(
   MSG_APPLYING_CHEAT,
   "กำลังปรับใช้การเปลี่ยนแปลงของสูตรโกง"
   )
MSG_HASH(
   MSG_APPLYING_PATCH,
   "กำลังปรับใช้แพตช์: %s"
   )
MSG_HASH(
   MSG_APPLYING_SHADER,
   "กำลังปรับใช้เชดเดอร์"
   )
MSG_HASH(
   MSG_AUDIO_MUTED,
   "ปิดเสียง"
   )
MSG_HASH(
   MSG_AUDIO_UNMUTED,
   "เปิดเสียง"
   )
MSG_HASH(
   MSG_AUTOCONFIG_FILE_ERROR_SAVING,
   "เกิดข้อผิดพลาดในการบันทึกโปรไฟล์คอนโทรลเลอร์"
   )
MSG_HASH(
   MSG_AUTOCONFIG_FILE_SAVED_SUCCESSFULLY_NAMED,
   "โปรไฟล์คอนโทรลเลอร์ถูกบันทึกเป็น “%s”"
   )
MSG_HASH(
   MSG_AUTOSAVE_FAILED,
   "ไม่สามารถเริ่มการบันทึกอัตโนมัติได้"
   )
MSG_HASH(
   MSG_AUTO_SAVE_STATE_TO,
   "บันทึกสถานะอัตโนมัติไปยัง"
   )
MSG_HASH(
   MSG_BRINGING_UP_COMMAND_INTERFACE_ON_PORT,
   "กำลังเปิดอินเทอร์เฟซคำสั่งบนพอร์ต"
   )
MSG_HASH(
   MSG_BYTES,
   "ไบต์"
   )
MSG_HASH(
   MSG_CANNOT_INFER_NEW_CONFIG_PATH,
   "ไม่สามารถกำหนดเส้นทางการตั้งค่าใหม่ได้ ใช้เวลาปัจจุบันแทน"
   )
MSG_HASH(
   MSG_COMPARING_WITH_KNOWN_MAGIC_NUMBERS,
   "กำลังเปรียบเทียบกับ Magic Number ที่รู้จัก..."
   )
MSG_HASH(
   MSG_COMPILED_AGAINST_API,
   "คอมไพล์กับ API"
   )
MSG_HASH(
   MSG_CONFIG_DIRECTORY_NOT_SET,
   "ยังไม่ได้กำหนดไดเรกทอรีการตั้งค่า ไม่สามารถบันทึกการตั้งค่าใหม่ได้"
   )
MSG_HASH(
   MSG_CONNECTED_TO,
   "เชื่อมต่อกับ"
   )
MSG_HASH(
   MSG_CONTENT_CRC32S_DIFFER,
   "ค่า CRC32 ของเนื้อหาไม่ตรงกัน ไม่สามารถใช้เกมที่ต่างกันได้"
   )
MSG_HASH(
   MSG_CONTENT_NETPACKET_CRC32S_DIFFER,
   "โฮสต์กำลังรันเกมที่ต่างออกไป"
   )
MSG_HASH(
   MSG_PING_TOO_HIGH,
   "ค่า ping ของคุณสูงเกินไปสำหรับโฮสต์นี้"
   )
MSG_HASH(
   MSG_CONTENT_LOADING_SKIPPED_IMPLEMENTATION_WILL_DO_IT,
   "ข้ามการโหลดเนื้อหา การทำงานจะโหลดเอง"
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
   MSG_CORE_DOES_NOT_SUPPORT_DISK_OPTIONS,
   "Core ไม่รองรับการควบคุมแผ่นดิสก์"
   )
MSG_HASH(
   MSG_CORE_OPTIONS_FILE_CREATED_SUCCESSFULLY,
   "ไฟล์ตัวเลือกของ Core ถูกสร้างสำเร็จแล้ว"
   )
MSG_HASH(
   MSG_CORE_OPTIONS_FILE_REMOVED_SUCCESSFULLY,
   "ไฟล์ตัวเลือกของ Core ถูกลบสำเร็จแล้ว"
   )
MSG_HASH(
   MSG_CORE_OPTIONS_RESET,
   "รีเซ็ตตัวเลือกทั้งหมดของ Core กลับไปเป็นค่าเริ่มต้นแล้ว"
   )
MSG_HASH(
   MSG_CORE_OPTIONS_FLUSHED,
   "ไฟล์ตัวเลือกของ Core ถูกบันทึกไปที่:"
   )
MSG_HASH(
   MSG_CORE_OPTIONS_FLUSH_FAILED,
   "ไม่สามารถบันทึกตัวเลือกของ Core ไปที่:"
   )
MSG_HASH(
   MSG_COULD_NOT_FIND_ANY_NEXT_DRIVER,
   "ไม่สามารถหาไดรเวอร์ถัดไปได้"
   )
MSG_HASH(
   MSG_COULD_NOT_FIND_COMPATIBLE_SYSTEM,
   "ไม่สามารถหาความเข้ากันได้ของระบบ"
   )
MSG_HASH(
   MSG_COULD_NOT_FIND_VALID_DATA_TRACK,
   "ไม่พบแทร็กข้อมูลที่ถูกต้อง"
   )
MSG_HASH(
   MSG_COULD_NOT_OPEN_DATA_TRACK,
   "ไม่สามารถเปิดแทร็กข้อมูลได้"
   )
MSG_HASH(
   MSG_COULD_NOT_READ_CONTENT_FILE,
   "ไม่สามารถอ่านไฟล์เนื้อหาได้"
   )
MSG_HASH(
   MSG_COULD_NOT_READ_MOVIE_HEADER,
   "ไม่สามารถอ่านส่วนหัวของไฟล์ภาพยนตร์ได้"
   )
MSG_HASH(
   MSG_COULD_NOT_READ_STATE_FROM_MOVIE,
   "ไม่สามารถอ่านสถานะจากภาพยนตร์ได้"
   )
MSG_HASH(
   MSG_CRC32_CHECKSUM_MISMATCH,
   "การตรวจสอบ CRC32 ไม่ตรงกันระหว่างไฟล์เนื้อหาและค่า checksum ที่บันทึกไว้ในส่วนหัวของไฟล์รีเพลย์ ทำให้มีความเป็นไปได้สูงที่การเล่นรีเพลย์จะไม่ตรงกันระหว่างการเล่นกลับ"
   )
MSG_HASH(
   MSG_CUSTOM_TIMING_GIVEN,
   "การตั้งค่าเวลาแบบกำหนดเองถูกระบุแล้ว"
   )
MSG_HASH(
   MSG_DECOMPRESSION_ALREADY_IN_PROGRESS,
   "การคลายไฟล์กำลังดำเนินการอยู่แล้ว"
   )
MSG_HASH(
   MSG_DECOMPRESSION_FAILED,
   "การคลายไฟล์ล้มเหลว"
   )
MSG_HASH(
   MSG_DETECTED_VIEWPORT_OF,
   "ตรวจพบวิวพอร์ตแล้ว"
   )
MSG_HASH(
   MSG_DID_NOT_FIND_A_VALID_CONTENT_PATCH,
   "ไม่พบแพตช์เนื้อหาที่ถูกต้อง"
   )
MSG_HASH(
   MSG_DISCONNECT_DEVICE_FROM_A_VALID_PORT,
   "ตัดการเชื่อมต่ออุปกรณ์ออกจากพอร์ตที่ถูกต้อง"
   )
MSG_HASH(
   MSG_DISK_CLOSED,
   "ปิดถาดดิสก์เสมือนแล้ว"
   )
MSG_HASH(
   MSG_DISK_EJECTED,
   "ถาดดิสก์เสมือนถูกดันออกแล้ว"
   )
MSG_HASH(
   MSG_DOWNLOADING,
   "กำลังดาวน์โหลด"
   )
MSG_HASH(
   MSG_INDEX_FILE,
   "หมายเลข"
   )
MSG_HASH(
   MSG_DOWNLOAD_FAILED,
   "ดาวน์โหลดล้มเหลว"
   )
MSG_HASH(
   MSG_ERROR,
   "ผิดพลาด"
   )
MSG_HASH(
   MSG_ERROR_LIBRETRO_CORE_REQUIRES_CONTENT,
   "คอร์ของ Libretro ต้องการไฟล์เนื้อหา แต่ไม่ได้มีการระบุไฟล์ใด ๆ"
   )
MSG_HASH(
   MSG_ERROR_LIBRETRO_CORE_REQUIRES_SPECIAL_CONTENT,
   "คอร์ของ Libretro ต้องการไฟล์เนื้อหาพิเศษ แต่ไม่ได้มีการระบุไฟล์ใด ๆ"
   )
MSG_HASH(
   MSG_ERROR_LIBRETRO_CORE_REQUIRES_VFS,
   "Core ไม่รองรับ VFS และการโหลดจากสำเนาในเครื่องล้มเหลว"
   )
MSG_HASH(
   MSG_ERROR_PARSING_ARGUMENTS,
   "เกิดข้อผิดพลาดในการประมวลผลการส่งข้อมูล"
   )
MSG_HASH(
   MSG_ERROR_SAVING_CORE_OPTIONS_FILE,
   "เกิดข้อผิดพลาดในการบันทึกไฟล์ตัวเลือกของ Core"
   )
MSG_HASH(
   MSG_ERROR_REMOVING_CORE_OPTIONS_FILE,
   "เกิดข้อผิดพลาดในการลบไฟล์ตัวเลือกของ Core"
   )
MSG_HASH(
   MSG_ERROR_SAVING_REMAP_FILE,
   "เกิดข้อผิดพลาดในการบันทึกไฟล์รีแมป"
   )
MSG_HASH(
   MSG_ERROR_REMOVING_REMAP_FILE,
   "เกิดข้อผิดพลาดในการลบไฟล์รีแมป"
   )
MSG_HASH(
   MSG_ERROR_SAVING_SHADER_PRESET,
   "เกิดข้อผิดพลาดในการบันทึกพรีเซ็ตเชดเดอร์"
   )
MSG_HASH(
   MSG_EXTERNAL_APPLICATION_DIR,
   "ไดเรกทอรีแอปพลิเคชันภายนอก"
   )
MSG_HASH(
   MSG_EXTRACTING,
   "แตกไฟล์"
   )
MSG_HASH(
   MSG_EXTRACTING_FILE,
   "กำลังแตกไฟล์"
   )
MSG_HASH(
   MSG_FAILED_SAVING_CONFIG_TO,
   "เกิดข้อผิดพลาดในการบันทึกไฟล์การตั้งค่า"
   )
MSG_HASH(
   MSG_FAILED_TO_ACCEPT_INCOMING_SPECTATOR,
   "เกิดข้อผิดพลาดในการยอมรับผู้ชมที่เข้ามา"
   )
MSG_HASH(
   MSG_FAILED_TO_ALLOCATE_MEMORY_FOR_PATCHED_CONTENT,
   "เกิดข้อผิดพลาดในการจัดสรรหน่วยความจำสำหรับไฟล์เนื้อหาที่ถูกแพตช์..."
   )
MSG_HASH(
   MSG_FAILED_TO_APPLY_SHADER,
   "เกิดข้อผิดพลาดในการใช้เชดเดอร์"
   )
MSG_HASH(
   MSG_FAILED_TO_APPLY_SHADER_PRESET,
   "เกิดข้อผิดพลาดในการใช้พรีเซ็ตเชดเดอร์:"
   )
MSG_HASH(
   MSG_FAILED_TO_BIND_SOCKET,
   "เกิดข้อผิดพลาดในการเชื่อมต่อ socket"
   )
MSG_HASH(
   MSG_FAILED_TO_CREATE_THE_DIRECTORY,
   "เกิดข้อผิดพลาดในการสร้างไดเรกทอรี"
   )
MSG_HASH(
   MSG_FAILED_TO_EXTRACT_CONTENT_FROM_COMPRESSED_FILE,
   "เกิดข้อผิดพลาดในการแยกเนื้อหาจากไฟล์บีบอัด"
   )
MSG_HASH(
   MSG_FAILED_TO_GET_NICKNAME_FROM_CLIENT,
   " เกิดข้อผิดพลาดในการดึงชื่อเล่นจากไคลเอนต์"
   )
MSG_HASH(
   MSG_FAILED_TO_LOAD,
   "โหลดล้มเหลว"
   )
MSG_HASH(
   MSG_FAILED_TO_LOAD_CONTENT,
   "ไม่สามารถโหลดเนื้อหาได้"
   )
MSG_HASH(
   MSG_FAILED_TO_LOAD_FROM_PLAYLIST,
   "ไม่สามารถโหลดจากเพลย์ลิสต์ได้"
   )
MSG_HASH(
   MSG_FAILED_TO_LOAD_MOVIE_FILE,
   "ไม่สามารถโหลดไฟล์ภาพยนตร์ได้"
   )
MSG_HASH(
   MSG_FAILED_TO_LOAD_OVERLAY,
   "ไม่สามารถโหลดโอเวอร์เลย์ได้"
   )
MSG_HASH(
   MSG_OSK_OVERLAY_NOT_SET,
   "ไม่ได้ตั้งค่าโอเวอร์เลย์แป้นพิมพ์"
   )
MSG_HASH(
   MSG_FAILED_TO_LOAD_STATE,
   "ไม่สามารถโหลดสถานะจาก"
   )
MSG_HASH(
   MSG_FAILED_TO_OPEN_LIBRETRO_CORE,
   "ไม่สามารถเปิดแกนหลัก libretro ได้"
   )
MSG_HASH(
   MSG_FAILED_TO_PATCH,
   "ไม่สามารถแพตช์ได้"
   )
MSG_HASH(
   MSG_FAILED_TO_RECEIVE_HEADER_FROM_CLIENT,
   "ไม่สามารถรับส่วนหัวจากไคลเอนต์ได้"
   )
MSG_HASH(
   MSG_FAILED_TO_RECEIVE_NICKNAME,
   "ไม่สามารถรับชื่อเล่นได้"
   )
MSG_HASH(
   MSG_FAILED_TO_RECEIVE_NICKNAME_FROM_HOST,
   "ไม่สามารถรับชื่อเล่นจากโฮสต์ได้"
   )
MSG_HASH(
   MSG_FAILED_TO_RECEIVE_NICKNAME_SIZE_FROM_HOST,
   "ไม่สามารถรับขนาดชื่อเล่นจากโฮสต์ได้"
   )
MSG_HASH(
   MSG_FAILED_TO_RECEIVE_SRAM_DATA_FROM_HOST,
   "ไม่สามารถรับข้อมูล SRAM จากโฮสต์ได้"
   )
MSG_HASH(
   MSG_FAILED_TO_REMOVE_DISK_FROM_TRAY,
   "ไม่สามารถนำแผ่นออกจากถาดได้"
   )
MSG_HASH(
   MSG_FAILED_TO_REMOVE_TEMPORARY_FILE,
   "ไม่สามารถลบไฟล์ชั่วคราวได้"
   )
MSG_HASH(
   MSG_FAILED_TO_SAVE_SRAM,
   "ไม่สามารถบันทึกข้อมูล SRAM ได้"
   )
MSG_HASH(
   MSG_FAILED_TO_LOAD_SRAM,
   "ไม่สามารถโหลดข้อมูล SRAM ได้"
   )
MSG_HASH(
   MSG_FAILED_TO_SAVE_STATE_TO,
   "ล้มเหลวในการบันทึกสถานะไปยัง"
   )
MSG_HASH(
   MSG_FAILED_TO_SEND_NICKNAME,
   "ไม่สามารถส่งชื่อเล่นได้"
   )
MSG_HASH(
   MSG_FAILED_TO_SEND_NICKNAME_SIZE,
   "ไม่สามารถส่งขนาดชื่อเล่นได้"
   )
MSG_HASH(
   MSG_FAILED_TO_SEND_NICKNAME_TO_CLIENT,
   "ไม่สามารถส่งชื่อเล่นไปยังไคลเอนต์ได้"
   )
MSG_HASH(
   MSG_FAILED_TO_SEND_NICKNAME_TO_HOST,
   "ไม่สามารถส่งขนาดชื่อเล่นไปยังโฮสต์ได้"
   )
MSG_HASH(
   MSG_FAILED_TO_SEND_SRAM_DATA_TO_CLIENT,
   "ไม่สามารถส่งข้อมูล SRAM ไปยังไคลเอนต์ได้"
   )
MSG_HASH(
   MSG_FAILED_TO_START_AUDIO_DRIVER,
   "ไม่สามารถเริ่มไดรเวอร์เสียงได้ ระบบจะทำงานต่อไปโดยไม่มีเสียง"
   )
MSG_HASH(
   MSG_FAILED_TO_START_MOVIE_RECORD,
   "ไม่สามารถเริ่มการบันทึกภาพยนตร์ได้"
   )
MSG_HASH(
   MSG_FAILED_TO_START_RECORDING,
   "ไม่สามารถเริ่มการบันทึกได้"
   )
MSG_HASH(
   MSG_FAILED_TO_TAKE_SCREENSHOT,
   "ไม่สามารถจับภาพหน้าจอได้"
   )
MSG_HASH(
   MSG_FAILED_TO_UNDO_LOAD_STATE,
   "ไม่สามารถเลิกทำการโหลดสถานะได้"
   )
MSG_HASH(
   MSG_FAILED_TO_UNDO_SAVE_STATE,
   "ล้มเหลวในการเลิกทำ บันทึกสถานะ"
   )
MSG_HASH(
   MSG_FAILED_TO_UNMUTE_AUDIO,
   "ไม่สามารถเลิกปิดเสียงได้"
   )
MSG_HASH(
   MSG_FATAL_ERROR_RECEIVED_IN,
   "เกิดข้อผิดพลาดร้ายแรงได้รับ"
   )
MSG_HASH(
   MSG_FILE_NOT_FOUND,
   "ไม่พบไฟล์"
   )
MSG_HASH(
   MSG_FOUND_AUTO_SAVESTATE_IN,
   "พบการบันทึกสถานะอัตโนมัติใน"
   )
MSG_HASH(
   MSG_FOUND_DISK_LABEL,
   "พบป้ายชื่อดิสก์"
   )
MSG_HASH(
   MSG_FOUND_FIRST_DATA_TRACK_ON_FILE,
   "พบแทร็กข้อมูลแรกในไฟล์"
   )
MSG_HASH(
   MSG_FOUND_LAST_STATE_SLOT,
   "พบช่องสถานะสุดท้าย"
   )
MSG_HASH(
   MSG_FOUND_LAST_REPLAY_SLOT,
   "พบช่องรีเพลย์สุดท้าย"
   )
MSG_HASH(
   MSG_REPLAY_LOAD_STATE_FAILED_INCOMPAT,
   "ไม่ใช่จากการบันทึกปัจจุบัน"
   )
MSG_HASH(
   MSG_REPLAY_LOAD_STATE_HALT_INCOMPAT,
   "ไม่ม่สามารถใช้ร่วมกับการรีเพลย์ได้"
   )
MSG_HASH(
   MSG_REPLAY_LOAD_STATE_FAILED_FUTURE_STATE,
   "ไม่สามารถโหลดสถานะล่วงหน้าในระหว่างการเล่นได้"
   )
MSG_HASH(
   MSG_REPLAY_LOAD_STATE_FAILED_WRONG_TIMELINE,
   "เกิดข้อผิดพลาดไทม์ไลน์ที่ไม่ถูกต้องระหว่างการเล่น"
   )
MSG_HASH(
   MSG_REPLAY_LOAD_STATE_OVERWRITING_REPLAY,
   "ไทม์ไลน์ไม่ถูกต้อง; กำลังเขียนทับการบันทึก"
   )
MSG_HASH(
   MSG_REPLAY_SEEK_TO_PREV_CHECKPOINT,
   "ย้อนกลับ"
   )
MSG_HASH(
   MSG_REPLAY_SEEK_TO_PREV_CHECKPOINT_FAILED,
   "การย้อนกลับล้มเหลว"
   )
MSG_HASH(
   MSG_REPLAY_SEEK_TO_NEXT_CHECKPOINT,
   "กรอไปข้างหน้า"
   )
MSG_HASH(
   MSG_REPLAY_SEEK_TO_NEXT_CHECKPOINT_FAILED,
   "กรอไปข้างหน้าล้มเหลว"
   )
MSG_HASH(
   MSG_REPLAY_SEEK_TO_FRAME,
   "กรอสำเร็จ"
   )
MSG_HASH(
   MSG_REPLAY_SEEK_TO_FRAME_FAILED,
   "กรอล้มเหลว"
   )
MSG_HASH(
   MSG_FOUND_SHADER,
   "พบเชดเดอร์"
   )
MSG_HASH(
   MSG_FRAMES,
   "เฟรม"
   )
MSG_HASH(
   MSG_GAME_SPECIFIC_CORE_OPTIONS_FOUND_AT,
   "พบตัวเลือกหลักที่เฉพาะเกม"
   )
MSG_HASH(
   MSG_FOLDER_SPECIFIC_CORE_OPTIONS_FOUND_AT,
   "พบตัวเลือกหลักที่เฉพาะโฟลเดอร์"
   )
MSG_HASH(
   MSG_GOT_INVALID_DISK_INDEX,
   "หมายเลขแผ่นดิสก์ไม่ถูกต้อง"
   )
MSG_HASH(
   MSG_GRAB_MOUSE_STATE,
   "ดึงสถานะเมาส์"
   )
MSG_HASH(
   MSG_GAME_FOCUS_ON,
   "โฟกัสเกม"
   )
MSG_HASH(
   MSG_GAME_FOCUS_OFF,
   "โฟกัสเกมปิดใช้งาน"
   )
MSG_HASH(
   MSG_HW_RENDERED_MUST_USE_POSTSHADED_RECORDING,
   "Libretro core ใช้การเรนเดอร์ด้วยฮาร์ดแวร์ จำเป็นต้องใช้การบันทึกแบบ post-shaded ด้วย"
   )
MSG_HASH(
   MSG_INFLATED_CHECKSUM_DID_NOT_MATCH_CRC32,
   "Inflated checksum ไม่ตรงกับค่า CRC32"
   )
MSG_HASH(
   MSG_INPUT_CHEAT,
   "ใส่สูตรโกง"
   )
MSG_HASH(
   MSG_INPUT_CHEAT_FILENAME,
   "ชื่อไฟล์สูตรโกงสำหรับอินพุต"
   )
MSG_HASH(
   MSG_INPUT_PRESET_FILENAME,
   "ชื่อไฟล์พรีเซ็ตสำหรับอินพุต"
   )
MSG_HASH(
   MSG_INPUT_OVERRIDE_FILENAME,
   "ชื่อไฟล์แทนที่สำหรับอินพุต"
   )
MSG_HASH(
   MSG_INPUT_REMAP_FILENAME,
   "ชื่อไฟล์รีแมปสำหรับอินพุต"
   )
MSG_HASH(
   MSG_INPUT_RENAME_ENTRY,
   "เปลี่ยนชื่อหัวข้อ"
   )
MSG_HASH(
   MSG_INTERFACE,
   "หน้าตา"
   )
MSG_HASH(
   MSG_INTERNAL_STORAGE,
   "ที่เก็บข้อมูลภายใน"
   )
MSG_HASH(
   MSG_REMOVABLE_STORAGE,
   "ที่เก็บข้อมูลแบบถอดได้"
   )
MSG_HASH(
   MSG_INVALID_NICKNAME_SIZE,
   "ขนาดชื่อเล่นไม่ถูกต้อง"
   )
MSG_HASH(
   MSG_IN_BYTES,
   "เป็นไบต์"
   )
MSG_HASH(
   MSG_IN_MEGABYTES,
   "เป็นเมกะไบต์"
   )
MSG_HASH(
   MSG_IN_GIGABYTES,
   "เป็นกิกะไบต์"
   )
MSG_HASH(
   MSG_LIBRETRO_ABI_BREAK,
   "ถูกคอมไพล์กับเวอร์ชันของ libretro ที่ต่างจากการทำงานของ libretro เวอร์ชันนี้"
   )
MSG_HASH(
   MSG_LIBRETRO_FRONTEND,
   "Frontend สำหรับ libretro"
   )
MSG_HASH(
   MSG_LOADED_STATE_FROM_SLOT,
   "โหลดสถานะจากช่อง: %d"
   )
MSG_HASH(
   MSG_LOADED_STATE_FROM_SLOT_AUTO,
   "โหลดสถานะจากช่อง: อัตโนมัติ"
   )
MSG_HASH(
   MSG_LOADING,
   "กำลังโหลด"
   )
MSG_HASH(
   MSG_FIRMWARE,
   "ไฟล์เฟิร์มแวร์หนึ่งไฟล์หรือมากกว่านั้นหายไป"
   )
MSG_HASH(
   MSG_LOADING_CONTENT_FILE,
   "กำลังโหลดไฟล์เนื้อหา"
   )
MSG_HASH(
   MSG_LOADING_HISTORY_FILE,
   "กำลังโหลดไฟล์ประวัติ"
   )
MSG_HASH(
   MSG_LOADING_FAVORITES_FILE,
   "ไฟล์รายการโปรดกำลังโหลดอยู่"
   )
MSG_HASH(
   MSG_LOADING_STATE,
   "กำลังโหลดไฟล์สถานะ"
   )
MSG_HASH(
   MSG_MEMORY,
   "หน่วยความจำ"
   )
MSG_HASH(
   MSG_MOVIE_FILE_IS_NOT_A_VALID_REPLAY_FILE,
   "ไฟล์ภาพยนตร์รีเพลย์ที่นำเข้าไม่ใช่ไฟล์ REPLAY ที่ถูกต้อง"
   )
MSG_HASH(
   MSG_MOVIE_FORMAT_DIFFERENT_SERIALIZER_VERSION,
   "รูปแบบไฟล์ภาพยนตร์รีเพลย์ที่นำเข้าดูเหมือนจะใช้เวอร์ชันตัวแปลงข้อมูลต่างออกไป ซึ่งมีแนวโน้มว่าจะล้มเหลว"
   )
MSG_HASH(
   MSG_MOVIE_PLAYBACK_ENDED,
   "การเล่นไฟล์ภาพยนตร์รีเพลย์ที่นำเข้าสิ้นสุดลง"
   )
MSG_HASH(
   MSG_MOVIE_RECORD_STOPPED,
   "หยุดการบันทึกภาพยนตร์"
   )
MSG_HASH(
   MSG_NETPLAY_FAILED,
   "ไม่สามารถเริ่มต้นการเล่นผ่านเครือข่ายได้"
   )
MSG_HASH(
   MSG_NETPLAY_UNSUPPORTED,
   "Core ไม่รองรับ Netplay"
   )
MSG_HASH(
   MSG_NO_CONTENT_STARTING_DUMMY_CORE,
   "ไม่มีเนื้อหา กำลังเริ่ม dummy Core"
   )
MSG_HASH(
   MSG_NO_SAVE_STATE_HAS_BEEN_OVERWRITTEN_YET,
   "ยังไม่มีการเขียนทับไฟล์สถานะบันทึก"
   )
MSG_HASH(
   MSG_NO_STATE_HAS_BEEN_LOADED_YET,
   "ยังไม่มีการโหลดไฟล์สถานะบันทึก"
   )
MSG_HASH(
   MSG_OVERRIDES_ERROR_SAVING,
   "เกิดข้อผิดพลาดในการบันทึกการเขียนทับ"
   )
MSG_HASH(
   MSG_OVERRIDES_ERROR_REMOVING,
   "เกิดข้อผิดพลาดในการลบการเขียนทับ"
   )
MSG_HASH(
   MSG_OVERRIDES_SAVED_SUCCESSFULLY,
   "การบันทึกการเขียนทับเสร็จสมบูรณ์แล้ว"
   )
MSG_HASH(
   MSG_OVERRIDES_REMOVED_SUCCESSFULLY,
   "การลบการเขียนทับเสร็จสมบูรณ์แล้ว"
   )
MSG_HASH(
   MSG_OVERRIDES_UNLOADED_SUCCESSFULLY,
   "การลบการเขียนทับเสร็จสมบูรณ์แล้ว"
   )
MSG_HASH(
   MSG_OVERRIDES_NOT_SAVED,
   "ไม่มีสิ่งที่จะบันทึก การเขียนทับไม่ได้ถูกบันทึก"
   )
MSG_HASH(
   MSG_OVERRIDES_ACTIVE_NOT_SAVING,
   "ไม่บันทึก การเขียนทับยังทำงานอยู่"
   )
MSG_HASH(
   MSG_PAUSED,
   "หยุดชั่วคราว"
   )
MSG_HASH(
   MSG_READING_FIRST_DATA_TRACK,
   "กำลังอ่านแทร็กข้อมูลแรก…"
   )
MSG_HASH(
   MSG_RECORDING_TERMINATED_DUE_TO_RESIZE,
   "การบันทึกถูกยุติลงเนื่องจากมีการปรับขนาด"
   )
MSG_HASH(
   MSG_RECORDING_TO,
   "บันทึกไปยัง"
   )
MSG_HASH(
   MSG_REDIRECTING_CHEATFILE_TO,
   "เปลี่ยนเส้นทางไฟล์สูตรโกงไปยัง"
   )
MSG_HASH(
   MSG_REDIRECTING_SAVEFILE_TO,
   "เปลี่ยนเส้นทางไฟล์บันทึกไปยัง"
   )
MSG_HASH(
   MSG_REDIRECTING_SAVESTATE_TO,
   "การเปลี่ยนเส้นทางไฟล์บันทึกสถานะไปยัง"
   )
MSG_HASH(
   MSG_REMAP_FILE_SAVED_SUCCESSFULLY,
   "บันทึกไฟล์รีแมปสำเร็จ"
   )
MSG_HASH(
   MSG_REMAP_FILE_REMOVED_SUCCESSFULLY,
   "ลบไฟล์รีแมปสำเร็จ"
   )
MSG_HASH(
   MSG_REMAP_FILE_RESET,
   "รีเซ็ตตัวเลือกการรีแมปอินพุตทั้งหมดกลับเป็นค่าเริ่มต้น"
   )
MSG_HASH(
   MSG_REMOVED_DISK_FROM_TRAY,
   "นำแผ่นออกจากถาด"
   )
MSG_HASH(
   MSG_REMOVING_TEMPORARY_CONTENT_FILE,
   "การลบไฟล์เนื้อหาชั่วคราว"
   )
MSG_HASH(
   MSG_RESET,
   "รีเซ็ต"
   )
MSG_HASH(
   MSG_RESTARTING_RECORDING_DUE_TO_DRIVER_REINIT,
   "เริ่มต้นการบันทึกใหม่เนื่องจากรีเซ็ตไดรเวอร์"
   )
MSG_HASH(
   MSG_RESTORED_OLD_SAVE_STATE,
   "เรียกคืนไฟล์บันทึกสถานะเก่า"
   )
MSG_HASH(
   MSG_RESTORING_DEFAULT_SHADER_PRESET_TO,
   "เชดเดอร์: กำลังคืนค่าเพรสเซ็ตเชดเดอร์เริ่มต้น"
   )
MSG_HASH(
   MSG_REVERTING_SAVEFILE_DIRECTORY_TO,
   "กำลังคืนค่าไดเรกทอรีไฟล์บันทึกไปยัง"
   )
MSG_HASH(
   MSG_REVERTING_SAVESTATE_DIRECTORY_TO,
   "กำลังคืนค่าไดเรกทอรีไฟล์บันทึกสถานะไปยัง"
   )
MSG_HASH(
   MSG_REWINDING,
   "กำลังย้อนกลับ"
   )
MSG_HASH(
   MSG_REWIND_BUFFER_CAPACITY_INSUFFICIENT,
   "ความจุของบัฟเฟอร์ไม่เพียงพอ"
   )
MSG_HASH(
   MSG_REWIND_UNSUPPORTED,
   "Core นี้ไม่สามารถบันทึกสถานะในรูปแบบที่จัดเรียงต่อเนื่อง จึงไม่สามารถใช้ฟังก์ชันย้อนกลับได้"
   )
MSG_HASH(
   MSG_REWIND_INIT,
   "กำลังเริ่มต้นบัฟเฟอร์ย้อนกลับด้วยขนาด"
   )
MSG_HASH(
   MSG_REWIND_INIT_FAILED,
   "ไม่สามารถเริ่มต้นบัฟเฟอร์ย้อนกลับได้ การย้อนกลับจะถูกปิดใช้งาน"
   )
MSG_HASH(
   MSG_REWIND_INIT_FAILED_THREADED_AUDIO,
   "การทำงานใช้ระบบเสียงแบบเธรด ไม่สามารถใช้การย้อนกลับได้"
   )
MSG_HASH(
   MSG_REWIND_REACHED_END,
   "ถึงจุดสิ้นสุดของบัฟเฟอร์ย้อนกลับแล้ว"
   )
MSG_HASH(
   MSG_SAVED_NEW_CONFIG_TO,
   "บันทึกการตั้งค่าไปยัง"
   )
MSG_HASH(
   MSG_SAVED_STATE_TO_SLOT,
   "บันทึกสถานะไปยังช่อง: %d"
   )
MSG_HASH(
   MSG_SAVED_STATE_TO_SLOT_AUTO,
   "บันทึกสถานะไปยังช่อง: อัตโนมัติ"
   )
MSG_HASH(
   MSG_SAVED_SUCCESSFULLY_TO,
   "บันทึกสำเร็จไปยัง"
   )
MSG_HASH(
   MSG_SAVING_RAM_TYPE,
   "กำลังบันทึกประเภท RAM"
   )
MSG_HASH(
   MSG_SAVING_STATE,
   "กำลังบันทึกสถานะ"
   )
MSG_HASH(
   MSG_SCANNING,
   "กำลังสแกน"
   )
MSG_HASH(
   MSG_SCANNING_OF_DIRECTORY_FINISHED,
   "การสแกนโฟลเดอร์เสร็จสิ้นแล้ว"
   )
MSG_HASH(
   MSG_SCANNING_NO_DATABASE,
   "การสแกนไม่สำเร็จ ไม่พบฐานข้อมูล"
   )
MSG_HASH(
   MSG_SENDING_COMMAND,
   "ส่งคำสั่ง"
   )
MSG_HASH(
   MSG_SEVERAL_PATCHES_ARE_EXPLICITLY_DEFINED,
   "มีการกำหนดแพตช์หลายรายการไว้อย่างชัดเจน กำลังละเว้นทั้งหมด…"
   )
MSG_HASH(
   MSG_SHADER,
   "เชดเดอร์"
   )
MSG_HASH(
   MSG_SHADER_PRESET_SAVED_SUCCESSFULLY,
   "บันทึกพรีเซ็ตเชดเดอร์สำเร็จ"
   )
MSG_HASH(
   MSG_SLOW_MOTION,
   "สโลว์โมชั่น"
   )
MSG_HASH(
   MSG_FAST_FORWARD,
   "เร่งความเร็ว"
   )
MSG_HASH(
   MSG_SLOW_MOTION_REWIND,
   "ย้อนกลับแบบสโลว์โมชั่น"
   )
MSG_HASH(
   MSG_SKIPPING_SRAM_LOAD,
   "ข้ามการโหลด SRAM"
   )
MSG_HASH(
   MSG_SRAM_WILL_NOT_BE_SAVED,
   "จะไม่บันทึก SRAM"
   )
MSG_HASH(
   MSG_BLOCKING_SRAM_OVERWRITE,
   "บล็อกการเขียนทับ SRAM"
   )
MSG_HASH(
   MSG_STARTING_MOVIE_PLAYBACK,
   "เริ่มเล่นภาพยนตร์"
   )
MSG_HASH(
   MSG_STARTING_MOVIE_RECORD_TO,
   "เริ่มบันทึกภาพยนตร์"
   )
MSG_HASH(
   MSG_STATE_SIZE,
   "ขนาดของสถานะ"
   )
MSG_HASH(
   MSG_STATE_SLOT,
   "ช่องบันทึก"
   )
MSG_HASH(
   MSG_REPLAY_SLOT,
   "ช่องรีเพลย์"
   )
MSG_HASH(
   MSG_TAKING_SCREENSHOT,
   "กำลังจับภาพหน้าจอ"
   )
MSG_HASH(
   MSG_SCREENSHOT_SAVED,
   "บันทึกภาพหน้าจอเรียบร้อย"
   )
MSG_HASH(
   MSG_ACHIEVEMENT_UNLOCKED,
   "ปลดล็อกความสำเร็จ"
   )
MSG_HASH(
   MSG_RARE_ACHIEVEMENT_UNLOCKED,
   "ปลดล็อกความสำเร็จหายาก"
   )
MSG_HASH(
   MSG_LEADERBOARD_STARTED,
   "เริ่มความพยายามในลีดเดอร์บอร์ด"
   )
MSG_HASH(
   MSG_LEADERBOARD_FAILED,
   "ความพยายามในลีดเดอร์บอร์ดล้มเหลว"
   )
MSG_HASH(
   MSG_LEADERBOARD_SUBMISSION,
   "ส่งข้อมูล %s สำหรับ %s" /* Submitted [value] for [leaderboard name] */
   )
MSG_HASH(
   MSG_LEADERBOARD_RANK,
   "อันดับ: %d" /* Rank: [leaderboard rank] */
   )
MSG_HASH(
   MSG_LEADERBOARD_BEST,
   "ดีที่สุด: %s" /* Best: [value] */
   )
MSG_HASH(
   MSG_CHANGE_THUMBNAIL_TYPE,
   "เปลี่ยนประเภทภาพตัวอย่าง"
   )
MSG_HASH(
   MSG_TOGGLE_FULLSCREEN_THUMBNAILS,
   "ภาพตัวอย่างแบบเต็มหน้าจอ"
   )
MSG_HASH(
   MSG_TOGGLE_CONTENT_METADATA,
   "เปิด/ปิด Metadata"
   )
MSG_HASH(
   MSG_NO_THUMBNAIL_AVAILABLE,
   "ไม่มีภาพตัวอย่าง ที่พร้อมใช้งาน"
   )
MSG_HASH(
   MSG_NO_THUMBNAIL_DOWNLOAD_POSSIBLE,
   "พยายามดาวน์โหลดรูปตัวอย่างที่มีอยู่ทั้งหมด สำหรับรายการนี้ในเพลย์ลิสต์แล้ว"
   )
MSG_HASH(
   MSG_PRESS_AGAIN_TO_QUIT,
   "กดอีกครั้งเพื่อออก..."
   )
MSG_HASH(
   MSG_PRESS_AGAIN_TO_CLOSE_CONTENT,
   "กดอีกครั้งเพื่อปิดเนื้อหา..."
   )
MSG_HASH(
   MSG_PRESS_AGAIN_TO_RESET,
   "กดอีกครั้งเพื่อรีเซ็ต..."
   )
MSG_HASH(
   MSG_TO,
   "ถึง"
   )
MSG_HASH(
   MSG_UNDID_LOAD_STATE,
   "ยกเลิกการโหลดสถานะบันทึกแล้ว"
   )
MSG_HASH(
   MSG_UNDOING_SAVE_STATE,
   "กำลังยกเลิกการบันทึกสถานะ"
   )
MSG_HASH(
   MSG_UNKNOWN,
   "ไม่ทราบ"
   )
MSG_HASH(
   MSG_UNPAUSED,
   "ยกเลิกการพักแล้ว"
   )
MSG_HASH(
   MSG_UNRECOGNIZED_COMMAND,
   "ได้รับคำสั่งที่ไม่รู้จัก \"%s\"\n"
   )
MSG_HASH(
   MSG_USING_CORE_NAME_FOR_NEW_CONFIG,
   "ใช้ชื่อ Core สำหรับการตั้งค่าใหม่"
   )
MSG_HASH(
   MSG_USING_LIBRETRO_DUMMY_CORE_RECORDING_SKIPPED,
   "ใช้ Core หลอกของ libretro ข้ามการบันทึก"
   )
MSG_HASH(
   MSG_VALUE_CONNECT_DEVICE_FROM_A_VALID_PORT,
   "เชื่อมต่ออุปกรณ์จากพอร์ตที่ถูกต้อง"
   )
MSG_HASH(
   MSG_VALUE_REBOOTING,
   "กำลังรีบูต..."
   )
MSG_HASH(
   MSG_VALUE_SHUTTING_DOWN,
   "กำลังปิดระบบ..."
   )
MSG_HASH(
   MSG_VERSION_OF_LIBRETRO_API,
   "เวอร์ชันของ libretro API"
   )
MSG_HASH(
   MSG_VIEWPORT_SIZE_CALCULATION_FAILED,
   "การคำนวณขนาด Viewport ล้มเหลว! จะดำเนินการต่อโดยใช้ข้อมูลดิบ ซึ่งอาจจะทำงานได้ไม่ถูกต้อง..."
   )
MSG_HASH(
   MSG_VIRTUAL_DISK_TRAY_EJECT,
   "ไม่สามารถดึงถาดแผ่นดิสก์เสมือนออกได้"
   )
MSG_HASH(
   MSG_VIRTUAL_DISK_TRAY_CLOSE,
   "ไม่สามารถปิดถาดแผ่นดิสก์เสมือนได้"
   )
MSG_HASH(
   MSG_AUTOLOADING_SAVESTATE_FROM,
   "กำลังโหลดสถานะบันทึกอัตโนมัติจาก"
   )
MSG_HASH(
   MSG_AUTOLOADING_SAVESTATE_FAILED,
   "การโหลดสถานะบันทึกอัตโนมัติจาก \"%s\" ล้มเหลว"
   )
MSG_HASH(
   MSG_AUTOLOADING_SAVESTATE_SUCCEEDED,
   "โหลดสถานะบันทึกอัตโนมัติจาก \"%s\" สำเร็จแล้ว"
   )
MSG_HASH(
   MSG_DEVICE_CONFIGURED_IN_PORT_NR,
   "กำหนดค่า %s ในพอร์ต %u แล้ว"
   )
MSG_HASH(
   MSG_DEVICE_DISCONNECTED_FROM_PORT_NR,
   "%s ถูกตัดการเชื่อมต่อจากพอร์ต %u"
   )
MSG_HASH(
   MSG_DEVICE_NOT_CONFIGURED_NR,
   "%s (%u/%u) ยังไม่ได้กำหนดค่า"
   )
MSG_HASH(
   MSG_DEVICE_NOT_CONFIGURED_FALLBACK_NR,
   "%s (%u/%u) ยังไม่ได้กำหนดค่า จะใช้ค่าสำรองแทน"
   )
MSG_HASH(
   MSG_BLUETOOTH_SCAN_COMPLETE,
   "สแกน Bluetooth เสร็จสิ้น"
   )
MSG_HASH(
   MSG_BLUETOOTH_PAIRING_REMOVED,
   "ลบการจับคู่แล้ว โปรดเริ่ม RetroArch ใหม่เพื่อเชื่อมต่อ/จับคู่ีอีกครั้ง"
   )
MSG_HASH(
   MSG_WIFI_SCAN_COMPLETE,
   "สแกน Wi-Fi เสร็จสิ้น"
   )
MSG_HASH(
   MSG_SCANNING_BLUETOOTH_DEVICES,
   "กำลังสแกนหาอุปกรณ์ Bluetooth..."
   )
MSG_HASH(
   MSG_SCANNING_WIRELESS_NETWORKS,
   "กำลังสแกนหาเครือข่ายไร้สาย..."
   )
MSG_HASH(
   MSG_ENABLING_WIRELESS,
   "กำลังเปิดใช้งาน Wi-Fi..."
   )
MSG_HASH(
   MSG_DISABLING_WIRELESS,
   "กำลังปิดใช้งาน Wi-Fi..."
   )
MSG_HASH(
   MSG_DISCONNECTING_WIRELESS,
   "กำลังตัดการเชื่อมต่อ Wi-Fi..."
   )
MSG_HASH(
   MSG_NETPLAY_LAN_SCANNING,
   "กำลังสแกนหาโฮสต์ Netplay..."
   )
MSG_HASH(
   MSG_PREPARING_FOR_CONTENT_SCAN,
   "กำลังเตรียมพร้อมสำหรับการสแกนเนื้อหา..."
   )
MSG_HASH(
   MSG_INPUT_ENABLE_SETTINGS_PASSWORD,
   "ใส่รหัสผ่าน"
   )
MSG_HASH(
   MSG_INPUT_ENABLE_SETTINGS_PASSWORD_OK,
   "รหัสผ่านถูกต้อง"
   )
MSG_HASH(
   MSG_INPUT_ENABLE_SETTINGS_PASSWORD_NOK,
   "รหัสผ่านไม่ถูกต้อง"
   )
MSG_HASH(
   MSG_INPUT_KIOSK_MODE_PASSWORD,
   "ใส่รหัสผ่าน"
   )
MSG_HASH(
   MSG_INPUT_KIOSK_MODE_PASSWORD_OK,
   "รหัสผ่านถูกต้อง"
   )
MSG_HASH(
   MSG_INPUT_KIOSK_MODE_PASSWORD_NOK,
   "รหัสผ่านไม่ถูกต้อง"
   )
MSG_HASH(
   MSG_CONFIG_OVERRIDE_LOADED,
   "โหลดการตั้งค่าแทนที่แล้ว"
   )
MSG_HASH(
   MSG_GAME_REMAP_FILE_LOADED,
   "โหลดไฟล์การเปลี่ยนปุ่มสำหรับเกมแล้ว"
   )
MSG_HASH(
   MSG_DIRECTORY_REMAP_FILE_LOADED,
   "โหลดไฟล์การเปลี่ยนปุ่มสำหรับโฟลเดอร์เนื้อหาแล้ว"
   )
MSG_HASH(
   MSG_CORE_REMAP_FILE_LOADED,
   "โหลดไฟล์การเปลี่ยนปุ่มสำหรับ Core แล้ว"
   )
MSG_HASH(
   MSG_REMAP_FILE_FLUSHED,
   "บันทึกตัวเลือกการเปลี่ยนปุ่มไปที่:"
   )
MSG_HASH(
   MSG_REMAP_FILE_FLUSH_FAILED,
   "ไม่สามารถบันทึกตัวเลือกการเปลี่ยนปุ่มไปที่:"
   )
MSG_HASH(
   MSG_RUNAHEAD_ENABLED,
   "รันเฟรมล่วงหน้าเปิดใช้งานแล้ว ลบเฟรมที่ล่าช้าออก: %u"
   )
MSG_HASH(
   MSG_RUNAHEAD_ENABLED_WITH_SECOND_INSTANCE,
   "เปิดใช้งานรันเฟรมล่วงหน้าด้วยอินสแตนซ์สำรอง ลบเฟรมที่ล่าช้าออก: %u"
   )
MSG_HASH(
   MSG_RUNAHEAD_DISABLED,
   "ปิดใช้งานรันเฟรมล่วงหน้า"
   )
MSG_HASH(
   MSG_RUNAHEAD_CORE_DOES_NOT_SUPPORT_SAVESTATES,
   "ปิดการทำงาน Run-Ahead เนื่องจากคอร์นี้ไม่รองรับการบันทึกสถานะ"
   )
MSG_HASH(
   MSG_RUNAHEAD_CORE_DOES_NOT_SUPPORT_RUNAHEAD,
   "ปิดการทำงาน Run-Ahead เนื่องจากคอร์นี้ไม่มีการรองรับ Save State แบบกำหนดแน่นอน"
   )
MSG_HASH(
   MSG_RUNAHEAD_FAILED_TO_SAVE_STATE,
   "ไม่สามารถบันทึกสถานะได้ จึงปิดการทำงาน Run-Ahead"
   )
MSG_HASH(
   MSG_RUNAHEAD_FAILED_TO_LOAD_STATE,
   "ไม่สามารถโหลดสถานะได้ จึงปิดการทำงาน Run-Ahead"
   )
MSG_HASH(
   MSG_RUNAHEAD_FAILED_TO_CREATE_SECONDARY_INSTANCE,
   "ไม่สามารถสร้างอินสแตนซ์ที่สองได้ จึงทำให้ Run-Ahead ใช้งานได้เพียงอินสแตนซ์เดียว"
   )
MSG_HASH(
   MSG_PREEMPT_ENABLED,
   "เปิดใช้งาน Preemptive Frames แล้ว ลบ Latency frames: %u"
   )
MSG_HASH(
   MSG_PREEMPT_DISABLED,
   "ปิดการทำงาน Preemptive Frames แล้ว"
   )
MSG_HASH(
   MSG_PREEMPT_CORE_DOES_NOT_SUPPORT_SAVESTATES,
   "ปิดการทำงาน Preemptive Frames เนื่องจากคอร์นี้ไม่รองรับการบันทึกสถานะ"
   )
MSG_HASH(
   MSG_PREEMPT_CORE_DOES_NOT_SUPPORT_PREEMPT,
   "ปิดการทำงาน Preemptive Frames เนื่องจากคอร์นี้ไม่มีการรองรับ บันทึกสถานะ แบบกำหนดแน่นอน"
   )
MSG_HASH(
   MSG_PREEMPT_FAILED_TO_ALLOCATE,
   "ไม่สามารถจัดสรรหน่วยความจำสำหรับ Preemptive Frames ได้"
   )
MSG_HASH(
   MSG_PREEMPT_FAILED_TO_SAVE_STATE,
   "ไม่สามารถบันทึกสถานะได้ จึงปิดการทำงาน Preemptive Frames"
   )
MSG_HASH(
   MSG_PREEMPT_FAILED_TO_LOAD_STATE,
   "ไม่สามารถโหลดสถานะได้ จึงปิดการทำงาน Preemptive Frames"
   )
MSG_HASH(
   MSG_SCANNING_OF_FILE_FINISHED,
   "การสแกนไฟล์เสร็จสิ้นแล้ว"
   )
MSG_HASH(
   MSG_CHEAT_INIT_SUCCESS,
   "การค้นหาโค้ดโกงเริ่มต้นสำเร็จแล้ว"
   )
MSG_HASH(
   MSG_CHEAT_INIT_FAIL,
   "ไม่สามารถเริ่มการค้นหาโค้ดโกงได้"
   )
MSG_HASH(
   MSG_CHEAT_SEARCH_NOT_INITIALIZED,
   "การค้นหายังไม่ได้ถูกเริ่มต้น/เปิดใช้งาน"
   )
MSG_HASH(
   MSG_CHEAT_SEARCH_FOUND_MATCHES,
   "จำนวนผลลัพธ์ที่ตรงกันใหม่ = %u"
   )
MSG_HASH(
   MSG_CHEAT_SEARCH_ADDED_MATCHES_SUCCESS,
   "เพิ่มผลลัพธ์ที่ตรงกัน %u รายการ"
   )
MSG_HASH(
   MSG_CHEAT_SEARCH_ADDED_MATCHES_FAIL,
   "ไม่สามารถเพิ่มผลลัพธ์ที่ตรงกันได้"
   )
MSG_HASH(
   MSG_CHEAT_SEARCH_ADD_MATCH_SUCCESS,
   "สร้างโค้ดจากผลลัพธ์ที่ตรงกันแล้ว"
   )
MSG_HASH(
   MSG_CHEAT_SEARCH_ADD_MATCH_FAIL,
   "ไม่สามารถสร้างโค้ดได้"
   )
MSG_HASH(
   MSG_CHEAT_SEARCH_DELETE_MATCH_SUCCESS,
   "ลบรายการที่ตรงกัน"
   )
MSG_HASH(
   MSG_CHEAT_SEARCH_ADDED_MATCHES_TOO_MANY,
   "ไม่มีพื้นที่เพียงพอ จำนวนโค้ดโกงที่สามารถใช้งานพร้อมกันได้สูงสุดคือ 100"
   )
MSG_HASH(
   MSG_CHEAT_ADD_TOP_SUCCESS,
   "เพิ่มโค้ดโกงใหม่ไว้ที่ด้านบนของรายการ"
   )
MSG_HASH(
   MSG_CHEAT_ADD_BOTTOM_SUCCESS,
   "เพิ่มโค้ดโกงใหม่ไว้ที่ด้านบนของรายการ"
   )
MSG_HASH(
   MSG_CHEAT_DELETE_ALL_SUCCESS,
   "ลบโค้ดโกงทั้งหมดแล้ว"
   )
MSG_HASH(
   MSG_CHEAT_RELOAD_ALL_SUCCESS,
   "โหลดโค้ดโกงทั้งหมดขึ้นมาใหม่แล้ว"
   )
MSG_HASH(
   MSG_CHEAT_ADD_BEFORE_SUCCESS,
   "เพิ่มโค้ดโกงใหม่ก่อนโค้ดนี้"
   )
MSG_HASH(
   MSG_CHEAT_ADD_AFTER_SUCCESS,
   "เพิ่มโค้ดโกงใหม่ก่อนโค้ดนี้"
   )
MSG_HASH(
   MSG_CHEAT_COPY_BEFORE_SUCCESS,
   "คัดลอกโค้ดโกงก่อนโค้ดนี้"
   )
MSG_HASH(
   MSG_CHEAT_COPY_AFTER_SUCCESS,
   "คัดลอกโค้ดโกงหลังโค้ดนี้"
   )
MSG_HASH(
   MSG_CHEAT_DELETE_SUCCESS,
   "ลบโค้ดโกงแล้ว"
   )
MSG_HASH(
   MSG_FAILED_TO_SET_DISK,
   "ไม่สามารถตั้งค่าดิสก์ได้"
   )
MSG_HASH(
   MSG_FAILED_TO_SET_INITIAL_DISK,
   "ไม่สามารถตั้งค่าดิสก์ที่ใช้ล่าสุดได้"
   )
MSG_HASH(
   MSG_FAILED_TO_CONNECT_TO_CLIENT,
   "ไม่สามารถเชื่อมต่อกับไคลเอนต์ได้"
   )
MSG_HASH(
   MSG_FAILED_TO_CONNECT_TO_HOST,
   "ไม่สามารถเชื่อมต่อกับโฮสต์ได้"
   )
MSG_HASH(
   MSG_NETPLAY_HOST_FULL,
   "โฮสต์ Netplay เต็มแล้ว"
   )
MSG_HASH(
   MSG_NETPLAY_BANNED,
   "คุณถูกแบนจากโฮสต์นี้"
   )
MSG_HASH(
   MSG_FAILED_TO_RECEIVE_HEADER_FROM_HOST,
   "ล้มเหลวในการรับส่วนหัวข้อมูลจากโฮสต์"
   )
MSG_HASH(
   MSG_CHEEVOS_LOGGED_IN_AS_USER,
   "RetroAchievements: เข้าสู่ระบบในชื่อ \"%s\"แล้ว"
   )
MSG_HASH(
   MSG_CHEEVOS_LOAD_STATE_PREVENTED_BY_HARDCORE_MODE,
   "คุณต้องหยุดชั่วคราวหรือปิดใช้งานโหมด Hardcore ของ Achievements เพื่อโหลดสถานะบันทึกเกม"
   )
MSG_HASH(
   MSG_CHEEVOS_LOAD_SAVEFILE_PREVENTED_BY_HARDCORE_MODE,
   "คุณต้องหยุดชั่วคราวหรือปิดใช้งานโหมด Hardcore ของ Achievements เพื่อโหลดไฟล์เซฟ srm"
   )
MSG_HASH(
   MSG_CHEEVOS_HARDCORE_MODE_DISABLED,
   "โหลดสถานะบันทึกเกมแล้ว ปิดใช้งานโหมด Hardcore ของ Achievements สำหรับเซสชันปัจจุบัน"
   )
MSG_HASH(
   MSG_CHEEVOS_HARDCORE_MODE_DISABLED_CHEAT,
   "เปิดใช้งานสูตรโกงแล้ว ปิดใช้งานโหมด Hardcore ของ Achievements สำหรับเซสชันปัจจุบัน"
   )
MSG_HASH(
   MSG_CHEEVOS_HARDCORE_MODE_CHANGED_BY_HOST,
   "โหมด Hardcore ของ Achievements ถูกเปลี่ยนโดยโฮสต์"
   )
MSG_HASH(
   MSG_CHEEVOS_HARDCORE_MODE_REQUIRES_NEWER_HOST,
   "โฮสต์ Netplay จำเป็นต้องได้รับการอัปเดต ปิดใช้งานโหมด Hardcore ของ Achievements สำหรับเซสชันปัจจุบัน"
   )
MSG_HASH(
   MSG_CHEEVOS_COMPLETED_GAME,
   "สำเร็จ %s แล้ว"
   )
MSG_HASH(
   MSG_CHEEVOS_HARDCORE_MODE_ENABLE,
   "เปิดใช้งานโหมด Hardcore ของ Achievements แล้ว ปิดใช้งานการบันทึกสถานะและการย้อนกลับแล้ว"
   )
MSG_HASH(
   MSG_CHEEVOS_GAME_HAS_NO_ACHIEVEMENTS,
   "เกมนี้ไม่มี Achievements"
   )
MSG_HASH(
   MSG_CHEEVOS_ALL_ACHIEVEMENTS_ACTIVATED,
   "ความสำเร็จทั้งหมด %d รายการถูกเปิดใช้งานสำหรับเซสชันนี้"
)
MSG_HASH(
   MSG_CHEEVOS_UNOFFICIAL_ACHIEVEMENTS_ACTIVATED,
   "เปิดใช้งานความสำเร็จที่ไม่เป็นทางการ %d รายการแล้ว"
)
MSG_HASH(
   MSG_CHEEVOS_NUMBER_ACHIEVEMENTS_UNLOCKED,
   "คุณปลดล็อกความสำเร็จแล้ว %d จาก %d รายการ"
)
MSG_HASH(
   MSG_CHEEVOS_UNSUPPORTED_COUNT,
   "ไม่รองรับ %d รายการ"
)
MSG_HASH(
   MSG_CHEEVOS_UNSUPPORTED_WARNING,
   "ตรวจพบความสำเร็จที่ไม่รองรับ โปรดลองใช้ Core อื่นหรืออัปเดต RetroArch"
)
MSG_HASH(
   MSG_CHEEVOS_RICH_PRESENCE_SPECTATING,
   "กำลังรับชม %s"
   )
MSG_HASH(
   MSG_CHEEVOS_HARDCORE_PAUSED_MANUAL_FRAME_DELAY,
   "หยุดโหมดฮาร์ดคอร์ชั่วคราว ไม่อนุญาตให้ตั้งค่าการหน่วงเฟรมวิดีโอด้วยตนเอง"
   )
MSG_HASH(
   MSG_CHEEVOS_HARDCORE_PAUSED_VSYNC_SWAP_INTERVAL,
   "หยุดโหมดฮาร์ดคอร์ชั่วคราว ไม่อนุญาตให้ตั้งค่าช่วงการสลับ VSync สูงกว่า 1"
   )
MSG_HASH(
   MSG_CHEEVOS_HARDCORE_PAUSED_BLACK_FRAME_INSERTION,
   "หยุดโหมดฮาร์ดคอร์ชั่วคราว ไม่อนุญาตให้ใช้การแทรกเฟรมดำ"
   )
MSG_HASH(
   MSG_CHEEVOS_HARDCORE_PAUSED_SETTING_NOT_ALLOWED,
   "หยุดโหมดฮาร์ดคอร์ชั่วคราว ไม่อนุญาตให้ใช้การตั้งค่า: %s=%s"
   )
MSG_HASH(
   MSG_CHEEVOS_HARDCORE_PAUSED_SYSTEM_NOT_FOR_CORE,
   "หยุดโหมดฮาร์ดคอร์ชั่วคราว คุณไม่สามารถรับความสำเร็จในโหมดฮาร์ดคอร์สำหรับ %s โดยใช้ %s ได้"
   )
MSG_HASH(
   MSG_CHEEVOS_GAME_NOT_IDENTIFIED,
   "RetroAchievements: ไม่สามารถระบุตัวตนของเกมได้"
   )
MSG_HASH(
   MSG_CHEEVOS_GAME_LOAD_FAILED,
   "RetroAchievements: การโหลดเกมล้มเหลว: %s"
   )
MSG_HASH(
   MSG_CHEEVOS_CHANGE_MEDIA_FAILED,
   "RetroAchievements: การเปลี่ยนสื่อล้มเหลว: %s"
   )
MSG_HASH(
   MSG_CHEEVOS_LOGIN_TOKEN_EXPIRED,
   "RetroAchievements: การเข้าสู่ระบบหมดอายุ โปรดป้อนรหัสผ่านใหม่และโหลดเกมอีกครั้ง"
   )
MSG_HASH(
   MSG_RESAMPLER_QUALITY_LOWEST,
   "ต่ำที่สุด"
   )
MSG_HASH(
   MSG_RESAMPLER_QUALITY_LOWER,
   "ต่ำ"
   )
MSG_HASH(
   MSG_RESAMPLER_QUALITY_NORMAL,
   "ปกติ"
   )
MSG_HASH(
   MSG_RESAMPLER_QUALITY_HIGHER,
   "สูง"
   )
MSG_HASH(
   MSG_RESAMPLER_QUALITY_HIGHEST,
   "สูงสุด"
   )
MSG_HASH(
   MSG_MISSING_ASSETS,
   "คำเตือน: เนื้อหาเสริม (Assets) สูญหาย โปรดใช้ตัวอัปเดตออนไลน์ หากทำได้"
   )
MSG_HASH(
   MSG_RGUI_MISSING_FONTS,
   "คำเตือน: รูปแบบอักษร (Fonts) สำหรับภาษาที่เลือกสูญหาย โปรดใช้ตัวอัปเดตออนไลน์หากทำได้"
   )
MSG_HASH(
   MSG_RGUI_INVALID_LANGUAGE,
   "คำเตือน: ไม่รองรับภาษานี้ - จะใช้ภาษาอังกฤษแทน"
   )
MSG_HASH(
   MSG_DUMPING_DISC,
   "คัดลอกข้อมูลจากแผ่น..."
   )
MSG_HASH(
   MSG_DRIVE_NUMBER,
   "ไดรฟ์ %d"
   )
MSG_HASH(
   MSG_LOAD_CORE_FIRST,
   "โปรดโหลด Core ก่อน"
   )
MSG_HASH(
   MSG_DISC_DUMP_FAILED_TO_READ_FROM_DRIVE,
   "ไม่สามารถอ่านข้อมูลจากไดรฟ์ได้ การทำสำเนา (Dump) ถูกยกเลิก"
   )
MSG_HASH(
   MSG_DISC_DUMP_FAILED_TO_WRITE_TO_DISK,
   "ไม่สามารถเขียนข้อมูลลงดิสก์ได้ การทำสำเนา (Dump) ถูกยกเลิก"
   )
MSG_HASH(
   MSG_NO_DISC_INSERTED,
   "ไม่มีแผ่นดิสก์อยู่ในไดรฟ์"
   )
MSG_HASH(
   MSG_SHADER_PRESET_REMOVED_SUCCESSFULLY,
   "ลบค่าที่ตั้งไว้ล่วงหน้าของเชดเดอร์สำเร็จแล้ว"
   )
MSG_HASH(
   MSG_ERROR_REMOVING_SHADER_PRESET,
   "เกิดข้อผิดพลาดในการลบค่าที่ตั้งไว้ล่วงหน้าของเชดเดอร์"
   )
MSG_HASH(
   MSG_MANUAL_CONTENT_SCAN_DAT_FILE_INVALID,
   "เลือกไฟล์ DAT ของ Arcade ไม่ถูกต้อง"
   )
MSG_HASH(
   MSG_MANUAL_CONTENT_SCAN_DAT_FILE_TOO_LARGE,
   "เลือกไฟล์ DAT ของ Arcade ที่มีขนาดใหญ่เกินไป (หน่วยความจำว่างไม่เพียงพอ)"
   )
MSG_HASH(
   MSG_MANUAL_CONTENT_SCAN_DAT_FILE_LOAD_ERROR,
   "ไม่สามารถโหลดไฟล์ DAT ของ Arcade ได้ (รูปแบบไม่ถูกต้อง?)"
   )
MSG_HASH(
   MSG_MANUAL_CONTENT_SCAN_INVALID_CONFIG,
   "การตั้งค่าการสแกนด้วยตนเองไม่ถูกต้อง"
   )
MSG_HASH(
   MSG_MANUAL_CONTENT_SCAN_INVALID_CONTENT,
   "ตรวจไม่พบเนื้อหาที่ถูกต้อง"
   )
MSG_HASH(
   MSG_MANUAL_CONTENT_SCAN_START,
   "กำลังสแกนเนื้อหา: "
   )
MSG_HASH(
   MSG_MANUAL_CONTENT_SCAN_PLAYLIST_CLEANUP,
   "กำลังตรวจสอบรายการปัจจุบัน: "
   )
MSG_HASH(
   MSG_MANUAL_CONTENT_SCAN_IN_PROGRESS,
   "กำลังสแกน: "
   )
MSG_HASH(
   MSG_MANUAL_CONTENT_SCAN_M3U_CLEANUP,
   "กำลังล้างรายการ M3U: "
   )
MSG_HASH(
   MSG_MANUAL_CONTENT_SCAN_END,
   "สแกนเสร็จสมบูรณ์: "
   )
MSG_HASH(
   MSG_CORE_BACKUP_SCANNING_CORE,
   "กำลังสแกน Core: "
   )
MSG_HASH(
   MSG_CORE_BACKUP_ALREADY_EXISTS,
   "มีไฟล์สำรองของ Core ที่ติดตั้งไว้อยู่แล้ว: "
   )
MSG_HASH(
   MSG_BACKING_UP_CORE,
   "สำรองข้อมูล Core: "
   )
MSG_HASH(
   MSG_PRUNING_CORE_BACKUP_HISTORY,
   "กำลังลบไฟล์สำรองที่ล้าสมัย: "
   )
MSG_HASH(
   MSG_CORE_BACKUP_COMPLETE,
   "สำรองข้อมูล Core เสร็จสมบูรณ์: "
   )
MSG_HASH(
   MSG_CORE_RESTORATION_ALREADY_INSTALLED,
   "ไฟล์สำรองของ Core ที่เลือกถูกติดตั้งไว้อยู่แล้ว: "
   )
MSG_HASH(
   MSG_RESTORING_CORE,
   "กำลังคืนค่า Core: "
   )
MSG_HASH(
   MSG_CORE_RESTORATION_COMPLETE,
   "คืนค่า Core เสร็จสมบูรณ์: "
   )
MSG_HASH(
   MSG_CORE_INSTALLATION_ALREADY_INSTALLED,
   "ไฟล์ Core ที่เลือกถูกติดตั้งไว้อยู่แล้ว: "
   )
MSG_HASH(
   MSG_INSTALLING_CORE,
   "กำลังติดตั้ง Core: "
   )
MSG_HASH(
   MSG_CORE_INSTALLATION_COMPLETE,
   "การติดตั้งคอร์เสร็จสมบูรณ์: "
   )
MSG_HASH(
   MSG_CORE_RESTORATION_INVALID_CONTENT,
   "เลือกไฟล์ Core ไม่ถูกต้อง: "
   )
MSG_HASH(
   MSG_CORE_BACKUP_FAILED,
   "สำรองข้อมูล Core ล้มเหลว: "
   )
MSG_HASH(
   MSG_CORE_RESTORATION_FAILED,
   "การคืนค่า Core ล้มเหลว: "
   )
MSG_HASH(
   MSG_CORE_INSTALLATION_FAILED,
   "การติดตั้ง Core ล้มเหลว: "
   )
MSG_HASH(
   MSG_CORE_RESTORATION_DISABLED,
   "การคืนค่า Core ถูกปิดใช้งาน - Core ถูกล็อก: "
   )
MSG_HASH(
   MSG_CORE_INSTALLATION_DISABLED,
   "การติดตั้ง Core ถูกปิดใช้งาน - Core ถูกล็อก: "
   )
MSG_HASH(
   MSG_CORE_LOCK_FAILED,
   "ล็อก Core ล้มเหลว: "
   )
MSG_HASH(
   MSG_CORE_UNLOCK_FAILED,
   "ปลดล็อก Core ล้มเหลว: "
   )
MSG_HASH(
   MSG_CORE_SET_STANDALONE_EXEMPT_FAILED,
   "ลบ Core จากรายการ 'เนื้อหาที่ไม่มี Core' ล้มเหลว: "
   )
MSG_HASH(
   MSG_CORE_UNSET_STANDALONE_EXEMPT_FAILED,
   "เพิ่ม Core เข้ารายการ 'เนื้อหาที่ไม่มี Core' ล้มเหลว: "
   )
MSG_HASH(
   MSG_CORE_DELETE_DISABLED,
   "การลบ Core ถูกปิดใช้งาน - Core ถูกล็อก: "
   )
MSG_HASH(
   MSG_UNSUPPORTED_VIDEO_MODE,
   "ไม่รองรับโหมด Visual"
   )
MSG_HASH(
   MSG_CORE_INFO_CACHE_UNSUPPORTED,
   "ไม่สามารถเขียนลงในไดเรกทอรีข้อมูล Core - การแคชข้อมูล Core จะถูกปิดใช้งาน"
   )
MSG_HASH(
   MSG_FOUND_ENTRY_STATE_IN,
   "พบรายการบันทึกสถานะใน"
   )
MSG_HASH(
   MSG_LOADING_ENTRY_STATE_FROM,
   "กำลังโหลดรายการบันทึกสถานะจาก"
   )
MSG_HASH(
   MSG_FAILED_TO_ENTER_GAMEMODE,
   "เข้าสู่ GameMode ล้มเหลว"
   )
MSG_HASH(
   MSG_FAILED_TO_ENTER_GAMEMODE_LINUX,
   "เข้าสู่ GameMode ล้มเหลว - โปรดตรวจสอบให้แน่ใจว่าได้ติดตั้งหรือรัน GameMode daemon ไว้แล้ว"
   )
MSG_HASH(
   MSG_VRR_RUNLOOP_ENABLED,
   "เปิดใช้งาน การซิงค์ให้ตรงกับอัตราเฟรมของเนื้อหาแล้ว"
   )
MSG_HASH(
   MSG_VRR_RUNLOOP_DISABLED,
   "ปิดใช้งาน การซิงค์ให้ตรงกับอัตราเฟรมของเนื้อหาแล้ว"
   )
MSG_HASH(
   MSG_VIDEO_REFRESH_RATE_CHANGED,
   "ปรับอัตราการรีเฟรชวิดีโอเป็น %s Hz แล้ว"
   )

/* Lakka */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_LAKKA,
   "ปรับปรุง Lakka"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FRONTEND_NAME,
   "ชื่อ Frontend"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_LAKKA_VERSION,
   "เวอร์ชัน Lakka"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REBOOT,
   "เริ่มระบบใหม่"
   )

/* Environment Specific Settings */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_SPLIT_JOYCON,
   "แยก Joy-Con"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_WIDGET_SCALE_FACTOR,
   "การแทนที่มาตราส่วนวิดเจ็ตกราฟิก"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_WIDGET_SCALE_FACTOR,
   "บังคับใช้การแทนที่มาตราส่วนด้วยตนเองเมื่อวาดวิดเจ็ตแสดงผล จะมีผลเฉพาะเมื่อปิดใช้งาน 'ปรับมาตราส่วนวิดเจ็ตกราฟิกโดยอัตโนมัติ' เท่านั้น สามารถใช้เพื่อเพิ่มหรือลดขนาดของการแ[...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCREEN_RESOLUTION,
   "ความละเอียดหน้าจอ"
   )
MSG_HASH(
   MSG_SCREEN_RESOLUTION_DEFAULT,
   "ความละเอียดหน้าจอ: ค่าเริ่มต้น"
   )
MSG_HASH(
   MSG_SCREEN_RESOLUTION_NO_DESC,
   "ความละเอียดหน้าจอ: %dx%d"
   )
MSG_HASH(
   MSG_SCREEN_RESOLUTION_DESC,
   "ความละเอียดหน้าจอ: %dx%d - %s"
   )
MSG_HASH(
   MSG_SCREEN_RESOLUTION_APPLYING_DEFAULT,
   "กำลังปรับใช้: ค่าเริ่มต้น"
   )
MSG_HASH(
   MSG_SCREEN_RESOLUTION_APPLYING_NO_DESC,
   "กำลังปรับใช้: %dx%d\nกด START เพื่อรีเซ็ต"
   )
MSG_HASH(
   MSG_SCREEN_RESOLUTION_APPLYING_DESC,
   "กำลังปรับใช้: %dx%d - %s\nกด START เพื่อรีเซ็ต"
   )
MSG_HASH(
   MSG_SCREEN_RESOLUTION_RESETTING_DEFAULT,
   "กำลังเปลี่ยนกลับเป็น: ค่าเริ่มต้น"
   )
MSG_HASH(
   MSG_SCREEN_RESOLUTION_RESETTING_NO_DESC,
   "กำลังเปลี่ยนกลับเป็น: %dx%d"
   )
MSG_HASH(
   MSG_SCREEN_RESOLUTION_RESETTING_DESC,
   "กำลังเปลี่ยนกลับเป็น: %dx%d - %s"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SCREEN_RESOLUTION,
   "เลือกโหมดการแสดงผล (จำเป็นต้องรีสตาร์ท)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHUTDOWN,
   "ปิดเครื่อง"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FILE_BROWSER_OPEN_UWP_PERMISSIONS,
   "เปิดใช้งานการเข้าถึงไฟล์ภายนอก"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FILE_BROWSER_OPEN_UWP_PERMISSIONS,
   "เปิดการตั้งค่าสิทธิ์การเข้าถึงไฟล์ของ Windows"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_OPEN_UWP_PERMISSIONS,
   "เปิดการตั้งค่าสิทธิ์ของ Windows เพื่อเปิดใช้งานความสามารถ broadFileSystemAccess"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FILE_BROWSER_OPEN_PICKER,
   "เปิด..."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FILE_BROWSER_OPEN_PICKER,
   "เปิดไดเรกทอรีอื่นโดยใช้ตัวเลือกไฟล์ของระบบ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FILTER_FLICKER,
   "ตัวกรองลดการกระพริบ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_GAMMA,
   "วิดีโอแกมมา"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SOFT_FILTER,
   "ตัวกรองภาพแบบนุ่มนวล"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BLUETOOTH_SETTINGS,
   "บลูทูธ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BLUETOOTH_SETTINGS,
   "สแกนหาอุปกรณ์บลูทูธและเชื่อมต่อ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_WIFI_SETTINGS,
   "สแกนหาเครือข่ายไร้สายและสร้างการเชื่อมต่อ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_WIFI_ENABLED,
   "เปิดใช้งาน Wi-Fi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_WIFI_NETWORK_SCAN,
   "เชื่อมต่อกับเครือข่าย"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_WIFI_NETWORKS,
   "เชื่อมต่อกับเครือข่าย"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_WIFI_DISCONNECT,
   "ตัดการเชื่อมต่อ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VFILTER,
   "ลดการกะพริบของภาพ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VI_WIDTH,
   "กำหนดความกว้างหน้าจอ VI"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_OVERSCAN_CORRECTION_TOP,
   "การชดเชยส่วนที่ล้นหน้าจอ (บน)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_OVERSCAN_CORRECTION_TOP,
   "ปรับการครอบตัดส่วนที่ล้นหน้าจอ โดยการลดขนาดภาพตามจำนวนเส้นสแกนที่ระบุ (นับจากด้านบนของหน้าจอ) อาจทำให้เกิดความผิดเพี้ยนจากการปรับขนาดภาพ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_OVERSCAN_CORRECTION_BOTTOM,
   "การชดเชยส่วนที่ล้นหน้าจอ (ล่าง)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_OVERSCAN_CORRECTION_BOTTOM,
   "ปรับการครอบตัดส่วนที่ล้นหน้าจอ โดยการลดขนาดภาพตามจำนวนเส้นสแกนที่ระบุ (นับจากด้านล่างของหน้าจอ) อาจทำให้เกิดความผิดเพี้ยนจากการปรับขนาดภาพ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SUSTAINED_PERFORMANCE_MODE,
   "โหมดประสิทธิภาพที่คงที่"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_PERFPOWER,
   "ประสิทธิภาพและการใช้พลังงานของ CPU"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_POLICY_ENTRY,
   "นโยบาย"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_PERF_MODE,
   "โหมดการควบคุม"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_PERF_MODE_MANUAL,
   "กำหนดเอง"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VALUE_CPU_PERF_MODE_MANUAL,
   "อนุญาตให้ปรับแต่งทุกรายละเอียดในแต่ละ CPU ด้วยตนเอง: governor, ความถี่, และอื่นๆ แนะนำสำหรับผู้ใช้ขั้นสูงเท่านั้น"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_PERF_MODE_MANAGED_PERF,
   "ประสิทธิภาพ (จัดการโดยระบบ)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VALUE_CPU_PERF_MODE_MANAGED_PERF,
   "โหมดเริ่มต้นและโหมดที่แนะนำ ให้ประสิทธิภาพสูงสุดขณะเล่น ในขณะที่ช่วยประหยัดพลังงานเมื่อหยุดเกมหรือเรียกดูเมนู"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_PERF_MODE_MANAGED_PER_CONTEXT,
   "กำหนดเอง (จัดการโดยระบบ)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VALUE_CPU_PERF_MODE_MANAGED_PER_CONTEXT,
   "อนุญาตให้เลือก governor ที่จะใช้ในเมนูและระหว่างการเล่นเกม แนะนำให้ใช้ Performance, Ondemand หรือ Schedutil ระหว่างการเล่นเกม"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_PERF_MODE_MAX_PERF,
   "ประสิทธิภาพสูงสุด"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VALUE_CPU_PERF_MODE_MAX_PERF,
   "ใช้ประสิทธิภาพสูงสุดเสมอ: ใช้ความถี่สูงสุดเพื่อประสบการณ์การใช้งานที่ดีที่สุด"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_PERF_MODE_MIN_POWER,
   "ใช้พลังงานต่ำสุด"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VALUE_CPU_PERF_MODE_MIN_POWER,
   "ใช้ความถี่ต่ำสุดเท่าที่มีเพื่อประหยัดพลังงาน มีประโยชน์สำหรับอุปกรณ์ที่ใช้แบตเตอรี่ แต่ประสิทธิภาพจะลดลงอย่างมาก"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_PERF_MODE_BALANCED,
   "สมดุล"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VALUE_CPU_PERF_MODE_BALANCED,
   "ปรับเปลี่ยนตามภาระงานในขณะนั้น ทำงานได้ดีกับอุปกรณ์และโปรแกรมจำลองส่วนใหญ่ และช่วยประหยัดพลังงาน เกมหรือ Core ที่ใช้ทรัพยากรสูงอาจมีประสิทธิภาพลดลงในอุปกรณ์บางเครื่อง"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_POLICY_MIN_FREQ,
   "ความถี่ต่ำสุด"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_POLICY_MAX_FREQ,
   "ความถี่สูงสุด"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_MANAGED_MIN_FREQ,
   "ความถี่ Core ต่ำสุด"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_MANAGED_MAX_FREQ,
   "ความถี่ Core สูงสุด"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_POLICY_MENU_GOVERNOR,
   "Governor เมนู"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GAMEMODE_ENABLE,
   "โหมดเกม"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GAMEMODE_ENABLE_LINUX,
   "สามารถปรับปรุงประสิทธิภาพ, ลดความหน่วง และแก้ไขปัญหาเสียงแตกพร่าได้ คุณจำเป็นต้องมี https://github.com/FeralInteractive/gamemode เพื่อให้ฟีเจอร์นี้ทำงานได้"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_GAMEMODE_ENABLE,
   "การเปิดใช้งาน Linux GameMode สามารถลดความหน่วง, แก้ไขปัญหาเสียงแตกพร่า และเพิ่มประสิทธิภาพโดยรวมให้สูงสุดได้ โดยการกำหนดค่า CPU และ GPU ให้ทำงานได้ดีที่สุดโดยอัตโนมัติ\nทั้งนี้จำเป็นต้องติ[...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PAL60_ENABLE,
   "ใช้โหมด PAL60"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_RESTART_KEY,
   "เริ่ม RetroArch ใหม่"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_RESTART_KEY,
   "ออกจากระบบแล้วเริ่ม RetroArch ใหม่ จำเป็นสำหรับการเปิดใช้งานการตั้งค่าเมนูบางอย่าง (ตัวอย่างเช่น เมื่อมีการเปลี่ยนไดรเวอร์เมนู)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_BLOCK_FRAMES,
   "ปิดกั้นเฟรม"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_PREFER_FRONT_TOUCH,
   "ให้ความสำคัญกับการสัมผัสด้านหน้า"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_INPUT_PREFER_FRONT_TOUCH,
   "ใช้การสัมผัสด้านหน้าแทนด้านหลัง"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TOUCH_ENABLE,
   "สัมผัส"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ICADE_ENABLE,
   "การจับคู่คีย์บอร์ดกับคอนโทรลเลอร์"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_KEYBOARD_GAMEPAD_MAPPING_TYPE,
   "ประเภทการจับคู่คีย์บอร์ดกับคอนโทรลเลอร์"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_SMALL_KEYBOARD_ENABLE,
   "คีย์บอร์ดขนาดเล็ก"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_BLOCK_TIMEOUT,
   "เวลาหมดการปิดกั้นอินพุต"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_BLOCK_TIMEOUT,
   "จำนวนมิลลิวินาทีที่ต้องรอเพื่อให้ได้ตัวอย่างอินพุตที่สมบูรณ์ ใช้ในกรณีที่คุณมีปัญหาเกี่ยวกับการกดปุ่มพร้อมกัน (สำหรับ Android เท่านั้น)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_REBOOT,
   "แสดง 'เริ่มระบบใหม่'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_REBOOT,
   "แสดงตัวเลือก 'เริ่มระบบใหม่'"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_SHUTDOWN,
   "แสดงตัวเลือก 'ปิดเครื่อง'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_SHUTDOWN,
   "แสดงตัวเลือก 'ปิดเครื่อง'"
   )
MSG_HASH(
   MSG_ROOM_PASSWORDED,
   "ใส่รหัสผ่านแล้ว"
   )
MSG_HASH(
   MSG_INTERNET,
   "อินเทอร์เน็ต"
   )
MSG_HASH(
   MSG_INTERNET_RELAY,
   "อินเทอร์เน็ต (Relay)"
   )
MSG_HASH(
   MSG_INTERNET_NOT_CONNECTABLE,
   "อินเทอร์เน็ต (ไม่สามารถเชื่อมต่อได้)"
   )
MSG_HASH(
   MSG_LOCAL,
   "ในเครื่อง"
   )
MSG_HASH(
   MSG_READ_WRITE,
   "สถานะหน่วยความจำภายใน: อ่าน/เขียน"
   )
MSG_HASH(
   MSG_READ_ONLY,
   "สถานะหน่วยความจำภายใน: อ่านอย่างเดียว"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BRIGHTNESS_CONTROL,
   "ความสว่างหน้าจอ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BRIGHTNESS_CONTROL,
   "เพิ่มหรือลดความสว่างหน้าจอ"
   )
#ifdef HAVE_LIBNX
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SWITCH_CPU_PROFILE,
   "โอเวอร์คล็อก CPU"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SWITCH_CPU_PROFILE,
   "โอเวอร์คล็อก CPU ของ Switch"
   )
#endif
#ifdef HAVE_LAKKA
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BLUETOOTH_ENABLE,
   "บลูทูธ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BLUETOOTH_ENABLE,
   "ตรวจสอบสถานะของบลูทูธ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LAKKA_SERVICES,
   "บริการ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SERVICES_SETTINGS,
   "จัดการบริการในระดับระบบปฏิบัติการ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAMBA_ENABLE,
   "แชร์โฟลเดอร์เครือข่ายผ่านโปรโตคอล SMB"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SSH_ENABLE,
   "ใช้ SSH เพื่อเข้าถึงบรรทัดคำสั่งจากระยะไกล"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOCALAP_ENABLE,
   "จุดเข้าใช้งาน Wi-Fi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOCALAP_ENABLE,
   "เปิดหรือปิดใช้งานจุดกระจายสัญญาณ Wi-Fi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEZONE,
   "เขตเวลา"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_TIMEZONE,
   "เลือกเขตเวลาของคุณเพื่อปรับวันที่และเวลาให้เข้ากับตำแหน่งที่ตั้งของคุณ"
   )
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
MSG_HASH(
   MENU_ENUM_LABEL_HELP_TIMEZONE,
   "แสดงรายการเขตเวลาที่พร้อมใช้งาน หลังจากเลือกเขตเวลาแล้ว วันและเวลาจะถูกปรับตามเขตเวลาที่เลือก โดยถือว่าเวลาของระบบหรือนาฬิกาของฮาร์ดแวร์ถูกตั้งค่าเป็น UTC ไว้แล้ว"
   )
#ifdef HAVE_LAKKA_SWITCH
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LAKKA_SWITCH_OPTIONS,
   "ตัวเลือก Nintendo Switch"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LAKKA_SWITCH_OPTIONS,
   "จัดการตัวเลือกเฉพาะของ Nintendo Switch"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SWITCH_OC_ENABLE,
   "โอเวอร์คล็อก CPU"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SWITCH_OC_ENABLE,
   "เปิดใช้งานความถี่ในการโอเวอร์คล็อก CPU"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SWITCH_CEC_ENABLE,
   "รองรับ CEC"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SWITCH_CEC_ENABLE,
   "เปิดใช้งานการเชื่อมต่อผ่าน CEC กับทีวีเมื่อวางเครื่องบนด็อก"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BLUETOOTH_ERTM_DISABLE,
   "ปิดการใช้งาน Bluetooth ERTM"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BLUETOOTH_ERTM_DISABLE,
   "ปิดการใช้งาน Bluetooth ERTM เพื่อแก้ไขปัญหาการจับคู่กับบางอุปกรณ์"
   )
#endif
MSG_HASH(
   MSG_LOCALAP_SWITCHING_OFF,
   "ปิดการใช้งานจุดกระจายสัญญาณ Wi-Fi"
   )
MSG_HASH(
   MSG_WIFI_DISCONNECT_FROM,
   "กำลังตัดการเชื่อมต่อจาก Wi-Fi '%s'"
   )
MSG_HASH(
   MSG_WIFI_CONNECTING_TO,
   "กำลังเชื่อมต่อกับ Wi-Fi '%s'"
   )
MSG_HASH(
   MSG_WIFI_EMPTY_SSID,
   "[ไม่มี SSID]"
   )
MSG_HASH(
   MSG_LOCALAP_ALREADY_RUNNING,
   "จุดกระจายสัญญาณ Wi-Fi เริ่มทำงานอยู่แล้ว"
   )
MSG_HASH(
   MSG_LOCALAP_NOT_RUNNING,
   "จุดกระจายสัญญาณ Wi-Fi ยังไม่เริ่มทำงาน"
   )
MSG_HASH(
   MSG_LOCALAP_STARTING,
   "กำลังเริ่มการทำงานจุดกระจายสัญญาณ Wi-Fi ด้วย SSID=%s และรหัสผ่าน=%s"
   )
MSG_HASH(
   MSG_LOCALAP_ERROR_CONFIG_CREATE,
   "ไม่สามารถสร้างไฟล์กำหนดค่าสำหรับจุดกระจายสัญญาณ Wi-Fi ได้"
   )
MSG_HASH(
   MSG_LOCALAP_ERROR_CONFIG_PARSE,
   "ไฟล์กำหนดค่าไม่ถูกต้อง - ไม่พบ APNAME หรือ PASSWORD ใน %s"
   )
#endif
#ifdef HAVE_LAKKA_SWITCH
#endif
#ifdef GEKKO
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_SCALE,
   "ขนาดเมาส์"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_MOUSE_SCALE,
   "ปรับมาตราส่วน x/y สำหรับความเร็วของจอยปืน Wiimote"
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TOUCH_SCALE,
   "ขนาดสัมผัส"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TOUCH_SCALE,
   "ปรับมาตราส่วน x/y ของพิกัดหน้าจอสัมผัสเพื่อให้สอดคล้องกับการปรับมาตราส่วนการแสดงผลของระบบปฏิบัติการ"
   )
#ifdef UDEV_TOUCH_SUPPORT
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TOUCH_VMOUSE_POINTER,
   "ใช้ Touch VMouse เป็นตัวชี้"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TOUCH_VMOUSE_POINTER,
   "เปิดใช้งานการส่งผ่านการสัมผัสจากหน้าจอสัมผัสอินพุต"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TOUCH_VMOUSE_MOUSE,
   "ใช้ Touch VMouse เป็นเมาส์"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TOUCH_VMOUSE_MOUSE,
   "เปิดใช้งานการจำลองเมาส์เสมือนโดยใช้การสัมผัสอินพุต"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TOUCH_VMOUSE_TOUCHPAD,
   "โหมดทัชแพด Touch VMouse"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TOUCH_VMOUSE_TOUCHPAD,
   "เปิดใช้งานร่วมกับเมาส์ เพื่อใช้หน้าจอสัมผัสเป็นทัชแพด"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TOUCH_VMOUSE_TRACKBALL,
   "โหมดแทร็กบอล Touch VMouse"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TOUCH_VMOUSE_TRACKBALL,
   "เปิดใช้งานร่วมกับเมาส์เพื่อใช้หน้าจอสัมผัสเป็นแทร็กบอล โดยเพิ่มแรงเฉื่อยให้กับตัวชี้เมาส์"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TOUCH_VMOUSE_GESTURE,
   "ท่าทางสัมผัส Touch VMouse"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TOUCH_VMOUSE_GESTURE,
   "เปิดใช้งานท่าทางสัมผัสหน้าจอ รวมถึงการแตะ การแตะแล้วลาก และการปัดนิ้ว"
   )
#endif
#ifdef HAVE_ODROIDGO2
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RGA_SCALING,
   "การปรับขนาด RGA"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_RGA_SCALING,
   "ปรับขนาด RGA และการกรองแบบ Bicubic แต่อาจทำให้วิดเจ็ตแสดงผลผิดปกติได้"
   )
#else
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_CTX_SCALING,
   "การปรับขนาดตามบริบทเฉพาะ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_CTX_SCALING,
   "การปรับขนาดด้วยบริบทฮาร์ดแวร์ (ถ้ามี)"
   )
#endif
#ifdef _3DS
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NEW3DS_SPEEDUP_ENABLE,
   "เปิดใช้งาน Clock New3DS / แคช L2"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NEW3DS_SPEEDUP_ENABLE,
   "เปิดใช้งานความเร็ว Clock ของ New3DS (804MHz) และแคช L2"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_3DS_LCD_BOTTOM,
   "หน้าจอด้านล่างของ 3DS"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_3DS_LCD_BOTTOM,
   "เปิดใช้งานการแสดงข้อมูลสถานะบนหน้าจอด้านล่าง ปิดการใช้งานเพื่อยืดอายุการใช้งานแบตเตอรี่และเพิ่มประสิทธิภาพการทำงาน"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_3DS_DISPLAY_MODE,
   "โหมดการแสดงผล 3DS"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_3DS_DISPLAY_MODE,
   "เลือกความแตกต่างระหว่างโหมดการแสดงผลแบบ 3D และ 2D โดยในโหมด '3D' พิกเซลจะเป็นรูปทรงสี่เหลี่ยมและมีการเพิ่มเอฟเฟกต์ความลึกเมื่อดูเมนูด่วน ส่วนโหมด '2D' จะให้ประสิทธิภาพการทำงานที่ดี[...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CTR_VIDEO_MODE_2D_400X240,
   "2D (เอฟเฟกต์ตารางพิกเซล)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CTR_VIDEO_MODE_2D_800X240,
   "2D (ความละเอียดสูง)"
   )
MSG_HASH(
   MSG_3DS_BOTTOM_MENU_DEFAULT,
   "แตะที่หน้าจอสัมผัสเพื่อเข้าสู่\nเมนู Retroarch"
   )
MSG_HASH(
   MSG_3DS_BOTTOM_MENU_ASSET_NOT_FOUND,
   "ไม่พบทรัพยากร"
   )
MSG_HASH(
   MSG_3DS_BOTTOM_MENU_NO_STATE_DATA,
   "ไม่มี\nข้อมูล"
   )
MSG_HASH(
   MSG_3DS_BOTTOM_MENU_NO_STATE_THUMBNAIL,
   "ไม่มี\nภาพหน้าจอ"
   )
MSG_HASH(
   MSG_3DS_BOTTOM_MENU_RESUME,
   "เล่นเกมต่อ"
   )
MSG_HASH(
   MSG_3DS_BOTTOM_MENU_SAVE_STATE,
   "สร้าง\nจุดคืนค่าข้อมูล"
   )
MSG_HASH(
   MSG_3DS_BOTTOM_MENU_LOAD_STATE,
   "โหลด\nจุดคืนค่าข้อมูล"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BOTTOM_ASSETS_DIRECTORY,
   "ไดเรกทอรีทรัพยากรหน้าจอด้านล่าง"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BOTTOM_ASSETS_DIRECTORY,
   "ไดเรกทอรีทรัพยากรหน้าจอด้านล่าง โดยในไดเรกทอรีต้องประกอบด้วยไฟล์ \"bottom_menu.png\""
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BOTTOM_FONT_ENABLE,
   "เปิดใช้งานแบบอักษร"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BOTTOM_FONT_ENABLE,
   "แสดงแบบอักษรเมนูด้านล่าง เปิดใช้งานเพื่อแสดงคำอธิบายปุ่มบนหน้าจอด้านล่าง ทั้งนี้ไม่รวมถึงวันที่ของบันทึกสถานะ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BOTTOM_FONT_COLOR_RED,
   "สีตัวอักษร แดง"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BOTTOM_FONT_COLOR_RED,
   "ปรับค่าสีแดงของแบบอักษรหน้าจอด้านล่าง"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BOTTOM_FONT_COLOR_GREEN,
   "สีตัวอักษร เขียว"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BOTTOM_FONT_COLOR_GREEN,
   "ปรับค่าสีเขียวของแบบอักษรหน้าจอด้านล่าง"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BOTTOM_FONT_COLOR_BLUE,
   "สีตัวอักษร น้ำเงิน"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BOTTOM_FONT_COLOR_BLUE,
   "ปรับค่าสีน้ำเงินของแบบอักษรหน้าจอด้านล่าง"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BOTTOM_FONT_COLOR_OPACITY,
   "ความโปร่งใสของสีแบบอักษร"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BOTTOM_FONT_COLOR_OPACITY,
   "ปรับความโปร่งใสของแบบอักษรหน้าจอด้านล่าง"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BOTTOM_FONT_SCALE,
   "ขนาดตัวอักษร"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BOTTOM_FONT_SCALE,
   "ปรับขนาดแบบอักษรหน้าจอด้านล่าง"
   )
#endif
#ifdef HAVE_QT
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_SCAN_FINISHED,
   "การสแกนเสร็จสิ้นแล้ว<br><br>\nเพื่อให้เนื้อหาถูกสแกนอย่างถูกต้อง คุณจะต้อง:\n<ul><li>มี Core ที่เข้ากันได้ดาวน์โหลดไว้แล้ว</li>\n<li>อัปเดต \"ไฟล์ข้อมูล Core\" ผ่านตัวอัปเดตออนไลน์</li>\n<li>อัปเดต \"ฐานข้อมูล\" ผ่านตัวอัปเดตออนไลน์</li>\n<li>รีสตาร์ท RetroArch หากเพิ่งดำเนินการตามขั้นตอนข้างต้น</li></ul>\nสุดท้าย เนื้อหาจะต้องตรงกับฐานข้อมูลที่มีอยู่จาก <a href=\"https://docs.libretro.com/guides/roms-playlists-thumbnails/#sources\">ที่นี่</a> หากยังคงใช้งานไม่ได้ โปรดพิจารณา <a href=\"https://www.github.com/libretro/RetroArch/issues\">ส่งรายงานข้อผิดพลาด</a>"
   )
#endif
MSG_HASH(
   MSG_IOS_TOUCH_MOUSE_ENABLED,
   "เปิดใช้งานเมาส์สัมผัสแล้ว"
   )
MSG_HASH(
   MSG_IOS_TOUCH_MOUSE_DISABLED,
   "ปิดใช้งานเมาส์สัมผัสแล้ว"
   )
MSG_HASH(
   MSG_SDL2_MIC_NEEDS_SDL2_AUDIO,
   "ไมโครโฟน SDL2 ต้องใช้ไดรเวอร์เสียง SDL2"
   )
MSG_HASH(
   MSG_ACCESSIBILITY_STARTUP,
   "เปิดการเข้าถึง RetroArch แล้ว เมนูหลัก โหลด Core"
   )
MSG_HASH(
   MSG_AI_SERVICE_STOPPED,
   "เปิดการเข้าถึง RetroArch แล้ว เมนูหลัก โหลด Core"
   )
#ifdef HAVE_GAME_AI
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GAME_AI_MENU_OPTION,
   "บังคับควบคุมผู้เล่นด้วย AI"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GAME_AI_MENU_OPTION,
   "ให้ AI เข้าควบคุมผู้เล่นแทน"
   )


MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GAME_AI_OVERRIDE_P1,
   "ควบคุมผู้เล่น 1"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GAME_AI_OVERRIDE_P1,
   "ควบคุมผู้เล่น 01"
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GAME_AI_OVERRIDE_P2,
   "ควบคุมผู้เล่น 2"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GAME_AI_OVERRIDE_P2,
   "ควบคุมผู้เล่น 02"
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GAME_AI_SHOW_DEBUG,
   "แสดงผล Debug"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GAME_AI_SHOW_DEBUG,
   "แสดงผล Debug"
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_GAME_AI,
   "แสดง 'Game AI'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_GAME_AI,
   "แสดงตัวเลือก 'Game AI'"
   )
#endif
#ifdef HAVE_SMBCLIENT
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SMB_CLIENT_SETTINGS,
   "การตั้งค่าเครือข่าย SMB"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SMB_CLIENT_SETTINGS,
   "กำหนดค่าการตั้งค่าการแชร์เครือข่าย SMB"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SMB_CLIENT_ENABLE,
   "เปิดใช้งานไคลเอนต์ SMB"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SMB_CLIENT_ENABLE,
   "เปิดใช้งานการเข้าถึงการแชร์เครือข่าย SMB แนะนำให้ใช้สาย LAN แทน Wi-Fi เพื่อการเชื่อมต่อที่เสถียรยิ่งขึ้น หมายเหตุ: การเปลี่ยนการตั้งค่าเหล่านี้จำเป็นต้องเริ่มการทำงาน RetroArch ใหม่"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SMB_CLIENT_SERVER,
   "เซิร์ฟเวอร์ SMB"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SMB_CLIENT_SERVER,
   "ที่อยู่ IP หรือชื่อโฮสต์ของเซิร์ฟเวอร์"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SMB_CLIENT_SHARE,
   "ชื่อแชร์ SMB"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SMB_CLIENT_SHARE,
   "ชื่อของการแชร์เครือข่ายที่ต้องการเข้าถึง"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SMB_CLIENT_SUBDIR,
   "ไดเรกทอรีย่อย SMB (ไม่บังคับ)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SMB_CLIENT_SUBDIR,
   "เส้นทางไดเรกทอรีย่อยในการแชร์"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SMB_CLIENT_USERNAME,
   "ชื่อผู้ใช้ SMB"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SMB_CLIENT_USERNAME,
   "ชื่อผู้ใช้สำหรับการยืนยันตัวตน"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SMB_CLIENT_PASSWORD,
   "รหัสผ่าน SMB"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SMB_CLIENT_PASSWORD,
   "รหัสผ่านสำหรับการยืนยันตัวตน"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SMB_CLIENT_WORKGROUP,
   "เวิร์กกรุ๊ป SMB"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SMB_CLIENT_WORKGROUP,
   "ชื่อเวิร์กกรุ๊ปหรือชื่อโดเมน"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SMB_CLIENT_AUTH_MODE,
   "โหมดการยืนยันตัวตน SMB"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SMB_CLIENT_AUTH_MODE,
   "เลือกโหมดการยืนยันตัวตนที่ใช้ในสภาพแวดล้อมของคุณ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SMB_CLIENT_NUM_CONTEXTS,
   "จำนวนการเชื่อมต่อ SMB สูงสุด"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SMB_CLIENT_NUM_CONTEXTS,
   "เลือกจำนวนการเชื่อมต่อสูงสุดที่ใช้ในสภาพแวดล้อมของคุณ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SMB_CLIENT_TIMEOUT,
   "หมดเวลา SMB"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SMB_CLIENT_TIMEOUT,
   "เลือกเวลาการหมดเวลาค่าเริ่มต้น (หน่วยเป็นวินาที)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SMB_CLIENT_BROWSE,
   "เรียกดูการแชร์ผ่าน SMB"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SMB_CLIENT_BROWSE,
   "เรียกดูไฟล์ในการแชร์ผ่าน SMB ที่กำหนดค่าไว้"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_SMB_CLIENT,
   "แสดง 'ตัวลูกข่าย SMB'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_SMB_CLIENT,
   "แสดงการตั้งค่า 'ตัวลูกข่าย SMB'"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SMB_CLIENT_SMB_SHARE,
   "การแชร์ผ่าน SMB"
   )
#endif
