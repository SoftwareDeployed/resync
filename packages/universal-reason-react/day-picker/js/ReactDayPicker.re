type mode = string;

type captionLayout = [
  | `Label
  | `Dropdown
  | `DropdownMonths
  | `DropdownYears
];

let captionLayoutToString = (value: captionLayout): string =>
  switch (value) {
  | `Label => "label"
  | `Dropdown => "dropdown"
  | `DropdownMonths => "dropdown-months"
  | `DropdownYears => "dropdown-years"
  };

type navLayout = [
  | `Around
  | `After
];

let navLayoutToString = (value: navLayout): string =>
  switch (value) {
  | `Around => "around"
  | `After => "after"
  };

type reactNode = React.element;

type singleDate = Js.Undefined.t(Js.Date.t);
type multipleDate = Js.Undefined.t(array(Js.Date.t));

type dateRange = {
  from: singleDate,
  to_: singleDate,
};

type rangeDate = Js.Undefined.t(dateRange);

type selected = [
  | `Single(singleDate)
  | `Multiple(multipleDate)
  | `Range(rangeDate)
];

type onSelect = [
  | `Single(singleDate => unit)
  | `Multiple(multipleDate => unit)
  | `Range(rangeDate => unit)
];

[@melange_only]
module ClientImpl = {
  type jsDateRange = {
    from: singleDate,
    [@mel.as "to"]
    to_: singleDate,
  };

  type jsRangeDate = Js.Undefined.t(jsDateRange);

  /* react-day-picker expects the raw value, not a variant wrapper */
  type jsSelected = Js.Json.t;

  type jsOnSelect = Js.Json.t;

  let emptyRangeDate: jsRangeDate = Js.Undefined.fromOption(None);

  let toJsRangeDate = (value: rangeDate): jsRangeDate =>
    switch (Js.Undefined.toOption(value)) {
    | Some(rangeValue) =>
      let nextValue: jsDateRange = {
        from: rangeValue.from,
        to_: rangeValue.to_,
      };
      Js.Undefined.return(nextValue);
    | None => emptyRangeDate
    };

  let fromJsRangeDate = (value: jsRangeDate): rangeDate =>
    switch (Js.Undefined.toOption(value)) {
    | Some(rangeValue) =>
      let nextValue: dateRange = {
        from: rangeValue.from,
        to_: rangeValue.to_,
      };
      Js.Undefined.return(nextValue);
    | None => Js.Undefined.fromOption(None)
    };

  /* Convert our polymorphic variant to the raw value that react-day-picker expects */
  let toJsSelected = (value: selected): jsSelected =>
    switch (value) {
    | `Single(date) => Obj.magic(date)
    | `Multiple(dates) => Obj.magic(dates)
    | `Range(rangeValue) => Obj.magic(toJsRangeDate(rangeValue))
    };

  let toJsOnSelect = (value: onSelect): jsOnSelect =>
    switch (value) {
    | `Single(callback) => Obj.magic(callback)
    | `Multiple(callback) => Obj.magic(callback)
    | `Range(callback) =>
      Obj.magic((dates: jsRangeDate) => callback(fromJsRangeDate(dates)))
    };

  [@mel.obj]
  external makeProps:
    (
      ~mode: string,
      ~onSelect: jsOnSelect,
      ~selected: jsSelected,
      ~captionLayout: string=?,
      ~reverseYears: bool=?,
      ~navLayout: string=?,
      ~disableNavigation: bool=?,
      ~hideNavigation: bool=?,
      ~animate: bool=?,
      ~fixedWeeks: bool=?,
      ~footer: reactNode=?,
      ~hideWeekdays: bool=?,
      ~numberOfMonths: int=?,
      ~reverseMonths: bool=?,
      ~pagedNavigation: bool=?,
      ~showOutsideDays: bool=?,
      ~showWeekNumber: bool=?,
      unit
    ) =>
    {
      .
      "mode": string,
      "selected": jsSelected,
      "onSelect": jsOnSelect,
      "captionLayout": string,
      "reverseYears": bool,
      "navLayout": string,
      "disableNavigation": bool,
      "hideNavigation": bool,
      "animate": bool,
      "fixedWeeks": bool,
      "footer": reactNode,
      "hideWeekdays": bool,
      "numberOfMonths": int,
      "reverseMonths": bool,
      "pagedNavigation": bool,
      "showOutsideDays": bool,
      "showWeekNumber": bool,
    };

