type input_premise = {
  id: string,
  name: string,
  description: string,
  updated_at: string,
};

type t = {
  inventory: array(Config.InventoryItem.t),
  premise: input_premise,
};

[@mel.scope "JSON"] external parseJSON: string => t = "parse";

let toConfig = data => {
  let input: t = data->parseJSON;
  let premise: PeriodList.Premise.t = {
    id: input.premise.id,
    name: input.premise.name,
    description: input.premise.description,
    updated_at: Js.Date.fromString(input.premise.updated_at),
  };
  let inventory: array(Config.InventoryItem.t) = Obj.magic(input.inventory);
  let config: Config.t = {
    inventory,
    premise: Some(premise),
  };
  config;
};
