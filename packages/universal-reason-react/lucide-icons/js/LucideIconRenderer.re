module Types = LucideIconTypes;

let defaultSize = 24;
let defaultStrokeWidth = 2;

type resolvedAttrs = {
  cx: option(string),
  cy: option(string),
  d: option(string),
  fill: option(string),
  height: option(string),
  points: option(string),
  r: option(string),
  rx: option(string),
  ry: option(string),
  width: option(string),
  x: option(string),
  x1: option(string),
  x2: option(string),
  y: option(string),
  y1: option(string),
  y2: option(string),
};

let emptyResolvedAttrs: resolvedAttrs = {
  cx: None,
  cy: None,
  d: None,
  fill: None,
  height: None,
  points: None,
  r: None,
  rx: None,
  ry: None,
  width: None,
  x: None,
  x1: None,
  x2: None,
  y: None,
  y1: None,
  y2: None,
};

let mergeClassName = className =>
  switch (className) {
  | Some(value) =>
    let trimmed = String.trim(value);
    if (trimmed == "" || trimmed == "lucide") {
      "lucide";
    } else {
      "lucide " ++ trimmed;
    }
  | None => "lucide"
  };

let resolvedStrokeWidth = (~absoluteStrokeWidth, ~size, ~strokeWidth) =>
  if (absoluteStrokeWidth) {
    float_of_int(strokeWidth) *. 24. /. float_of_int(size);
  } else {
    float_of_int(strokeWidth);
  };

let floatToString = value => {
  let integerValue = int_of_float(value);
  if (value == float_of_int(integerValue)) {
    string_of_int(integerValue);
  } else {
    let text = string_of_float(value);
    let length = String.length(text);
    if (length > 0 && String.get(text, length - 1) == '.') {
      String.sub(text, 0, length - 1);
    } else {
      text;
    }
  };
};

let tagName = tag =>
  switch (tag) {
  | Types.Circle => "circle"
  | Types.Ellipse => "ellipse"
  | Types.Line => "line"
  | Types.Path => "path"
  | Types.Polygon => "polygon"
  | Types.Polyline => "polyline"
  | Types.Rect => "rect"
  };

let addAttr = (resolved: resolvedAttrs, attr) =>
  switch (attr) {
  | Types.Cx(value) => {...resolved, cx: Some(value)}
  | Types.Cy(value) => {...resolved, cy: Some(value)}
  | Types.D(value) => {...resolved, d: Some(value)}
  | Types.Fill(value) => {...resolved, fill: Some(value)}
  | Types.Height(value) => {...resolved, height: Some(value)}
  | Types.Points(value) => {...resolved, points: Some(value)}
  | Types.R(value) => {...resolved, r: Some(value)}
  | Types.Rx(value) => {...resolved, rx: Some(value)}
  | Types.Ry(value) => {...resolved, ry: Some(value)}
  | Types.Width(value) => {...resolved, width: Some(value)}
  | Types.X(value) => {...resolved, x: Some(value)}
  | Types.X1(value) => {...resolved, x1: Some(value)}
  | Types.X2(value) => {...resolved, x2: Some(value)}
  | Types.Y(value) => {...resolved, y: Some(value)}
  | Types.Y1(value) => {...resolved, y1: Some(value)}
  | Types.Y2(value) => {...resolved, y2: Some(value)}
  };

let resolveAttrs = attrs => Array.fold_left(addAttr, emptyResolvedAttrs, attrs);

