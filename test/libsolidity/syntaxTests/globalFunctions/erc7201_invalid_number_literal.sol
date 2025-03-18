contract C {
    function f() public pure returns (uint) {
        return erc7201(123);
    }
}
// ----
// TypeError 6896: (82-85): The argument to erc7201() builtin must be string.
