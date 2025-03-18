contract C {
    string constant const = "string with spaces";
    string constant notConst = "string with spaces";
    function constArg() public pure returns (uint) {
        return erc7201(const);
    }
    function notConstArg() public pure returns (uint) {
        return erc7201(notConst);
    }
}
// ----