[@platform native]
module Impl = {
  let propListOfResolvedAttrs = (resolved: resolvedAttrs) => {
    let props = [];
    let props =
      switch (resolved.cx) {
      | Some(value) => [React.JSX.string("cx", "cx", value), ...props]
      | None => props
      };
    let props =
      switch (resolved.cy) {
      | Some(value) => [React.JSX.string("cy", "cy", value), ...props]
      | None => props
      };
    let props =
      switch (resolved.d) {
      | Some(value) => [React.JSX.string("d", "d", value), ...props]
      | None => props
      };
    let props =
      switch (resolved.fill) {
      | Some(value) => [React.JSX.string("fill", "fill", value), ...props]
      | None => props
      };
    let props =
      switch (resolved.height) {
      | Some(value) => [React.JSX.string("height", "height", value), ...props]
      | None => props
      };
    let props =
      switch (resolved.points) {
      | Some(value) => [React.JSX.string("points", "points", value), ...props]
      | None => props
      };
    let props =
      switch (resolved.r) {
      | Some(value) => [React.JSX.string("r", "r", value), ...props]
      | None => props
      };
    let props =
      switch (resolved.rx) {
      | Some(value) => [React.JSX.string("rx", "rx", value), ...props]
      | None => props
      };
    let props =
      switch (resolved.ry) {
      | Some(value) => [React.JSX.string("ry", "ry", value), ...props]
      | None => props
      };
    let props =
      switch (resolved.width) {
      | Some(value) => [React.JSX.string("width", "width", value), ...props]
      | None => props
      };
    let props =
      switch (resolved.x) {
      | Some(value) => [React.JSX.string("x", "x", value), ...props]
      | None => props
      };
    let props =
      switch (resolved.x1) {
      | Some(value) => [React.JSX.string("x1", "x1", value), ...props]
      | None => props
      };
    let props =
      switch (resolved.x2) {
      | Some(value) => [React.JSX.string("x2", "x2", value), ...props]
      | None => props
      };
    let props =
      switch (resolved.y) {
      | Some(value) => [React.JSX.string("y", "y", value), ...props]
      | None => props
      };
    let props =
      switch (resolved.y1) {
      | Some(value) => [React.JSX.string("y1", "y1", value), ...props]
      | None => props
      };
    let props =
      switch (resolved.y2) {
      | Some(value) => [React.JSX.string("y2", "y2", value), ...props]
      | None => props
      };
    props->List.rev;
  };

  let renderChild = (child: Types.child): React.element => {
    let props = propListOfResolvedAttrs(resolveAttrs(child.attrs));
    React.createElementWithKey(~key=child.key, tagName(child.tag), props, []);
  };

  let render = (
    ~name: string,
    ~absoluteStrokeWidth=?,
    ~ariaLabel=?,
    ~className=?,
    ~color=?,
    ~size=?,
    ~strokeWidth=?,
    ~title=?,
    iconNode: Types.iconNode,
  ) => {
    let _ = name;
    let resolvedColor =
      switch (color) {
      | Some(value) => value
      | None => "currentColor"
      };
    let resolvedSize =
      switch (size) {
      | Some(value) => value
      | None => defaultSize
      };
    let requestedStrokeWidth =
      switch (strokeWidth) {
      | Some(value) => value
      | None => defaultStrokeWidth
      };
    let useAbsoluteStrokeWidth =
      switch (absoluteStrokeWidth) {
      | Some(value) => value
      | None => false
      };
    let strokeWidthValue =
      resolvedStrokeWidth(
        ~absoluteStrokeWidth=useAbsoluteStrokeWidth,
        ~size=resolvedSize,
        ~strokeWidth=requestedStrokeWidth,
      );
    let hasA11yProp =
      switch (ariaLabel, title) {
      | (Some(_), _)
      | (_, Some(_)) => true
      | _ => false
      };
    let props = [
      React.JSX.string("xmlns", "xmlns", "http://www.w3.org/2000/svg"),
      React.JSX.string("viewBox", "viewBox", "0 0 24 24"),
      React.JSX.string("fill", "fill", "none"),
      React.JSX.string("stroke", "stroke", resolvedColor),
      React.JSX.string("stroke-linecap", "strokeLinecap", "round"),
      React.JSX.string("stroke-linejoin", "strokeLinejoin", "round"),
      React.JSX.int("width", "width", resolvedSize),
      React.JSX.int("height", "height", resolvedSize),
      React.JSX.string("stroke-width", "strokeWidth", floatToString(strokeWidthValue)),
      React.JSX.string("class", "className", mergeClassName(className)),
    ]
    @
    switch (title) {
    | Some(value) => [React.JSX.string("title", "title", value)]
    | None => []
    }
    @
    switch (ariaLabel) {
    | Some(value) => [React.JSX.string("aria-label", "ariaLabel", value)]
    | None => []
    }
    @
    if (hasA11yProp) {
      [];
    } else {
      [React.JSX.string("aria-hidden", "ariaHidden", "true")];
    };

    let children = Array.map(renderChild, iconNode)->Array.to_list;
    React.createElement("svg", props, children);
  };
};

[@platform js]
module Impl = {
  type jsValue;

  external jsString: string => jsValue = "%identity";
  external jsBool: bool => jsValue = "%identity";
  external domPropsOfDict: Js.Dict.t(jsValue) => ReactDOM.domProps = "%identity";

  [@mel.obj]
  external makeElementProps:
    (
      ~key: string=?,
      ~cx: string=?,
      ~cy: string=?,
      ~d: string=?,
      ~fill: string=?,
      ~height: string=?,
      ~points: string=?,
      ~r: string=?,
      ~rx: string=?,
      ~ry: string=?,
      ~width: string=?,
      ~x: string=?,
      ~x1: string=?,
      ~x2: string=?,
      ~y: string=?,
      ~y1: string=?,
      ~y2: string=?,
      unit
    ) =>
    ReactDOM.domProps = "";

