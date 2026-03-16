let get = name =>
  switch (name) {
  | "outdent" => Some(LucideGeneratedNodes_L.list_indent_decrease)
  | "octagon-alert" => Some(LucideGeneratedNodes_O.octagon_alert)
  | "octagon-minus" => Some(LucideGeneratedNodes_O.octagon_minus)
  | "octagon-pause" => Some(LucideGeneratedNodes_O.octagon_pause)
  | "octagon-x" => Some(LucideGeneratedNodes_O.octagon_x)
  | "octagon" => Some(LucideGeneratedNodes_O.octagon)
  | "omega" => Some(LucideGeneratedNodes_O.omega)
  | "option" => Some(LucideGeneratedNodes_O.option)
  | "orbit" => Some(LucideGeneratedNodes_O.orbit)
  | "origami" => Some(LucideGeneratedNodes_O.origami)
  | _ => None
  };
