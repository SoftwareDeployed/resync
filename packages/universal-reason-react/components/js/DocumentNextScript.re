[@react.component]
let make = (~serializedState="", ~stateId="initial-store", ~scripts=[||]) => {
  let scriptNodes =
    scripts->Js.Array.map(~f=src => <script key=src type_="module" src=src />)
    ->React.array;
  <>
    <script
      type_="text/json"
      id=stateId
      dangerouslySetInnerHTML={"__html": serializedState}
    />
    {scriptNodes}
  </>;
};