  [@mel.module "react-day-picker"]
  external make:
    {
      ..
      "mode": string,
      "onSelect": jsOnSelect,
      "selected": jsSelected,
      "captionLayout": string,
      "navLayout": string,
      "disableNavigation": bool,
      "hideNavigation": bool,
      "animate": bool,
      "fixedWeeks": bool,
      "footer": reactNode,
      "hideWeekdays": bool,
      "numberOfMonths": int,
      "reverseMonths": bool,
      "pagedNavigation": bool,
      "showOutsideDays": bool,
      "showWeekNumber": bool,
    } =>
    React.element =
    "DayPicker";

  let render =
      (
        ~mode,
        ~selected,
        ~onSelect,
        ~captionLayout: option(captionLayout)=?,
        ~reverseYears: option(bool)=?,
        ~navLayout: option(navLayout)=?,
        ~disableNavigation: option(bool)=?,
        ~hideNavigation: option(bool)=?,
        ~animate: option(bool)=?,
        ~fixedWeeks: option(bool)=?,
        ~footer: option(reactNode)=?,
        ~hideWeekdays: option(bool)=?,
        ~numberOfMonths: option(int)=?,
        ~reverseMonths: option(bool)=?,
        ~pagedNavigation: option(bool)=?,
        ~showOutsideDays: option(bool)=?,
        ~showWeekNumber: option(bool)=?,
        (),
      ) => {
    let captionLayout =
      switch (captionLayout) {
      | Some(value) => Some(captionLayoutToString(value))
      | None => None
      };
    let navLayout =
      switch (navLayout) {
      | Some(value) => Some(navLayoutToString(value))
      | None => None
      };
    let props =
      makeProps(
        ~mode,
        ~onSelect=toJsOnSelect(onSelect),
        ~selected=toJsSelected(selected),
        ~captionLayout?,
        ~reverseYears?,
        ~navLayout?,
        ~disableNavigation?,
        ~hideNavigation?,
        ~animate?,
        ~fixedWeeks?,
        ~footer?,
        ~hideWeekdays?,
        ~numberOfMonths?,
        ~reverseMonths?,
        ~pagedNavigation?,
        ~showOutsideDays?,
        ~showWeekNumber?,
        (),
      );
    React.createElement(make, props);
  };
};

