contract C {
    string constant unicodeStr = unicode"Hello 😃";
    function f() public pure returns (uint) {
        return erc7201(unicodeStr);
    }
}
// ----
