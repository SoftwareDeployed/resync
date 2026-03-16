[@react.component]
let make = (~children: unit => React.element) => {
  <ServerOrClientRender client=children server={() => React.null} />;
};
