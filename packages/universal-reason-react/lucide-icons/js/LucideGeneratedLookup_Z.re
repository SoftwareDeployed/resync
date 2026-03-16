let get = name =>
  switch (name) {
  | "zap-off" => Some(LucideGeneratedNodes_Z.zap_off)
  | "zap" => Some(LucideGeneratedNodes_Z.zap)
  | "zoom-in" => Some(LucideGeneratedNodes_Z.zoom_in)
  | _ => None
  };
