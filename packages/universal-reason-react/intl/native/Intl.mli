module NumberFormatter : sig
  module Style : sig
    type t = Decimal | Currency | Percent
  end

  type part = {
    type_ : string;
    value : string;
  }

  type t

  val make :
    ?locale:string ->
    ?style:Style.t ->
    ?currency:string ->
    ?minimumFractionDigits:int ->
    ?maximumFractionDigits:int ->
    ?useGrouping:bool ->
    unit ->
    t

  val format : t -> float -> string
  val formatToParts : t -> float -> part list

  val formatWithOptions :
    ?locale:string ->
    ?style:Style.t ->
    ?currency:string ->
    ?minimumFractionDigits:int ->
    ?maximumFractionDigits:int ->
    ?useGrouping:bool ->
    float ->
    string

  val formatToPartsWithOptions :
    ?locale:string ->
    ?style:Style.t ->
    ?currency:string ->
    ?minimumFractionDigits:int ->
    ?maximumFractionDigits:int ->
    ?useGrouping:bool ->
    float ->
    part list
end

module DateTimeFormatter : sig
  module Style : sig
    type t = Full | Long | Medium | Short
  end

  module Text : sig
    type t = Narrow | Short | Long
  end

  module Numeric : sig
    type t = Numeric | TwoDigit
  end

  module Month : sig
    type t = Numeric | TwoDigit | Narrow | Short | Long
  end

  module HourCycle : sig
    type t = H11 | H12 | H23 | H24
  end

  module TimeZoneName : sig
    type t =
      | Short
      | Long
      | ShortOffset
      | LongOffset
      | ShortGeneric
      | LongGeneric
  end

  type part = {
    type_ : string;
    value : string;
  }

  type t

  val make :
    ?locale:string ->
    ?timeZone:string ->
    ?dateStyle:Style.t ->
    ?timeStyle:Style.t ->
    ?weekday:Text.t ->
    ?era:Text.t ->
    ?year:Numeric.t ->
    ?month:Month.t ->
    ?day:Numeric.t ->
    ?hour:Numeric.t ->
    ?minute:Numeric.t ->
    ?second:Numeric.t ->
    ?fractionalSecondDigits:int ->
    ?timeZoneName:TimeZoneName.t ->
    ?hour12:bool ->
    ?hourCycle:HourCycle.t ->
    unit ->
    t

  val format : t -> float -> string
  val formatToParts : t -> float -> part list

  val formatWithOptions :
    ?locale:string ->
    ?timeZone:string ->
    ?dateStyle:Style.t ->
    ?timeStyle:Style.t ->
    ?weekday:Text.t ->
    ?era:Text.t ->
    ?year:Numeric.t ->
    ?month:Month.t ->
    ?day:Numeric.t ->
    ?hour:Numeric.t ->
    ?minute:Numeric.t ->
    ?second:Numeric.t ->
    ?fractionalSecondDigits:int ->
    ?timeZoneName:TimeZoneName.t ->
    ?hour12:bool ->
    ?hourCycle:HourCycle.t ->
    float ->
    string

  val formatToPartsWithOptions :
    ?locale:string ->
    ?timeZone:string ->
    ?dateStyle:Style.t ->
    ?timeStyle:Style.t ->
    ?weekday:Text.t ->
    ?era:Text.t ->
    ?year:Numeric.t ->
    ?month:Month.t ->
    ?day:Numeric.t ->
    ?hour:Numeric.t ->
    ?minute:Numeric.t ->
    ?second:Numeric.t ->
    ?fractionalSecondDigits:int ->
    ?timeZoneName:TimeZoneName.t ->
    ?hour12:bool ->
    ?hourCycle:HourCycle.t ->
    float ->
    part list
end
