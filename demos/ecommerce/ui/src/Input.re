type input_premise = {
  id: string,
  name: string,
  description: string,
  updated_at: string,
};

type t = {
  inventory: array(Model.InventoryItem.t),
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
  let inventory: array(Model.InventoryItem.t) = Obj.magic(input.inventory);
  let config: Model.t = {
    inventory,
    premise: Some(premise),
  };
  config;
};
