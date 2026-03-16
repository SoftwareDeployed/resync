[@react.component]
let make = (~text: string) =>
  <p className="text-gray-800 text-xl"> {React.string(text)} </p>;