[@native_only]
module ServerImpl = {
  let modeFromString = (value: mode): ReactDayPickerNative.mode =>
    switch (value) {
    | "multiple" => `Multiple
    | "range" => `Range
    | _ => `Single
    };

  let captionLayoutFromShared =
      (value: captionLayout): ReactDayPickerNative.captionLayout =>
    switch (value) {
    | `Label => `Label
    | `Dropdown => `Dropdown
    | `DropdownMonths => `DropdownMonths
    | `DropdownYears => `DropdownYears
    };

  let navLayoutFromShared = (value: navLayout): ReactDayPickerNative.navLayout =>
    switch (value) {
    | `Around => `Around
    | `After => `After
    };

  let emptyRangeDate: ReactDayPickerNative.rangeDate =
    Js.Undefined.fromOption(None);

  let toNativeRangeDate = (value: rangeDate): ReactDayPickerNative.rangeDate =>
    switch (Js.Undefined.toOption(value)) {
    | Some(rangeValue) =>
      let nextValue: ReactDayPickerNative.dateRange = {
        from: rangeValue.from,
        to_: rangeValue.to_,
      };
      Js.Undefined.return(nextValue);
    | None => emptyRangeDate
    };

  let fromNativeRangeDate = (value: ReactDayPickerNative.rangeDate): rangeDate =>
    switch (Js.Undefined.toOption(value)) {
    | Some(rangeValue) =>
      let nextValue: dateRange = {
        from: rangeValue.from,
        to_: rangeValue.to_,
      };
      Js.Undefined.return(nextValue);
    | None => Js.Undefined.fromOption(None)
    };

  let toNativeSelected = (value: selected): ReactDayPickerNative.selected =>
    switch (value) {
    | `Single(date) => `Single(date)
    | `Multiple(dates) => `Multiple(dates)
    | `Range(rangeValue) => `Range(toNativeRangeDate(rangeValue))
    };

  let toNativeOnSelect = (value: onSelect): ReactDayPickerNative.onSelect =>
    switch (value) {
    | `Single(callback) => `Single(callback)
    | `Multiple(callback) => `Multiple(callback)
    | `Range(callback) =>
      `Range(
        (dates: ReactDayPickerNative.rangeDate) =>
          callback(fromNativeRangeDate(dates)),
      )
    };

  let render =
      (
        ~mode,
        ~selected,
        ~onSelect,
        ~captionLayout: option(captionLayout)=?,
        ~reverseYears: option(bool)=?,
        ~navLayout: option(navLayout)=?,
        ~disableNavigation: option(bool)=?,
        ~hideNavigation: option(bool)=?,
        ~animate: option(bool)=?,
        ~fixedWeeks: option(bool)=?,
        ~footer: option(reactNode)=?,
        ~hideWeekdays: option(bool)=?,
        ~numberOfMonths: option(int)=?,
        ~reverseMonths: option(bool)=?,
        ~pagedNavigation: option(bool)=?,
        ~showOutsideDays: option(bool)=?,
        ~showWeekNumber: option(bool)=?,
        (),
      ) => {
    let captionLayout =
      switch (captionLayout) {
      | Some(value) => Some(captionLayoutFromShared(value))
      | None => None
      };
    let navLayout =
      switch (navLayout) {
      | Some(value) => Some(navLayoutFromShared(value))
      | None => None
      };
    ReactDayPickerNative.make(
      ~mode=modeFromString(mode),
      ~onSelect=toNativeOnSelect(onSelect),
      ~selected=toNativeSelected(selected),
      ~captionLayout?,
      ~reverseYears?,
      ~navLayout?,
      ~disableNavigation?,
      ~hideNavigation?,
      ~animate?,
      ~fixedWeeks?,
      ~footer?,
      ~hideWeekdays?,
      ~numberOfMonths?,
      ~reverseMonths?,
      ~pagedNavigation?,
      ~showOutsideDays?,
      ~showWeekNumber?,
      (),
    );
  };
};

[@react.component]
let make =
    (
      ~mode: mode,
      ~selected: selected,
      ~onSelect: onSelect,
      ~captionLayout: option(captionLayout)=?,
      ~reverseYears: option(bool)=?,
      ~navLayout: option(navLayout)=?,
      ~disableNavigation: option(bool)=?,
      ~hideNavigation: option(bool)=?,
      ~animate: option(bool)=?,
      ~fixedWeeks: option(bool)=?,
      ~footer: option(reactNode)=?,
      ~hideWeekdays: option(bool)=?,
      ~numberOfMonths: option(int)=?,
      ~reverseMonths: option(bool)=?,
      ~pagedNavigation: option(bool)=?,
      ~showOutsideDays: option(bool)=?,
      ~showWeekNumber: option(bool)=?,
      (),
    ) =>
  switch%platform (Runtime.platform) {
  | Native =>
    ServerImpl.render(
      ~mode,
      ~selected,
      ~onSelect,
      ~captionLayout?,
      ~reverseYears?,
      ~navLayout?,
      ~disableNavigation?,
      ~hideNavigation?,
      ~animate?,
      ~fixedWeeks?,
      ~footer?,
      ~hideWeekdays?,
      ~numberOfMonths?,
      ~reverseMonths?,
      ~pagedNavigation?,
      ~showOutsideDays?,
      ~showWeekNumber?,
      (),
    )
  | Melange =>
    ClientImpl.render(
      ~mode,
      ~selected,
      ~onSelect,
      ~captionLayout?,
      ~reverseYears?,
      ~navLayout?,
      ~disableNavigation?,
      ~hideNavigation?,
      ~animate?,
      ~fixedWeeks?,
      ~footer?,
      ~hideWeekdays?,
      ~numberOfMonths?,
      ~reverseMonths?,
      ~pagedNavigation?,
      ~showOutsideDays?,
      ~showWeekNumber?,
      (),
    )
  };
