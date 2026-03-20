type unumber_formatter
type uformatted_number

val unumf_openForSkeletonAndLocale :
  Unsigned.UInt16.t Ctypes.ptr -> int -> string -> int32 Ctypes.ptr -> unit Ctypes.ptr

val unumf_openResult : int32 Ctypes.ptr -> unit Ctypes.ptr

val unumf_formatDouble :
  unit Ctypes.ptr -> float -> unit Ctypes.ptr -> int32 Ctypes.ptr -> unit

val unumf_formatInt :
  unit Ctypes.ptr -> int64 -> unit Ctypes.ptr -> int32 Ctypes.ptr -> unit

val unumf_resultToString :
  unit Ctypes.ptr -> Unsigned.UInt16.t Ctypes.ptr -> int -> int32 Ctypes.ptr -> int

val unumf_close : unit Ctypes.ptr -> unit

val unumf_closeResult : unit Ctypes.ptr -> unit

val u_errorName : int32 -> string
