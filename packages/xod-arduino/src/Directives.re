open Belt;

type code = string;

module Endis = {
  type t =
    | Enable
    | Disable
    | Auto;
  let toBoolean = (hint, default) =>
    switch (hint) {
    | Enable => true
    | Disable => false
    | Auto => default
    };
  let fromString = tok =>
    switch (tok) {
    | "enable" => Enable
    | "disable" => Disable
    | _ => Auto
    };
};

module Pragma = {
  type t = list(string);
  let filterPragmasByFeature = (pragmas: list(t), feature) =>
    pragmas
    |. List.keep(pragma =>
         switch (pragma) {
         | [] => false
         | [feat, ..._] => feat == feature
         }
       );
};

module Re = {
  let replace = (~flags="gm", str, regex, sub) =>
    Js.String.replaceByRe(
      Js.Re.fromStringWithFlags(regex, ~flags),
      sub,
      str,
    );
  let remove = (~flags="gm", str, regex) => replace(~flags, str, regex, "");
  let test = (~flags="gm", str, regex) =>
    Js.Re.test_(Js.Re.fromStringWithFlags(regex, ~flags), str);
  let matches = (~flags="gm", str, regex) => {
    let reObj = Js.Re.fromStringWithFlags(regex, ~flags);
    /* Extracts the current match, ignoring capturing groups.
       Assumption: 0-th always exists and refers to the full match */
    let fullMatch = execResult =>
      execResult
      |. Js.Re.captures
      |. Array.getExn(0)
      |. Js.Nullable.toOption
      |. Option.getExn;
    let rec captureNext = () : list(string) =>
      switch (Js.Re.exec_(reObj, str)) {
      | None => []
      | Some(result) => List.add(captureNext(), fullMatch(result))
      };
    captureNext();
  };
};

