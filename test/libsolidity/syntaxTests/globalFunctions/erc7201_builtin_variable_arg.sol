contract C {
    string constant const = "ABC";
    string notConst = "XYZ";
    function constantArg() public pure returns (uint) {
        return erc7201(const);
    }
    function notConstantArg() public view returns (uint) {
        return erc7201(notConst);
    }
}
// ----