  let makeSvgProps = (
    ~xmlns,
    ~viewBox,
    ~fill,
    ~stroke,
    ~strokeLinecap,
    ~strokeLinejoin,
    ~width,
    ~height,
    ~strokeWidth,
    ~className,
    ~ariaHidden=?,
    ~ariaLabel=?,
    ~title=?,
    (),
  ) => {
    let props: Js.Dict.t(jsValue) = Js.Dict.empty();
    Js.Dict.set(props, "xmlns", jsString(xmlns));
    Js.Dict.set(props, "viewBox", jsString(viewBox));
    Js.Dict.set(props, "fill", jsString(fill));
    Js.Dict.set(props, "stroke", jsString(stroke));
    Js.Dict.set(props, "strokeLinecap", jsString(strokeLinecap));
    Js.Dict.set(props, "strokeLinejoin", jsString(strokeLinejoin));
    Js.Dict.set(props, "width", jsString(width));
    Js.Dict.set(props, "height", jsString(height));
    Js.Dict.set(props, "strokeWidth", jsString(strokeWidth));
    Js.Dict.set(props, "className", jsString(className));
    switch (ariaHidden) {
    | Some(value) => Js.Dict.set(props, "aria-hidden", jsBool(value))
    | None => ()
    };
    switch (ariaLabel) {
    | Some(value) => Js.Dict.set(props, "aria-label", jsString(value))
    | None => ()
    };
    switch (title) {
    | Some(value) => Js.Dict.set(props, "title", jsString(value))
    | None => ()
    };
    domPropsOfDict(props);
  };

  let renderChild = (child: Types.child): React.element => {
    let resolved = resolveAttrs(child.attrs);
    let {cx, cy, d, fill, height, points, r, rx, ry, width, x, x1, x2, y, y1, y2} = resolved;
    let props =
      makeElementProps(
        ~key=child.key,
        ~cx?,
        ~cy?,
        ~d?,
        ~fill?,
        ~height?,
        ~points?,
        ~r?,
        ~rx?,
        ~ry?,
        ~width?,
        ~x?,
        ~x1?,
        ~x2?,
        ~y?,
        ~y1?,
        ~y2?,
        (),
      );
    ReactDOM.createDOMElementVariadic(tagName(child.tag), ~props, [||]);
  };

  let render = (
    ~name: string,
    ~absoluteStrokeWidth=?,
    ~ariaLabel=?,
    ~className=?,
    ~color=?,
    ~size=?,
    ~strokeWidth=?,
    ~title=?,
    iconNode: Types.iconNode,
  ) => {
    let _ = name;
    let resolvedColor =
      switch (color) {
      | Some(value) => value
      | None => "currentColor"
      };
    let resolvedSize =
      switch (size) {
      | Some(value) => value
      | None => defaultSize
      };
    let requestedStrokeWidth =
      switch (strokeWidth) {
      | Some(value) => value
      | None => defaultStrokeWidth
      };
    let useAbsoluteStrokeWidth =
      switch (absoluteStrokeWidth) {
      | Some(value) => value
      | None => false
      };
    let strokeWidthValue =
      resolvedStrokeWidth(
        ~absoluteStrokeWidth=useAbsoluteStrokeWidth,
        ~size=resolvedSize,
        ~strokeWidth=requestedStrokeWidth,
      )->floatToString;
    let hasA11yProp =
      switch (ariaLabel, title) {
      | (Some(_), _)
      | (_, Some(_)) => true
      | _ => false
      };
    let ariaHidden = if (hasA11yProp) {None} else {Some(true)};

    let props =
      makeSvgProps(
        ~xmlns="http://www.w3.org/2000/svg",
        ~viewBox="0 0 24 24",
        ~fill="none",
        ~stroke=resolvedColor,
        ~strokeLinecap="round",
        ~strokeLinejoin="round",
        ~width=string_of_int(resolvedSize),
        ~height=string_of_int(resolvedSize),
        ~strokeWidth=strokeWidthValue,
        ~className=mergeClassName(className),
        ~ariaHidden?,
        ~ariaLabel?,
        ~title?,
        (),
      );
    let children = Array.map(renderChild, iconNode);
    ReactDOM.createDOMElementVariadic("svg", ~props, children);
  };
};

let render = Impl.render;
