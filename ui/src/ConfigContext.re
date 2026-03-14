let context = React.createContext(Config.SSR.empty);

module Provider = {
  let make = React.Context.provider(context);
};
