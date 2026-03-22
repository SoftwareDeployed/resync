val udat_open :
  int ->
  int ->
  string ->
  Unsigned.UInt16.t Ctypes.ptr ->
  int ->
  Unsigned.UInt16.t Ctypes.ptr ->
  int ->
  int32 Ctypes.ptr ->
  unit Ctypes.ptr

val udat_close : unit Ctypes.ptr -> unit

val udat_format :
  unit Ctypes.ptr ->
  float ->
  Unsigned.UInt16.t Ctypes.ptr ->
  int ->
  unit Ctypes.ptr ->
  int32 Ctypes.ptr ->
  int

val udat_formatForFields :
  unit Ctypes.ptr ->
  float ->
  Unsigned.UInt16.t Ctypes.ptr ->
  int ->
  unit Ctypes.ptr ->
  int32 Ctypes.ptr ->
  int

val udatpg_open : string -> int32 Ctypes.ptr -> unit Ctypes.ptr

val udatpg_close : unit Ctypes.ptr -> unit

val udatpg_getBestPatternWithOptions :
  unit Ctypes.ptr ->
  Unsigned.UInt16.t Ctypes.ptr ->
  int ->
  int ->
  Unsigned.UInt16.t Ctypes.ptr ->
  int ->
  int32 Ctypes.ptr ->
  int

val udatpg_getDefaultHourCycle : unit Ctypes.ptr -> int32 Ctypes.ptr -> int

val ufieldpositer_open : int32 Ctypes.ptr -> unit Ctypes.ptr

val ufieldpositer_next : unit Ctypes.ptr -> int Ctypes.ptr -> int Ctypes.ptr -> int

val ufieldpositer_close : unit Ctypes.ptr -> unit

val u_strFromUTF8 :
  Unsigned.UInt16.t Ctypes.ptr ->
  int ->
  int Ctypes.ptr ->
  string ->
  int ->
  int32 Ctypes.ptr ->
  Unsigned.UInt16.t Ctypes.ptr

val u_strToUTF8 :
  char Ctypes.ptr ->
  int ->
  int Ctypes.ptr ->
  Unsigned.UInt16.t Ctypes.ptr ->
  int ->
  int32 Ctypes.ptr ->
  char Ctypes.ptr

val uloc_forLanguageTag :
  string -> char Ctypes.ptr -> int -> int Ctypes.ptr -> int32 Ctypes.ptr -> int

val ucal_getCanonicalTimeZoneID :
  Unsigned.UInt16.t Ctypes.ptr ->
  int ->
  Unsigned.UInt16.t Ctypes.ptr ->
  int ->
  char Ctypes.ptr ->
  int32 Ctypes.ptr ->
  int

val u_errorName : int32 -> string
