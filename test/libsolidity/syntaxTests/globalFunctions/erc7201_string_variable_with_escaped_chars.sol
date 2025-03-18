contract C {
    string constant const = "\n\"\'\\abc\
def";
    string notConst = "\n\"\'\\abc\
def";
    function constArg() public pure returns (uint) {
        return erc7201(const);
    }
    function notConstArg() public view returns (uint) {
        return erc7201(notConst);
    }
}
// ----
