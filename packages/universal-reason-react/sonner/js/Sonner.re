[@melange_only]
type navigator;

[@melange_only]
type clipboard;

[@melange_only]
type location;

[@melange_only]
[@mel.module "sonner"]
external toaster: React.component(Js.t({. richColors: bool})) = "Toaster";

[@melange_only]
[@mel.module "sonner"]
[@mel.scope "toast"]
external toastSuccess: string => unit = "success";

[@melange_only]
[@mel.module "sonner"]
[@mel.scope "toast"]
external toastError: string => unit = "error";

[@melange_only]
external navigator: navigator = "navigator";

[@melange_only]
external location: location = "location";

[@melange_only]
[@mel.get]
external clipboard: navigator => clipboard = "clipboard";

[@melange_only]
[@mel.send]
external writeText: (clipboard, string) => Js.Promise.t(unit) = "writeText";

[@melange_only]
[@mel.get]
external href: location => string = "href";

[@melange_only]
let renderToaster = () => React.createElement(toaster, [%obj {richColors: true}]);

[@melange_only]
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

[@melange_only]
let showError = message => toastError(message);

[@native_only]
let renderToaster = () => React.null;

[@native_only]
let copyCurrentUrl = () => Js.Promise.resolve();

[@native_only]
let showError = _message => ();
