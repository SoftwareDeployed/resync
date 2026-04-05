[@platform js]
module JSImport = {
  [%%raw
    {|
import React from "react";
import { Toaster, toast } from "sonner";

export const __renderToaster = () =>
  React.createElement(Toaster, { richColors: true });

export const __copyCurrentUrl = async () => {
  const url = window.location.href;

  try {
    await navigator.clipboard.writeText(url);
    toast.success(`${url} was copied to the clipboard`);
  } catch (_error) {
    toast.error("Failed to copy URL to the clipboard");
  }
};

export const __showError = (message) => {
  toast.error(message);
};
|}];

  external renderToaster: unit => React.element = "__renderToaster";
  external copyCurrentUrl: unit => Js.Promise.t(unit) = "__copyCurrentUrl";
  external showError: string => unit = "__showError";
};

[@platform js]
let renderToaster = JSImport.renderToaster;

[@platform js]
let copyCurrentUrl = JSImport.copyCurrentUrl;

[@platform js]
let showError = JSImport.showError;

[@platform native]
let renderToaster = () => React.null;

[@platform native]
let copyCurrentUrl = () => Js.Promise.resolve();

[@platform native]
let showError = _message => ();
