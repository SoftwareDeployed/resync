type mode = [
  | `Single
  | `Multiple
  | `Range
];

let modeToString = (v: mode): string =>
  switch (v) {
  | `Single => "single"
  | `Multiple => "multiple"
  | `Range => "range"
  };

type captionLayout = [
  | `Label
  | `Dropdown
  | `DropdownMonths
  | `DropdownYears
];

let captionLayoutToString = (v: captionLayout): string =>
  switch (v) {
  | `Label => "label"
  | `Dropdown => "dropdown"
  | `DropdownMonths => "dropdown-months"
  | `DropdownYears => "dropdown-years"
  };

type navLayout = [
  | `Around
  | `After
];

let navLayoutToString = (v: navLayout): string =>
  switch (v) {
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

type classNames = {
  root: string,
  months: string,
  nav: string,
  buttonPrevious: string,
  buttonNext: string,
  chevron: string,
  month: string,
  monthCaption: string,
  dropdowns: string,
  dropdownRoot: string,
  dropdown: string,
  monthsDropdown: string,
  yearsDropdown: string,
  captionLabel: string,
  monthGrid: string,
  weekdays: string,
  weekday: string,
  weekNumberHeader: string,
  weeks: string,
  week: string,
  weekNumber: string,
  day: string,
  dayButton: string,
  footer: string,
  dayToday: string,
  daySelected: string,
  dayOutside: string,
  dayHidden: string,
  dayRangeStart: string,
  dayRangeMiddle: string,
  dayRangeEnd: string,
};

let classNames: classNames = {
  root: "rdp-root",
  months: "rdp-months",
  nav: "rdp-nav",
  buttonPrevious: "rdp-button_previous",
  buttonNext: "rdp-button_next",
  chevron: "rdp-chevron",
  month: "rdp-month",
  monthCaption: "rdp-month_caption",
  dropdowns: "rdp-dropdowns",
  dropdownRoot: "rdp-dropdown_root",
  dropdown: "rdp-dropdown",
  monthsDropdown: "rdp-months_dropdown",
  yearsDropdown: "rdp-years_dropdown",
  captionLabel: "rdp-caption_label",
  monthGrid: "rdp-month_grid",
  weekdays: "rdp-weekdays",
  weekday: "rdp-weekday",
  weekNumberHeader: "rdp-week_number_header",
  weeks: "rdp-weeks",
  week: "rdp-week",
  weekNumber: "rdp-week_number",
  day: "rdp-day",
  dayButton: "rdp-day_button",
  footer: "rdp-footer",
  dayToday: "rdp-today",
  daySelected: "rdp-selected",
  dayOutside: "rdp-outside",
  dayHidden: "rdp-hidden",
  dayRangeStart: "rdp-range_start",
  dayRangeMiddle: "rdp-range_middle",
  dayRangeEnd: "rdp-range_end",
};

type calendarDay = {
  date: Js.Date.t,
  isOutside: bool,
};

type calendarWeek = {
  days: array(calendarDay),
  weekNumber: int,
};

let dateYear = (date: Js.Date.t): int =>
  int_of_float(Js.Date.getFullYear(date));
let dateMonth = (date: Js.Date.t): int =>
  int_of_float(Js.Date.getMonth(date));
let dateDay = (date: Js.Date.t): int => int_of_float(Js.Date.getDate(date));
let dayOfWeek = (date: Js.Date.t): int =>
  int_of_float(Js.Date.getDay(date));

let monthNames = [|
  "January",
  "February",
  "March",
  "April",
  "May",
  "June",
  "July",
  "August",
  "September",
  "October",
  "November",
  "December",
|];

let getMonthName = (index: int): string =>
  if (index >= 0 && index < Array.length(monthNames)) {
    monthNames[index];
  } else {
    "";
  };

let newDate = (~year: int, ~month: int, ~day: int): Js.Date.t => {
  Js.Date.make(
    ~year=float_of_int(year),
    ~month=float_of_int(month),
    ~date=float_of_int(day),
    ~hours=12.,
    ~minutes=0.,
    ~seconds=0.,
    (),
  );
};

let setMonth = (date: Js.Date.t, month: int): Js.Date.t => {
  newDate(~year=dateYear(date), ~month, ~day=dateDay(date));
};

let startOfMonth = (date: Js.Date.t): Js.Date.t =>
  newDate(~year=dateYear(date), ~month=dateMonth(date), ~day=1);

let isBefore = (a: Js.Date.t, b: Js.Date.t): bool => {
  let yearA = dateYear(a);
  let yearB = dateYear(b);
  if (yearA != yearB) {
    yearA < yearB;
  } else {
    let monthA = dateMonth(a);
    let monthB = dateMonth(b);
    if (monthA != monthB) {
      monthA < monthB;
    } else {
      dateDay(a) < dateDay(b);
    };
  };
};
let isAfter = (a: Js.Date.t, b: Js.Date.t): bool => {
  let yearA = dateYear(a);
  let yearB = dateYear(b);
  if (yearA != yearB) {
    yearA > yearB;
  } else {
    let monthA = dateMonth(a);
    let monthB = dateMonth(b);
    if (monthA != monthB) {
      monthA > monthB;
    } else {
      dateDay(a) > dateDay(b);
    };
  };
};

let isSameDay = (a: Js.Date.t, b: Js.Date.t): bool => {
  dateYear(a) == dateYear(b)
  && dateMonth(a) == dateMonth(b)
  && dateDay(a) == dateDay(b);
};

let daysInMonth = (year: int, month: int): int => {
  switch (month mod 12) {
  | 0 => 31
  | 1 =>
    if (year mod 4 == 0 && (year mod 100 != 0 || year mod 400 == 0)) {
      29;
    } else {
      28;
    }
  | 2 => 31
  | 3 => 30
  | 4 => 31
  | 5 => 30
  | 6 => 31
  | 7 => 31
  | 8 => 30
  | 9 => 31
  | 10 => 30
  | 11 => 31
  | _ => 30
  };
};

let formatMonthYear = (date: Js.Date.t): string => {
  let monthName =
    if (dateMonth(date) >= 0 && dateMonth(date) < Array.length(monthNames)) {
      Some(monthNames[dateMonth(date)]);
    } else {
      None;
    };
  switch (monthName) {
  | Some(value) => value ++ " " ++ string_of_int(dateYear(date))
  | None => string_of_int(dateYear(date))
  };
};

let formatDayNumber = (date: Js.Date.t): string =>
  string_of_int(dateDay(date));

let formatISODate = (date: Js.Date.t): string => {
  let y = dateYear(date);
  let m = dateMonth(date) + 1;
  let d = dateDay(date);
  let mm =
    if (m < 10) {
      "0" ++ string_of_int(m);
    } else {
      string_of_int(m);
    };
  let dd =
    if (d < 10) {
      "0" ++ string_of_int(d);
    } else {
      string_of_int(d);
    };
  string_of_int(y) ++ "-" ++ mm ++ "-" ++ dd;
};

let formatYearMonth = (date: Js.Date.t): string => {
  let y = dateYear(date);
  let m = dateMonth(date) + 1;
  let mm =
    if (m < 10) {
      "0" ++ string_of_int(m);
    } else {
      string_of_int(m);
    };
  string_of_int(y) ++ "-" ++ mm;
};

let weekdayShort = [|"Su", "Mo", "Tu", "We", "Th", "Fr", "Sa"|];
let weekdayLabel = [|
  "Sunday",
  "Monday",
  "Tuesday",
  "Wednesday",
  "Thursday",
  "Friday",
  "Saturday",
|];
let getWeekdayName = (index: int): string =>
  if (index >= 0 && index < Array.length(weekdayLabel)) {
    weekdayLabel[index];
  } else {
    "";
  };
let getWeekdayShort = (index: int): string =>
  if (index >= 0 && index < Array.length(weekdayShort)) {
    weekdayShort[index];
  } else {
    "";
  };

let getDayOfYear = (date: Js.Date.t): int => {
  let yearVal = dateYear(date);
  let monthVal = dateMonth(date);
  let dayVal = dateDay(date);
  let accumulator = ref(0);
  for (monthIndex in 0 to monthVal - 1) {
    accumulator := accumulator^ + daysInMonth(yearVal, monthIndex);
  };
  accumulator^ + dayVal;
};

let getWeekNumber = (date: Js.Date.t): int => {
  let yearVal = dateYear(date);
  let firstOfYear = newDate(~year=yearVal, ~month=0, ~day=1);
  let firstWeekday = dayOfWeek(firstOfYear);
  (getDayOfYear(date) - 1 + firstWeekday) / 7 + 1;
};

let buildMonthWeeks =
    (monthDate: Js.Date.t, ~weekStartsOn: int=0, ~fixedWeeks: bool=false, ())
    : array(calendarWeek) => {
  let yearVal = dateYear(monthDate);
  let monthVal = dateMonth(monthDate);
  let firstDayOfMonth = newDate(~year=yearVal, ~month=monthVal, ~day=1);
  let lastDay = daysInMonth(yearVal, monthVal);
  let firstDayWeekday = (dayOfWeek(firstDayOfMonth) - weekStartsOn + 7) mod 7;
  let totalWeeks =
    if (fixedWeeks) {
      6;
    } else {
      (firstDayWeekday + lastDay + 6) / 7;
    };

  let prevMonth =
    if (monthVal == 0) {
      11;
    } else {
      monthVal - 1;
    };
  let prevYear =
    if (monthVal == 0) {
      yearVal - 1;
    } else {
      yearVal;
    };
  let prevMonthDays = daysInMonth(prevYear, prevMonth);

  let weeks = ref([]);
  let dayCounter = ref(1);
  let nextMonthDay = ref(1);

  for (weekIndex in 0 to totalWeeks - 1) {
    let weekDays = ref([]);
    for (dayIndex in 0 to 6) {
      let dayInfo =
        if (weekIndex == 0 && dayIndex < firstDayWeekday) {
          let prevDay = prevMonthDays - firstDayWeekday + dayIndex + 1;
          {
            date: newDate(~year=prevYear, ~month=prevMonth, ~day=prevDay),
            isOutside: true,
          };
        } else if (dayCounter^ > lastDay) {
          let nextMonth =
            if (monthVal == 11) {
              0;
            } else {
              monthVal + 1;
            };
          let nextYear =
            if (monthVal == 11) {
              yearVal + 1;
            } else {
              yearVal;
            };
          let dayObj =
            newDate(~year=nextYear, ~month=nextMonth, ~day=nextMonthDay^);
          nextMonthDay := nextMonthDay^ + 1;
          {
            date: dayObj,
            isOutside: true,
          };
        } else {
          let dayObj =
            newDate(~year=yearVal, ~month=monthVal, ~day=dayCounter^);
          dayCounter := dayCounter^ + 1;
          {
            date: dayObj,
            isOutside: false,
          };
        };
      weekDays := [dayInfo, ...weekDays^];
    };

    let reversedWeekDays = List.rev(weekDays^);
    let weekDaysArray = Array.of_list(reversedWeekDays);
    let firstDate =
      switch (reversedWeekDays) {
      | [value, ..._] => value
      | [] => {
          date: newDate(~year=yearVal, ~month=monthVal, ~day=1),
          isOutside: false,
        }
      };
    let weekNum = getWeekNumber(firstDate.date);

    weeks :=
      [
        {
          days: weekDaysArray,
          weekNumber: weekNum,
        },
        ...weeks^,
      ];
  };

  Array.of_list(List.rev(weeks^));
};

let isDateSelected = (date: Js.Date.t, selected: option(selected)): bool => {
  switch (selected) {
  | None => false
  | Some(`Single(optionDate)) =>
    switch (Js.Undefined.toOption(optionDate)) {
    | Some(value) => isSameDay(date, value)
    | None => false
    }
  | Some(`Multiple(optionDates)) =>
    switch (Js.Undefined.toOption(optionDates)) {
    | Some(values) => values |> Array.exists(value => isSameDay(date, value))
    | None => false
    }
  | Some(`Range(optionRange)) =>
    switch (Js.Undefined.toOption(optionRange)) {
    | None => false
    | Some(rangeValue) =>
      switch (
        Js.Undefined.toOption(rangeValue.from),
        Js.Undefined.toOption(rangeValue.to_),
      ) {
      | (Some(from), Some(to_)) =>
        (isAfter(date, from) || isSameDay(date, from))
        && (isBefore(date, to_) || isSameDay(date, to_))
      | (Some(from), None) => isSameDay(date, from)
      | (None, Some(to_)) => isSameDay(date, to_)
      | (None, None) => false
      }
    }
  };
};

let isFocusableDay = (day: calendarDay): bool => !day.isOutside;

let getInitialFocusTarget = (
  weeks: array(calendarWeek),
  selected: option(selected),
  today: Js.Date.t,
): option(Js.Date.t) => {
  let firstFocusable = ref(None);
  let selectedFocusable = ref(None);
  let todayFocusable = ref(None);
  let setIfEmpty = (targetRef, value) =>
    switch (targetRef^) {
    | None => targetRef := Some(value)
    | Some(_) => ()
    };

  weeks
  |> Array.iter((week: calendarWeek) =>
       week.days
       |> Array.iter((day: calendarDay) => {
            if (isFocusableDay(day)) {
              setIfEmpty(firstFocusable, day.date);
              if (isSameDay(day.date, today)) {
                setIfEmpty(todayFocusable, day.date);
              };
              if (isDateSelected(day.date, selected)) {
                setIfEmpty(selectedFocusable, day.date);
              };
            };
          })
     );

  switch (selectedFocusable^) {
  | Some(_) as target => target
  | None =>
    switch (todayFocusable^) {
    | Some(_) as target => target
    | None => firstFocusable^
    }
  };
};

type rangePosition =
  | NoRange
  | RangeStart
  | RangeMiddle
  | RangeEnd
  | RangeStartAndEnd;

let getRangePosition =
    (date: Js.Date.t, selected: option(selected)): rangePosition => {
  switch (selected) {
  | Some(`Range(optionRange)) =>
    switch (Js.Undefined.toOption(optionRange)) {
    | Some(rangeValue) =>
      let startOpt = Js.Undefined.toOption(rangeValue.from);
      let endOpt = Js.Undefined.toOption(rangeValue.to_);
      switch (startOpt, endOpt) {
      | (Some(start), Some(rangeEnd)) =>
        let isStart = isSameDay(date, start);
        let isEnd = isSameDay(date, rangeEnd);
        if (isStart && isEnd) {
          RangeStartAndEnd;
        } else if (isStart) {
          RangeStart;
        } else if (isEnd) {
          RangeEnd;
        } else if (isAfter(date, start) && isBefore(date, rangeEnd)) {
          RangeMiddle;
        } else {
          NoRange;
        };
      | _ => NoRange
      };
    | None => NoRange
    }
  | _ => NoRange
  };
};

let joinClassList = (values: array(string)): string => {
  Array.fold_left(
    (acc, value) =>
      if (String.length(acc) == 0) {
        value;
      } else {
        acc ++ " " ++ value;
      },
    "",
    values,
  );
};

let ordinalSuffix = (day: int): string => {
  let mod100 = day mod 100;
  if (mod100 == 11 || mod100 == 12 || mod100 == 13) {
    "th";
  } else {
    switch (day mod 10) {
    | 1 => "st"
    | 2 => "nd"
    | 3 => "rd"
    | _ => "th"
    };
  };
};

let formatAriaDayLabel = (date: Js.Date.t, ~isToday: bool, ~isSelected: bool): string => {
  let prefix = if (isToday) {"Today, "} else {""};
  let suffix = if (isSelected) {", selected"} else {""};
  prefix
  ++ getWeekdayName(dayOfWeek(date))
  ++ ", "
  ++ getMonthName(dateMonth(date))
  ++ " "
  ++ string_of_int(dateDay(date))
  ++ ordinalSuffix(dateDay(date))
  ++ ", "
  ++ string_of_int(dateYear(date))
  ++ suffix;
};

let getDayClasses =
    (
      day: calendarDay,
      selected: option(selected),
      today: Js.Date.t,
      showOutsideDays: bool,
    )
    : string => {
  let modifiers = ref([]);
  if (isSameDay(day.date, today)) {
    modifiers := List.concat([modifiers^, [classNames.dayToday]]);
  };
  if (isDateSelected(day.date, selected)) {
    modifiers := List.concat([modifiers^, [classNames.daySelected]]);
  };
  if (day.isOutside && !showOutsideDays) {
    modifiers := List.concat([modifiers^, [classNames.dayHidden]]);
  };
  if (day.isOutside) {
    modifiers := List.concat([modifiers^, [classNames.dayOutside]]);
  };
  switch (getRangePosition(day.date, selected)) {
  | RangeStart =>
    modifiers := List.concat([modifiers^, [classNames.dayRangeStart]])
  | RangeMiddle =>
    modifiers := List.concat([modifiers^, [classNames.dayRangeMiddle]])
  | RangeEnd =>
    modifiers := List.concat([modifiers^, [classNames.dayRangeEnd]])
  | RangeStartAndEnd =>
    modifiers := List.concat([modifiers^, [classNames.dayRangeStart, classNames.dayRangeEnd]])
  | NoRange => ()
  };

  let classModifiers = Array.of_list(modifiers^);
  let joined = joinClassList(classModifiers);
  if (String.length(joined) == 0) {
    classNames.day;
  } else {
    classNames.day ++ " " ++ joined;
  };
};

let stringProp =
    (name: string, jsxName: string, value: string): React.JSX.prop =>
  React.JSX.string(name, jsxName, value);
let intProp = (name: string, jsxName: string, value: int): React.JSX.prop =>
  React.JSX.int(name, jsxName, value);
let boolProp = (name: string, jsxName: string, value: bool): React.JSX.prop =>
  React.JSX.bool(name, jsxName, value);
let styleProp = (value: list((string, string, string))): React.JSX.prop =>
  React.JSX.style(value);
let classNameProp = (value: string): React.JSX.prop =>
  React.JSX.string("class", "className", value);
let roleProp = (value: string): React.JSX.prop =>
  React.JSX.string("role", "role", value);
let scopeProp = (value: string): React.JSX.prop =>
  React.JSX.string("scope", "scope", value);
let ariaLabelProp = (value: string): React.JSX.prop =>
  stringProp("aria-label", "ariaLabel", value);
let ariaHiddenProp = (value: string): React.JSX.prop =>
  stringProp("aria-hidden", "ariaHidden", value);

let element = (~key=?, ~props=[], tag, children) =>
  React.createElementWithKey(~key?, tag, props, children);

let renderCaptionChevron = () =>
  element(
    "svg",
    ~props=[
      classNameProp(classNames.chevron),
      intProp("width", "width", 18),
      intProp("height", "height", 18),
      stringProp("viewBox", "viewBox", "0 0 24 24"),
    ],
    [
      element(
        "polygon",
        ~props=[
          stringProp(
            "points",
            "points",
            "6.77 8 12.5 13.57 18.24 8 20 9.72 12.5 17 5 9.72",
          ),
        ],
        [],
      ),
    ],
  );

let renderMonthOption = (monthIndex: int, selectedMonth: int) => {
  let props =
    List.concat([
      [stringProp("value", "value", string_of_int(monthIndex))],
      monthIndex == selectedMonth
        ? [stringProp("selected", "selected", "")] : [],
    ]);
  element("option", ~props, [React.string(getMonthName(monthIndex))]);
};

let renderYearOption = (yearValue: int, selectedYear: int) => {
  let props =
    List.concat([
      [stringProp("value", "value", string_of_int(yearValue))],
      yearValue == selectedYear
        ? [stringProp("selected", "selected", "")] : [],
    ]);
  element("option", ~props, [React.string(string_of_int(yearValue))]);
};

let renderDropdownRoot =
    (
      ~disabled: bool,
      ~selectClassName: string,
      ~ariaLabel: string,
      ~selectChildren: array(React.element),
      ~displayLabel: string,
    ) =>
  element(
    "span",
    ~props=[
      stringProp(
        "data-disabled",
        "dataDisabled",
        disabled ? "true" : "false",
      ),
      classNameProp(classNames.dropdownRoot),
    ],
    [
      element(
        "select",
        ~props=
          List.concat([
            [classNameProp(selectClassName), ariaLabelProp(ariaLabel)],
            disabled ? [boolProp("disabled", "disabled", true)] : [],
          ]),
        [React.array(selectChildren)],
      ),
      element(
        "span",
        ~props=[
          classNameProp(classNames.captionLabel),
          ariaHiddenProp("true"),
        ],
        [React.string(displayLabel), renderCaptionChevron()],
      ),
    ],
  );

let renderCaptionDropdowns =
    (
      ~captionLayout: captionLayout,
      ~monthDate: Js.Date.t,
      ~navigationDisabled: bool,
      ~todayYear: int,
      ~reverseYears: bool,
    ) => {
  let selectedMonth = dateMonth(monthDate);
  let selectedYear = dateYear(monthDate);
  let monthOptions =
    Array.init(12, monthIndex =>
      renderMonthOption(monthIndex, selectedMonth)
    );
  let startYear = todayYear - 100;
  let yearCount = todayYear - startYear + 1;
  let yearOptions =
    Array.init(
      yearCount,
      index => {
        let yearValue = reverseYears ? todayYear - index : startYear + index;
        renderYearOption(yearValue, selectedYear);
      },
    );

  let liveRegion =
    element(
      "span",
      ~props=[
        roleProp("status"),
        stringProp("aria-live", "ariaLive", "polite"),
        styleProp([
          ("border", "border", "0"),
          ("clip", "clip", "rect(0 0 0 0)"),
          ("height", "height", "1px"),
          ("margin", "margin", "-1px"),
          ("overflow", "overflow", "hidden"),
          ("padding", "padding", "0"),
          ("position", "position", "absolute"),
          ("width", "width", "1px"),
          ("white-space", "whiteSpace", "nowrap"),
          ("word-wrap", "wordWrap", "normal"),
        ]),
      ],
      [React.string(formatMonthYear(monthDate))],
    );

  switch (captionLayout) {
  | `Dropdown =>
    element(
      "div",
      ~props=[classNameProp(classNames.dropdowns)],
      [
        renderDropdownRoot(
          ~disabled=navigationDisabled,
          ~selectClassName=
            classNames.dropdown ++ " " ++ classNames.monthsDropdown,
          ~ariaLabel="Choose the Month",
          ~selectChildren=monthOptions,
          ~displayLabel=getMonthName(selectedMonth),
        ),
        renderDropdownRoot(
          ~disabled=navigationDisabled,
          ~selectClassName=
            classNames.dropdown ++ " " ++ classNames.yearsDropdown,
          ~ariaLabel="Choose the Year",
          ~selectChildren=yearOptions,
          ~displayLabel=string_of_int(selectedYear),
        ),
        liveRegion,
      ],
    )
  | `DropdownMonths =>
    element(
      "div",
      ~props=[classNameProp(classNames.dropdowns)],
      [
        renderDropdownRoot(
          ~disabled=navigationDisabled,
          ~selectClassName=
            classNames.dropdown ++ " " ++ classNames.monthsDropdown,
          ~ariaLabel="Choose the Month",
          ~selectChildren=monthOptions,
          ~displayLabel=getMonthName(selectedMonth),
        ),
        element("span", [React.string(string_of_int(selectedYear))]),
        liveRegion,
      ],
    )
  | `DropdownYears =>
    element(
      "div",
      ~props=[classNameProp(classNames.dropdowns)],
      [
        element("span", [React.string(getMonthName(selectedMonth))]),
        renderDropdownRoot(
          ~disabled=navigationDisabled,
          ~selectClassName=
            classNames.dropdown ++ " " ++ classNames.yearsDropdown,
          ~ariaLabel="Choose the Year",
          ~selectChildren=yearOptions,
          ~displayLabel=string_of_int(selectedYear),
        ),
        liveRegion,
      ],
    )
  | `Label => liveRegion
  };
};

let renderWeekdayHeader = (showWeekNum: bool, ~animate: bool) => {
  let weekdayCells =
    Array.init(7, dayIndex =>
      element(
        "th",
        ~props=[
          ariaLabelProp(getWeekdayName(dayIndex)),
          classNameProp(classNames.weekday),
          scopeProp("col"),
        ],
        [React.string(getWeekdayShort(dayIndex))],
      )
    );

  let weekNumberHeader =
    if (showWeekNum) {
      element(
        "th",
        ~props=[
          ariaLabelProp("Week Number"),
          classNameProp(classNames.weekNumberHeader),
          scopeProp("col"),
        ],
        [React.string("")],
      );
    } else {
      React.null;
    };

  element(
    "thead",
    ~props=[stringProp("aria-hidden", "ariaHidden", "true")],
    [
      element(
        "tr",
        ~props=
          animate
            ? [
              stringProp(
                "data-animated-weekdays",
                "dataAnimatedWeekdays",
                "true",
              ),
              classNameProp(classNames.weekdays),
            ]
            : [classNameProp(classNames.weekdays)],
        [weekNumberHeader, React.array(weekdayCells)],
      ),
    ],
  );
};

let renderWeekRow =
    (
      week: calendarWeek,
      ~showWeekNum: bool,
      ~focusTarget: option(Js.Date.t),
      selected: option(selected),
      today: Js.Date.t,
      showOutsideDays: bool,
    ) => {
  let weekNumberCell =
    if (showWeekNum) {
      element(
        "th",
        ~props=[
          ariaLabelProp("Week " ++ string_of_int(week.weekNumber)),
          classNameProp(classNames.weekNumber),
          scopeProp("row"),
          roleProp("rowheader"),
        ],
        [React.string(string_of_int(week.weekNumber))],
      );
    } else {
      React.null;
    };

  let dayCells =
    week.days
    |> Array.map((day: calendarDay) => {
         let cellClassName =
           getDayClasses(day, selected, today, showOutsideDays);
         let dayIsToday = isSameDay(day.date, today);
         let dayIsSelected = isDateSelected(day.date, selected);
         let dayIsFocusTarget =
           switch (focusTarget) {
           | Some(target) => isSameDay(day.date, target)
           | None => false
           };
         let hasContent = showOutsideDays || !day.isOutside;
         let tdProps = ref([classNameProp(cellClassName), roleProp("gridcell")]);
         if (dayIsSelected) {
           tdProps :=
             List.concat([
               tdProps^,
               [stringProp("aria-selected", "ariaSelected", "true")],
             ]);
         };
         tdProps :=
           List.concat([
             tdProps^,
             [stringProp("data-day", "dataDay", formatISODate(day.date))],
           ]);
         if (dayIsSelected) {
           tdProps :=
             List.concat([
               tdProps^,
               [stringProp("data-selected", "dataSelected", "true")],
             ]);
         };
         if (day.isOutside) {
           tdProps :=
             List.concat([
               tdProps^,
               showOutsideDays
                 ? [
                   stringProp(
                     "data-month",
                     "dataMonth",
                     formatYearMonth(day.date),
                   ),
                   stringProp("data-outside", "dataOutside", "true"),
                 ]
                 : [
                   stringProp(
                     "data-month",
                     "dataMonth",
                     formatYearMonth(day.date),
                   ),
                   stringProp("data-hidden", "dataHidden", "true"),
                   stringProp("data-outside", "dataOutside", "true"),
                 ],
             ]);
         };
         if (dayIsToday) {
           tdProps :=
             List.concat([
               tdProps^,
               [stringProp("data-today", "dataToday", "true")],
             ]);
         };

          let content =
            if (hasContent) {
              element(
                "button",
                ~props=[
                  classNameProp(classNames.dayButton),
                  stringProp("type", "type", "button"),
                  intProp("tabindex", "tabIndex", dayIsFocusTarget ? 0 : (-1)),
                  ariaLabelProp(
                    formatAriaDayLabel(
                      day.date,
                     ~isToday=dayIsToday,
                     ~isSelected=dayIsSelected,
                   ),
                 ),
               ],
               [React.string(formatDayNumber(day.date))],
             );
           } else {
             React.string("");
           };

         element("td", ~props=tdProps^, [content]);
       });

  element(
    "tr",
    ~props=[classNameProp(classNames.week)],
    [weekNumberCell, React.array(dayCells)],
  );
};

let renderMonth =
    (
      monthIndex: int,
      ~multiSelectable: bool,
      ~showOutsideDays: bool,
      ~showWeekNumber: bool,
      ~hideWeekdays: bool,
      ~fixedWeeks: bool,
      ~captionLayout: option(captionLayout),
      ~reverseYears: bool,
      ~animate: bool,
      ~navigationDisabled: bool,
      ~embeddedNavigation: option(React.element),
      ~todayYear: int,
      today: Js.Date.t,
      selected: option(selected),
    ) => {
  let monthStart =
    startOfMonth(today) |> setMonth(_, dateMonth(today) + monthIndex);
  let weeks = buildMonthWeeks(monthStart, ~weekStartsOn=0, ~fixedWeeks, ());
  let focusTarget = getInitialFocusTarget(weeks, selected, today);
  let weekdayHeader =
    hideWeekdays ? React.null : renderWeekdayHeader(showWeekNumber, ~animate);
  let weekRows =
    weeks
    |> Array.map((week: calendarWeek) =>
         renderWeekRow(
            week,
            ~showWeekNum=showWeekNumber,
            ~focusTarget,
            selected,
            today,
            showOutsideDays,
         )
       );

  let captionContent =
    switch (captionLayout) {
    | Some(`Dropdown as layout)
    | Some(`DropdownMonths as layout)
    | Some(`DropdownYears as layout) =>
      renderCaptionDropdowns(
        ~captionLayout=layout,
        ~monthDate=monthStart,
        ~navigationDisabled,
        ~todayYear,
        ~reverseYears,
      )
    | _ =>
      element(
        "span",
        ~props=[
          classNameProp(classNames.captionLabel),
          roleProp("status"),
          stringProp("aria-live", "ariaLive", "polite"),
        ],
        [React.string(formatMonthYear(monthStart))],
      )
    };

  let captionProps =
    animate
      ? [
        stringProp("data-animated-caption", "dataAnimatedCaption", "true"),
        classNameProp(classNames.monthCaption),
      ]
      : [classNameProp(classNames.monthCaption)];
  let monthProps =
    animate
      ? [
        stringProp("data-animated-month", "dataAnimatedMonth", "true"),
        classNameProp(classNames.month),
      ]
      : [classNameProp(classNames.month)];
  let weeksProps =
    animate
      ? [
        stringProp("data-animated-weeks", "dataAnimatedWeeks", "true"),
        classNameProp(classNames.weeks),
      ]
      : [classNameProp(classNames.weeks)];
  let monthChildren =
    switch (embeddedNavigation) {
    | Some(navigation) => [
        element("div", ~props=captionProps, [captionContent]),
        navigation,
        element(
          "table",
          ~props=[
            roleProp("grid"),
            stringProp(
              "aria-multiselectable",
              "ariaMultiselectable",
              multiSelectable ? "true" : "false",
            ),
            ariaLabelProp(formatMonthYear(monthStart)),
            classNameProp(classNames.monthGrid),
          ],
          [
            weekdayHeader,
            element("tbody", ~props=weeksProps, [React.array(weekRows)]),
          ],
        ),
      ]
    | None => [
        element("div", ~props=captionProps, [captionContent]),
        element(
          "table",
          ~props=[
            roleProp("grid"),
            stringProp(
              "aria-multiselectable",
              "ariaMultiselectable",
              multiSelectable ? "true" : "false",
            ),
            ariaLabelProp(formatMonthYear(monthStart)),
            classNameProp(classNames.monthGrid),
          ],
          [
            weekdayHeader,
            element("tbody", ~props=weeksProps, [React.array(weekRows)]),
          ],
        ),
      ]
    };

  element(
    "div",
    ~key=string_of_int(monthIndex),
    ~props=monthProps,
    monthChildren,
  );
};

let renderNavigation = (~animate: bool, ~navigationDisabled: bool) => {
  let chevron = (points: string) =>
    element(
      "svg",
      ~props=[
        classNameProp(classNames.chevron),
        intProp("width", "width", 24),
        intProp("height", "height", 24),
        stringProp("viewBox", "viewBox", "0 0 24 24"),
      ],
      [
        element(
          "polygon",
          ~props=[stringProp("points", "points", points)],
          [],
        ),
      ],
    );

  element(
    "nav",
    ~props=
      animate
        ? [
          stringProp("data-animated-nav", "dataAnimatedNav", "true"),
          classNameProp(classNames.nav),
          ariaLabelProp("Navigation bar"),
        ]
        : [classNameProp(classNames.nav), ariaLabelProp("Navigation bar")],
    [
      element(
        "button",
        ~props=
          navigationDisabled
            ? [
              stringProp("type", "type", "button"),
              classNameProp(classNames.buttonPrevious),
              intProp("tabindex", "tabIndex", -1),
              stringProp("aria-disabled", "ariaDisabled", "true"),
              ariaLabelProp("Go to the Previous Month"),
            ]
            : [
              stringProp("type", "type", "button"),
              classNameProp(classNames.buttonPrevious),
              ariaLabelProp("Go to the Previous Month"),
            ],
        [
          chevron(
            "16 18.112 9.81111111 12 16 5.87733333 14.0888889 4 6 12 14.0888889 20",
          ),
        ],
      ),
      element(
        "button",
        ~props=
          navigationDisabled
            ? [
              stringProp("type", "type", "button"),
              classNameProp(classNames.buttonNext),
              intProp("tabindex", "tabIndex", -1),
              stringProp("aria-disabled", "ariaDisabled", "true"),
              ariaLabelProp("Go to the Next Month"),
            ]
            : [
              stringProp("type", "type", "button"),
              classNameProp(classNames.buttonNext),
              ariaLabelProp("Go to the Next Month"),
            ],
        [
          chevron(
            "8 18.112 14.18888889 12 8 5.87733333 9.91111111 4 18 12 9.91111111 20",
          ),
        ],
      ),
    ],
  );
};

let make =
    (
      ~mode: option(mode)=?,
      ~onSelect: option(onSelect)=?,
      ~selected: option(selected)=?,
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
      ~key: option(string)=?,
      (),
    ) => {
  ignore(onSelect);
  ignore(key);
  let currentMode =
    switch (mode) {
    | Some(value) => modeToString(value)
    | None => modeToString(`Single)
    };
  let multiSelectable = currentMode == "multiple" || currentMode == "range";
  let reverseYearsValue =
    switch (reverseYears) {
    | Some(value) => value
    | None => false
    };
  let navLayoutAfter =
    switch (navLayout) {
    | Some(`After) => true
    | _ => false
    };
  let navigationDisabled =
    switch (disableNavigation) {
    | Some(value) => value
    | None => false
    };
  let navigationHidden =
    switch (hideNavigation) {
    | Some(value) => value
    | None => false
    };
  let animateValue =
    switch (animate) {
    | Some(value) => value
    | None => false
    };
  ignore(pagedNavigation);
  let showOutside =
    switch (showOutsideDays) {
    | Some(value) => value
    | None => false
    };
  let showWeekNum =
    switch (showWeekNumber) {
    | Some(value) => value
    | None => false
    };
  let hideWeekdaysValue =
    switch (hideWeekdays) {
    | Some(value) => value
    | None => false
    };
  let numberOfMonthsValue =
    switch (numberOfMonths) {
    | Some(value) => value
    | None => 1
    };
  let reverseMonthsValue =
    switch (reverseMonths) {
    | Some(value) => value
    | None => false
    };
  let fixedWeeksValue =
    switch (fixedWeeks) {
    | Some(value) => value
    | None => false
    };
  let today = Js.Date.make();
  let todayYear = dateYear(today);
  let navigationElement =
    if (navigationHidden) {
      None;
    } else {
      Some(renderNavigation(~animate=animateValue, ~navigationDisabled));
    };
  let monthIndices =
    Array.init(numberOfMonthsValue, idx =>
      if (reverseMonthsValue) {
        numberOfMonthsValue - idx - 1;
      } else {
        idx;
      }
    );
  let monthElements =
    monthIndices
    |> Array.mapi((renderIndex, monthIndex) => {
         let embeddedNavigation =
           if (navLayoutAfter && renderIndex == numberOfMonthsValue - 1) {
             navigationElement;
           } else {
             None;
           };
          renderMonth(
            monthIndex,
            ~multiSelectable,
            ~showOutsideDays=showOutside,
            ~showWeekNumber=showWeekNum,
            ~hideWeekdays=hideWeekdaysValue,
           ~fixedWeeks=fixedWeeksValue,
           ~captionLayout,
           ~reverseYears=reverseYearsValue,
           ~animate=animateValue,
           ~navigationDisabled,
           ~embeddedNavigation,
           ~todayYear,
           today,
           selected,
         );
       });

  let monthsChildren =
    switch (navigationElement) {
    | Some(value) when !navLayoutAfter => [
        value,
        React.array(monthElements),
      ]
    | _ => [React.array(monthElements)]
    };

  let footerElement =
    switch (footer) {
    | Some(value) =>
      element(
        "div",
        ~props=[
          classNameProp(classNames.footer),
          roleProp("status"),
          stringProp("aria-live", "ariaLive", "polite"),
        ],
        [value],
      )
    | None => React.null
    };

  let rootProps =
    List.concat([
      [
        classNameProp(classNames.root),
        stringProp("lang", "lang", "en-US"),
        stringProp("data-mode", "dataMode", currentMode),
      ],
      numberOfMonthsValue > 1
        ? [stringProp("data-multiple-months", "dataMultipleMonths", "true")]
        : [],
      showWeekNum
        ? [stringProp("data-week-numbers", "dataWeekNumbers", "true")] : [],
      navLayoutAfter
        ? [
          stringProp(
            "data-nav-layout",
            "dataNavLayout",
            navLayoutToString(`After),
          ),
        ]
        : [],
    ]);

  element(
    "div",
    ~props=rootProps,
    [
      element(
        "div",
        ~props=[classNameProp(classNames.months)],
        monthsChildren,
      ),
      footerElement,
    ],
  );
};
