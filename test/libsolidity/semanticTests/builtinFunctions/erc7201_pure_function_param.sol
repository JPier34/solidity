contract C {
    function f() public pure returns (string memory) {
        return "example.main";
    }
    function test() public pure returns (uint) {
        return erc7201(f());
    }
}
// ----
// test() -> 10958655983261152271848436692291137275443024275653522991983264966744321209600
