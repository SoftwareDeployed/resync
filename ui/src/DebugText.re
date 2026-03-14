[@react.component]
let make = (~text: string) =>
  <p className="text-gray-400"> {React.string(text)} </p>;
