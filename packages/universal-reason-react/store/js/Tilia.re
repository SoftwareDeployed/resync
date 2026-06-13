module Core = {
  [@mel.module "tilia/dist/index.mjs"] external make: 'a => 'a = "tilia";
  [@mel.module "tilia/dist/index.mjs"]
  external source:
    (. 'a, [@mel.uncurried] ((. 'a, 'a => unit) => 'ignored)) => 'a =
    "source";
  type signal('a);
  [@mel.module "tilia/dist/index.mjs"] external lift: signal('a) => 'a = "lift";
  [@mel.module "tilia/dist/index.mjs"]
  external signal: 'a => (signal('a), 'a => unit) = "signal";
  [@mel.module "tilia/dist/index.mjs"] external observe: _ => unit = "observe";
  [@mel.module "tilia/dist/index.mjs"] external computed: (unit => 'a) => 'a = "computed";
  type deriver('p) = {
    /**
   * Return a derived value to be inserted into a tilia object. This is like
   * a computed but with the tilia object as parameter.
   *
   * @param f The computation function that takes the tilia object as parameter.
   */
    derived: 'a. ('p => 'a) => 'a,
  };
  [@mel.module "tilia/dist/index.mjs"]
  external carve: (deriver('a) => 'a) => 'a = "carve";
};

[@platform native]
module React = {
  let leaf = c => c;
  let useTilia = () => ();
};

[@platform js]
module React = {
  [@mel.module "@tilia/react"]
  external leaf: React.component('a) => React.component('a) = "leaf";
  [@mel.module "@tilia/react"] external useTilia: unit => unit = "useTilia";
};
