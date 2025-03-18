contract C {
    string constant const = ("x");
    string notConst = ("y");
    function constArg() public pure returns (uint) {
        return erc7201(const);
    }
    function notConstArg() public view returns (uint) {
        return erc7201(notConst);
    }
}
// ----
