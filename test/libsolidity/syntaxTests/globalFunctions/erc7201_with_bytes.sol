bytes constant b = "abcdef";
contract C {
    function f() public pure returns (uint256) {
        return erc7201(b);
    }
}
// ----
// TypeError 6896: (114-115): The argument to erc7201() builtin must be string. The supplied argument has type bytes.
