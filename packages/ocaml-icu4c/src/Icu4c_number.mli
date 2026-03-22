(** Number formatting using ICU4C *)

module Style : sig
  type t = Decimal | Currency | Percent
end

type options = {
  style : Style.t;
  currency : string option;
  locale : string;
  minimum_fraction_digits : int option;
  maximum_fraction_digits : int option;
  use_grouping : bool option;
}

val make_options :
  ?style:Style.t ->
  ?currency:string ->
  ?locale:string ->
  ?minimum_fraction_digits:int ->
  ?maximum_fraction_digits:int ->
  ?use_grouping:bool ->
  unit ->
  options

type t
(** A number formatter instance *)

val make : options -> t
(** Create a new number formatter with the given options *)

val format : t -> float -> string
(** Format a float value using the formatter *)

val format_with_options : options -> float -> string
(** One-shot format with options *)

val close : t -> unit
(** Close the formatter and free resources *)
