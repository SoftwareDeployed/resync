open Ctypes
open Foreign

let version_suffix = Icu_version.version_suffix

type unumber_formatter
type uformatted_number

let with_suffix name =
  name ^ version_suffix

let unumf_openForSkeletonAndLocale =
   foreign (with_suffix "unumf_openForSkeletonAndLocale")
     (ptr uint16_t @-> int @-> string @-> ptr int32_t @-> returning (ptr void))

let unumf_openResult =
   foreign (with_suffix "unumf_openResult")
     (ptr int32_t @-> returning (ptr void))

let unumf_formatDouble =
   foreign (with_suffix "unumf_formatDouble")
     (ptr void @-> double @-> ptr void @-> ptr int32_t @-> returning void)

let unumf_formatInt =
   foreign (with_suffix "unumf_formatInt")
     (ptr void @-> int64_t @-> ptr void @-> ptr int32_t @-> returning void)

let unumf_resultToString =
   foreign (with_suffix "unumf_resultToString")
     (ptr void @-> ptr uint16_t @-> int @-> ptr int32_t @-> returning int)

let unumf_close =
   foreign (with_suffix "unumf_close")
     (ptr void @-> returning void)

let unumf_closeResult =
   foreign (with_suffix "unumf_closeResult")
     (ptr void @-> returning void)

let u_errorName =
   foreign (with_suffix "u_errorName")
     (int32_t @-> returning string)
