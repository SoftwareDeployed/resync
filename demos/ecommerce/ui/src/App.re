[@react.component]
let make =
    (
      ~routeRoot: option(string)=?,
      ~serverPath: option(string)=?,
      ~serverSearch="",
    ) => {
  <UniversalRouter router=Routes.router routeRoot=?routeRoot serverPath=?serverPath serverSearch />;
};