module Code = {
  /*
    Regexp from here: https://regex101.com/r/qY4xD3/15
    Found a link here: https://stackoverflow.com/questions/36454069/how-to-remove-c-style-comments-from-code
    Edited: removed one `\n` in the second non-capturing group to avoid removing new lines
   */
  let allCommentsRegexp = {|(?:\/\/(?:\\\n|[^\n])*)|(?:\/\*[\s\S]*?\*\/)|((?:"([^(\\\s])\([^)]*\)\2")|(?:@"[^"]*?")|(?:"(?:\?\?'|\\\\|\\"|\\\n|[^"])*?")|(?:'(?:\\\\|\\'|\\\n|[^'])*?'))|};
  let trimRegexp = {|\s*$|};
  let stripCppComments = code =>
    code |. Re.replace(allCommentsRegexp, "$1") |. Re.remove(trimRegexp);
  let doesReferSymbol = (symbol, code) =>
    code |. stripCppComments |. Re.test("\\b" ++ symbol ++ "\\b");
  let doesReferTemplateSymbol = (symbol, templateArg, code) =>
    code
    |. stripCppComments
    |. Re.test("\\b" ++ symbol ++ "\\s*\\<\\s*" ++ templateArg ++ "\\s*\\>");
  let pragmaHeadRegexp = {|#\s*pragma\s+XOD\s+|};
  let pragmaLineRegexp = pragmaHeadRegexp ++ ".*";
  let identifierOrStringRegexp = {foo|[\w._-]+|".*?"|foo};
  let enclosingQuotesRegexp = {|^"|"$|};
  let isOutput = identifier => Re.test(identifier, {|^output_|});
  let tokenizePragma = (pragmaLine: string) : Pragma.t =>
    pragmaLine
    |. Re.remove(pragmaHeadRegexp)
    |. Re.matches(identifierOrStringRegexp)
    |. List.map(token => Re.remove(token, enclosingQuotesRegexp));
  let findXodPragmas = code : list(Pragma.t) =>
    code
    |. stripCppComments
    |. Re.matches(pragmaLineRegexp)
    |. List.map(tokenizePragma);
  /*
      Returns whether a particular #pragma feature enabled, disabled, or set to auto.
      Default is auto
   */
  let lastPragmaEndis = (code, feature) : Endis.t =>
    code
    |. findXodPragmas
    |. Pragma.filterPragmasByFeature(feature)
    |. List.reverse
    |. List.head
    |. (
      lastPragma =>
        switch (lastPragma) {
        | Some([_, x, ..._]) => Endis.fromString(x)
        | _ => Endis.Auto
        }
    );
};

let areTimeoutsEnabled = code =>
  code
  |. Code.lastPragmaEndis("timeouts")
  |. Endis.toBoolean(Code.doesReferSymbol("setTimeout", code));

let isNodeIdEnabled = code =>
  code
  |. Code.lastPragmaEndis("nodeid")
  |. Endis.toBoolean(Code.doesReferSymbol("getNodeId", code));

let doesRaiseErrors = (code) =>
  code
  |. Code.lastPragmaEndis("error_raise")
  |. Endis.toBoolean(Code.doesReferSymbol("raiseError", code));

let implementsEvaluateTmpl = code =>
  code
  |. Code.lastPragmaEndis("evaluate_tmpl")
  |. Endis.toBoolean(Code.doesReferSymbol("evaluateTmpl", code));

let wantsStateConstructorWithParams = (code) =>
  code
  |. Code.lastPragmaEndis("state_constructor_params")
  |. Endis.toBoolean(false);

let isDirtienessEnabled = (code, identifier) =>
  code
  |. Code.findXodPragmas
  |. List.reduce(
       Code.isOutput(identifier)  /* dirtieness enabled on outputs by default */
       || Code.doesReferTemplateSymbol("isInputDirty", identifier, code),
       (acc, pragma) =>
       switch (pragma) {
       | ["dirtieness", hintTok] =>
         Endis.toBoolean(Endis.fromString(hintTok), acc)
       | ["dirtieness", hintTok, ident] when ident == identifier =>
         Endis.toBoolean(Endis.fromString(hintTok), acc)
       | _ => acc
       }
     );

type evaluateOnPinSettings = {
  enabled: bool,
  exceptions: Set.String.t
}

let mergeList = (s: Set.String.t, arr: list(string)): Set.String.t =>
  s |. Set.String.mergeMany(List.toArray(arr))

let removeList = (s: Set.String.t, arr: list(string)): Set.String.t =>
  s |. Set.String.removeMany(List.toArray(arr))

let getEvaluateOnPinSettings = (code) =>
  code
  |. Code.findXodPragmas
  |. Pragma.filterPragmasByFeature("evaluate_on_pin")
  |. List.reduce(
    {
      enabled: true,
      exceptions: Set.String.empty,
    },
    (acc, pragma) => switch (pragma) {
      | [_, "enable"] => {
        enabled: true,
        exceptions: Set.String.empty,
      }
      | [_, "disable"] => {
        enabled: false,
        exceptions: Set.String.empty,
      }
      | [_, "enable", ...enabledPins] => {
        ...acc,
        exceptions: acc.enabled 
          ? removeList(acc.exceptions, enabledPins)
          : mergeList(acc.exceptions, enabledPins)
      }
      | [_, "disable", ...disabledPins] => {
        ...acc,
        exceptions: acc.enabled 
          ? mergeList(acc.exceptions, disabledPins)
          : removeList(acc.exceptions, disabledPins)
      }
      | _ => acc
    }
  );

let doesCatchErrors = (code) =>
  code
  |. Code.lastPragmaEndis("error_catch")
  |. Endis.toBoolean(Code.doesReferSymbol("getError", code));

let findXodPragmas = Code.findXodPragmas;

let urlRegexp = {|^https?:\/\/(www\.)?[-a-zA-Z0-9@:%._\+~#=]{2,256}\.[a-z]{2,6}\b([-a-zA-Z0-9@:%_\+.~#?&//=]*)$|};

let findRequireUrls = code =>
  code
  |. Code.findXodPragmas
  |. List.reduce([], (acc, v) =>
       switch (v) {
       | ["require", url] => Re.test(url, urlRegexp) ? [url, ...acc] : acc
       | _ => acc
       }
     );

let stripCppComments = Code.stripCppComments;
