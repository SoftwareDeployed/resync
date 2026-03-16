[@react.component]
let make = (~children, ~className: option(string)=?) => {
  <div
    className={
      [|
        "m-1 p-1 bg-slate-100/70 border-hairline border-b-2 border-slate-200 shadow-sm shadow-slate-200/70 rounded-lg",
        switch (className) {
        | Some(className) => className
        | None => ""
        },
      |]
      ->Cx.cx
    }>
    children
  </div>;
};
