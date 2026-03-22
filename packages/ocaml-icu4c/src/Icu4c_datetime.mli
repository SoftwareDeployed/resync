(** DateTime formatting using ICU4C *)

module Style : sig
  type t = Full | Long | Medium | Short
end

module Text : sig
  type t = Narrow | Short | Long
end

module Numeric : sig
  type t = Numeric | Two_digit
end

module Month : sig
  type t = Numeric | Two_digit | Narrow | Short | Long
end

module Hour_cycle : sig
  type t = H11 | H12 | H23 | H24
end

module Time_zone_name : sig
  type t =
    | Short
    | Long
    | Short_offset
    | Long_offset
    | Short_generic
    | Long_generic
end

type options = {
  locale : string;
  time_zone : string option;
  date_style : Style.t option;
  time_style : Style.t option;
  weekday : Text.t option;
  era : Text.t option;
  year : Numeric.t option;
  month : Month.t option;
  day : Numeric.t option;
  hour : Numeric.t option;
  minute : Numeric.t option;
  second : Numeric.t option;
  fractional_second_digits : int option;
  time_zone_name : Time_zone_name.t option;
  hour12 : bool option;
  hour_cycle : Hour_cycle.t option;
}

val make_options :
  ?locale:string ->
  ?time_zone:string ->
  ?date_style:Style.t ->
  ?time_style:Style.t ->
  ?weekday:Text.t ->
  ?era:Text.t ->
  ?year:Numeric.t ->
  ?month:Month.t ->
  ?day:Numeric.t ->
  ?hour:Numeric.t ->
  ?minute:Numeric.t ->
  ?second:Numeric.t ->
  ?fractional_second_digits:int ->
  ?time_zone_name:Time_zone_name.t ->
  ?hour12:bool ->
  ?hour_cycle:Hour_cycle.t ->
  unit ->
  options

type t
(** A datetime formatter instance *)

val make : options -> t
(** Create a new datetime formatter with the given options *)

val format : t -> float -> string
(** Format a timestamp (float milliseconds) using the formatter *)

val format_with_options : options -> float -> string
(** One-shot format with options *)

val close : t -> unit
(** Close the formatter and free resources *)
