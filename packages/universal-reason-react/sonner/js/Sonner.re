[@platform js]
type navigator;

[@platform js]
type clipboard;

[@platform js]
type location;

[@platform js]
[@mel.module "sonner"]
external toaster: React.component(Js.t({. richColors: bool})) = "Toaster";

[@platform js]
[@mel.module "sonner"]
[@mel.scope "toast"]
external toastSuccess: string => unit = "success";

[@platform js]
[@mel.module "sonner"]
[@mel.scope "toast"]
external toastError: string => unit = "error";

[@platform js]
external navigator: navigator = "navigator";

[@platform js]
external location: location = "location";

[@platform js]
[@mel.get]
external clipboard: navigator => clipboard = "clipboard";

[@platform js]
[@mel.send]
external writeText: (clipboard, string) => Js.Promise.t(unit) = "writeText";

[@platform js]
[@mel.get]
external href: location => string = "href";

[@platform js]
let renderToaster = () => React.createElement(toaster, [%obj {richColors: true}]);

[@platform js]
let copyCurrentUrl = () => {
  let url = href(location);
  Js.Promise.catch(
    _ => {
      toastError("Failed to copy URL to the clipboard");
      Js.Promise.resolve();
    },
    Js.Promise.then_(
      () => {
        toastSuccess(url ++ " was copied to the clipboard");
        Js.Promise.resolve();
      },
      writeText(clipboard(navigator), url),
    ),
  );
};

[@platform js]
let showError = message => toastError(message);

[@platform native]
let renderToaster = () => React.null;

[@platform native]
let copyCurrentUrl = () => Js.Promise.resolve();

[@platform native]
let showError = _message => ();
