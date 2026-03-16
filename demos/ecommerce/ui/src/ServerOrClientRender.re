[@react.component]
let make = (~server: unit => React.element, ~client: unit => React.element) => {
  let isClientMounted = UseMounted.use();

  switch (isClientMounted) {
  | false => server()
  | true => client()
  };
};
