type hooks('config) = {
  init: 'config => 'config,
  mount: StoreSource.actions('config) => unit,
  afterSet: 'config => unit,
};

let none: hooks('config) = {
  init: config => config,
  mount: _actions => (),
  afterSet: _config => (),
};

let applyInit = (~layers: array(hooks('config)), config: 'config) => {
  let rec loop = (index, current) =>
    if (index >= Array.length(layers)) {
      current;
    } else {
      let layer = Array.get(layers, index);
      loop(index + 1, layer.init(current));
    };
  loop(0, config);
};

let mount = (~layers: array(hooks('config)), actions: StoreSource.actions('config)) => {
  let rec loop = index =>
    if (index < Array.length(layers)) {
      let layer = Array.get(layers, index);
      layer.mount(actions);
      loop(index + 1);
    };
  loop(0);
};

let afterSet = (~layers: array(hooks('config)), config: 'config) => {
  let rec loop = index =>
    if (index < Array.length(layers)) {
      let layer = Array.get(layers, index);
      layer.afterSet(config);
      loop(index + 1);
    };
  loop(0);
};

let source = (~layers: array(hooks('config)), config: 'config) =>
  switch%platform (Runtime.platform) {
  | Client =>
    if (Array.length(layers) == 0) {
      config;
    } else {
      let initial = applyInit(~layers, config);
      let configSource =
        StoreSource.make(
          ~afterSet=next => afterSet(~layers, next),
          ~mount=actions => mount(~layers, actions),
          initial,
        );
      configSource.value;
    }
  | Server => config
  };
