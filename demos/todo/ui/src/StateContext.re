module Context = {
  let context: React.Context.t(option(Config.config)) = React.createContext(None);

  let useState = () => {
    React.useContext(context);
  };

  module Provider = {
    let make = React.Context.provider(context);
  };
};
