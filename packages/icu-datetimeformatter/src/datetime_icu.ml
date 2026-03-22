open Ctypes
open Foreign

let version_suffix = Datetime_icu_version.version_suffix

let with_suffix name =
  name ^ version_suffix

let udat_open =
  foreign (with_suffix "udat_open")
    (int @-> int @-> string @-> ptr uint16_t @-> int @-> ptr uint16_t @-> int @-> ptr int32_t @-> returning (ptr void))

let udat_close =
  foreign (with_suffix "udat_close")
    (ptr void @-> returning void)

let udat_format =
  foreign (with_suffix "udat_format")
    (ptr void @-> double @-> ptr uint16_t @-> int @-> ptr void @-> ptr int32_t @-> returning int)

let udat_formatForFields =
  foreign (with_suffix "udat_formatForFields")
    (ptr void @-> double @-> ptr uint16_t @-> int @-> ptr void @-> ptr int32_t @-> returning int)

let udatpg_open =
  foreign (with_suffix "udatpg_open")
    (string @-> ptr int32_t @-> returning (ptr void))

let udatpg_close =
  foreign (with_suffix "udatpg_close")
    (ptr void @-> returning void)

let udatpg_getBestPatternWithOptions =
  foreign (with_suffix "udatpg_getBestPatternWithOptions")
    (ptr void @-> ptr uint16_t @-> int @-> int @-> ptr uint16_t @-> int @-> ptr int32_t @-> returning int)

let udatpg_getDefaultHourCycle =
  foreign (with_suffix "udatpg_getDefaultHourCycle")
    (ptr void @-> ptr int32_t @-> returning int)

let ufieldpositer_open =
  foreign (with_suffix "ufieldpositer_open")
    (ptr int32_t @-> returning (ptr void))

let ufieldpositer_next =
  foreign (with_suffix "ufieldpositer_next")
    (ptr void @-> ptr int @-> ptr int @-> returning int)

let ufieldpositer_close =
  foreign (with_suffix "ufieldpositer_close")
    (ptr void @-> returning void)

let u_strFromUTF8 =
  foreign (with_suffix "u_strFromUTF8")
    (ptr uint16_t @-> int @-> ptr int @-> string @-> int @-> ptr int32_t @-> returning (ptr uint16_t))

let u_strToUTF8 =
  foreign (with_suffix "u_strToUTF8")
    (ptr char @-> int @-> ptr int @-> ptr uint16_t @-> int @-> ptr int32_t @-> returning (ptr char))

let uloc_forLanguageTag =
  foreign (with_suffix "uloc_forLanguageTag")
    (string @-> ptr char @-> int @-> ptr int @-> ptr int32_t @-> returning int)

let ucal_getCanonicalTimeZoneID =
  foreign (with_suffix "ucal_getCanonicalTimeZoneID")
    (ptr uint16_t @-> int @-> ptr uint16_t @-> int @-> ptr char @-> ptr int32_t @-> returning int)

let u_errorName =
  foreign (with_suffix "u_errorName")
    (int32_t @-> returning string)
